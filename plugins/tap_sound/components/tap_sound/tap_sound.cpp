#include "tap_sound.h"

namespace esphome::tap_sound {

void TapSound::on_press() {
  if (sound_on_ != nullptr && !sound_on_->state) return;
  sounds_.play(tick_);
}

void TapSound::on_step() {
  if (sound_on_ != nullptr && !sound_on_->state) return;
  sounds_.play(step_);
}

bool TapSound::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F12A8";
  page.toggle(text("tap_sound"), [this] { return sound_on_ == nullptr || sound_on_->state; },
              [this](bool on) { if (sound_on_ != nullptr) on ? sound_on_->turn_on() : sound_on_->turn_off(); });
  return true;
}

}  // namespace esphome::tap_sound
