#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/number/number.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::tab5_audio {

class Tab5Audio : public Component, public tessera::Plugin {
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
  uint32_t since_{0};
  std::vector<int16_t> click_;
  const int16_t *sound_{nullptr};
  size_t samples_{0}, sent_{0};
  int16_t *owned_{nullptr};
  bool discard_{false};
  int16_t *take_{nullptr};
  std::atomic<size_t> taken_{0};
  std::atomic<bool> recording_{false};
};

}  // namespace esphome::tab5_audio
