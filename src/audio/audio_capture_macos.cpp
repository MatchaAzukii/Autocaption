#ifdef PLATFORM_MACOS

#include "audio_capture.h"
#include "logger.h"
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

namespace autocaption {

struct MacOSAudioCapture::Impl {
    AudioUnit audio_unit = nullptr;
    AudioDeviceID device_id = kAudioObjectUnknown;
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (audio_unit) {
            AudioOutputUnitStop(audio_unit);
            AudioUnitUninitialize(audio_unit);
            AudioComponentInstanceDispose(audio_unit);
            audio_unit = nullptr;
        }
    }
};

// Callback for audio input
static OSStatus AudioInputCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData) {
    auto* capture = static_cast<MacOSAudioCapture*>(inRefCon);
    auto* impl = capture->getImpl();
    
    AudioBufferList buffer_list;
    buffer_list.mNumberBuffers = 1;
    buffer_list.mBuffers[0].mNumberChannels = capture->getNativeChannels();
    buffer_list.mBuffers[0].mDataByteSize = inNumberFrames * capture->getNativeChannels() * sizeof(float);
    
    std::vector<float> buffer(inNumberFrames * capture->getNativeChannels());
    buffer_list.mBuffers[0].mData = buffer.data();
    
    OSStatus status = AudioUnitRender(impl->audio_unit, ioActionFlags, inTimeStamp,
                                       inBusNumber, inNumberFrames, &buffer_list);
    auto callback = capture->getCallback();
    if (status == noErr && callback) {
        uint64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        callback(buffer.data(), inNumberFrames * capture->getNativeChannels(), timestamp_ms);
    }
    
    return noErr;
}

MacOSAudioCapture::MacOSAudioCapture() : impl_(std::make_unique<Impl>()) {}

MacOSAudioCapture::~MacOSAudioCapture() {
    stop();
}

bool MacOSAudioCapture::initialize(int sample_rate, int channels) {
    // Find default input device (BlackHole 2ch should be set as default)
    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    UInt32 data_size = sizeof(AudioDeviceID);
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                  &property_address,
                                                  0, nullptr,
                                                  &data_size, &impl_->device_id);
    if (status != noErr) {
        LOG_ERROR("Failed to get default input device");
        return false;
    }
    
    // Get device name
    CFStringRef device_name = nullptr;
    data_size = sizeof(CFStringRef);
    property_address.mSelector = kAudioObjectPropertyName;
    status = AudioObjectGetPropertyData(impl_->device_id, &property_address,
                                        0, nullptr, &data_size, &device_name);
    if (status == noErr && device_name) {
        char name[256];
        CFStringGetCString(device_name, name, sizeof(name), kCFStringEncodingUTF8);
        LOG_INFO("Using audio device: " + std::string(name));
        CFRelease(device_name);
    }
    
    // Create AudioUnit
    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        LOG_ERROR("Failed to find HAL output component");
        return false;
    }
    
    status = AudioComponentInstanceNew(component, &impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to create AudioUnit instance");
        return false;
    }
    
    // Set device
    status = AudioUnitSetProperty(impl_->audio_unit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global,
                                  0,
                                  &impl_->device_id,
                                  sizeof(impl_->device_id));
    if (status != noErr) {
        LOG_ERROR("Failed to set audio device");
        return false;
    }
    
    // Enable input
    UInt32 enable_input = 1;
    status = AudioUnitSetProperty(impl_->audio_unit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  1,
                                  &enable_input,
                                  sizeof(enable_input));
    if (status != noErr) {
        LOG_ERROR("Failed to enable input");
        return false;
    }
    
    // Disable output
    UInt32 enable_output = 0;
    status = AudioUnitSetProperty(impl_->audio_unit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,
                                  &enable_output,
                                  sizeof(enable_output));
    
    // Set format
    AudioStreamBasicDescription format = {};
    format.mSampleRate = sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBytesPerPacket = channels * sizeof(float);
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = channels * sizeof(float);
    format.mChannelsPerFrame = channels;
    format.mBitsPerChannel = 32;
    
    status = AudioUnitSetProperty(impl_->audio_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  1,
                                  &format,
                                  sizeof(format));
    if (status != noErr) {
        LOG_ERROR("Failed to set stream format");
        return false;
    }
    
    // Set callback
    AURenderCallbackStruct callback_struct = {};
    callback_struct.inputProc = AudioInputCallback;
    callback_struct.inputProcRefCon = this;
    
    status = AudioUnitSetProperty(impl_->audio_unit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &callback_struct,
                                  sizeof(callback_struct));
    if (status != noErr) {
        LOG_ERROR("Failed to set input callback");
        return false;
    }
    
    // Initialize
    status = AudioUnitInitialize(impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to initialize AudioUnit");
        return false;
    }
    
    native_sample_rate_ = sample_rate;
    native_channels_ = channels;
    
    LOG_INFO("macOS audio capture initialized (CoreAudio HAL)");
    return true;
}

bool MacOSAudioCapture::start() {
    if (!impl_->audio_unit) return false;
    
    OSStatus status = AudioOutputUnitStart(impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to start audio unit");
        return false;
    }
    
    is_capturing_ = true;
    LOG_INFO("macOS audio capture started");
    LOG_INFO("NOTE: For system audio capture, use BlackHole 2ch with Multi-Output Device");
    return true;
}

bool MacOSAudioCapture::stop() {
    is_capturing_ = false;
    
    if (impl_->audio_unit) {
        AudioOutputUnitStop(impl_->audio_unit);
    }
    
    LOG_INFO("macOS audio capture stopped");
    return true;
}

void MacOSAudioCapture::captureThread() {
    // Audio is captured in callback, this thread just keeps alive
    while (is_capturing_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace autocaption

#endif // PLATFORM_MACOS
