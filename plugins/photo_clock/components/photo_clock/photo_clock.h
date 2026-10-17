#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::photo_clock {

// The plugin: an ESPHome component and a tessera::Plugin. It registers its screensaver in setup().
class PhotoClock : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace esphome::photo_clock
