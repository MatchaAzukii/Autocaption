#include "ring_buffer.h"

namespace autocaption {

RingBuffer::RingBuffer(size_t capacity_samples) 
    : capacity_(capacity_samples + 1) { // +1 for lock-free condition
    buffer_ = new float[capacity_];
    std::fill(buffer_, buffer_ + capacity_, 0.0f);
}

RingBuffer::~RingBuffer() {
    delete[] buffer_;
}

size_t RingBuffer::write(const float* data, size_t samples) {
    size_t write_pos = write_pos_.load(std::memory_order_relaxed);
    size_t read_pos = read_pos_.load(std::memory_order_acquire);
    
    size_t available = (write_pos >= read_pos) 
        ? capacity_ - (write_pos - read_pos) - 1
        : read_pos - write_pos - 1;
    
    size_t to_write = std::min(samples, available);
    
    for (size_t i = 0; i < to_write; ++i) {
        buffer_[write_pos] = data[i];
        write_pos = (write_pos + 1) % capacity_;
    }
    
    write_pos_.store(write_pos, std::memory_order_release);
    return to_write;
}

size_t RingBuffer::read(float* data, size_t samples) {
    size_t read_pos = read_pos_.load(std::memory_order_relaxed);
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    
    size_t available = (write_pos >= read_pos)
        ? write_pos - read_pos
        : capacity_ - read_pos + write_pos;
    
    size_t to_read = std::min(samples, available);
    
    for (size_t i = 0; i < to_read; ++i) {
        data[i] = buffer_[read_pos];
        read_pos = (read_pos + 1) % capacity_;
    }
    
    read_pos_.store(read_pos, std::memory_order_release);
    return to_read;
}

size_t RingBuffer::available() const {
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    size_t read_pos = read_pos_.load(std::memory_order_acquire);
    
    return (write_pos >= read_pos)
        ? write_pos - read_pos
        : capacity_ - read_pos + write_pos;
}

size_t RingBuffer::freeSpace() const {
    return capacity_ - available() - 1;
}

void RingBuffer::clear() {
    write_pos_.store(0, std::memory_order_release);
    read_pos_.store(0, std::memory_order_release);
}

size_t RingBuffer::peek(float* data, size_t samples, size_t offset) const {
    size_t read_pos = read_pos_.load(std::memory_order_relaxed);
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    
    size_t available = (write_pos >= read_pos)
        ? write_pos - read_pos
        : capacity_ - read_pos + write_pos;
    
    if (offset >= available) return 0;
    
    size_t to_read = std::min(samples, available - offset);
    size_t pos = (read_pos + offset) % capacity_;
    
    for (size_t i = 0; i < to_read; ++i) {
        data[i] = buffer_[pos];
        pos = (pos + 1) % capacity_;
    }
    
    return to_read;
}

// AudioSegmentQueue implementation
void AudioSegmentQueue::push(Segment segment) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        queue_.push(std::move(segment));
    }
    cv_.notify_one();
}

std::optional<AudioSegmentQueue::Segment> AudioSegmentQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    
    if (queue_.empty()) return std::nullopt;
    
    Segment segment = std::move(queue_.front());
    queue_.pop();
    return segment;
}

std::optional<AudioSegmentQueue::Segment> AudioSegmentQueue::tryPop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool has_data = cv_.wait_for(lock, timeout, [this] { return stopped_ || !queue_.empty(); });
    
    if (!has_data || queue_.empty()) return std::nullopt;
    
    Segment segment = std::move(queue_.front());
    queue_.pop();
    return segment;
}

bool AudioSegmentQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

size_t AudioSegmentQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void AudioSegmentQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) queue_.pop();
}

void AudioSegmentQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

} // namespace autocaption
