#include "tab5_audio.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tab5_audio {

static const char *const TAG = "tab5_audio";

static constexpr uint32_t MIC_RATE = 16000;
static constexpr uint32_t SPEAKER_RATE = 48000;
static constexpr size_t RECORD_SAMPLES = MIC_RATE * 5;
static constexpr uint32_t AMPLIFIER_MS = 50;
static constexpr float CODEC_VOLUME = 0.75f;

static void sine(int16_t *out, size_t count, float hz, float level) {
  const float fade = SPEAKER_RATE * 0.004f;
  for (size_t i = 0; i < count; ++i) {
    const float edge = std::min(1.0f, std::min<float>(i, count - 1 - i) / fade);
    out[i] = static_cast<int16_t>(level * edge * std::sin(2.0f * static_cast<float>(M_PI) * hz * i / SPEAKER_RATE));
  }
}

void Tab5Audio::setup() {
  dac_->set_volume(CODEC_VOLUME);
  dac_->set_mute_off();
  click_.resize(SPEAKER_RATE * 60 / 1000);
  sine(click_.data(), click_.size(), 1000.0f, 12000.0f);

  volume_->add_on_state_callback([this](float value) {
    speaker_->set_volume(std::clamp(value, 0.0f, 100.0f) / 100.0f);
  });
  if (volume_->has_state()) speaker_->set_volume(std::clamp(volume_->state, 0.0f, 100.0f) / 100.0f);
  mute_->add_on_state_callback([this](bool on) { microphone_->set_mute_state(on); });
  microphone_->set_mute_state(mute_->state);

  microphone_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (!recording_.load() || take_ == nullptr) return;
    size_t at = taken_.load();
    const size_t frames = data.size() / 4;
    for (size_t f = 0; f < frames && at < RECORD_SAMPLES; ++f)
      take_[at++] = static_cast<int16_t>(data[4 * f] | (data[4 * f + 1] << 8));
    taken_.store(at);
  });
}

int Tab5Audio::volume() const {
  return volume_->has_state() && !std::isnan(volume_->state) ? static_cast<int>(volume_->state) : 50;
}

bool Tab5Audio::play(const int16_t *pcm, size_t count, Job job, int16_t *owned) {
  if (job_ != Job::NONE || pcm == nullptr || count == 0) {
    if (owned) RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(owned, count);
    return false;
  }
  job_ = job;
  sound_ = pcm;
  samples_ = count;
  sent_ = 0;
  owned_ = owned;
  amplifier_->turn_on();
  step_ = Step::AMPLIFIER;
  since_ = millis();
  return true;
}

void Tab5Audio::record() {
  if (job_ != Job::NONE) return;
  take_ = RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).allocate(RECORD_SAMPLES);
  if (take_ == nullptr) {
    ESP_LOGW(TAG, "No room for a recording");
    return;
  }
  taken_.store(0);
  discard_ = false;
  job_ = Job::RECORD;
  recording_.store(true);
  microphone_->start();
  step_ = Step::RECORDING;
  since_ = millis();
}

void Tab5Audio::stop() {
  if (job_ == Job::RECORD && step_ == Step::RECORDING) {
    recording_.store(false);
    microphone_->stop();
    discard_ = true;
    step_ = Step::STOPPING_MIC;
    since_ = millis();
    return;
  }
  if (job_ == Job::RECORD) return;
  if (job_ != Job::NONE) speaker_->stop();
  done();
}

void Tab5Audio::done() {
  amplifier_->turn_off();
  RAMAllocator<int16_t> psram(RAMAllocator<int16_t>::ALLOC_EXTERNAL);
  if (owned_) psram.deallocate(owned_, samples_);
  if (take_) psram.deallocate(take_, RECORD_SAMPLES);
  owned_ = take_ = nullptr;
  sound_ = nullptr;
  samples_ = sent_ = 0;
  job_ = Job::NONE;
  step_ = Step::IDLE;
}

