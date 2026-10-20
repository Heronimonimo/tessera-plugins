#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/esp_video_camera/esp_video_camera.h"
#include "esphome/components/smart_display/plugin_api.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::screen_camera {

// The plugin: an item for the top bar that shows while pictures leave the screen's camera, in red while it streams. It listens to the camera as
// ESPHome's API does (camera::CameraListener), on the main loop: a picture, or a stream that starts or stops.
// And, only while a person switches it on (the setting "Camera light level and motion", off by default), the room's
// light level and motion, worked out on the screen from the camera's frames: no picture leaves the screen for them. The
// camera hands its processed frames (RGB565) to this plugin on its own task (RawVideoFrameConsumer); the plugin reads a
// coarse grid of them and gives the results to the main loop. An eye in the top bar can show while it runs.
class ScreenCamera : public Component,
                     public tessera::Plugin,
                     public camera::CameraListener,
                     public esp_video_camera::RawVideoFrameConsumer {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  void on_interval(uint32_t now_ms) override;

  void set_camera(esp_video_camera::ESPVideoCamera *c) { camera_ = c; }
#ifdef USE_SENSOR
  void set_illuminance(sensor::Sensor *s) { illuminance_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_motion(binary_sensor::BinarySensor *s) { motion_ = s; }
#endif
  // Sensing on or off (the setting). Before setup() it is only remembered.
  void set_sensing(bool on);

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  void on_stream_start() override;
  void on_stream_stop() override;
  // On the camera's task, for every frame while sensing is on.
  void consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) override;

  // Whether someone is watching: a picture left the screen a moment ago. Live: a stream runs and its pictures still
  // come, so a stream that ends without a word from the API (a viewer that went away) goes quiet by itself.
  bool watching() const;
  bool live() const;

 protected:
  // The coarse grid sensing reads: 32 x 18 cells, each the mean of 4 x 4 points.
  static constexpr int GRID_W = 32;
  static constexpr int GRID_H = 18;

  void apply_sensing_();
  void publish_sensing_();
  esp_video_camera::ESPVideoCamera *video_() const { return camera_; }

  esp_video_camera::ESPVideoCamera *camera_{};
#ifdef USE_SENSOR
  sensor::Sensor *illuminance_{};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *motion_{};
#endif
  bool streaming_{false};
  bool shown_{false};          // what the top bar shows now, so a change draws it at once
  uint32_t last_picture_{0};   // millis() of the last picture, 0 before the first
  uint32_t last_stream_{0};    // millis() of the last sign of a stream: a request for it or one of its pictures

  // Sensing. Set up on the main loop; the camera's task owns the grid and the sample clock while it runs.
  bool sensing_wanted_{false};
  bool sensing_on_{false};
  bool set_up_{false};
  uint8_t grid_[GRID_W * GRID_H]{};
  bool grid_valid_{false};
  uint32_t last_sample_ms_{0};
  float last_exposure_product_{0};
  uint32_t settle_until_ms_{0};   // no motion before this: the light changed and the camera is settling
  float light_level_{-1.0f};      // smoothed light level in lux, on the camera's task
  // What the camera's task found, for the main loop.
  std::atomic<uint32_t> last_motion_ms_{0};
  std::atomic<uint32_t> lux_x10_{UINT32_MAX};
  uint32_t published_lux_x10_{UINT32_MAX};
  uint32_t last_lux_publish_ms_{0};
  bool motion_shown_{false};
};

}  // namespace esphome::screen_camera
