#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace hwpanel::core {

// Thread-safe fixed-capacity circular buffer. Oldest elements are overwritten
// once the buffer is full. Used for the service-side telemetry history
// (5 minutes @ 1 Hz by default).
template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(std::size_t capacity) : cap_(capacity == 0 ? 1 : capacity) {}

  void Push(const T& value) {
    std::lock_guard lock(mutex_);
    if (buf_.size() < cap_) {
      buf_.push_back(value);
    } else {
      buf_[head_] = value;
      head_ = (head_ + 1) % cap_;
    }
    ++total_;
  }

  std::size_t Size() const {
    std::lock_guard lock(mutex_);
    return buf_.size();
  }

  std::size_t Capacity() const { return cap_; }

  uint64_t TotalPushed() const {
    std::lock_guard lock(mutex_);
    return total_;
  }

  // The most recent |n| elements in chronological order.
  std::vector<T> LastN(std::size_t n) const {
    std::lock_guard lock(mutex_);
    const std::size_t count = std::min(n, buf_.size());
    std::vector<T> out;
    out.reserve(count);
    for (std::size_t i = buf_.size() - count; i < buf_.size(); ++i) {
      out.push_back(buf_[(head_ + i) % cap_]);
    }
    return out;
  }

  std::vector<T> All() const { return LastN(Size()); }

  void Clear() {
    std::lock_guard lock(mutex_);
    buf_.clear();
    head_ = 0;
    total_ = 0;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<T> buf_;
  std::size_t head_ = 0;   // index of the oldest element once full
  uint64_t total_ = 0;
  std::size_t cap_;
};

}  // namespace hwpanel::core
