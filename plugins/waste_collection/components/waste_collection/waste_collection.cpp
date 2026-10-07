#include "waste_collection.h"
#include "esphome/core/time.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome::waste_collection {

using tessera::Font;
namespace ui = tessera::ui;

// ---- What goes out, and when ----

// The colour of a kind of waste, by the words collection calendars use for it (Dutch and English): paper blue,
// garden and food waste green, plastic and packaging amber, glass purple, the rest in the card's muted ink.
static theme::Role colour_of(const std::string &what) {
  std::string low = what;
  std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
  auto has = [&](std::initializer_list<const char *> words) {
    for (const char *word : words)
      if (low.find(word) != std::string::npos) return true;
    return false;
  };
  if (has({"papier", "paper", "karton", "cardboard"})) return theme::MARK_BLUE;
  if (has({"gft", "groen", "green", "garden", "tuin", "bio", "organic", "food", "compost"})) return theme::MARK_GREEN;
  if (has({"plastic", "pmd", "pbd", "packag", "verpakking", "blik"})) return theme::MARK_AMBER;
  if (has({"glas", "glass"})) return theme::MARK_PURPLE;
  return theme::MUTED;
}

// Days since 1970 of a civil date, so two dates subtract to whole days (Howard Hinnant's days_from_civil).
static int32_t civil_days(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  return era * 146097 + static_cast<int32_t>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}

// The day of "2026-10-09", "2026-10-09 07:00:00" or "2026-10-09T07:00:00+02:00" as days since 1970; INT32_MIN for none.
static int32_t day_of(const std::string &text) {
  int y = 0, m = 0, d = 0;
  if (text.size() < 10 || sscanf(text.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31)
    return INT32_MIN;
  return civil_days(y, m, d);
}

// Today on the screen's own calendar, as days since 1970; INT32_MIN while the clock is not set.
static int32_t today() {
  const uint32_t now = tessera::epoch();
  if (!now) return INT32_MIN;
  const ESPTime t = ESPTime::from_epoch_local(now);
  return civil_days(t.year, t.month, t.day_of_month);
}

// A day (days since 1970) as the top bar writes it: "Fri 9 Oct".
static std::string day_text(int32_t day) { return tessera::date_text(static_cast<uint32_t>(day) * 86400u + 43200u); }

// "Today", "Tomorrow", "In 3 days".
static std::string when_text(const tessera::Plugin *plugin, int32_t days) {
  return days == 0 ? std::string(plugin->text("today")) : tessera::days_text(days);
}

// ---- The tile: the next collection of its calendar ----
class NextTile : public tessera::Tile {
 public:
  explicit NextTile(WasteCollection *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    entity_ = c.entity;
    tile_ = c.tile;
    const int head = std::max(ui::line_height(Font::TITLE), ui::px(ui::large() ? 16 : 10));
    dot_ = ui::block(c.parent, theme::MUTED);
    const int dot = ui::px(ui::large() ? 12 : 8);
    lv_obj_set_size(dot_, dot, dot);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(dot_, 0, (head - dot) / 2);
    what_ = ui::label(c.parent, Font::TITLE, theme::INK);
    lv_obj_set_pos(what_, dot + ui::px(6), (head - ui::line_height(Font::TITLE)) / 2);
    lv_obj_set_size(what_, std::max(1, width_ - dot - ui::px(6)), ui::line_height(Font::TITLE));
    head_ = head;
    when_ = ui::label(c.parent, Font::VALUE, theme::INK);
    lv_obj_set_width(when_, width_);
    date_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(date_, width_);
    lv_obj_set_pos(date_, 0, height_ - ui::line_height(Font::BODY));
  }

  void on_state(JsonObjectConst data) override {
    what_text_ = data["attributes"]["message"] | "";
    const uint32_t start = data["attributes"]["start_time"] | 0u;
    start_ = start;
    wait_ = data["wait"] | "";
  }

  void on_tick(uint32_t) override {
    const int32_t days = start_ ? tessera::days_from_today(start_) : INT32_MIN;
    if (what_text_.empty() || days == INT32_MIN || days < 0) {
      paint(theme::MUTED, "", plugin_->text(wait_.empty() ? "none" : "waiting"), "");
      return;
    }
    paint(colour_of(what_text_), what_text_, when_text(plugin_, days), tessera::date_text(start_));
  }

  void on_theme() override {
    ui::set_color(what_, theme::INK);
    ui::set_color(date_, theme::MUTED);
    on_tick(0);
  }

  void on_tap() override {
    tessera::open_card(plugin_->plugin_id(), "upcoming", entity_, tile_);
  }

 private:
  void paint(theme::Role colour, const std::string &what, const std::string &when, const std::string &date) {
    lv_obj_set_style_bg_color(dot_, ui::color(colour), 0);
    lv_obj_set_style_opa(dot_, what.empty() ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    ui::set_text(what_, what);
    ui::set_text(date_, date);
    // The largest face "Tomorrow" fits in, between the name and the date under it.
    const int room = height_ - head_ - (date.empty() ? 0 : ui::line_height(Font::BODY));
    Font face = Font::BODY_LARGE;
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE})
      if (ui::text_width(when, f) <= width_ && ui::line_height(f) <= room) { face = f; break; }
    ui::set_font(when_, face);
    ui::set_text(when_, when);
    const int h = ui::line_height(face);
    lv_obj_set_height(when_, h);
    lv_obj_set_pos(when_, 0, head_ + std::max(0, (room - h) / 2));
  }

  WasteCollection *plugin_;
  std::string entity_, what_text_, wait_;
  int width_ = 0, height_ = 0, head_ = 0, tile_ = -1;
  uint32_t start_ = 0;
  lv_obj_t *dot_{}, *what_{}, *when_{}, *date_{};
};

