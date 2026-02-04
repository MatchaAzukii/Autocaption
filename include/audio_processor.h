#pragma once

#include "common.h"
#include "ring_buffer.h"
#include "audio_capture.h"

namespace autocaption {

// Audio processor: resample, downmix, normalize
class AudioProcessor {
public:
    AudioProcessor(int input_sample_rate, int input_channels);
    ~AudioProcessor();

    // Process audio buffer: downmix + resample + normalize
    std::vector<float> process(const float* input, size_t samples);

    // Process interleaved stereo to mono float at target sample rate
    std::vector<float> processInterleaved(const float* input, size_t frames);

    // Get output sample rate (always 16000)
    static constexpr int getOutputSampleRate() { return SAMPLE_RATE; }
    static constexpr int getOutputChannels() { return CHANNELS; }

private:
    int input_sample_rate_;
    int input_channels_;
    
    // Simple linear interpolation resampler state
    double resample_ratio_;
    std::vector<float> resample_buffer_;
    
    // Resample using linear interpolation
    std::vector<float> resample(const float* input, size_t samples);
};

// Audio pipeline: capture -> process -> VAD -> ASR
class AudioPipeline {
public:
    AudioPipeline();
    ~AudioPipeline();

    // Initialize pipeline
    bool initialize();

    // Start processing
    bool start();

    // Stop processing
    void stop();

    // Get processed audio queue
    AudioSegmentQueue& getSegmentQueue() { return segment_queue_; }

    // Set VAD callback
    using VADCallback = std::function<void(bool is_speech, uint64_t timestamp_ms)>;
    void setVADCallback(VADCallback callback) { vad_callback_ = callback; }

private:
    void processingLoop();
    void onAudioCaptured(const float* data, size_t samples, uint64_t timestamp_ms);

    std::unique_ptr<AudioCapture> capture_;
    std::unique_ptr<AudioProcessor> processor_;
    std::unique_ptr<RingBuffer> ring_buffer_;
    
    AudioSegmentQueue segment_queue_;
    VADCallback vad_callback_;
    
    std::thread processing_thread_;
    std::atomic<bool> running_{false};
    
    static constexpr size_t RING_BUFFER_SECONDS = 5;
    static constexpr size_t PROCESSING_WINDOW_MS = 100; // 100ms windows
};

} // namespace autocaption