void Tab5Audio::loop() {
  const uint32_t now = millis();
  switch (step_) {
    case Step::IDLE:
      break;
    case Step::AMPLIFIER:
      if (now - since_ >= AMPLIFIER_MS) {
        speaker_->start();
        step_ = Step::STARTING;
        since_ = now;
      }
      break;
    case Step::STARTING:
      if (speaker_->is_running()) {
        step_ = Step::FEEDING;
      } else if (now - since_ > 1000) {
        ESP_LOGW(TAG, "The speaker did not start");
        speaker_->stop();
        done();
      }
      break;
    case Step::FEEDING: {
      const size_t total = samples_ * sizeof(int16_t);
      sent_ += speaker_->play(reinterpret_cast<const uint8_t *>(sound_) + sent_, total - sent_, 0);
      if (sent_ >= total) {
        speaker_->finish();
        step_ = Step::FINISHING;
        since_ = now;
      }
      break;
    }
    case Step::FINISHING:
      if (speaker_->is_stopped() || now - since_ > 3000) {
        speaker_->stop();
        done();
      }
      break;
    case Step::RECORDING:
      if (taken_.load() >= RECORD_SAMPLES || now - since_ >= 5000) {
        recording_.store(false);
        microphone_->stop();
        step_ = Step::STOPPING_MIC;
        since_ = now;
      }
      break;
    case Step::STOPPING_MIC:
      if (microphone_->is_stopped() || now - since_ > 1000) {
        if (discard_) {
          done();
          break;
        }
        const size_t taken = taken_.load();
        if (taken == 0) {
          done();
          ESP_LOGW(TAG, "Nothing came from the microphone");
          break;
        }
        const size_t factor = SPEAKER_RATE / MIC_RATE;
        const size_t count = taken * factor;
        int16_t *playback = RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).allocate(count);
        if (playback == nullptr) {
          done();
          ESP_LOGW(TAG, "No room for microphone playback");
          break;
        }
        for (size_t i = 0; i < taken; ++i) std::fill_n(playback + i * factor, factor, take_[i]);
        RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(take_, RECORD_SAMPLES);
        take_ = nullptr;
        job_ = Job::NONE;
        step_ = Step::IDLE;
        play(playback, count, Job::PLAYBACK, playback);
      }
      break;
  }
}

void Tab5Audio::on_touch() {
  if (!tap_sound_->state || volume() <= 0 || job_ != Job::NONE) return;
  if (!speaker_->is_stopped() || !microphone_->is_stopped()) return;
  play(click_.data(), click_.size(), Job::CLICK);
}

bool Tab5Audio::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F057E";
  page.toggle(text("mute"), [this]() { return mute_->state; },
              [this](bool on) { if (on) mute_->turn_on(); else mute_->turn_off(); });
  page.number(text("volume"), 0, 100, 5, "%", [this]() { return volume(); },
              [this](int value) {
                auto call = volume_->make_call();
                call.set_value(value);
                call.perform();
              });
  page.toggle(text("tap_sound"), [this]() { return tap_sound_->state; },
              [this](bool on) { if (on) tap_sound_->turn_on(); else tap_sound_->turn_off(); });
  page.action(
      text("tone"), "\U000F0387",
      [this]() {
        if (job_ == Job::TONE) { stop(); return; }
        if (job_ == Job::CLICK) stop();
        if (job_ != Job::NONE) return;
        const size_t count = SPEAKER_RATE * 600 / 1000;
        int16_t *tone = RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).allocate(count);
        if (tone == nullptr) return;
        sine(tone, count, 1000.0f, 12000.0f);
        play(tone, count, Job::TONE, tone);
      },
      nullptr, [this]() -> std::string { return job_ == Job::TONE ? text("playing") : ""; })
      .active([this]() { return job_ == Job::TONE; });
  page.action(
      text("record"), "\U000F036C",
      [this]() {
        if (job_ == Job::RECORD || job_ == Job::PLAYBACK) { stop(); return; }
        if (job_ == Job::CLICK) stop();
        record();
      },
      nullptr, [this]() -> std::string {
        if (job_ == Job::RECORD && step_ == Step::RECORDING) {
          const long left = std::max<long>(1, 5 - static_cast<long>((millis() - since_) / 1000));
          return tessera::format(text("recording"), left);
        }
        return job_ == Job::PLAYBACK ? text("playing") : "";
      })
      .active([this]() { return job_ == Job::RECORD || job_ == Job::PLAYBACK; });
  return true;
}

}  // namespace esphome::tab5_audio
