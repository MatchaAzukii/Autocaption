#pragma once

#include "common.h"
#include "ring_buffer.h"

namespace autocaption {

// Abstract base class for platform-specific audio capture
class AudioCapture {
public:
    using AudioCallback = std::function<void(const float* data, size_t samples, uint64_t timestamp_ms)>;

    AudioCapture();
    virtual ~AudioCapture();

    // Initialize capture
    virtual bool initialize(int sample_rate = 48000, int channels = 2) = 0;

    // Start capture
    virtual bool start() = 0;

    // Stop capture
    virtual bool stop() = 0;

    // Check if capturing
    virtual bool isCapturing() const = 0;

    // Get native sample rate
    int getNativeSampleRate() const { return native_sample_rate_; }
    int getNativeChannels() const { return native_channels_; }

    // Set callback for audio data
    void setCallback(AudioCallback callback) { callback_ = callback; }
    
    // Get callback (for internal use)
    AudioCallback getCallback() const { return callback_; }

    // Factory method
    static std::unique_ptr<AudioCapture> create();

protected:
    AudioCallback callback_;
    int native_sample_rate_ = 48000;
    int native_channels_ = 2;
    std::atomic<bool> is_capturing_{false};
};

// Platform-specific implementations
#ifdef PLATFORM_WINDOWS
class WindowsAudioCapture : public AudioCapture {
public:
    WindowsAudioCapture();
    ~WindowsAudioCapture() override;

    bool initialize(int sample_rate = 48000, int channels = 2) override;
    bool start() override;
    bool stop() override;
    bool isCapturing() const override { return is_capturing_.load(); }

private:
    void captureThread();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::thread thread_;
};
#endif

#ifdef PLATFORM_MACOS
// Forward declaration for callback
class MacOSAudioCapture;

class MacOSAudioCapture : public AudioCapture {
public:
    MacOSAudioCapture();
    ~MacOSAudioCapture() override;

    bool initialize(int sample_rate = 48000, int channels = 2) override;
    bool start() override;
    bool stop() override;
    bool isCapturing() const override { return is_capturing_.load(); }

protected:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::thread thread_;

public:
    // For internal callback use
    Impl* getImpl() { return impl_.get(); }

private:
    void captureThread();
};
#endif

#ifdef PLATFORM_LINUX
class LinuxAudioCapture : public AudioCapture {
public:
    LinuxAudioCapture();
    ~LinuxAudioCapture() override;

    bool initialize(int sample_rate = 48000, int channels = 2) override;
    bool start() override;
    bool stop() override;
    bool isCapturing() const override { return is_capturing_.load(); }

private:
    void captureThread();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::thread thread_;
};
#endif

} // namespace autocaption
