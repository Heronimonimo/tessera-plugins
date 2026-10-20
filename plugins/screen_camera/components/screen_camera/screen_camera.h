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

// The plugin: the screen's camera as a camera of its device in Home Assistant (esp_video_camera, plugin.yaml). The core
// shows its dot while Home Assistant takes pictures (it listens to ts_camera itself, plugin API 0.17), so this plugin
// draws nothing of its own for that.
// And, only while a person switches it on (the setting "Camera light level and motion", off by default), the room's
// light level and motion, worked out on the screen from the camera's frames: no picture leaves the screen for them. The
// camera hands its processed frames (RGB565) to this plugin on its own task (RawVideoFrameConsumer); the plugin reads a
// coarse grid of them and gives the results to the main loop. While it runs it tells the core (tessera::capturing), so
// the dot shows then too; with the setting "Camera motion wakes the screen" (off by default) motion wakes the screen.
class ScreenCamera : public Component,
                     public tessera::Plugin,
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
  // Motion wakes the screen or not (the setting, off by default).
  void set_wake_on_motion(bool on) { wake_on_motion_ = on; }
  // On the camera's task, for every frame while sensing is on.
  void consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) override;

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
  bool wake_on_motion_{false};
  uint32_t woken_for_{0};      // the motion sample the screen was last woken for
  uint32_t last_wake_ms_{0};

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
