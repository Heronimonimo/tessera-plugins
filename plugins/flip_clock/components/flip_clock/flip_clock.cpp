#include "flip_clock.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::flip_clock {

using tessera::Font;
namespace ui = tessera::ui;

// A face of the screensaver's clock (plugin API 0.14): the hours and the minutes as two flip cards in the screen's
// largest digits, a seconds dot between them, and the date with the outside temperature under them.
//
// A saver is drawn in the dark look on the black glass whatever the screen's own look, so its roles are white on black.
// The screen moves the whole of it a millimetre every minute (the manifest has no `moves: true`), so it stands still
// itself. show() makes every part once; on_state() and on_tick() only paint what changed: the dot each second, the
// digits each minute, the line under them when the day or the temperature changes.
class FlipSaver : public tessera::Saver {
 public:
  void show(const tessera::SaverContext &c) override {
    width_ = c.width;
    // An option the person never touched may not be in the word: read each with its default.
    seconds_ = c.options["seconds"] | true;
    pad_ = ui::mm(3);
    gap_ = ui::mm(4);
    // The cards are as large as the screen's largest digits make them: the font sizes with the glass, a card with it.
    card_w_ = ui::text_width("88", Font::DIGITS) + 2 * pad_;
    card_h_ = ui::line_height(Font::DIGITS) + 2 * pad_;
    const int row = ui::line_height(Font::TITLE);
    top_ = (c.height - card_h_ - gap_ - row) / 2;
    left_ = (c.width - 2 * card_w_ - gap_) / 2;
    hours_ = flip_card(c.parent, left_);
    minutes_ = flip_card(c.parent, left_ + card_w_ + gap_);
    // The seconds dot, in the middle of the gap between the cards.
    const int dot = ui::mm(2);
    dot_ = ui::block(c.parent, theme::INK_SOFT);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(dot_, dot, dot);
    lv_obj_set_pos(dot_, left_ + card_w_ + (gap_ - dot) / 2, top_ + (card_h_ - dot) / 2);
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    // AM or PM on a 12-hour screen, small at the foot of the minutes' card, outside it.
    suffix_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_pos(suffix_, left_ + 2 * card_w_ + gap_ + ui::px(6), top_ + card_h_ - ui::line_height(Font::BODY));
    // The line under the cards: the date, and the temperature beside it.
    row_y_ = top_ + card_h_ + gap_;
    date_ = ui::label(c.parent, Font::TITLE, theme::INK_SOFT);
    temperature_ = ui::label(c.parent, Font::TITLE, theme::MUTED);
  }

  // The clock step's contract: `temperature` ("21°", "" for none) and `items` (the row of entities, not shown here).
  void on_state(JsonObjectConst data) override {
    const char *degrees = data["temperature"] | "";
    if (std::strncmp(degrees, degrees_, sizeof(degrees_)) == 0) return;
    std::snprintf(degrees_, sizeof(degrees_), "%s", degrees);
    paint_row();
  }

  void on_tick(uint32_t epoch) override {
    if (!epoch) {  // the screen's clock is not set yet
      ui::set_text(lv_obj_get_child(hours_, 0), "--");
      ui::set_text(lv_obj_get_child(minutes_, 0), "--");
      return;
    }
    // The dot: on in even seconds, off in odd ones. Only its own small box is drawn again.
    if (seconds_ && (epoch % 2 == 0) == lv_obj_has_flag(dot_, LV_OBJ_FLAG_HIDDEN)) {
      if (epoch % 2 == 0) lv_obj_remove_flag(dot_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    }
    if (epoch / 60 == minute_) return;
    minute_ = epoch / 60;
    // The time as the screen writes it ("14:05", or "2:05 PM" on 12 hours): the hours before the colon, the two
    // digits after it, and what follows them.
    const std::string clock = tessera::clock_text(epoch);
    const size_t colon = clock.find(':');
    if (colon == std::string::npos) return;
    ui::set_text(lv_obj_get_child(hours_, 0), clock.substr(0, colon));
    ui::set_text(lv_obj_get_child(minutes_, 0), clock.substr(colon + 1, 2));
    const size_t space = clock.find(' ', colon);
    const std::string suffix = space == std::string::npos ? "" : clock.substr(space + 1);
    const int sw = ui::text_width(suffix, Font::BODY);
    lv_obj_set_width(suffix_, sw > 0 ? sw : 1);
    ui::set_text(suffix_, suffix);
    const tessera::LocalTime now = tessera::local_time(epoch);
    if (now.day != day_) {
      day_ = now.day;
      paint_row();
    }
  }

 private:
  // A flip card: a raised block with the digits in its middle and a hinge across them, the black line where the card
  // folds. The hinge is a child of the digits, so it lies over them.
  lv_obj_t *flip_card(lv_obj_t *parent, int x) {
    lv_obj_t *card = ui::block(parent, theme::CARD);
    lv_obj_set_size(card, card_w_, card_h_);
    lv_obj_set_pos(card, x, top_);
    lv_obj_t *digits = ui::label(card, Font::DIGITS, theme::INK);
    lv_obj_set_width(digits, card_w_);
    lv_obj_set_style_text_align(digits, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(digits, 0, pad_);
    lv_obj_t *hinge = ui::block(digits, theme::CAMERA_PAGE);
    lv_obj_set_style_radius(hinge, 0, 0);
    const int line = ui::px(2) > 1 ? ui::px(2) : 1;
    lv_obj_set_size(hinge, card_w_, line);
    lv_obj_set_pos(hinge, 0, (ui::line_height(Font::DIGITS) - line) / 2);
    return card;
  }

  // The date and the temperature, side by side in the middle under the cards.
  void paint_row() {
    const uint32_t epoch = tessera::epoch();
    const std::string date = epoch ? tessera::date_text(epoch) : "";
    const std::string degrees = degrees_;
    const int dw = ui::text_width(date, Font::TITLE), tw = degrees.empty() ? 0 : ui::text_width(degrees, Font::TITLE);
    const int between = tw && dw ? ui::mm(3) : 0;
    const int x = (width_ - dw - between - tw) / 2;
    ui::set_text(date_, date);
    lv_obj_set_width(date_, dw > 0 ? dw : 1);
    lv_obj_set_pos(date_, x, row_y_);
    ui::set_text(temperature_, degrees);
    lv_obj_set_width(temperature_, tw > 0 ? tw : 1);
    lv_obj_set_pos(temperature_, x + dw + between, row_y_);
  }

  int width_ = 0, pad_ = 0, gap_ = 0, card_w_ = 0, card_h_ = 0, top_ = 0, left_ = 0, row_y_ = 0;
  bool seconds_ = true;
  uint32_t minute_ = 0;
  int day_ = 0;
  char degrees_[16] = "";
  lv_obj_t *hours_{}, *minutes_{}, *dot_{}, *suffix_{}, *date_{}, *temperature_{};
};

void FlipClock::setup() {
  // The id is the saver's id in the manifest, and its step must be the manifest's (clock).
  add_saver("flip", tessera::SaverStep::CLOCK, []() { return new FlipSaver(); });
}

}  // namespace esphome::flip_clock
