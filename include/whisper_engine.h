#pragma once

#include "common.h"
#include "ring_buffer.h"
#include "vad.h"

// Forward declaration for whisper
struct whisper_context;
struct whisper_full_params;

namespace autocaption {

// Whisper ASR engine configuration
struct WhisperConfig {
    std::string model_path;
    std::string language = "auto";  // "auto" for auto-detect, or "en", "zh", etc.
    bool translate = false;         // Translate to English
    int beam_size = 5;
    float word_thold = 0.01f;
    int n_threads = 4;
    bool use_gpu = false;
    
    // Streaming parameters
    size_t step_ms = 1000;          // Step size for streaming
    size_t length_ms = 10000;       // Max audio length to process at once
    bool keep_context = true;       // Keep context between chunks
};

// Whisper ASR engine
class WhisperEngine {
public:
    WhisperEngine();
    ~WhisperEngine();

    // Initialize whisper model
    bool initialize(const WhisperConfig& config);

    // Process audio segment and get transcription
    struct Transcription {
        float start_time;
        float end_time;
        std::string text;
        float confidence;
        std::string language;
    };
    std::vector<Transcription> transcribe(const float* audio, size_t samples, 
                                           uint64_t timestamp_offset_ms = 0);

    // Streaming transcribe - process partial buffer
    std::vector<Transcription> transcribeStreaming(const float* audio, size_t samples,
                                                    bool use_previous_context = true);

    // Check if initialized
    bool isInitialized() const { return ctx_ != nullptr; }

    // Get supported languages
    static std::vector<std::pair<std::string, std::string>> getSupportedLanguages();

    // Shutdown
    void shutdown();

private:
    WhisperConfig config_;
    whisper_context* ctx_ = nullptr;
    
    // Context for streaming
    std::vector<float> audio_buffer_;
    std::vector<Transcription> previous_transcriptions_;
    
    // Thread safety
    mutable std::mutex mutex_;
};

// ASR Pipeline configuration
struct ASRConfig {
    WhisperConfig whisper_config;
    VAD::Config vad_config;
    size_t min_speech_duration_ms = 500;
    size_t max_speech_duration_ms = 30000;
    size_t stream_window_ms = 3000;  // Step size for sliding window (default: 3000ms)
    bool use_sliding_window = true;  // Enable sliding window for better context
};

// ASR Pipeline: VAD + Whisper
class ASRPipeline {
public:
    ASRPipeline();
    ~ASRPipeline();

    bool initialize(const ASRConfig& config);
    void shutdown();

    // Process audio from queue
    void processLoop(AudioSegmentQueue& queue);

    // Set callback for transcription results
    using TranscriptionCallback = std::function<void(const CaptionSegment& segment)>;
    void setTranscriptionCallback(TranscriptionCallback callback) {
        transcription_callback_ = callback;
    }

    // Stop processing
    void stop() { running_ = false; }

private:
    std::vector<WhisperEngine::Transcription> processSpeechSegment(
        const float* audio, size_t samples, 
        uint64_t start_ms, uint64_t end_ms,
        const std::string& context = "");

    std::unique_ptr<WhisperEngine> whisper_;
    std::unique_ptr<VAD> vad_;
    
    ASRConfig config_;
    TranscriptionCallback transcription_callback_;
    std::atomic<bool> running_{false};
    
    uint64_t segment_id_ = 0;
    
    // Speech buffer for accumulating audio between VAD triggers
    std::vector<float> speech_buffer_;
};

} // namespace autocaption
