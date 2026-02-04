#include "audio_capture.h"
#include "logger.h"

namespace autocaption {

AudioCapture::AudioCapture() = default;
AudioCapture::~AudioCapture() = default;

std::unique_ptr<AudioCapture> AudioCapture::create() {
#ifdef PLATFORM_WINDOWS
    LOG_INFO("Creating Windows audio capture (WASAPI)");
    return std::make_unique<WindowsAudioCapture>();
#elif defined(PLATFORM_MACOS)
    LOG_INFO("Creating macOS audio capture (CoreAudio)");
    return std::make_unique<MacOSAudioCapture>();
#elif defined(PLATFORM_LINUX)
    LOG_INFO("Creating Linux audio capture (PulseAudio)");
    return std::make_unique<LinuxAudioCapture>();
#else
    LOG_ERROR("Unsupported platform");
    return nullptr;
#endif
}

} // namespace autocaption
