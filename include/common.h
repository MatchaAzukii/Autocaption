#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

// Platform detection - use predefined macros from CMake if available
#ifndef PLATFORM_WINDOWS
    #ifdef _WIN32
        #define PLATFORM_WINDOWS
        #ifndef NOMINMAX
            #define NOMINMAX
        #endif
        #include <windows.h>
    #endif
#endif

#ifndef PLATFORM_MACOS
    #ifdef __APPLE__
        #define PLATFORM_MACOS
        #include <TargetConditionals.h>
    #endif
#endif

#ifndef PLATFORM_LINUX
    #if !defined(_WIN32) && !defined(__APPLE__)
        #define PLATFORM_LINUX
    #endif
#endif

namespace autocaption {

// Audio format constants
constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;
constexpr float MS_PER_FRAME = 1000.0f / SAMPLE_RATE;

// Audio frame structure
struct AudioFrame {
    std::vector<float> data;
    uint64_t timestamp_ms;
    
    AudioFrame() = default;
    explicit AudioFrame(size_t samples) : data(samples) {}
};

// Caption segment structure
struct CaptionSegment {
    uint64_t id;
    float start_time;
    float end_time;
    std::string source_text;
    std::string translated_text;
    std::string language;
    
    std::string toJson() const;
};

// VAD result structure
struct VADResult {
    bool is_speech;
    float speech_probability;
    uint64_t timestamp_ms;
};

// Utility functions
inline std::string formatTimestamp(float seconds) {
    int hrs = static_cast<int>(seconds) / 3600;
    int mins = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hrs << ":"
        << std::setw(2) << mins << ":"
        << std::setw(2) << secs << "."
        << std::setw(3) << ms;
    return oss.str();
}

inline float calculateRMS(const float* data, size_t count) {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum += data[i] * data[i];
    }
    return std::sqrt(sum / count);
}

inline void normalizeAudio(float* data, size_t count, float target_peak = 0.95f) {
    if (count == 0) return;
    float max_val = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        max_val = std::max(max_val, std::abs(data[i]));
    }
    if (max_val > 0.0f && max_val < target_peak) {
        float scale = target_peak / max_val;
        for (size_t i = 0; i < count; ++i) {
            data[i] *= scale;
        }
    }
}

} // namespace autocaption
