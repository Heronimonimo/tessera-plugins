#pragma once
// The screen's own camera as live video: H.264 from the ESP32-P4's hardware encoder and the microphone as Opus, served
// over RTSP for Home Assistant (its go2rtc turns it into WebRTC without transcoding). See __init__.py and README.md,
// "Video". Every number below was measured on a reTerminal D1001; the comments say what went wrong without it.
#include <atomic>
#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/esp_video_camera/esp_video_camera.h"
#include "esphome/components/screen_camera/screen_camera.h"
#ifdef USE_SCREEN_VIDEO_SOUND
#include "esphome/components/microphone/microphone_source.h"
#include <freertos/stream_buffer.h>
#endif

#include "esp_h264_enc_single_hw.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace esphome::screen_video {

class ScreenVideo : public Component, public esp_video_camera::RawVideoFrameConsumer {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_camera(esp_video_camera::ESPVideoCamera *c) { camera_ = c; }
  void set_screen_camera(screen_camera::ScreenCamera *s) { screen_camera_ = s; }
  void set_port(uint16_t p) { port_ = p; }
  void set_password(const char *p) { password_ = p; }
#ifdef USE_SCREEN_VIDEO_SOUND
  void set_microphone_source(microphone::MicrophoneSource *m) { mic_ = m; }
#endif

  // On the camera's capture task, for every frame from boot on.
  void consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) override;

 protected:
  // The stream: 720p at 15 frames a second, 1 Mbit/s, a key frame every second. The encoder is made for this size
  // once, at boot, and keeps it (a viewer never chooses another).
  static constexpr uint16_t WIDTH = 1280, HEIGHT = 720;
  static constexpr uint8_t FRAMERATE = 15, GOP = 15;
  static constexpr uint32_t BITRATE = 1000000;
  // One access unit at most. A key frame of a dark, noisy room is over 200 KB, and an encoder whose output did not fit
  // stays stuck for good, so this is generous.
  static constexpr size_t ENCODED_BYTES = 1024 * 1024;
  // Access units between the encoder (camera's task) and the sender (the server's task).
  static constexpr int SLOTS = 3;
  // go2rtc, Home Assistant's own stream (HLS and stills), and one more. With one, Home Assistant's own connection
  // waited beside go2rtc's and its frontend fell back to HLS: three seconds late and no sound.
  static constexpr int MAX_VIEWERS = 3;
  // RTP packets of an access unit are gathered and sent in one go, in PSRAM: internal RAM is the Wi-Fi's and lwIP's,
  // and below about 90 KB of it free the whole chip stalled now and then.
  static constexpr size_t OUT_BYTES = 64 * 1024;
  // What a viewer's connection holds unsent (lwIP's send buffer, 64 KB with Tessera's P4 setting) lives in internal
  // RAM, and so does every packet the Wi-Fi chip hands over: with three viewers, live camera tiles on the glass and the
  // voice assistant, internal RAM ran out and the Wi-Fi driver's next packet aborted the chip. Below this much free the
  // video skips frames, and starts again with a key frame once there is room.
  static constexpr size_t INTERNAL_LOW = 80 * 1024, INTERNAL_AGAIN = 110 * 1024;

  struct Slot {
    uint8_t *data{nullptr};
    uint32_t length{0};
    uint32_t timestamp{0};
    std::atomic<uint8_t> state{0};  // 0 free, 1 filled, 2 sending
  };
  struct Viewer {
    int fd{-1};
    std::string pending;
    bool playing{false};
    int video_channel{0};
    int sound_channel{-1};
    uint16_t seq{0}, sound_seq{0};
    uint32_t video_packets{0}, video_octets{0};
    uint32_t silent_since{0}, reports_at{0};
  };

  static void server_task_(void *self);
  void serve_();
  void accept_(int listener);
  void read_(Viewer &v);
  bool request_(Viewer &v, const std::string &request);
  void drop_(Viewer &v);
  bool anyone_playing_() const;
  bool send_all_(int fd, const uint8_t *data, size_t length);
  bool queue_(int fd, const uint8_t *data, size_t length);
  bool flush_(int fd);
  bool send_access_unit_(Viewer &v, const Slot &slot);
  bool send_reports_(Viewer &v);
#ifdef USE_SCREEN_VIDEO_SOUND
  void setup_sound_();
  void send_sound_();
#endif

  esp_video_camera::ESPVideoCamera *camera_{nullptr};
  screen_camera::ScreenCamera *screen_camera_{nullptr};
  uint16_t port_{8554};
  const char *password_{""};
  std::string expected_auth_;

  // The camera's task owns the encoder.
  esp_h264_enc_handle_t encoder_{nullptr};
  bool encoder_failed_{false};
  bool size_reported_{false};
  uint32_t last_frame_us_{0};
  Slot slots_[SLOTS];
  QueueHandle_t ready_{nullptr};  // indices of filled slots, oldest first

  // The server's task owns the viewers and the send batch.
  Viewer viewers_[MAX_VIEWERS];
  uint8_t *out_{nullptr};
  size_t out_len_{0};
  uint32_t ssrc_{0x54455353};

  std::atomic<bool> playing_{false};  // a viewer plays: the camera's task encodes
  std::atomic<bool> want_idr_{false};
  bool watched_{false};                // what the loop last told Screen camera (its dot)

  // What the tasks count, for the loop's line every minute while someone watches.
  std::atomic<uint32_t> frames_{0}, bytes_{0}, dropped_{0}, failed_{0}, held_{0};
  bool holding_{false};  // the server's task: internal RAM is low, frames are skipped
  uint32_t reported_ms_{0}, reported_frames_{0}, reported_bytes_{0};

#ifdef USE_SCREEN_VIDEO_SOUND
  // 20 ms of sound an Opus frame, at the microphone's own rate (16 kHz on every screen with one).
  microphone::MicrophoneSource *mic_{nullptr};
  StreamBufferHandle_t sound_{nullptr};   // little-endian samples, from the microphone's task to the server's
  std::atomic<bool> wants_sound_{false};  // a viewer set the sound up and plays: the loop starts the microphone
  bool listening_{false};
  uint32_t sound_rate_{16000};
  uint32_t sound_ssrc_{0x54455354};
  void *opus_{nullptr};
  uint8_t opus_out_[1500];
  int opus_in_bytes_{0}, opus_out_bytes_{0};
  std::atomic<uint32_t> sound_packets_{0}, sound_lost_{0}, sound_skipped_{0};
  uint32_t reported_sound_{0};
  // The sound's clock: when the first packet's samples were heard (esp_timer, us) and its RTP timestamp (48 kHz).
  int64_t sound_start_us_{0};
  uint32_t sound_start_ts_{0};
  uint32_t sound_sent_{0};  // samples sent since PLAY, at the microphone's rate
  uint32_t sound_octets_{0}, sound_report_packets_{0};
  // The microphone's last delivery: when, and how many samples it had delivered in all by then.
  std::atomic<int64_t> mic_at_us_{0};
  std::atomic<uint32_t> mic_samples_{0};
  uint32_t mic_samples_at_play_{0};
#endif
};

}  // namespace esphome::screen_video
