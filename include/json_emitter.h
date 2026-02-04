#pragma once

#include "common.h"
#include "whisper_engine.h"

namespace autocaption {

// JSON emitter for caption events
class JsonEmitter {
public:
    JsonEmitter();
    ~JsonEmitter();

    // Convert caption segment to JSON
    static std::string toJson(const CaptionSegment& segment);
    
    // Convert transcription to caption segment
    static CaptionSegment toCaptionSegment(const WhisperEngine::Transcription& trans,
                                            uint64_t id);

    // Batch convert
    static std::string toJsonArray(const std::vector<CaptionSegment>& segments);
};

// WebSocket server for real-time caption streaming
class CaptionServer {
public:
    using Port = uint16_t;
    
    CaptionServer();
    ~CaptionServer();

    // Start server on port
    bool start(Port port = 8765);
    
    // Stop server
    void stop();

    // Broadcast caption to all connected clients
    void broadcast(const CaptionSegment& segment);
    void broadcast(const std::string& json_message);

    // Check if running
    bool isRunning() const;

    // Get number of connected clients
    size_t getClientCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Stdout emitter for simple IPC
class StdoutEmitter {
public:
    StdoutEmitter();
    ~StdoutEmitter();

    // Emit caption to stdout as JSON line
    void emit(const CaptionSegment& segment);
    void emit(const std::string& json_message);

    // Enable/disable
    void setEnabled(bool enabled) { enabled_ = enabled; }

private:
    std::mutex mutex_;
    bool enabled_ = true;
};

// File emitter for logging
class FileEmitter {
public:
    FileEmitter();
    ~FileEmitter();

    // Open log file
    bool open(const std::string& filepath);
    void close();

    // Emit caption to file
    void emit(const CaptionSegment& segment);
    void emit(const std::string& json_message);

private:
    std::ofstream file_;
    std::mutex mutex_;
};

} // namespace autocaption
