#include "audio_processor.h"
#include "logger.h"

namespace autocaption {

AudioProcessor::AudioProcessor(int input_sample_rate, int input_channels)
    : input_sample_rate_(input_sample_rate)
    , input_channels_(input_channels) {
    resample_ratio_ = static_cast<double>(SAMPLE_RATE) / input_sample_rate;
    resample_buffer_.resize(1024);
    LOG_INFO("AudioProcessor initialized: " + std::to_string(input_sample_rate) + 
             "Hz " + std::to_string(input_channels) + "ch -> " +
             std::to_string(SAMPLE_RATE) + "Hz mono");
}

AudioProcessor::~AudioProcessor() = default;

std::vector<float> AudioProcessor::process(const float* input, size_t samples) {
    // Input samples are per-channel
    size_t frames = samples / input_channels_;
    return processInterleaved(input, frames);
}

std::vector<float> AudioProcessor::processInterleaved(const float* input, size_t frames) {
    // Step 1: Downmix to mono
    std::vector<float> mono;
    mono.reserve(frames);
    
    if (input_channels_ == 1) {
        mono.assign(input, input + frames);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < input_channels_; ++ch) {
                sum += input[i * input_channels_ + ch];
            }
            mono.push_back(sum / input_channels_);
        }
    }
    
    // Step 2: Resample
    std::vector<float> resampled = resample(mono.data(), mono.size());
    
    // Step 3: Normalize
    normalizeAudio(resampled.data(), resampled.size());
    
    return resampled;
}

std::vector<float> AudioProcessor::resample(const float* input, size_t samples) {
    if (input_sample_rate_ == SAMPLE_RATE) {
        return std::vector<float>(input, input + samples);
    }
    
    size_t output_samples = static_cast<size_t>(samples * resample_ratio_);
    std::vector<float> output;
    output.reserve(output_samples);
    
    for (size_t i = 0; i < output_samples; ++i) {
        double src_idx = i / resample_ratio_;
        size_t idx0 = static_cast<size_t>(src_idx);
        size_t idx1 = std::min(idx0 + 1, samples - 1);
        
        double frac = src_idx - idx0;
        float val = static_cast<float>((1.0 - frac) * input[idx0] + frac * input[idx1]);
        output.push_back(val);
    }
    
    return output;
}

// AudioPipeline implementation
AudioPipeline::AudioPipeline() = default;

AudioPipeline::~AudioPipeline() {
    stop();
}

bool AudioPipeline::initialize() {
    // Create audio capture
    capture_ = AudioCapture::create();
    if (!capture_) {
        LOG_ERROR("Failed to create audio capture");
        return false;
    }
    
    if (!capture_->initialize()) {
        LOG_ERROR("Failed to initialize audio capture");
        return false;
    }
    
    // Create audio processor
    processor_ = std::make_unique<AudioProcessor>(
        capture_->getNativeSampleRate(),
        capture_->getNativeChannels()
    );
    
    // Create ring buffer (5 seconds at 16kHz mono)
    ring_buffer_ = std::make_unique<RingBuffer>(SAMPLE_RATE * RING_BUFFER_SECONDS);
    
    // Set up capture callback
    capture_->setCallback([this](const float* data, size_t samples, uint64_t timestamp_ms) {
        onAudioCaptured(data, samples, timestamp_ms);
    });
    
    LOG_INFO("AudioPipeline initialized");
    return true;
}

bool AudioPipeline::start() {
    if (!capture_) return false;
    
    running_ = true;
    
    if (!capture_->start()) {
        LOG_ERROR("Failed to start audio capture");
        return false;
    }
    
    processing_thread_ = std::thread(&AudioPipeline::processingLoop, this);
    
    LOG_INFO("AudioPipeline started");
    return true;
}

void AudioPipeline::stop() {
    running_ = false;
    
    if (capture_) {
        capture_->stop();
    }
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    LOG_INFO("AudioPipeline stopped");
}

void AudioPipeline::processingLoop() {
    const size_t window_samples = SAMPLE_RATE * PROCESSING_WINDOW_MS / 1000;
    std::vector<float> buffer(window_samples);
    
    while (running_) {
        // Wait for enough data
        if (ring_buffer_->available() < window_samples) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Read window
        size_t read = ring_buffer_->read(buffer.data(), window_samples);
        if (read == 0) continue;
        
        // Create segment
        AudioSegmentQueue::Segment segment;
        segment.data.assign(buffer.data(), buffer.data() + read);
        segment.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        segment.is_speech = false; // Will be determined by VAD later
        
        segment_queue_.push(std::move(segment));
    }
}

void AudioPipeline::onAudioCaptured(const float* data, size_t samples, uint64_t /*timestamp_ms*/) {
    // Process audio: downmix and resample
    auto processed = processor_->process(data, samples);
    
    if (!processed.empty()) {
        // Write to ring buffer
        ring_buffer_->write(processed.data(), processed.size());
    }
}

} // namespace autocaption