// ---- The card: the coming collections of four weeks, asked of Home Assistant through the app ----
class UpcomingCard : public tessera::Card {
 public:
  explicit UpcomingCard(WasteCollection *plugin) : plugin_(plugin) {}
  ~UpcomingCard() override {
    if (plugin_->open_card_ == this) plugin_->open_card_ = nullptr;
  }

  void open(const tessera::CardContext &c) override {
    parent_ = c.parent;
    width_ = c.width;
    height_ = c.height;
    note_ = ui::label(parent_, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_width(note_, width_);
    ui::set_text(note_, plugin_->text("waiting"));
    plugin_->open_card_ = this;
    // The calendar it was opened for, else the one the plugin reads (a sensor tile of a waste integration opens it too).
    const std::string entity = c.entity ? c.entity : "";
    const std::string calendar = entity.rfind("calendar.", 0) == 0 ? entity : plugin_->calendar();
    JsonDocument request;
    request["ask"] = "call_service:calendar.get_events";
    request["data"]["entity_id"] = calendar;
    request["data"]["duration"]["days"] = 28;
    calendar_ = calendar;
    asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
    if (!asked_) ui::set_text(note_, plugin_->text("failed"));
  }

  // The answer: {"re", "ok", "result": {"calendar.x": {"events": [{"start", "summary"}, ...]}}}.
  void answer(JsonObjectConst message) {
    if ((message["re"] | 0u) != asked_) return;
    if (!(message["ok"] | false)) {
      ui::set_text(note_, plugin_->text("failed"));
      return;
    }
    struct Row { int32_t day; std::string what; };
    std::vector<Row> rows;
    for (JsonObjectConst event : message["result"][calendar_]["events"].as<JsonArrayConst>()) {
      const int32_t day = day_of(event["start"] | "");
      if (day != INT32_MIN) rows.push_back({day, event["summary"] | ""});
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.day < b.day; });
    if (rows.empty()) {
      ui::set_text(note_, plugin_->text("none"));
      return;
    }
    lv_obj_add_flag(note_, LV_OBJ_FLAG_HIDDEN);
    // A row per collection, as many as fit: its colour, what goes out, and the day on the right.
    const int line = std::max(ui::line_height(Font::BODY_LARGE), ui::line_height(Font::BODY));
    const int gap = ui::px(ui::large() ? 14 : 8), dot = ui::px(ui::large() ? 12 : 8), step = line + gap;
    const int fit = std::max(1, (height_ + gap) / step);
    const int32_t now = today();
    for (int i = 0; i < static_cast<int>(rows.size()) && i < fit; ++i) {
      const int y = i * step;
      auto *mark = ui::block(parent_, colour_of(rows[i].what));
      lv_obj_set_size(mark, dot, dot);
      lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_pos(mark, 0, y + (line - dot) / 2);
      const int32_t days = now == INT32_MIN ? -1 : rows[i].day - now;
      const std::string when = days >= 0 && days <= 1 ? when_text(plugin_, days) : day_text(rows[i].day);
      auto *right = ui::label(parent_, Font::BODY, theme::MUTED);
      const int when_w = ui::text_width(when, Font::BODY) + ui::px(2);
      lv_obj_set_size(right, when_w, ui::line_height(Font::BODY));
      lv_obj_set_pos(right, width_ - when_w, y + (line - ui::line_height(Font::BODY)) / 2);
      ui::set_text(right, when);
      auto *what = ui::label(parent_, Font::BODY_LARGE, theme::INK);
      lv_obj_set_size(what, std::max(1, width_ - when_w - dot - ui::px(16)), ui::line_height(Font::BODY_LARGE));
      lv_obj_set_pos(what, dot + ui::px(8), y + (line - ui::line_height(Font::BODY_LARGE)) / 2);
      ui::set_text(what, rows[i].what);
    }
  }

