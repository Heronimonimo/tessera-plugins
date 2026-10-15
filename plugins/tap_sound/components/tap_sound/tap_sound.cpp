#include "tap_sound.h"

namespace esphome::tap_sound {

void TapSound::on_press() {
  if (sound_on_ != nullptr && !sound_on_->state) return;
  sounds_.play(tick_);
}

void TapSound::on_step() {
  if (sound_on_ != nullptr && !sound_on_->state) return;
  // Each step after the one before, 30 ms apart at the least: a quick drag ticks evenly instead of in clumps.
  sounds_.add(step_, STEP_SPAN_MS);
}

bool TapSound::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F12A8";
  page.toggle(text("tap_sound"), [this] { return sound_on_ == nullptr || sound_on_->state; },
              [this](bool on) { if (sound_on_ != nullptr) on ? sound_on_->turn_on() : sound_on_->turn_off(); });
  return true;
}

}  // namespace esphome::tap_sound
