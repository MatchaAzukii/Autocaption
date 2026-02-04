#pragma once

#include "common.h"
#include <cstring>

namespace autocaption {

// Lock-free ring buffer for audio data
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity_samples);
    ~RingBuffer();

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Write audio data to ring buffer
    // Returns number of samples actually written
    size_t write(const float* data, size_t samples);

    // Read audio data from ring buffer
    // Returns number of samples actually read
    size_t read(float* data, size_t samples);

    // Get available samples for reading
    size_t available() const;

    // Get free space for writing
    size_t freeSpace() const;

    // Clear buffer
    void clear();

    // Peek at data without removing (for VAD)
    size_t peek(float* data, size_t samples, size_t offset = 0) const;

private:
    alignas(64) float* buffer_;
    size_t capacity_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};
};

// Thread-safe audio buffer queue for processed segments
class AudioSegmentQueue {
public:
    struct Segment {
        std::vector<float> data;
        uint64_t timestamp_ms;
        bool is_speech;
    };

    void push(Segment segment);
    std::optional<Segment> pop();
    std::optional<Segment> tryPop(std::chrono::milliseconds timeout);
    bool empty() const;
    size_t size() const;
    void clear();
    void stop();  // Wakes up all waiting threads

private:
    std::queue<Segment> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

} // namespace autocaption
