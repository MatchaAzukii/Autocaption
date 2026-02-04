#pragma once

#include "common.h"

namespace autocaption {

// Voice Activity Detection using energy-based approach with smoothing
// Can be replaced with Silero VAD model later
class VAD {
public:
    struct Config {
        float energy_threshold;      // RMS energy threshold
        float speech_ratio_threshold; // Ratio of frames above threshold
        size_t window_ms;              // Analysis window in ms
        size_t smoothing_frames;         // Number of frames for smoothing
        float hangover_ms;          // Hangover time after speech ends
        
        Config() 
            : energy_threshold(0.01f)
            , speech_ratio_threshold(0.6f)
            , window_ms(100)
            , smoothing_frames(3)
            , hangover_ms(300.0f) {}
    };

    explicit VAD(const Config& config = Config());
    ~VAD();

    // Process audio frame, returns VAD result
    VADResult process(const float* audio, size_t samples, int sample_rate);

    // Reset state
    void reset();

    // Update configuration
    void setConfig(const Config& config) { config_ = config; }

private:
    Config config_;
    
    // State
    std::vector<bool> history_;
    size_t history_pos_ = 0;
    bool is_speech_ = false;
    uint64_t last_speech_end_ms_ = 0;
    
    // Energy calculation
    bool detectSpeech(const float* audio, size_t samples) const;
};

// Advanced VAD using Silero model (placeholder for future implementation)
class SileroVAD {
public:
    struct Config {
        float threshold = 0.5f;
        float min_speech_duration_ms = 250.0f;
        float max_speech_duration_s = 30.0f;
        float min_silence_duration_ms = 100.0f;
    };

    SileroVAD();
    ~SileroVAD();

    bool initialize(const std::string& model_path);
    VADResult process(const float* audio, size_t samples, int sample_rate);
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace autocaption
