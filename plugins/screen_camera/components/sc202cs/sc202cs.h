#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome::sc202cs {

// The SC202CS's black level target, set once after esp_video_camera has started the sensor (__init__.py says why).
class SC202CS : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  // After esp_video_camera (DATA), whose setup writes the sensor's start registers.
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  bool found_{false};
  bool set_{false};
};

}  // namespace esphome::sc202cs
