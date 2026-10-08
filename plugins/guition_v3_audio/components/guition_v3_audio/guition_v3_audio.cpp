#include "guition_v3_audio.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::guition_v3_audio {

static const char *const TAG = "guition_v3_audio";

static constexpr uint32_t SPEAKER_RATE = 16000;
static constexpr uint32_t MICROPHONE_RATE = 16000;
static constexpr size_t CHANNELS = 1;
static constexpr size_t RECORD_SAMPLES = MICROPHONE_RATE * 5;
static constexpr float CODEC_VOLUME = 0.75f;

static void sine(int16_t *out, size_t frames, float hz, float level) {
  const float fade = SPEAKER_RATE * 0.004f;
  for (size_t i = 0; i < frames; ++i) {
    const float edge = std::min(1.0f, std::min<float>(i, frames - 1 - i) / fade);
    const int16_t sample =
        static_cast<int16_t>(level * edge * std::sin(2.0f * static_cast<float>(M_PI) * hz * i / SPEAKER_RATE));
    for (size_t channel = 0; channel < CHANNELS; ++channel) out[i * CHANNELS + channel] = sample;
  }
}

void GuitionV3Audio::setup() {
  dac_->set_volume(CODEC_VOLUME);
  dac_->set_mute_off();
  click_.resize(SPEAKER_RATE * 60 / 1000 * CHANNELS);
  sine(click_.data(), SPEAKER_RATE * 60 / 1000, 1000.0f, 12000.0f);

  volume_->add_on_state_callback([this](float value) { speaker_->set_volume(std::clamp(value, 0.0f, 100.0f) / 100.0f); });
  if (volume_->has_state()) speaker_->set_volume(std::clamp(volume_->state, 0.0f, 100.0f) / 100.0f);
  mute_->add_on_state_callback([this](bool on) { microphone_->set_mute_state(on); });
  microphone_->set_mute_state(mute_->state);

  microphone_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (!recording_.load() || take_ == nullptr) return;
    size_t at = taken_.load();
    const size_t frames = data.size() / sizeof(int16_t);
    for (size_t i = 0; i < frames && at < RECORD_SAMPLES; ++i) {
      const uint16_t sample = static_cast<uint16_t>(data[2 * i]) | (static_cast<uint16_t>(data[2 * i + 1]) << 8);
      take_[at++] = static_cast<int16_t>(sample);
    }
    taken_.store(at);
  });
}

int GuitionV3Audio::volume() const {
  return volume_->has_state() && !std::isnan(volume_->state) ? static_cast<int>(volume_->state) : 50;
}

bool GuitionV3Audio::play(const int16_t *pcm, size_t count, Job job, int16_t *owned) {
  if (job_ != Job::NONE || pcm == nullptr || count == 0) {
    if (owned) RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(owned, count);
    return false;
  }
  job_ = job;
  sound_ = pcm;
  samples_ = count;
  sent_ = 0;
  owned_ = owned;
  speaker_->set_audio_stream_info(
      audio::AudioStreamInfo(16, CHANNELS, job == Job::PLAYBACK ? MICROPHONE_RATE : SPEAKER_RATE));
  step_ = Step::STARTING;
  since_ = millis();
  speaker_->start();
  return true;
}

void GuitionV3Audio::record() {
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

void GuitionV3Audio::stop() {
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

void GuitionV3Audio::done() {
  RAMAllocator<int16_t> psram(RAMAllocator<int16_t>::ALLOC_EXTERNAL);
  if (owned_) psram.deallocate(owned_, samples_);
  if (take_) psram.deallocate(take_, RECORD_SAMPLES);
  owned_ = take_ = nullptr;
  sound_ = nullptr;
  samples_ = sent_ = 0;
  job_ = Job::NONE;
  step_ = Step::IDLE;
}

void GuitionV3Audio::loop() {
  const uint32_t now = millis();
  switch (step_) {
    case Step::IDLE:
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
        int16_t *mono = take_;
        const size_t frames = taken_.load();
        take_ = nullptr;
        job_ = Job::NONE;
        step_ = Step::IDLE;
        if (frames == 0) {
          RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(mono, RECORD_SAMPLES);
          ESP_LOGW(TAG, "Nothing came from the microphone");
          break;
        }
        int16_t *mono_copy = RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).allocate(frames);
        if (mono_copy == nullptr) {
          RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(mono, RECORD_SAMPLES);
          ESP_LOGW(TAG, "No room for microphone playback");
          break;
        }
        std::copy_n(mono, frames, mono_copy);
        RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).deallocate(mono, RECORD_SAMPLES);
        play(mono_copy, frames, Job::PLAYBACK, mono_copy);
      }
      break;
  }
}

void GuitionV3Audio::on_touch() {
  if (!tap_sound_->state || volume() <= 0 || job_ != Job::NONE) return;
  if (!speaker_->is_stopped() || !microphone_->is_stopped()) return;
  play(click_.data(), click_.size(), Job::CLICK);
}

bool GuitionV3Audio::settings(tessera::SettingsPage &page) {
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
        const size_t frames = SPEAKER_RATE * 600 / 1000;
        const size_t samples = frames * CHANNELS;
        int16_t *tone = RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_EXTERNAL).allocate(samples);
        if (tone == nullptr) return;
        sine(tone, frames, 1000.0f, 12000.0f);
        play(tone, samples, Job::TONE, tone);
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

}  // namespace esphome::guition_v3_audio
