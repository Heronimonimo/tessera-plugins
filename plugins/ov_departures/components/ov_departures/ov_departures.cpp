#include "ov_departures.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome::ov_departures {

using tessera::Font;
namespace ui = tessera::ui;

// One departure as the add-on maps it (tessera-plugin.yaml, fetch "departures"): the line's public number, where it
// goes, and when it leaves (expected, and as planned), in seconds since 1970.
struct Departure {
  std::string line, to;
  uint32_t at = 0, plan = 0;
};

// The tile. A single cell: the next departure big, with its line and destination above it and the two after it below.
// Two columns or more: a row per departure, as many as fit. The add-on sends new times once a minute; the countdown
// runs every second on the screen's own clock, and only a text that changes is set again.
class NextTile : public tessera::Tile {
 public:
  explicit NextTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    walk_ = c.options["walk"] | 0;
    has_stop_ = !std::string(c.options["stop"] | "").empty();
    late_ = c.options["late"] | true;
    list_ = c.columns >= 2;
    head_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY)) + ui::px(4);
    gap_ = ui::px(ui::large() ? 8 : 5);
    if (list_) {
      const int row = head_ + gap_;
      rows_.resize(std::max(1, std::min(6, (height_ + gap_) / row)));
      for (size_t i = 0; i < rows_.size(); ++i) make_row(rows_[i], c.parent, static_cast<int>(i) * row);
      note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(note_, width_);
      lv_obj_set_pos(note_, 0, height_ - ui::line_height(Font::BODY));
    } else {
      rows_.resize(1);
      make_row(rows_[0], c.parent, 0);
      big_ = ui::label(c.parent, Font::VALUE, theme::INK);
      lv_obj_set_width(big_, width_);
      later_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(later_, width_);
      lv_obj_set_pos(later_, 0, height_ - ui::line_height(Font::BODY));
    }
  }

  void on_state(JsonObjectConst data) override {
    departures_.clear();
    for (JsonObjectConst item : data["items"].as<JsonArrayConst>()) {
      Departure d;
      d.line = item["line"] | "";
      d.to = item["to"] | "";
      d.at = item["at"] | 0u;
      d.plan = item["plan"] | 0u;
      if (d.at) departures_.push_back(std::move(d));
    }
    std::sort(departures_.begin(), departures_.end(), [](const Departure &a, const Departure &b) { return a.at < b.at; });
    stale_ = data["stale"] | false;
    wait_ = data["wait"] | "";
  }

  void on_tick(uint32_t now) override {
    // The departures you can still make: not gone, and not leaving before you could walk there.
    std::vector<const Departure *> next;
    for (const auto &d : departures_)
      if (now && d.at + 30 >= now + static_cast<uint32_t>(walk_) * 60) next.push_back(&d);
    const char *why = !has_stop_ ? "choose_stop" : !now ? "waiting" : !wait_.empty() ? (wait_ == "failed" ? "failed" : "waiting")
                                                        : next.empty() ? "none" : nullptr;
    if (list_) paint_list(next, why, now);
    else paint_single(next, why, now);
  }

  void on_theme() override {
    for (auto &r : rows_) {
      lv_obj_set_style_bg_color(r.badge, ui::color(theme::ACCENT), 0);
      ui::set_color(r.number, theme::ON_ACCENT);
      ui::set_color(r.to, list_ ? theme::INK : theme::MUTED);
      if (r.when) ui::set_color(r.when, theme::INK);
    }
    if (big_) ui::set_color(big_, theme::INK);
    if (later_) ui::set_color(later_, theme::MUTED);
    if (note_) ui::set_color(note_, theme::MUTED);
  }

 private:
  struct Row {
    lv_obj_t *badge{}, *number{}, *to{}, *when{};
    int y = 0;
  };

  void make_row(Row &r, lv_obj_t *parent, int y) {
    r.y = y;
    r.badge = ui::block(parent, theme::ACCENT);
    r.number = ui::label(r.badge, Font::TITLE, theme::ON_ACCENT);
    lv_label_set_long_mode(r.number, LV_LABEL_LONG_CLIP);
    lv_obj_center(r.number);
    r.to = ui::label(parent, Font::BODY, list_ ? theme::INK : theme::MUTED);
    if (list_) {
      r.when = ui::label(parent, Font::TITLE, theme::INK);
      lv_obj_set_style_text_align(r.when, LV_TEXT_ALIGN_RIGHT, 0);
    }
  }

  // The line's badge as wide as its number, and the rest of the row beside it.
  void place_row(Row &r, const std::string &line, const std::string &to, const std::string &when) {
    const bool shown = !line.empty();
    lv_obj_set_style_opa(r.badge, shown ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    ui::set_text(r.number, line);
    const int pad = ui::px(ui::large() ? 8 : 5);
    const int badge_w = shown ? std::min(width_ / 2, ui::text_width(line, Font::TITLE) + 2 * pad) : 0;
    lv_obj_set_size(r.badge, std::max(1, badge_w), head_);
    lv_obj_set_pos(r.badge, 0, r.y);
    lv_obj_center(r.number);
    const int x = shown ? badge_w + ui::px(6) : 0;
    int right = width_;
    if (r.when) {
      const int when_w = when.empty() ? 0 : ui::text_width(when, Font::TITLE) + ui::px(2);
      ui::set_text(r.when, when);
      lv_obj_set_size(r.when, std::max(1, when_w), ui::line_height(Font::TITLE));
      lv_obj_set_pos(r.when, width_ - when_w, r.y + (head_ - ui::line_height(Font::TITLE)) / 2);
      right = width_ - when_w - (when_w ? ui::px(6) : 0);
    }
    ui::set_text(r.to, to);
    lv_obj_set_size(r.to, std::max(1, right - x), ui::line_height(Font::BODY));
    lv_obj_set_pos(r.to, x, r.y + (head_ - ui::line_height(Font::BODY)) / 2);
  }

  // Where it goes, and how late it runs when that is a minute or more (OVapi's expected time against the timetable's).
  std::string with_delay(const Departure &d) const {
    if (!late_ || !d.plan || d.at < d.plan + 60) return d.to;
    return d.to + "  " + tessera::format(plugin_->text("late"), static_cast<long>((d.at - d.plan) / 60));
  }

  // How long until a departure leaves. The next one you can make counts down to the second (12:05, 0:42), so you know
  // exactly when to go; the ones after it in whole minutes, which is all a glance at them needs. An hour or more away is
  // a time of day.
  std::string when_text(uint32_t at, uint32_t now, bool exact = false) const {
    if (exact) {
      if (at <= now) return plugin_->text("now");
      const uint32_t left = at - now;
      if (left >= 3600) return tessera::clock_text(at);
      char text[8];
      snprintf(text, sizeof text, "%u:%02u", static_cast<unsigned>(left / 60), static_cast<unsigned>(left % 60));
      return text;
    }
    if (at <= now + 59) return plugin_->text("now");
    const uint32_t minutes = (at - now) / 60;
    if (minutes >= 60) return tessera::clock_text(at);
    return tessera::format(plugin_->text("minutes"), static_cast<long>(minutes));
  }

  void paint_single(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    Row &r = rows_[0];
    if (why) {
      place_row(r, "", "", "");
      big_text(plugin_->text(why), Font::BODY_LARGE, theme::MUTED);
      ui::set_text(later_, "");
      return;
    }
    const Departure &d = *next[0];
    place_row(r, d.line, with_delay(d), "");
    // The time takes the largest face that fits the card. The line with the next times stays only where it costs the
    // time nothing: on a small cell (the CYD's) it goes, and the time is as large as the cell allows.
    const std::string when = when_text(d.at, now, true);
    // The face is chosen for the shape of the time, not its digits, so it never jumps a size as a second ticks by.
    const std::string shape = digits_as_zero(when);
    const Font alone = fitting(shape, height_ - head_);
    const bool with_later = fitting(shape, height_ - head_ - ui::line_height(Font::BODY)) == alone;
    const Font face = alone;
    big_text(when, face, theme::INK, with_later);
    std::string list;
    for (size_t i = 1; i < next.size() && i < 3; ++i) {
      if (!list.empty()) list += " \u00B7 ";
      list += when_text(next[i]->at, now);
    }
    if (!with_later) ui::set_text(later_, "");
    else if (stale_) ui::set_text(later_, plugin_->text("old"));
    else ui::set_text(later_, list.empty() ? std::string() : tessera::fill(plugin_->text("then"), "list", list));
  }

  static std::string digits_as_zero(std::string text) {
    for (char &c : text)
      if (c >= '1' && c <= '9') c = '0';
    return text;
  }

  // The largest of the big faces `text` fits in, `room` high and the card wide; BODY_LARGE when none does.
  Font fitting(const std::string &text, int room) const {
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE})
      if (ui::text_width(text, f) <= width_ && ui::line_height(f) <= room) return f;
    return Font::BODY_LARGE;
  }

  void big_text(const std::string &text, Font face, theme::Role role, bool with_later = true) {
    ui::set_font(big_, face);
    ui::set_color(big_, role);
    ui::set_text(big_, text);
    const int h = ui::line_height(face);
    lv_obj_set_height(big_, h);
    // Between the head and the line under it (or the card's foot without that line), in the middle.
    const int top = head_, bottom = height_ - (with_later ? ui::line_height(Font::BODY) : 0);
    lv_obj_set_pos(big_, 0, top + std::max(0, (bottom - top - h) / 2));
  }

  void paint_list(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i == 0 && why) {
        place_row(rows_[0], "", plugin_->text(why), "");
        continue;
      }
      if (why || i >= next.size()) {
        place_row(rows_[i], "", "", "");
        continue;
      }
      place_row(rows_[i], next[i]->line, with_delay(*next[i]), when_text(next[i]->at, now, i == 0));
    }
    // The bottom row says when the times are old, if there is room below the rows.
    const int used = static_cast<int>(rows_.size()) * (head_ + gap_);
    ui::set_text(note_, stale_ && used + ui::line_height(Font::BODY) <= height_ ? plugin_->text("old") : "");
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, head_ = 0, gap_ = 0, walk_ = 0;
  bool list_ = false, has_stop_ = false, stale_ = false, late_ = true;
  std::string wait_;
  std::vector<Row> rows_;
  lv_obj_t *big_{}, *later_{}, *note_{};
  std::vector<Departure> departures_;
};

void OvDepartures::setup() {
  add_tile("next", [this]() { return new NextTile(this); });
}

}  // namespace esphome::ov_departures