 private:
  WasteCollection *plugin_;
  lv_obj_t *parent_{}, *note_{};
  int width_ = 0, height_ = 0;
  uint32_t asked_ = 0;
  std::string calendar_;
};

// ---- The plugin ----

void WasteCollection::setup() {
  add_tile("next", [this]() { return new NextTile(this); });
  add_card("upcoming", [this]() { return new UpcomingCard(this); });
  // A calendar tile of Home Assistant's own whose tap is set to it opens the card for that calendar.
  add_tap_action("upcoming", [this](const tessera::TapContext &c) {
    tessera::open_card(plugin_id(), "upcoming", c.entity, c.tile);
  });
  add_bar_item("soon", [this]() { return bar(); });
}

// "Tomorrow: Paper" in the top bar from the day before (or as many days ahead as set), when the switch is on.
tessera::BarItem WasteCollection::bar() const {
  tessera::BarItem item;
  if (!in_bar_ || !in_bar_->state || !message_ || !start_ || message_->state.empty()) return item;
  const int32_t now = today(), day = day_of(start_->state);
  if (now == INT32_MIN || day == INT32_MIN) return item;
  const int32_t days = day - now;
  const int32_t ahead = days_ahead_ && !std::isnan(days_ahead_->state) ? static_cast<int32_t>(days_ahead_->state) : 1;
  if (days < 0 || days > ahead) return item;
  item.shown = true;
  item.icon = 0xF044C;  // recycle
  const char *form = days == 0 ? "bar_today" : days == 1 ? "bar_tomorrow" : "bar_later";
  item.text = tessera::fill(tessera::fill(text(form), "what", message_->state).c_str(), "when", tessera::days_text(days));
  return item;
}

bool WasteCollection::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F044C";
  page.toggle(text("in_bar"), [this]() { return in_bar_ && in_bar_->state; },
              [this](bool on) { if (in_bar_) { if (on) in_bar_->turn_on(); else in_bar_->turn_off(); } });
  page.number(text("days_ahead"), 0, 3, 1, "",
              [this]() { return days_ahead_ && !std::isnan(days_ahead_->state) ? static_cast<int>(days_ahead_->state) : 1; },
              [this](int value) {
                if (!days_ahead_) return;
                auto call = days_ahead_->make_call();
                call.set_value(value);
                call.perform();
              });
  page.card(text("open_card"), "\U000F00ED", "upcoming");
  return true;
}

void WasteCollection::on_message(JsonObjectConst message) {
  if (open_card_) open_card_->answer(message);
}

}  // namespace esphome::waste_collection
