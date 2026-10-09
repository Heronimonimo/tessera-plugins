#include "tab5_audio.h"
#include <cstdio>
#include <string>

namespace esphome::tab5_audio {

using tessera::Font;
namespace ui = tessera::ui;

// Days since 1970-01-01 for a date (Howard Hinnant's days_from_civil): the difference of two of these is whole days.
static long days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}

// The tile: a big number of days, and what happens then under it.
// create() makes the parts once; on_tick() sets their texts, and ui::set_text changes a label only when its text differs.
class DaysTile : public tessera::Tile {
 public:
  explicit DaysTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    unsigned y = 0, m = 0, d = 0;
    const char *date = c.options["date"] | "";
    has_date_ = sscanf(date, "%u-%u-%u", &y, &m, &d) == 3 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
    if (has_date_) target_ = days_from_civil(static_cast<int>(y), m, d);
    what_text_ = c.options["what"] | "";
    if (what_text_.empty()) what_text_ = c.name;

    number_ = ui::label(c.parent, Font::VALUE, theme::INK);
    lv_obj_set_width(number_, width_);
    lv_obj_set_style_text_align(number_, LV_TEXT_ALIGN_CENTER, 0);
    what_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(what_, width_);
    lv_obj_set_style_text_align(what_, LV_TEXT_ALIGN_CENTER, 0);
  }

  void on_tick(uint32_t epoch) override {
    std::string big, small = what_text_;
    if (!has_date_) {
      big = plugin_->text("no_date");
    } else if (!epoch) {
      big = "";  // the screen's clock is not set yet
    } else {
      const tessera::LocalTime now = tessera::local_time(epoch);
      const long left = target_ - days_from_civil(now.year, static_cast<unsigned>(now.month), static_cast<unsigned>(now.day));
      big = left == 0 ? plugin_->text("today") : left < 0 ? plugin_->text("passed") : tessera::format(plugin_->text("days"), left);
    }
    // The largest of the screen's fonts the words fit in.
    Font face = Font::VALUE;
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE}) {
      face = f;
      if (ui::text_width(big, f) <= width_) break;
    }
    ui::set_font(number_, face);
    ui::set_text(number_, big);
    ui::set_text(what_, small);
    // The number and the words under it, together in the middle of the card.
    const int nh = ui::line_height(face), wh = small.empty() ? 0 : ui::line_height(Font::BODY);
    const int top = (height_ - nh - wh) / 2;
    lv_obj_set_pos(number_, 0, top);
    lv_obj_set_height(number_, nh);
    lv_obj_set_pos(what_, 0, top + nh);
    lv_obj_set_height(what_, wh ? wh : 1);
  }

  // Light and dark: the theme roles have other values now.
  void on_theme() override {
    ui::set_color(number_, theme::INK);
    ui::set_color(what_, theme::MUTED);
  }

 private:
  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  bool has_date_ = false;
  long target_ = 0;
  std::string what_text_;
  lv_obj_t *number_{}, *what_{};
};

void Tab5Audio::setup() {
  // The id is the tile's id in the manifest; the core makes a DaysTile for every card that shows one.
  add_tile("days_until", [this]() { return new DaysTile(this); });
}

}  // namespace esphome::tab5_audio
