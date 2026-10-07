#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::ov_departures {

// The plugin: one tile type, "next" (plugin:ov_departures.next in a layout).
class OvDepartures : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace esphome::ov_departures
