#pragma once
// The camera's frames, shared (docs/PICTURE_ARCHITECTURE.md in the Tessera repository, section 5). esp_video_camera
// hands its raw frames to one consumer, and this hub is that one: Screen camera's light level and motion and the part
// Video's encoder add themselves in their setup() and switch themselves on and off, each frame goes to every consumer
// that is on, in the order they added themselves, on the camera's capture task, and the camera runs while any is on.
// Each consumer keeps its own pace (sensing reads a frame every 400 ms, the video 15 a second).
//
// The camera hands out one layout, set before it first starts: the first consumer to add itself names it, and a later
// one must read it. The video's setup() runs before sensing's (AFTER_WIFI before LATE), so with the part Video the
// frames are its YUV420, and sensing, which reads both, takes them; without it they are RGB565.
#include <atomic>

#include "esphome/components/esp_video_camera/esp_video_camera.h"

namespace esphome::screen_camera {

class FrameHub : public esp_video_camera::RawVideoFrameConsumer {
 public:
  using Format = esp_video_camera::RawVideoPixelFormat;
  using Consumer = esp_video_camera::RawVideoFrameConsumer;
  static constexpr int MAX_CONSUMERS = 4;

  void set_camera(esp_video_camera::ESPVideoCamera *camera) { camera_ = camera; }
  // From a consumer's setup(), on the main loop. `either`: the consumer reads RGB565 and the video's YUV420 alike.
  // False when the camera hands its frames to another consumer already, or in a layout this one does not read.
  bool add(Consumer *consumer, Format format, bool either = false);
  // On or off, from the main loop: the camera starts when the first consumer is on and stops when none is. False when
  // the consumer was not added, or the camera hands out no raw frames (a pipeline without them).
  bool set_on(Consumer *consumer, bool on);

  // On the camera's task.
  void consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) override;

 protected:
  struct Entry {
    std::atomic<Consumer *> consumer{nullptr};
    std::atomic<bool> on{false};
  };
  Entry *find_(Consumer *consumer);
  bool any_on_() const;

  esp_video_camera::ESPVideoCamera *camera_{nullptr};
  Entry entries_[MAX_CONSUMERS];
  std::atomic<int> count_{0};  // entries added; the camera's task reads up to it
  bool registered_{false};
  Format format_{Format::RGB565_LE};
};

}  // namespace esphome::screen_camera
