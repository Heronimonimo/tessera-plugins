#include "photo_clock.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace esphome::photo_clock {

using tessera::Font;
namespace ui = tessera::ui;

// A face of the screensaver's photos (plugin API 0.14): the photo over the whole glass, and the time small in a corner
// with the date under it, on a shade that keeps them readable on a bright photo.
//
// The photo is the step's own picture: one place with the address "saver:picture", which the app answers with the
// photo on the glass, cut to the place. Its key carries the photo's mark, so the next photo comes under the same
// address by itself. The photo fills the glass and stays put; the time and date walk Tessera's slow circle against
// burn-in (tessera::drift, `moves: true`), so nothing stands on the same pixels for hours and no black edge shows.
class PhotoSaver : public tessera::Saver {
 public:
  void show(const tessera::SaverContext &c) override {
    width_ = c.width;
    height_ = c.height;
    // An option the person never touched may not be in the word: read it with its default.
    const char *corner = c.options["corner"] | "bottom_left";
    right_ = std::strstr(corner, "right") != nullptr;
    top_ = std::strncmp(corner, "top", 3) == 0;
    if (ui::pictures()) {
      photo_ = ui::picture(c.parent);
      lv_obj_set_style_radius(photo_, 0, 0);
      lv_obj_set_size(photo_, c.width, c.height);
      lv_obj_set_pos(photo_, 0, 0);
    }
    // The shade, made after the photo so it lies over it, and the two lines in it.
    shade_ = ui::block(c.parent, theme::CAMERA_PAGE);
    lv_obj_set_style_bg_opa(shade_, LV_OPA_50, 0);
    lv_obj_add_flag(shade_, LV_OBJ_FLAG_HIDDEN);
    time_ = ui::label(shade_, Font::HEADLINE, theme::INK);
    date_ = ui::label(shade_, Font::BODY, theme::INK_SOFT);
  }

  // The photos step's contract: `picture` ("saver:picture"), its `mark`, `index`, `count` and `album`.
  void on_state(JsonObjectConst data) override {
    if (photo_) ui::set_picture(photo_, data["picture"] | "", tessera::Fit::COVER);
  }

  // The time and the date, once a minute: their texts, the shade's size and its corner.
  void on_tick(uint32_t epoch) override {
    if (!epoch || epoch / 60 == minute_) return;  // no clock yet, or the same minute
    minute_ = epoch / 60;
    const std::string time = tessera::clock_text(epoch), date = tessera::date_text(epoch);
    const int pad = ui::mm(2), m = ui::mm(4);
    const int tw = ui::text_width(time, Font::HEADLINE), dw = ui::text_width(date, Font::BODY);
    const int th = ui::line_height(Font::HEADLINE), dh = ui::line_height(Font::BODY);
    const int w = std::max(tw, dw) + 2 * pad, h = th + dh + 2 * pad;
    ui::set_text(time_, time);
    ui::set_text(date_, date);
    lv_obj_set_width(time_, tw);
    lv_obj_set_width(date_, dw);
    // In the shade, against the side of the glass's edge it stands at.
    lv_obj_set_pos(time_, right_ ? w - pad - tw : pad, pad);
    lv_obj_set_pos(date_, right_ ? w - pad - dw : pad, pad + th);
    lv_obj_set_size(shade_, w, h);
    const tessera::Point at = tessera::drift(epoch);
    lv_obj_set_pos(shade_, (right_ ? width_ - m - w : m) + at.x, (top_ ? m : height_ - m - h) + at.y);
    lv_obj_remove_flag(shade_, LV_OBJ_FLAG_HIDDEN);
  }

 private:
  int width_ = 0, height_ = 0;
  bool right_ = false, top_ = false;
  uint32_t minute_ = 0;
  lv_obj_t *photo_{}, *shade_{}, *time_{}, *date_{};
};

void PhotoClock::setup() {
  // The id is the saver's id in the manifest, and its step must be the manifest's (photos).
  add_saver("photo", tessera::SaverStep::PHOTOS, []() { return new PhotoSaver(); });
}

}  // namespace esphome::photo_clock
