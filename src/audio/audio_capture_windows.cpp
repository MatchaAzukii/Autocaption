#ifdef PLATFORM_WINDOWS

#include "audio_capture.h"
#include "logger.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace autocaption {

struct WindowsAudioCapture::Impl {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audio_client = nullptr;
    IAudioCaptureClient* capture_client = nullptr;
    WAVEFORMATEX* format = nullptr;
    HANDLE event_handle = nullptr;
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (capture_client) {
            capture_client->Release();
            capture_client = nullptr;
        }
        if (audio_client) {
            audio_client->Stop();
            audio_client->Release();
            audio_client = nullptr;
        }
        if (format) {
            CoTaskMemFree(format);
            format = nullptr;
        }
        if (device) {
            device->Release();
            device = nullptr;
        }
        if (enumerator) {
            enumerator->Release();
            enumerator = nullptr;
        }
        if (event_handle) {
            CloseHandle(event_handle);
            event_handle = nullptr;
        }
        CoUninitialize();
    }
};

WindowsAudioCapture::WindowsAudioCapture() : impl_(std::make_unique<Impl>()) {}

WindowsAudioCapture::~WindowsAudioCapture() {
    stop();
}

bool WindowsAudioCapture::initialize(int sample_rate, int channels) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_ERROR("CoInitializeEx failed: " + std::to_string(hr));
        return false;
    }
    
    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&impl_->enumerator)
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create device enumerator: " + std::to_string(hr));
        return false;
    }
    
    // Get default audio endpoint (render device for loopback)
    hr = impl_->enumerator->GetDefaultAudioEndpoint(
        eRender,  // Render device for loopback
        eConsole, // Console role
        &impl_->device
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get default audio endpoint: " + std::to_string(hr));
        return false;
    }
    
    // Activate audio client
    hr = impl_->device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&impl_->audio_client)
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio client: " + std::to_string(hr));
        return false;
    }
    
    // Get mix format
    hr = impl_->audio_client->GetMixFormat(&impl_->format);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get mix format: " + std::to_string(hr));
        return false;
    }
    
    native_sample_rate_ = impl_->format->nSamplesPerSec;
    native_channels_ = impl_->format->nChannels;
    
    LOG_INFO("Audio format: " + std::to_string(native_sample_rate_) + "Hz, " +
             std::to_string(native_channels_) + "ch, " +
             std::to_string(impl_->format->wBitsPerSample) + "bit");
    
    // Initialize audio client in loopback mode
    REFERENCE_TIME buffer_duration = 10000000; // 1 second in 100-nanosecond units
    hr = impl_->audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0,
        impl_->format,
        nullptr
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize audio client: " + std::to_string(hr));
        return false;
    }
    
    // Create event handle
    impl_->event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!impl_->event_handle) {
        LOG_ERROR("Failed to create event handle");
        return false;
    }
    
    hr = impl_->audio_client->SetEventHandle(impl_->event_handle);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set event handle: " + std::to_string(hr));
        return false;
    }
    
    // Get capture client
    hr = impl_->audio_client->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&impl_->capture_client)
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get capture client: " + std::to_string(hr));
        return false;
    }
    
    LOG_INFO("Windows audio capture initialized (WASAPI loopback)");
    return true;
}

bool WindowsAudioCapture::start() {
    if (!impl_->audio_client) return false;
    
    HRESULT hr = impl_->audio_client->Start();
    if (FAILED(hr)) {
        LOG_ERROR("Failed to start audio client: " + std::to_string(hr));
        return false;
    }
    
    is_capturing_ = true;
    thread_ = std::thread(&WindowsAudioCapture::captureThread, this);
    
    LOG_INFO("Windows audio capture started");
    return true;
}

bool WindowsAudioCapture::stop() {
    is_capturing_ = false;
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    if (impl_->audio_client) {
        impl_->audio_client->Stop();
    }
    
    LOG_INFO("Windows audio capture stopped");
    return true;
}

void WindowsAudioCapture::captureThread() {
    const size_t buffer_size = native_sample_rate_ * native_channels_; // 1 second buffer
    std::vector<float> buffer(buffer_size);
    
    while (is_capturing_) {
        DWORD wait_result = WaitForSingleObject(impl_->event_handle, 100);
        if (wait_result != WAIT_OBJECT_0) continue;
        
        UINT32 packet_length = 0;
        HRESULT hr = impl_->capture_client->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) continue;
        
        while (packet_length != 0) {
            BYTE* data = nullptr;
            UINT32 frames_available = 0;
            DWORD flags = 0;
            
            hr = impl_->capture_client->GetBuffer(
                &data,
                &frames_available,
                &flags,
                nullptr,
                nullptr
            );
            if (FAILED(hr)) break;
            
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && callback_) {
                // Convert to float
                size_t samples = frames_available * native_channels_;
                if (samples > buffer.size()) {
                    buffer.resize(samples);
                }
                
                if (impl_->format->wBitsPerSample == 32) {
                    // Float32 format
                    const float* src = reinterpret_cast<const float*>(data);
                    std::copy(src, src + samples, buffer.begin());
                } else if (impl_->format->wBitsPerSample == 16) {
                    // Int16 format
                    const int16_t* src = reinterpret_cast<const int16_t*>(data);
                    for (size_t i = 0; i < samples; ++i) {
                        buffer[i] = src[i] / 32768.0f;
                    }
                }
                
                uint64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                
                callback_(buffer.data(), samples, timestamp_ms);
            }
            
            impl_->capture_client->ReleaseBuffer(frames_available);
            impl_->capture_client->GetNextPacketSize(&packet_length);
        }
    }
}

} // namespace autocaption

#endif // PLATFORM_WINDOWS
