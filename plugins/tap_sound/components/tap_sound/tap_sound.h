#pragma once

#include "esphome/core/component.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/smart_display/plugin_sound.h"

namespace esphome::tap_sound {

// The plugin: a tick the moment a finger lands on something that takes a tap (on_press, plugin API 0.11), a softer one
// for every step of a dragged slider (on_step), and the switch that turns both off on the screen's settings page. A tick
// never waits for one before it: a quick second tap starts it again.
class TapSound : public Component, public tessera::Plugin {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void loop() override { sounds_.loop(); }
  void on_press() override;
  void on_step() override;
  bool settings(tessera::SettingsPage &page) override;
  void before_update() override { sounds_.stop(); }

  void set_speaker(speaker::Speaker *s) { sounds_.set_speaker(s); }
  void set_sound_on(switch_::Switch *s) { sound_on_ = s; }
  void set_tick(tessera::Sound s) { tick_ = s; }
  void set_step(tessera::Sound s) { step_ = s; }

 protected:
  static constexpr uint32_t STEP_SPAN_MS = 30;
  switch_::Switch *sound_on_{};
  tessera::Sound tick_, step_;
  tessera::SoundPlayer sounds_;
};

}  // namespace esphome::tap_sound
