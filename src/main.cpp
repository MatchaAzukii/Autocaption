#include <iostream>
#include <string>
#include <signal.h>

#include "audio_processor.h"
#include "whisper_engine.h"
#include "json_emitter.h"
#include "logger.h"

using namespace autocaption;

namespace {
    std::atomic<bool> g_running{true};
    
    void signalHandler(int sig) {
        LOG_INFO("Received signal " + std::to_string(sig) + ", shutting down...");
        g_running = false;
    }
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -m, --model <path>       Path to Whisper model file (required)" << std::endl;
    std::cout << "  -l, --language <lang>    Language code (default: auto)" << std::endl;
    std::cout << "  -t, --threads <n>        Number of threads (default: 4)" << std::endl;
    std::cout << "  -p, --port <port>        WebSocket port (default: 8765)" << std::endl;
    std::cout << "  --stream-window <ms>     Streaming window in ms (default: 3000)" << std::endl;
    std::cout << "  --stdout                 Output to stdout as JSON lines" << std::endl;
    std::cout << "  --file <path>            Output to file as JSON lines" << std::endl;
    std::cout << "  --log <path>             Log file path" << std::endl;
    std::cout << "  --gpu                    Use GPU acceleration" << std::endl;
    std::cout << "  -h, --help               Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program << " -m models/ggml-base.bin" << std::endl;
    std::cout << "  " << program << " -m models/ggml-small.bin -l zh --stdout" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    WhisperConfig whisper_config;
    ASRConfig asr_config;
    
    std::string log_file;
    std::string output_file;
    uint16_t websocket_port = 8765;
    bool use_stdout = false;
    bool use_websocket = true;
    bool use_gpu = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-m" || arg == "--model") {
            if (++i < argc) whisper_config.model_path = argv[i];
        } else if (arg == "-l" || arg == "--language") {
            if (++i < argc) whisper_config.language = argv[i];
        } else if (arg == "-t" || arg == "--threads") {
            if (++i < argc) whisper_config.n_threads = std::stoi(argv[i]);
        } else if (arg == "-p" || arg == "--port") {
            if (++i < argc) websocket_port = static_cast<uint16_t>(std::stoi(argv[i]));
        } else if (arg == "--stream-window") {
            if (++i < argc) asr_config.stream_window_ms = std::stoul(argv[i]);
        } else if (arg == "--stdout") {
            use_stdout = true;
        } else if (arg == "--file") {
            if (++i < argc) output_file = argv[i];
        } else if (arg == "--log") {
            if (++i < argc) log_file = argv[i];
        } else if (arg == "--gpu") {
            use_gpu = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }
    
    // Check required arguments
    if (whisper_config.model_path.empty()) {
        std::cerr << "Error: Model path is required. Use -m or --model." << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Setup logging
    if (!log_file.empty()) {
        Logger::instance().setLogFile(log_file);
    }
    
    LOG_INFO("========================================");
    LOG_INFO("Autocaption - Real-time Speech-to-Text");
    LOG_INFO("========================================");
    
    whisper_config.use_gpu = use_gpu;
    asr_config.whisper_config = whisper_config;
    // vad_config is already default-initialized in asr_config
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#ifndef PLATFORM_WINDOWS
    signal(SIGQUIT, signalHandler);
#endif
    
    // Initialize audio pipeline
    AudioPipeline audio_pipeline;
    if (!audio_pipeline.initialize()) {
        LOG_ERROR("Failed to initialize audio pipeline");
        return 1;
    }
    
    // Initialize ASR pipeline
    ASRPipeline asr_pipeline;
    if (!asr_pipeline.initialize(asr_config)) {
        LOG_ERROR("Failed to initialize ASR pipeline");
        return 1;
    }
    
    // Initialize emitters
    CaptionServer ws_server;
    StdoutEmitter stdout_emitter;
    FileEmitter file_emitter;
    
    if (use_websocket) {
        if (!ws_server.start(websocket_port)) {
            LOG_WARN("Failed to start WebSocket server, continuing without it");
            use_websocket = false;
        }
    }
    
    if (!output_file.empty()) {
        if (!file_emitter.open(output_file)) {
            LOG_WARN("Failed to open output file: " + output_file);
        }
    }
    
    // Set up transcription callback
    asr_pipeline.setTranscriptionCallback(
        [&](const CaptionSegment& segment) {
            if (use_websocket && ws_server.isRunning()) {
                ws_server.broadcast(segment);
            }
            if (use_stdout) {
                stdout_emitter.emit(segment);
            }
            if (!output_file.empty()) {
                file_emitter.emit(segment);
            }
        }
    );
    
    // Start audio capture
    if (!audio_pipeline.start()) {
        LOG_ERROR("Failed to start audio pipeline");
        return 1;
    }
    
    // Start ASR processing
    std::thread asr_thread([&]() {
        asr_pipeline.processLoop(audio_pipeline.getSegmentQueue());
    });
    
    LOG_INFO("========================================");
    LOG_INFO("System ready! Capturing audio...");
    if (use_websocket) {
        LOG_INFO("WebSocket server running on ws://localhost:" + std::to_string(websocket_port));
    }
    LOG_INFO("Press Ctrl+C to stop");
    LOG_INFO("========================================");
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Print status periodically
        static auto last_status = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status).count() >= 60) {
            LOG_INFO("Status: capturing... (queue: " + 
                     std::to_string(audio_pipeline.getSegmentQueue().size()) + 
                     ", WS clients: " + 
                     std::to_string(ws_server.getClientCount()) + ")");
            last_status = now;
        }
    }
    
    // Shutdown
    LOG_INFO("Shutting down...");
    
    // Stop the queue first to wake up ASR thread
    audio_pipeline.getSegmentQueue().stop();
    
    asr_pipeline.stop();
    audio_pipeline.stop();
    ws_server.stop();
    
    if (asr_thread.joinable()) {
        asr_thread.join();
    }
    
    LOG_INFO("Goodbye!");
    return 0;
}
