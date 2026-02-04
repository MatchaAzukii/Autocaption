#ifdef PLATFORM_LINUX

#include "audio_capture.h"
#include "logger.h"
#include <pulse/simple.h>
#include <pulse/error.h>

namespace autocaption {

struct LinuxAudioCapture::Impl {
    pa_simple* pa = nullptr;
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (pa) {
            pa_simple_free(pa);
            pa = nullptr;
        }
    }
};

LinuxAudioCapture::LinuxAudioCapture() : impl_(std::make_unique<Impl>()) {}

LinuxAudioCapture::~LinuxAudioCapture() {
    stop();
}

bool LinuxAudioCapture::initialize(int sample_rate, int channels) {
    pa_sample_spec ss = {};
    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.rate = sample_rate;
    ss.channels = channels;
    
    pa_buffer_attr buffer_attr = {};
    buffer_attr.maxlength = static_cast<uint32_t>(-1);
    buffer_attr.tlength = static_cast<uint32_t>(-1);
    buffer_attr.prebuf = static_cast<uint32_t>(-1);
    buffer_attr.minreq = static_cast<uint32_t>(-1);
    buffer_attr.fragsize = sample_rate * channels * sizeof(float) / 10; // 100ms
    
    int error = 0;
    impl_->pa = pa_simple_new(
        nullptr,                    // Server
        "autocaption",              // Application name
        PA_STREAM_RECORD,           // Direction
        nullptr,                    // Device (default monitor)
        "System Audio",             // Stream description
        &ss,                        // Sample spec
        nullptr,                    // Channel map
        &buffer_attr,               // Buffer attributes
        &error                      // Error code
    );
    
    if (!impl_->pa) {
        LOG_ERROR("Failed to create PulseAudio stream: " + std::string(pa_strerror(error)));
        return false;
    }
    
    native_sample_rate_ = sample_rate;
    native_channels_ = channels;
    
    LOG_INFO("Linux audio capture initialized (PulseAudio)");
    LOG_INFO("NOTE: Capture from monitor source for system audio");
    return true;
}

bool LinuxAudioCapture::start() {
    if (!impl_->pa) return false;
    
    is_capturing_ = true;
    thread_ = std::thread(&LinuxAudioCapture::captureThread, this);
    
    LOG_INFO("Linux audio capture started");
    return true;
}

bool LinuxAudioCapture::stop() {
    is_capturing_ = false;
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    LOG_INFO("Linux audio capture stopped");
    return true;
}

void LinuxAudioCapture::captureThread() {
    const size_t frames_per_read = native_sample_rate_ / 10; // 100ms
    const size_t samples_per_read = frames_per_read * native_channels_;
    std::vector<float> buffer(samples_per_read);
    
    int error = 0;
    
    while (is_capturing_) {
        int ret = pa_simple_read(impl_->pa, buffer.data(), 
                                  buffer.size() * sizeof(float), &error);
        if (ret < 0) {
            LOG_ERROR("PulseAudio read error: " + std::string(pa_strerror(error)));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (callback_) {
            uint64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            callback_(buffer.data(), samples_per_read, timestamp_ms);
        }
    }
}

} // namespace autocaption

#endif // PLATFORM_LINUX
