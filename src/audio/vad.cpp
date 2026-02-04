#include "vad.h"
#include "logger.h"

namespace autocaption {

VAD::VAD(const Config& config) : config_(config) {
    history_.resize(config.smoothing_frames, false);
}

VAD::~VAD() = default;

VADResult VAD::process(const float* audio, size_t samples, int sample_rate) {
    bool current_speech = detectSpeech(audio, samples);
    
    // Add to history
    history_[history_pos_] = current_speech;
    history_pos_ = (history_pos_ + 1) % config_.smoothing_frames;
    
    // Smoothing: majority vote
    size_t speech_count = std::count(history_.begin(), history_.end(), true);
    bool smoothed_speech = speech_count > config_.smoothing_frames / 2;
    
    // Hangover logic
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (smoothed_speech) {
        is_speech_ = true;
    } else if (is_speech_) {
        float silence_duration = static_cast<float>(now_ms - last_speech_end_ms_);
        if (silence_duration > config_.hangover_ms) {
            is_speech_ = false;
        }
    }
    
    if (!smoothed_speech && is_speech_) {
        last_speech_end_ms_ = now_ms;
    }
    
    (void)samples; (void)sample_rate; // unused in this implementation
    float probability = static_cast<float>(speech_count) / config_.smoothing_frames;
    
    return VADResult{
        .is_speech = is_speech_,
        .speech_probability = probability,
        .timestamp_ms = now_ms
    };
}

void VAD::reset() {
    std::fill(history_.begin(), history_.end(), false);
    history_pos_ = 0;
    is_speech_ = false;
    last_speech_end_ms_ = 0;
}

bool VAD::detectSpeech(const float* audio, size_t samples) const {
    float rms = calculateRMS(audio, samples);
    
    // Count samples above threshold
    size_t above_threshold = 0;
    float threshold = config_.energy_threshold;
    
    for (size_t i = 0; i < samples; ++i) {
        if (std::abs(audio[i]) > threshold) {
            ++above_threshold;
        }
    }
    
    float ratio = static_cast<float>(above_threshold) / samples;
    return ratio > config_.speech_ratio_threshold || rms > config_.energy_threshold * 2;
}

// SileroVAD placeholder implementation
class SileroVAD::Impl {};

SileroVAD::SileroVAD() : impl_(std::make_unique<Impl>()) {}
SileroVAD::~SileroVAD() = default;

bool SileroVAD::initialize(const std::string& /*model_path*/) {
    LOG_INFO("SileroVAD initialization not yet implemented, using energy-based VAD");
    return false;
}

VADResult SileroVAD::process(const float* audio, size_t samples, int sample_rate) {
    // Fallback to energy-based VAD
    static VAD fallback_vad;
    return fallback_vad.process(audio, samples, sample_rate);
}

void SileroVAD::reset() {
}

} // namespace autocaption
