#include "screen_video.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

#include <lwip/sockets.h>
#include <mbedtls/base64.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_SCREEN_VIDEO_SOUND
#include "encoder/impl/esp_opus_enc.h"
#endif

namespace esphome::screen_video {

static const char *const TAG = "screen_video";
static constexpr size_t RTP_PAYLOAD = 1400;  // an RTP packet's payload at most: under any Wi-Fi MTU
static constexpr uint32_t REPORT_MS = 60000;

static void *psram_dma(size_t bytes) {
  return heap_caps_aligned_calloc(64, 1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
}

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
  // The camera's raw frames in the encoder's own YUV420 (esp_video_camera device: csi), so no conversion at all. This
  // stream is the camera's one raw consumer; Screen camera's light level and motion get the frames from it.
  if (camera_ == nullptr ||
      !camera_->register_raw_frame_consumer(this, esp_video_camera::RawVideoPixelFormat::YUV420_OUYY_EVYY)) {
    ESP_LOGE(TAG, "The camera hands its frames to another consumer already");
    mark_failed();
    return;
  }
  if (screen_camera_ != nullptr) screen_camera_->take_frames_from_video();
  // The camera runs from boot and the encoder is made on its task at the first frame, once, for the life of the
  // screen. Early, because esp_h264 keeps its reference frame in internal RAM: made at PLAY, after the pages were
  // built, it failed for memory. And after the camera driver came up: made in setup(), before the camera started, the
  // first encoded frame hung the whole chip.
  camera_->start_raw_frame_consumer(this);
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
    const uint32_t crop_us = crop_us_.load();
    ESP_LOGI(TAG, "%.1f fps, %u kbit/s, cutting %.1f ms a frame; since boot %u dropped, %u failed, %u skipped for internal RAM, "
             "%u bytes of it free",
             n * 1000.0f / span, (unsigned) ((bytes - reported_bytes_) * 8ULL / span), (crop_us - reported_crop_us_) / 1000.0f / n,
             (unsigned) dropped_.load(), (unsigned) failed_.load(), (unsigned) held_.load(),
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    reported_crop_us_ = crop_us;
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

// ---- The camera's task: a frame, H.264 ----
void ScreenVideo::consume_raw_video_frame(const esp_video_camera::RawVideoFrame &frame) {
  // Light level and motion read the same frame first, at their own pace (Screen camera).
  if (screen_camera_ != nullptr) screen_camera_->consume_raw_video_frame(frame);
  if (frame.pixel_format != esp_video_camera::RawVideoPixelFormat::YUV420_OUYY_EVYY || frame.stride_bytes != frame.width)
    return;
  if (encoder_ == nullptr && !encoder_failed_) {
    // The stream's size: the camera's, cut to whole macroblocks, its middle. The camera's frame is the encoder's own
    // YUV420 (a line of U Y Y..., a line of V Y Y..., 3 bytes for 2 pixels): a cut starts on an even line, so the
    // lines keep their U and V, and on an even pixel, so the groups of two stay whole.
    width_ = frame.width & ~15;
    height_ = frame.height & ~15;
    if (width_ < 160 || height_ < 96 || static_cast<uint32_t>(width_) * height_ > MAX_PIXELS) {
      ESP_LOGE(TAG, "The camera gives %ux%u frames; the video takes %s. Give the camera a smaller mode in the part's "
               "YAML (esp_video_camera resolution).",
               frame.width, frame.height, width_ < 160 || height_ < 96 ? "nothing this small" : "at most 1280x800");
      encoder_failed_ = true;
      return;
    }
    if (width_ != frame.width || height_ != frame.height) {
      crop_x_ = ((frame.width - width_) / 2) & ~1;
      crop_y_ = ((frame.height - height_) / 2) & ~1;
      crop_ = static_cast<uint8_t *>(psram_dma(static_cast<size_t>(width_) * height_ * 3 / 2));
      // The PPA cuts it, by DMA: a memcpy of the frame on the camera's task took 77 ms (the camera's buffer is not
      // cached). Its burst of 64 bytes, as every PPA client on a screen: the panel keeps its share of the bus.
      ppa_client_config_t client{};
      client.oper_type = PPA_OPERATION_SRM;
      client.max_pending_trans_num = 1;
      client.data_burst_length = PPA_DATA_BURST_LENGTH_64;
      if (crop_ == nullptr || ppa_register_client(&client, &ppa_) != ESP_OK) {
        ESP_LOGE(TAG, "No memory or PPA for the cut frame");
        encoder_failed_ = true;
        return;
      }
    }
    const size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    esp_h264_enc_cfg_hw_t cfg{};
    cfg.pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY;
    cfg.gop = GOP;
    cfg.fps = FRAMERATE;
    cfg.res.width = width_;
    cfg.res.height = height_;
    cfg.rc.bitrate = BITRATE;
    cfg.rc.qp_min = 25;
    cfg.rc.qp_max = 42;
    if (esp_h264_enc_hw_new(&cfg, &encoder_) != ESP_H264_ERR_OK || encoder_ == nullptr ||
        esp_h264_enc_open(encoder_) != ESP_H264_ERR_OK) {
      ESP_LOGE(TAG, "The H.264 encoder did not start (internal RAM free: %u bytes)",
               (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
      encoder_failed_ = true;
      encoder_ = nullptr;
      return;
    }
    if (crop_ != nullptr) {
      ESP_LOGW(TAG, "H.264 encoder ready at %ux%u, the middle of the camera's %ux%u; it keeps %d bytes of internal RAM. "
               "The cut costs the camera's task about 60 ms a frame (fewer frames a second): a camera mode in whole "
               "macroblocks of 16 is better",
               width_, height_, frame.width, frame.height, (int) internal_before - (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    } else {
      ESP_LOGI(TAG, "H.264 encoder ready at %ux%u, the camera's own size; it keeps %d bytes of internal RAM", width_,
               height_, (int) internal_before - (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
  }
  if (encoder_ == nullptr || !playing_.load()) return;
  if (frame.width < crop_x_ + width_ || frame.height < crop_y_ + height_) return;  // a frame of another size: not ours
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time());
  if (last_frame_us_ != 0 && now - last_frame_us_ < 1000000U / FRAMERATE - 3000U) return;
  last_frame_us_ = now;
  // A frame dropped before it is encoded leaves the encoder's references whole: the stream goes on undamaged.
  int index = -1;
  for (int i = 0; i < SLOTS && index < 0; ++i) {
    uint8_t free = 0;
    if (slots_[i].state.compare_exchange_strong(free, 1)) index = i;
  }
  if (index < 0) {
    dropped_++;
    return;
  }
  Slot &slot = slots_[index];
  if (want_idr_.exchange(false)) {
    esp_h264_enc_param_hw_handle_t param = nullptr;
    if (esp_h264_enc_hw_get_param_hd(encoder_, &param) == ESP_H264_ERR_OK && param != nullptr)
      esp_h264_enc_force_idr(&param->base);
  }
  const uint8_t *picture = frame.data;
  if (crop_ != nullptr) {
    // The cut: the middle block of the camera's frame, YUV420 in and out, no scaling, by the PPA.
    const int64_t started = esp_timer_get_time();
    ppa_srm_oper_config_t cut{};
    cut.in.buffer = frame.data;
    cut.in.pic_w = frame.width;
    cut.in.pic_h = frame.height;
    cut.in.block_w = width_;
    cut.in.block_h = height_;
    cut.in.block_offset_x = crop_x_;
    cut.in.block_offset_y = crop_y_;
    cut.in.srm_cm = PPA_SRM_COLOR_MODE_YUV420;
    cut.in.yuv_range = PPA_COLOR_RANGE_LIMIT;
    cut.in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
    cut.out.buffer = crop_;
    cut.out.buffer_size = static_cast<size_t>(width_) * height_ * 3 / 2;
    cut.out.pic_w = width_;
    cut.out.pic_h = height_;
    cut.out.srm_cm = PPA_SRM_COLOR_MODE_YUV420;
    cut.out.yuv_range = PPA_COLOR_RANGE_LIMIT;
    cut.out.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
    cut.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    cut.scale_x = cut.scale_y = 1.0f;
    cut.mode = PPA_TRANS_MODE_BLOCKING;
    const bool ok = ppa_do_scale_rotate_mirror(ppa_, &cut) == ESP_OK;
    crop_us_ += static_cast<uint32_t>(esp_timer_get_time() - started);
    if (!ok) {
      failed_++;
      slot.state = 0;
      return;
    }
    picture = crop_;
  }
  esp_h264_enc_in_frame_t in{};
  in.raw_data.buffer = const_cast<uint8_t *>(picture);
  in.raw_data.len = static_cast<uint32_t>(width_) * height_ * 3 / 2;
  in.pts = frame.timestamp_90khz;
  esp_h264_enc_out_frame_t out{};
  out.raw_data.buffer = slot.data;
  out.raw_data.len = ENCODED_BYTES;
  if (esp_h264_enc_process(encoder_, &in, &out) != ESP_H264_ERR_OK || out.length == 0 || out.length > ENCODED_BYTES) {
    failed_++;
    slot.state = 0;
    return;
  }
  slot.length = out.length;
  slot.timestamp = frame.timestamp_90khz;
  frames_++;
  bytes_ += out.length;
  if (xQueueSend(ready_, &index, 0) != pdTRUE) slot.state = 0;
}

#ifdef USE_SCREEN_VIDEO_SOUND
void ScreenVideo::setup_sound_() {
  if (mic_ == nullptr) return;
  const size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  sound_rate_ = mic_->get_audio_stream_info().get_sample_rate();
  // A second of sound between the microphone's task and the server's, in PSRAM: more than any pause of the sender.
  sound_ = xStreamBufferCreateWithCaps(sound_rate_ * 2, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  // As Espressif's own WebRTC port drives this encoder: the library's defaults (complexity 0, no FEC), only the rate,
  // the channel and the bitrate set. esp_audio_codec 2.5.0: 2.6 and later build only for P4 chips of revision 3.
  esp_opus_enc_config_t opus = ESP_OPUS_ENC_CONFIG_DEFAULT();
  opus.sample_rate = sound_rate_;
  opus.channel = 1;
  opus.bitrate = 32000;
  opus.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS;
  int out_bytes = 0;
  if (sound_ == nullptr || esp_opus_enc_open(&opus, sizeof(opus), &opus_) != ESP_AUDIO_ERR_OK ||
      esp_opus_enc_get_frame_size(opus_, &opus_in_bytes_, &out_bytes) != ESP_AUDIO_ERR_OK ||
      out_bytes > static_cast<int>(sizeof(opus_out_)) || opus_in_bytes_ <= 0 || opus_in_bytes_ > 1920) {
    ESP_LOGE(TAG, "No Opus encoder: the video goes without sound");
    opus_ = nullptr;
    return;
  }
  opus_out_bytes_ = out_bytes;
  ESP_LOGI(TAG, "Opus ready; it keeps %d bytes of internal RAM",
           (int) internal_before - (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  mic_->add_data_callback([this](const std::vector<uint8_t> &data) {
    mic_samples_ += data.size() / 2;
    mic_at_us_ = esp_timer_get_time();
    if (!wants_sound_.load()) return;
    // Whole chunks only. A partial write (an odd count of bytes once the buffer is nearly full) splits every later
    // 16-bit sample over two, and the sound is white noise until the next PLAY. A chunk that does not fit is dropped
    // whole, and its samples move the RTP clock on, so picture and sound stay in step.
    if (xStreamBufferSpacesAvailable(sound_) < data.size()) {
      sound_lost_ += data.size();
      sound_skipped_ += data.size() / 2;
      return;
    }
    xStreamBufferSend(sound_, data.data(), data.size(), 0);
  });
}
#endif

// ---- RTSP: RTP over each viewer's own connection (RFC 2326 10.12), H.264 as RFC 6184 (single NAL units, FU-A) ----
void ScreenVideo::server_task_(void *self) {
  static_cast<ScreenVideo *>(self)->serve_();
  // Only when the port could not be opened. A task made with its stack in PSRAM is not deleted from itself: it rests.
  while (true) vTaskDelay(portMAX_DELAY);
}

bool ScreenVideo::queue_(int fd, const uint8_t *data, size_t length) {
  if (length > OUT_BYTES) return send_all_(fd, data, length);
  if (out_len_ + length > OUT_BYTES && !flush_(fd)) return false;
  memcpy(out_ + out_len_, data, length);
  out_len_ += length;
  return true;
}

bool ScreenVideo::flush_(int fd) {
  const size_t length = out_len_;
  out_len_ = 0;
  return length == 0 || send_all_(fd, out_, length);
}

bool ScreenVideo::send_all_(int fd, const uint8_t *data, size_t length) {
  while (length > 0) {
    const int n = send(fd, data, length, 0);
    if (n <= 0) return false;
    data += n;
    length -= n;
  }
  return true;
}

// The value of a header line, found without regard to case.
static std::string header(const std::string &request, const char *name) {
  const std::string key = std::string("\r\n") + name + ":";
  std::string lower = request, lkey = key;
  for (auto &ch : lower) ch = tolower(ch);
  for (auto &ch : lkey) ch = tolower(ch);
  size_t at = lower.find(lkey);
  if (at == std::string::npos) return "";
  at += key.size();
  while (at < request.size() && request[at] == ' ') ++at;
  const size_t end = request.find("\r\n", at);
  return request.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

void ScreenVideo::serve_() {
  const int listener = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in at{};
  at.sin_family = AF_INET;
  at.sin_port = htons(port_);
  at.sin_addr.s_addr = htonl(INADDR_ANY);
  int on = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (listener < 0 || bind(listener, reinterpret_cast<sockaddr *>(&at), sizeof(at)) != 0 || listen(listener, 2) != 0) {
    ESP_LOGE(TAG, "Cannot listen on port %u", port_);
    return;
  }
  // One user, "tessera", with Basic authentication (RTSP's own, as go2rtc and ffmpeg send it). The password is worked
  // out from the screen's API key when it is built (__init__.py), and so by Home Assistant's tessera_video.
  unsigned char encoded[128];
  size_t encoded_len = 0;
  const std::string user_pass = std::string("tessera:") + password_;
  mbedtls_base64_encode(encoded, sizeof(encoded), &encoded_len, reinterpret_cast<const unsigned char *>(user_pass.data()),
                        user_pass.size());
  expected_auth_ = "Basic " + std::string(reinterpret_cast<char *>(encoded), encoded_len);
  while (true) {
    // Whoever has something to say, within the sender's pace (sound every 20 ms).
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(listener, &readable);
    int highest = listener;
    for (auto &v : viewers_) {
      if (v.fd < 0) continue;
      FD_SET(v.fd, &readable);
      highest = std::max(highest, v.fd);
    }
    timeval wait{0, 10000};
    if (select(highest + 1, &readable, nullptr, nullptr, &wait) > 0) {
      if (FD_ISSET(listener, &readable)) accept_(listener);
      for (auto &v : viewers_)
        if (v.fd >= 0 && FD_ISSET(v.fd, &readable)) read_(v);
    }
    const uint32_t now = millis();
    for (auto &v : viewers_)
      if (v.fd >= 0 && !v.playing && now - v.silent_since > 30000) drop_(v);
    if (!anyone_playing_()) continue;
    // Every access unit the encoder finished, to every viewer that plays; the sound first (a picture late is a
    // picture dropped, a sound late is a gap).
    // Room in internal RAM first (INTERNAL_LOW): heap_caps_get_free_size reads a counter, it walks nothing.
    const size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (!holding_ && internal < INTERNAL_LOW) {
      holding_ = true;
      ESP_LOGW(TAG, "Internal RAM low (%u bytes free): the video skips frames until there is room", (unsigned) internal);
    } else if (holding_ && internal >= INTERNAL_AGAIN) {
      holding_ = false;
      want_idr_ = true;  // the frames in between are gone: a key frame to start from
      ESP_LOGI(TAG, "Room again in internal RAM: the video goes on");
    }
    int index;
    while (xQueueReceive(ready_, &index, 0) == pdTRUE) {
#ifdef USE_SCREEN_VIDEO_SOUND
      if (wants_sound_.load()) send_sound_();
#endif
      Slot &slot = slots_[index];
      slot.state = 2;
      if (holding_) {
        held_++;
      } else {
        for (auto &v : viewers_)
          if (v.fd >= 0 && v.playing && !send_access_unit_(v, slot)) drop_(v);
      }
      slot.state = 0;
    }
#ifdef USE_SCREEN_VIDEO_SOUND
    if (wants_sound_.load()) send_sound_();
#endif
    // Sender reports every second on both tracks: what a viewer lines picture and sound up by.
    for (auto &v : viewers_) {
      if (v.fd < 0 || !v.playing || now - v.reports_at < 1000) continue;
      v.reports_at = now;
      if (!send_reports_(v)) drop_(v);
    }
  }
}

bool ScreenVideo::anyone_playing_() const {
  for (const auto &v : viewers_)
    if (v.fd >= 0 && v.playing) return true;
  return false;
}

void ScreenVideo::accept_(int listener) {
  const int fd = accept(listener, nullptr, nullptr);
  if (fd < 0) return;
  Viewer *free = nullptr;
  for (auto &v : viewers_)
    if (v.fd < 0 && free == nullptr) free = &v;
  if (free == nullptr) {
    ESP_LOGW(TAG, "A viewer was refused: %d watch already", MAX_VIEWERS);
    close(fd);
    return;
  }
  timeval send_wait{2, 0};
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_wait, sizeof(send_wait));
  int nodelay = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
  *free = Viewer{};
  free->fd = fd;
  free->silent_since = millis();
}

void ScreenVideo::drop_(Viewer &v) {
  if (v.fd < 0) return;
  close(v.fd);
  v.fd = -1;
  v.playing = false;
  if (!anyone_playing_()) {
    playing_ = false;
    int index;
    while (xQueueReceive(ready_, &index, 0) == pdTRUE) slots_[index].state = 0;
#ifdef USE_SCREEN_VIDEO_SOUND
    wants_sound_ = false;
#endif
    ESP_LOGI(TAG, "Nobody watches any more");
  }
}

void ScreenVideo::read_(Viewer &v) {
  char chunk[1024];
  const int n = recv(v.fd, chunk, sizeof(chunk), 0);
  if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    drop_(v);
    return;
  }
  if (n <= 0) return;
  v.pending.append(chunk, n);
  v.silent_since = millis();
  // What the viewer sent: RTCP over the connection ('$' channel length data), skipped, or RTSP requests.
  while (v.fd >= 0 && !v.pending.empty()) {
    if (v.pending[0] == '$') {
      if (v.pending.size() < 4) break;
      const size_t len = (static_cast<uint8_t>(v.pending[2]) << 8) | static_cast<uint8_t>(v.pending[3]);
      if (v.pending.size() < 4 + len) break;
      v.pending.erase(0, 4 + len);
      continue;
    }
    const size_t end = v.pending.find("\r\n\r\n");
    if (end == std::string::npos) {
      if (v.pending.size() > 4096) drop_(v);  // no request is this long
      break;
    }
    const std::string request = "\r\n" + v.pending.substr(0, end + 2);
    v.pending.erase(0, end + 4);
    if (!request_(v, request)) drop_(v);
  }
}

bool ScreenVideo::request_(Viewer &v, const std::string &request) {
  sockaddr_in self{};
  socklen_t self_len = sizeof(self);
  getsockname(v.fd, reinterpret_cast<sockaddr *>(&self), &self_len);
  char ip[16];
  inet_ntoa_r(self.sin_addr, ip, sizeof(ip));
  const std::string method = request.substr(2, request.find(' ', 2) - 2);
  const std::string cseq = header(request, "CSeq");
  std::string reply = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\n";
  std::string body;
  const bool allowed = header(request, "Authorization") == expected_auth_;
  bool teardown = false;
  if (method != "OPTIONS" && !allowed) {
    reply = "RTSP/1.0 401 Unauthorized\r\nCSeq: " + cseq + "\r\nWWW-Authenticate: Basic realm=\"Tessera\"\r\n";
  } else if (method == "OPTIONS") {
    reply += "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER\r\n";
  } else if (method == "DESCRIBE") {
    body = std::string("v=0\r\no=- 0 0 IN IP4 ") + ip + "\r\ns=Tessera camera\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\n" +
           "a=control:*\r\nm=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\n" +
           "a=fmtp:96 packetization-mode=1;profile-level-id=42e01f\r\na=control:track1\r\n";
#ifdef USE_SCREEN_VIDEO_SOUND
    // Opus in RTP counts at 48 kHz and says two channels whatever it carries (RFC 7587).
    if (opus_ != nullptr)
      body += "m=audio 0 RTP/AVP 111\r\na=rtpmap:111 opus/48000/2\r\n"
              "a=fmtp:111 minptime=10;useinbandfec=1;sprop-stereo=0\r\na=control:track2\r\n";
#endif
    std::string base = request.substr(method.size() + 3, request.find(' ', method.size() + 3) - method.size() - 3);
    if (base.empty() || base.back() != '/') base += '/';
    reply += "Content-Base: " + base + "\r\nContent-Type: application/sdp\r\n";
  } else if (method == "SETUP") {
    const std::string transport = header(request, "Transport");
    if (transport.find("interleaved") == std::string::npos) {
      reply = "RTSP/1.0 461 Unsupported Transport\r\nCSeq: " + cseq + "\r\n";
    } else {
      // The channels the viewer asked for (interleaved=a-b): the picture's on track1, the sound's on track2.
      const size_t at = transport.find("interleaved=");
      const bool sound = request.substr(0, request.find("RTSP/1.0")).find("track2") != std::string::npos;
      const int channel = at == std::string::npos ? (sound ? 2 : 0) : std::clamp(atoi(transport.c_str() + at + 12), 0, 254);
      if (sound) {
        v.sound_channel = channel;
      } else {
        v.video_channel = channel;
      }
      reply += "Transport: RTP/AVP/TCP;unicast;interleaved=" + std::to_string(channel) + "-" +
               std::to_string(channel + 1) + "\r\nSession: 54455353;timeout=60\r\n";
    }
  } else if (method == "PLAY") {
    reply += "Session: 54455353\r\nRange: npt=0.000-\r\n";
  } else if (method == "TEARDOWN") {
    reply += "Session: 54455353\r\n";
    teardown = true;
  } else if (method != "GET_PARAMETER" && method != "SET_PARAMETER") {
    reply = "RTSP/1.0 405 Method Not Allowed\r\nCSeq: " + cseq + "\r\n";
  }
  if (!body.empty()) reply += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  reply += "\r\n" + body;
  if (!send_all_(v.fd, reinterpret_cast<const uint8_t *>(reply.data()), reply.size())) return false;
  if (teardown) return false;
  if (method == "PLAY" && allowed) {
    const bool first = !anyone_playing_();
    v.playing = true;
    v.reports_at = 0;
    if (first) {
      int stale;
      while (xQueueReceive(ready_, &stale, 0) == pdTRUE) {
      }
      for (auto &slot : slots_) slot.state = 0;
      last_frame_us_ = 0;
      playing_ = true;
    }
    want_idr_ = true;  // a key frame for the newcomer, soon
#ifdef USE_SCREEN_VIDEO_SOUND
    if (opus_ != nullptr && v.sound_channel >= 0 && !wants_sound_.load()) {
      vTaskDelay(pdMS_TO_TICKS(5));  // a write of the microphone's task that was under way finishes
      xStreamBufferReset(sound_);
      sound_skipped_ = 0;
      esp_opus_enc_reset(opus_);
      sound_start_us_ = 0;
      sound_sent_ = 0;
      sound_octets_ = 0;
      sound_report_packets_ = 0;
      mic_samples_at_play_ = mic_samples_.load();
      wants_sound_ = true;
    }
#endif
    ESP_LOGI(TAG, "A viewer watches%s", first ? "" : " (one more)");
  }
  return true;
}

bool ScreenVideo::send_access_unit_(Viewer &v, const Slot &slot) {
  const int fd = v.fd;
  // The access unit's NAL units, between Annex-B start codes (00 00 01 or 00 00 00 01).
  const uint8_t *d = slot.data;
  const size_t len = slot.length;
  size_t starts[64], ends[64];
  int count = 0;
  for (size_t i = 0; i + 3 <= len && count < 64; ++i) {
    if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
      if (count > 0) ends[count - 1] = (i > 0 && d[i - 1] == 0) ? i - 1 : i;
      starts[count++] = i + 3;
      i += 2;
    }
  }
  if (count == 0) return true;
  ends[count - 1] = len;
  uint8_t packet[4 + 12 + 2 + RTP_PAYLOAD];
  auto rtp = [&](const uint8_t *payload_head, size_t head_len, const uint8_t *payload, size_t payload_len,
                 bool marker) -> bool {
    const size_t rtp_len = 12 + head_len + payload_len;
    packet[0] = '$';
    packet[1] = static_cast<uint8_t>(v.video_channel);
    packet[2] = rtp_len >> 8;
    packet[3] = rtp_len & 0xFF;
    uint8_t *h = packet + 4;
    h[0] = 0x80;
    h[1] = (marker ? 0x80 : 0) | 96;
    h[2] = v.seq >> 8;
    h[3] = v.seq & 0xFF;
    ++v.seq;
    h[4] = slot.timestamp >> 24;
    h[5] = slot.timestamp >> 16;
    h[6] = slot.timestamp >> 8;
    h[7] = slot.timestamp;
    h[8] = ssrc_ >> 24;
    h[9] = ssrc_ >> 16;
    h[10] = ssrc_ >> 8;
    h[11] = ssrc_;
    if (head_len) memcpy(h + 12, payload_head, head_len);
    memcpy(h + 12 + head_len, payload, payload_len);
    v.video_packets++;
    v.video_octets += head_len + payload_len;
    return queue_(fd, packet, 4 + rtp_len);
  };
  for (int k = 0; k < count; ++k) {
    const uint8_t *nal = d + starts[k];
    const size_t nal_len = ends[k] - starts[k];
    if (nal_len == 0) continue;
    const bool last = k == count - 1;
    if (nal_len <= RTP_PAYLOAD) {
      if (!rtp(nullptr, 0, nal, nal_len, last)) return false;
      continue;
    }
    // FU-A: the NAL unit's header in two bytes, then its body in pieces.
    const uint8_t indicator = (nal[0] & 0xE0) | 28;
    const uint8_t type = nal[0] & 0x1F;
    size_t at = 1;
    while (at < nal_len) {
      const size_t piece = std::min(RTP_PAYLOAD - 2, nal_len - at);
      const bool first = at == 1, end = at + piece == nal_len;
      const uint8_t head[2] = {indicator, static_cast<uint8_t>((first ? 0x80 : 0) | (end ? 0x40 : 0) | type)};
      if (!rtp(head, 2, nal + at, piece, last && end)) return false;
      at += piece;
    }
  }
  return flush_(fd);
}

#ifdef USE_SCREEN_VIDEO_SOUND
void ScreenVideo::send_sound_() {
  // Every whole 20 ms the microphone gave, as one Opus frame in one RTP packet (RFC 7587) to every viewer that set the
  // sound up: its timestamp counts at 48 kHz whatever the microphone's rate, 960 a frame.
  if (opus_ == nullptr) return;
  uint8_t raw[1920];
  uint8_t packet[4 + 12 + sizeof(opus_out_)];
  const uint32_t samples = opus_in_bytes_ / 2;
  while (xStreamBufferBytesAvailable(sound_) >= static_cast<size_t>(opus_in_bytes_)) {
    if (xStreamBufferReceive(sound_, raw, opus_in_bytes_, 0) != static_cast<size_t>(opus_in_bytes_)) break;
    if (sound_start_us_ == 0) {
      // When these samples were heard: the microphone's last delivery, less what it delivered after them.
      const uint32_t after = mic_samples_.load() - mic_samples_at_play_ - samples;
      sound_start_us_ = mic_at_us_.load() - static_cast<int64_t>(after) * 1000000 / sound_rate_ -
                        static_cast<int64_t>(samples) * 1000000 / sound_rate_;
      sound_start_ts_ = static_cast<uint32_t>(sound_start_us_ * 48 / 1000);
    }
    esp_audio_enc_in_frame_t in{raw, static_cast<uint32_t>(opus_in_bytes_)};
    esp_audio_enc_out_frame_t out{};
    out.buffer = opus_out_;
    out.len = opus_out_bytes_;
    const esp_audio_err_t err = esp_opus_enc_process(opus_, &in, &out);
    sound_sent_ += sound_skipped_.exchange(0);  // the samples the microphone dropped: time passed without them
    const uint32_t ts = sound_start_ts_ + static_cast<uint32_t>(static_cast<uint64_t>(sound_sent_) * 48000 / sound_rate_);
    sound_sent_ += samples;
    if (err != ESP_AUDIO_ERR_OK || out.encoded_bytes == 0) continue;
    const size_t rtp_len = 12 + out.encoded_bytes;
    packet[0] = '$';
    packet[2] = rtp_len >> 8;
    packet[3] = rtp_len & 0xFF;
    uint8_t *hd = packet + 4;
    hd[0] = 0x80;
    hd[1] = 111;
    hd[4] = ts >> 24;
    hd[5] = ts >> 16;
    hd[6] = ts >> 8;
    hd[7] = ts;
    hd[8] = sound_ssrc_ >> 24;
    hd[9] = sound_ssrc_ >> 16;
    hd[10] = sound_ssrc_ >> 8;
    hd[11] = sound_ssrc_;
    memcpy(hd + 12, opus_out_, out.encoded_bytes);
    for (auto &v : viewers_) {
      if (v.fd < 0 || !v.playing || v.sound_channel < 0) continue;
      packet[1] = static_cast<uint8_t>(v.sound_channel);
      hd[2] = v.sound_seq >> 8;
      hd[3] = v.sound_seq & 0xFF;
      ++v.sound_seq;
      if (!send_all_(v.fd, packet, 4 + rtp_len)) drop_(v);
    }
    sound_packets_++;
    sound_octets_ += out.encoded_bytes;
    sound_report_packets_++;
  }
}
#endif

// An RTCP sender report (RFC 3550 6.4.1) over the connection, on the channel after the track's own.
static void sender_report(uint8_t *at, uint8_t channel, uint32_t ssrc, uint64_t now_us, uint32_t rtp, uint32_t packets,
                          uint32_t octets) {
  at[0] = '$';
  at[1] = channel;
  at[2] = 0;
  at[3] = 28;
  uint8_t *r = at + 4;
  r[0] = 0x80;
  r[1] = 200;  // SR
  r[2] = 0;
  r[3] = 6;  // 28 bytes, in 32-bit words less one
  const uint32_t ntp_s = static_cast<uint32_t>(now_us / 1000000) + 2208988800UL;
  const uint32_t ntp_f = static_cast<uint32_t>(((now_us % 1000000) << 32) / 1000000);
  const uint32_t words[6] = {ssrc, ntp_s, ntp_f, rtp, packets, octets};
  for (int i = 0; i < 6; ++i) {
    r[4 + i * 4] = words[i] >> 24;
    r[5 + i * 4] = words[i] >> 16;
    r[6 + i * 4] = words[i] >> 8;
    r[7 + i * 4] = words[i];
  }
}

bool ScreenVideo::send_reports_(Viewer &v) {
  // Both clocks against one wall clock, esp_timer: the camera stamps its frames at 90 kHz from it, the sound's start
  // was taken from it.
  const int fd = v.fd;
  const int64_t now = esp_timer_get_time();
  uint8_t buf[2 * 32];
  size_t len = 0;
  if (v.video_packets > 0) {
    sender_report(buf + len, v.video_channel + 1, ssrc_, now, static_cast<uint32_t>(static_cast<uint64_t>(now) * 9 / 100),
                  v.video_packets, v.video_octets);
    len += 32;
  }
#ifdef USE_SCREEN_VIDEO_SOUND
  if (v.sound_channel >= 0 && sound_start_us_ != 0) {
    const uint32_t rtp = sound_start_ts_ + static_cast<uint32_t>((now - sound_start_us_) * 48 / 1000);
    sender_report(buf + len, v.sound_channel + 1, sound_ssrc_, now, rtp, sound_report_packets_, sound_octets_);
    len += 32;
  }
#endif
  if (len == 0) return true;
  if (!queue_(fd, buf, len)) return false;
  return flush_(fd);
}

}  // namespace esphome::screen_video
