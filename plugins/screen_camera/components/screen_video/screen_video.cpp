// The video as a component: its setup, its line in the log, and the loop that tells Screen camera of a viewer and
// starts and stops the microphone. The frame path is video_encoder.cpp, the server rtsp_server.cpp, the sound
// opus_sound.cpp (README.md, "Video").
#include "screen_video.h"
#include "screen_video_shared.h"

#include "esp_heap_caps.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::screen_video {

static constexpr uint32_t REPORT_MS = 60000;

void ScreenVideo::setup() {
  const size_t internal_at_start = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  ready_ = xQueueCreate(SLOTS, sizeof(int));
  out_ = static_cast<uint8_t *>(heap_caps_malloc(OUT_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  for (auto &slot : slots_) slot.data = static_cast<uint8_t *>(psram_dma(ENCODED_BYTES));
  if (ready_ == nullptr || out_ == nullptr || slots_[SLOTS - 1].data == nullptr) {
    ESP_LOGE(TAG, "No memory for the video");
    mark_failed();
    return;
  }
  // The camera's raw frames in the encoder's own YUV420 (esp_video_camera device: csi), so no conversion at all,
  // through Screen camera's frame hub, which hands the same frames to its light level and motion. The video adds itself
  // first (its setup runs before Screen camera's), so the camera hands out this layout.
  if (camera_ == nullptr || screen_camera_ == nullptr ||
      !screen_camera_->frames().add(this, esp_video_camera::RawVideoPixelFormat::YUV420_OUYY_EVYY)) {
    ESP_LOGE(TAG, "The camera hands its frames to another consumer already");
    mark_failed();
    return;
  }
  // The camera runs from boot and the encoder is made on its task at the first frame, once, for the life of the
  // screen. Early, because esp_h264 keeps its reference frame in internal RAM: made at PLAY, after the pages were
  // built, it failed for memory. And after the camera driver came up: made in setup(), before the camera started, the
  // first encoded frame hung the whole chip.
  screen_camera_->frames().set_on(this, true);
#ifdef USE_SCREEN_VIDEO_SOUND
  setup_sound_();
#endif
  // Opus encodes on this task, and it takes a deep stack (about 27 KB): in PSRAM, as every big buffer of the video,
  // since internal RAM is what the screen runs short of.
  if (xTaskCreatePinnedToCoreWithCaps(server_task_, "screen_video", 32768, this, 4, nullptr, 0,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    ESP_LOGE(TAG, "No memory for the video's server");
    mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Set up; internal RAM: %u bytes free before, %u after", (unsigned) internal_at_start,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void ScreenVideo::dump_config() {
  ESP_LOGCONFIG(TAG, "Screen video:\n  RTSP port: %u\n  H.264: the camera's size (%ux%u so far), %u fps, %u kbit/s, "
                "a key frame every %u frames",
                port_, width_, height_, FRAMERATE, (unsigned) (BITRATE / 1000), GOP);
#ifdef USE_SCREEN_VIDEO_SOUND
  ESP_LOGCONFIG(TAG, "  Sound: Opus, %u Hz mono, 32 kbit/s%s", (unsigned) sound_rate_, opus_ ? "" : " (no encoder)");
#endif
}

void ScreenVideo::loop() {
  // Screen camera's dot while anyone watches (the core sees Home Assistant's own pictures by itself, not this).
  const bool watched = playing_.load();
  if (watched != watched_) {
    watched_ = watched;
    if (screen_camera_ != nullptr) screen_camera_->set_video_watched(watched);
  }
#ifdef USE_SCREEN_VIDEO_SOUND
  // The microphone starts and stops on the loop, as everything else that listens does. ESPHome counts its listeners,
  // so the voice assistant keeps it while it listens too.
  if (mic_ != nullptr && wants_sound_.load() != listening_) {
    listening_ = !listening_;
    if (listening_) {
      mic_->start();
    } else {
      mic_->stop();
    }
  }
#endif
  const uint32_t now = millis();
  if (now - reported_ms_ < REPORT_MS) return;
  const uint32_t span = now - reported_ms_;
  reported_ms_ = now;
  const uint32_t frames = frames_.load(), bytes = bytes_.load(), n = frames - reported_frames_;
  if (n > 0) {
    const uint32_t crop_us = crop_us_.load(), encode_us = encode_us_.load();
    ESP_LOGI(TAG, "%.1f fps, %u kbit/s, cutting %.1f ms and encoding %.1f ms a frame; since boot %u dropped, %u failed, "
             "%u skipped for internal RAM, %u bytes of it free, %d held by the encoder",
             n * 1000.0f / span, (unsigned) ((bytes - reported_bytes_) * 8ULL / span), (crop_us - reported_crop_us_) / 1000.0f / n,
             (encode_us - reported_encode_us_) / 1000.0f / n,
             (unsigned) dropped_.load(), (unsigned) failed_.load(), (unsigned) held_.load(),
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL), encoder_internal_.load());
    reported_crop_us_ = crop_us;
    reported_encode_us_ = encode_us;
  }
  reported_frames_ = frames;
  reported_bytes_ = bytes;
#ifdef USE_SCREEN_VIDEO_SOUND
  const uint32_t packets = sound_packets_.load();
  if (packets != reported_sound_)
    ESP_LOGI(TAG, "Sound: %u packets of 20 ms, %u bytes lost since boot", (unsigned) (packets - reported_sound_),
             (unsigned) sound_lost_.load());
  reported_sound_ = packets;
#endif
}

}  // namespace esphome::screen_video
