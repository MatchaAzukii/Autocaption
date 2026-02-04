#include "json_emitter.h"
#include "logger.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <unordered_set>

namespace autocaption {

namespace {
    // UTF-8 safe JSON string escape
    std::string escapeJsonStringImpl(const std::string& input) {
        std::ostringstream oss;
        oss << '"';
        
        for (size_t i = 0; i < input.length(); ) {
            unsigned char c = input[i];
            
            // ASCII characters
            if (c < 0x80) {
                switch (c) {
                    case '"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (c >= 0x20) {
                            oss << c;
                        } else {
                            // Control characters
                            oss << "\\u" << std::hex << std::setfill('0') << std::setw(4) << (int)c;
                            oss << std::dec; // Reset to decimal
                        }
                }
                ++i;
            }
            // UTF-8 multi-byte sequences - pass through as-is (JSON supports UTF-8)
            else {
                // Determine sequence length
                size_t seq_len = 0;
                if ((c & 0xE0) == 0xC0) seq_len = 2;      // 2-byte
                else if ((c & 0xF0) == 0xE0) seq_len = 3; // 3-byte (Chinese)
                else if ((c & 0xF8) == 0xF0) seq_len = 4; // 4-byte
                else seq_len = 1; // Invalid, treat as single byte
                
                // Output the complete UTF-8 sequence
                if (i + seq_len <= input.length()) {
                    oss.write(input.data() + i, seq_len);
                    i += seq_len;
                } else {
                    // Incomplete sequence, output replacement
                    oss << "\uFFFD";
                    ++i;
                }
            }
        }
        
        oss << '"';
        return oss.str();
    }
}

// CaptionSegment implementation
std::string CaptionSegment::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":" << id << ",";
    oss << "\"start\":" << std::fixed << std::setprecision(3) << start_time << ",";
    oss << "\"end\":" << std::fixed << std::setprecision(3) << end_time << ",";
    oss << "\"source_text\":" << escapeJsonStringImpl(source_text) << ",";
    oss << "\"translated_text\":" << escapeJsonStringImpl(translated_text) << ",";
    oss << "\"language\":" << escapeJsonStringImpl(language);
    oss << "}";
    return oss.str();
}

// JsonEmitter implementation
JsonEmitter::JsonEmitter() = default;
JsonEmitter::~JsonEmitter() = default;

std::string JsonEmitter::toJson(const CaptionSegment& segment) {
    return segment.toJson();
}

CaptionSegment JsonEmitter::toCaptionSegment(const WhisperEngine::Transcription& trans,
                                              uint64_t id) {
    CaptionSegment segment;
    segment.id = id;
    segment.start_time = trans.start_time;
    segment.end_time = trans.end_time;
    segment.source_text = trans.text;
    segment.translated_text = "";
    segment.language = trans.language;
    return segment;
}

std::string JsonEmitter::toJsonArray(const std::vector<CaptionSegment>& segments) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) oss << ",";
        oss << segments[i].toJson();
    }
    oss << "]";
    return oss.str();
}

// WebSocket server implementation
using websocket_server = websocketpp::server<websocketpp::config::asio>;
using connection_hdl = websocketpp::connection_hdl;

struct CaptionServer::Impl {
    websocket_server server;
    std::vector<connection_hdl> connections;
    std::mutex connections_mutex;
    std::thread server_thread;
    std::atomic<bool> running{false};
    
    void onOpen(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        connections.push_back(hdl);
        LOG_INFO("WebSocket client connected. Total: " + std::to_string(connections.size()));
    }
    
    void onClose(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = std::find_if(connections.begin(), connections.end(),
            [&hdl](const connection_hdl& elem) {
                return !elem.owner_before(hdl) && !hdl.owner_before(elem);
            });
        if (it != connections.end()) {
            connections.erase(it);
        }
        LOG_INFO("WebSocket client disconnected. Total: " + std::to_string(connections.size()));
    }
    
    void onMessage(connection_hdl /*hdl*/, websocket_server::message_ptr msg) {
        // Handle incoming messages if needed
        LOG_DEBUG("Received WebSocket message: " + msg->get_payload());
    }
};

CaptionServer::CaptionServer() : impl_(std::make_unique<Impl>()) {}

CaptionServer::~CaptionServer() {
    stop();
}

bool CaptionServer::start(Port port) {
    try {
        impl_->server.init_asio();
        impl_->server.set_reuse_addr(true);
        
        impl_->server.set_open_handler([this](connection_hdl hdl) {
            impl_->onOpen(hdl);
        });
        
        impl_->server.set_close_handler([this](connection_hdl hdl) {
            impl_->onClose(hdl);
        });
        
        impl_->server.set_message_handler([this](connection_hdl hdl, websocket_server::message_ptr msg) {
            impl_->onMessage(hdl, msg);
        });
        
        impl_->server.listen(port);
        impl_->server.start_accept();
        
        impl_->running = true;
        impl_->server_thread = std::thread([this]() {
            impl_->server.run();
        });
        
        LOG_INFO("WebSocket server started on port " + std::to_string(port));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start WebSocket server: " + std::string(e.what()));
        return false;
    }
}

void CaptionServer::stop() {
    impl_->running = false;
    
    if (impl_->server_thread.joinable()) {
        impl_->server.stop();
        impl_->server_thread.join();
    }
    
    LOG_INFO("WebSocket server stopped");
}

void CaptionServer::broadcast(const CaptionSegment& segment) {
    broadcast(segment.toJson());
}

void CaptionServer::broadcast(const std::string& json_message) {
    std::lock_guard<std::mutex> lock(impl_->connections_mutex);
    
    for (auto& hdl : impl_->connections) {
        try {
            impl_->server.send(hdl, json_message, websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to send WebSocket message: " + std::string(e.what()));
        }
    }
}

bool CaptionServer::isRunning() const {
    return impl_->running;
}

size_t CaptionServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(impl_->connections_mutex);
    return impl_->connections.size();
}

// StdoutEmitter implementation
StdoutEmitter::StdoutEmitter() = default;
StdoutEmitter::~StdoutEmitter() = default;

void StdoutEmitter::emit(const CaptionSegment& segment) {
    emit(segment.toJson());
}

void StdoutEmitter::emit(const std::string& json_message) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << json_message << std::endl;
}

// FileEmitter implementation
FileEmitter::FileEmitter() = default;

FileEmitter::~FileEmitter() {
    close();
}

bool FileEmitter::open(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(filepath, std::ios::app);
    return file_.is_open();
}

void FileEmitter::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

void FileEmitter::emit(const CaptionSegment& segment) {
    emit(segment.toJson());
}

void FileEmitter::emit(const std::string& json_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_ << json_message << std::endl;
        file_.flush();
    }
}

} // namespace autocaption
