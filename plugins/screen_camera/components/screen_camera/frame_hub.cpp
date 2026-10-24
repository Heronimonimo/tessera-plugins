#include "frame_hub.h"

namespace esphome::screen_camera {

bool FrameHub::add(Consumer *consumer, Format format, bool either) {
  if (consumer == nullptr || camera_ == nullptr) return false;
  if (!registered_) {
    if (!camera_->register_raw_frame_consumer(this, format)) return false;
    registered_ = true;
    format_ = format;
  } else if (format != format_ && !either) {
    return false;
  }
  const int n = count_.load(std::memory_order_relaxed);
  if (find_(consumer) != nullptr) return true;
  if (n >= MAX_CONSUMERS) return false;
  entries_[n].consumer.store(consumer, std::memory_order_relaxed);
  count_.store(n + 1, std::memory_order_release);
  return true;
}

FrameHub::Entry *FrameHub::find_(Consumer *consumer) {
  const int n = count_.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i)
    if (entries_[i].consumer.load(std::memory_order_relaxed) == consumer) return &entries_[i];
  return nullptr;
}

bool FrameHub::any_on_() const {
  const int n = count_.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i)
    if (entries_[i].on.load(std::memory_order_acquire)) return true;
  return false;
}

bool FrameHub::set_on(Consumer *consumer, bool on) {
  Entry *entry = find_(consumer);
  if (entry == nullptr) return false;
  if (on) {
    const bool running = any_on_();
    entry->on.store(true, std::memory_order_release);
    // The camera runs already for another consumer: nothing to ask of it.
    if (running || camera_->start_raw_frame_consumer(this)) return true;
    entry->on.store(false, std::memory_order_release);
    return false;
  }
  entry->on.store(false, std::memory_order_release);
  if (!any_on_()) camera_->stop_raw_frame_consumer(this);
  return true;
}

void FrameHub::consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) {
  const int n = count_.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i) {
    if (!entries_[i].on.load(std::memory_order_acquire)) continue;
    Consumer *consumer = entries_[i].consumer.load(std::memory_order_relaxed);
    if (consumer != nullptr) consumer->consume_raw_video_frame(frame);
  }
}

}  // namespace esphome::screen_camera
