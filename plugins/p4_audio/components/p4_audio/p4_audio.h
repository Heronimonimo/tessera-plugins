#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/number/number.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::p4_audio {

// The plugin: a click on every tap the screen takes (Tap sound), and on its settings page the three settings and two
// tests: a tone from the speaker, and five seconds from the microphone played back. One sound at a time: a click never
// interrupts a test, and a test stops at a second tap on its row.
class P4Audio : public Component, public tessera::Plugin {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  bool settings(tessera::SettingsPage &page) override;
  void on_touch() override;
  void before_update() override { stop(); }

  void set_speaker(speaker::Speaker *s) { speaker_ = s; }
  void set_microphone(microphone::Microphone *m) { microphone_ = m; }
  void set_dac(audio_dac::AudioDac *d) { dac_ = d; }
  void set_amplifier(switch_::Switch *s) { amplifier_ = s; }
  void set_mute(switch_::Switch *s) { mute_ = s; }
  void set_tap_sound(switch_::Switch *s) { tap_sound_ = s; }
  void set_volume(number::Number *n) { volume_ = n; }

 protected:
  enum class Job : uint8_t { NONE, CLICK, TONE, RECORD, PLAYBACK };
  enum class Step : uint8_t { IDLE, AMPLIFIER, STARTING, FEEDING, FINISHING, RECORDING, STOPPING_MIC };

  // Plays `count` samples (16-bit mono at 16 kHz) as `job`; false when something else is playing. `owned` is freed
  // when it is done (a buffer in PSRAM).
  bool play(const int16_t *pcm, size_t count, Job job, int16_t *owned = nullptr);
  void record();
  void stop();
  void done();
  int volume() const;

  speaker::Speaker *speaker_{};
  microphone::Microphone *microphone_{};
  audio_dac::AudioDac *dac_{};
  switch_::Switch *amplifier_{}, *mute_{}, *tap_sound_{};
  number::Number *volume_{};

  Job job_{Job::NONE};
  Step step_{Step::IDLE};
  uint32_t since_{0};             // when the step began, millis()
  std::vector<int16_t> click_;    // made once at setup
  const int16_t *sound_{nullptr}; // what plays now
  size_t samples_{0}, sent_{0};   // its length in samples, and the bytes of it the speaker took
  int16_t *owned_{nullptr};       // freed when it is done
  bool discard_{false};           // a recording stopped by hand: not played back
  // The recording, in PSRAM, written by the microphone's callback while `recording_` is set.
  int16_t *take_{nullptr};
  std::atomic<size_t> taken_{0};
  std::atomic<bool> recording_{false};
};

}  // namespace esphome::p4_audio
