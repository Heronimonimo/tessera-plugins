#include "electricity_prices.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace esphome::electricity_prices {

using tessera::Font;
namespace ui = tessera::ui;

struct PricePoint {
  double value;
  int64_t at;
  bool tomorrow;
};

struct Forecast {
  std::vector<PricePoint> points;
  double current = 0;
  bool has_current = false;
  bool has_today = false;
  bool has_tomorrow = false;
  bool unavailable = false;
  int decimals = 3;
  std::string unit;
};

static constexpr size_t kMaxPricesPerDay = 16;

static bool read_price(JsonVariantConst value, double &price) {
  if (value.isNull() || value.is<bool>()) return false;
  if (value.is<const char *>()) {
    const char *text = value.as<const char *>();
    if (!text) return false;
    char *end = nullptr;
    price = strtod(text, &end);
    if (end == text) return false;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') ++end;
    if (*end != '\0') return false;
  } else if (value.is<double>()) {
    price = value.as<double>();
  } else {
    return false;
  }
  return std::isfinite(price);
}

static int64_t days_since_epoch(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
  const unsigned day_of_year = (153 * adjusted_month + 2) / 5 + day - 1;
  const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return static_cast<int64_t>(era) * 146097 + day_of_era - 719468;
}

static bool read_time(JsonVariantConst value, int64_t &at) {
  if (value.is<int64_t>() || value.is<uint64_t>() || value.is<double>()) {
    const double epoch = value.as<double>();
    if (!std::isfinite(epoch)) return false;
    at = static_cast<int64_t>(epoch > 1e12 || epoch < -1e12 ? epoch / 1000 : epoch);
    return true;
  }
  const char *text = value.as<const char *>();
  if (!text || std::char_traits<char>::length(text) < 19) return false;

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (sscanf(text, "%4d-%2d-%2d%*c%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6 ||
      (text[10] != 'T' && text[10] != ' ') || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 60)
    return false;
  static constexpr int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (day < 1 || day > month_days[month - 1] + (month == 2 && leap ? 1 : 0)) return false;

  const char *suffix = text + 19;
  if (*suffix == '.') {
    ++suffix;
    while (*suffix >= '0' && *suffix <= '9') ++suffix;
  }
  int offset = 0;
  if (*suffix == '+' || *suffix == '-') {
    const int sign = *suffix == '+' ? 1 : -1;
    int offset_hour = 0, offset_minute = 0;
    if (sscanf(suffix + 1, "%2d:%2d", &offset_hour, &offset_minute) != 2 ||
        offset_hour > 23 || offset_minute > 59)
      return false;
    offset = sign * (offset_hour * 3600 + offset_minute * 60);
  } else if (*suffix != '\0' && *suffix != 'Z' && *suffix != 'z') {
    return false;
  }
  at = days_since_epoch(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
       hour * 3600 + minute * 60 + second - offset;
  return true;
}

static void append_prices(JsonArrayConst rows, bool nordpool, bool tomorrow, Forecast &forecast) {
  const size_t count = std::min(rows.size(), kMaxPricesPerDay);
  for (size_t i = 0; i < count; ++i) {
    JsonObjectConst row = rows[i].as<JsonObjectConst>();
    double price = 0;
    int64_t at = 0;
    const JsonVariantConst time = row[nordpool ? "start" : "time"];
    if (read_price(row[nordpool ? "value" : "price"], price) && read_time(time, at))
      forecast.points.push_back({price, at, tomorrow});
  }
}

static void append_values(JsonArrayConst rows, bool tomorrow, Forecast &forecast) {
  const size_t count = std::min(rows.size(), kMaxPricesPerDay);
  for (size_t i = 0; i < count; ++i) {
    double price = 0;
    if (read_price(rows[i], price)) forecast.points.push_back({price, static_cast<int64_t>(i), tomorrow});
  }
}

static void read_forecast(JsonObjectConst data, Forecast &forecast) {
  forecast = Forecast{};
  JsonObjectConst attributes = data["attributes"].as<JsonObjectConst>();
  forecast.decimals = std::clamp(attributes["suggested_display_precision"] | 3, 0, 4);
  forecast.unit = attributes["unit_of_measurement"] | "";
  forecast.has_current = read_price(data["state"], forecast.current);
  const std::string state = data["state"] | "";
  forecast.unavailable = !forecast.has_current && (state == "unavailable" || state == "unknown");

  JsonArrayConst raw_today = attributes["raw_today"].as<JsonArrayConst>();
  JsonArrayConst raw_tomorrow = attributes["raw_tomorrow"].as<JsonArrayConst>();
  JsonArrayConst prices_today = attributes["prices_today"].as<JsonArrayConst>();
  JsonArrayConst prices_tomorrow = attributes["prices_tomorrow"].as<JsonArrayConst>();
  JsonArrayConst today = attributes["today"].as<JsonArrayConst>();
  JsonArrayConst tomorrow = attributes["tomorrow"].as<JsonArrayConst>();

  const bool has_raw_today = !raw_today.isNull() && raw_today.size() > 0;
  const bool has_raw_tomorrow = !raw_tomorrow.isNull() && raw_tomorrow.size() > 0;
  const bool has_prices_today = !prices_today.isNull() && prices_today.size() > 0;
  const bool has_prices_tomorrow = !prices_tomorrow.isNull() && prices_tomorrow.size() > 0;
  const bool has_today = !today.isNull() && today.size() > 0;
  const bool has_tomorrow = !tomorrow.isNull() && tomorrow.size() > 0;
  forecast.has_today = has_raw_today || has_prices_today || has_today;
  forecast.has_tomorrow = has_raw_tomorrow || has_prices_tomorrow || has_tomorrow;

  if (has_raw_today) append_prices(raw_today, true, false, forecast);
  else if (has_prices_today) append_prices(prices_today, false, false, forecast);
  else if (has_today) append_values(today, false, forecast);
  if (has_raw_tomorrow) append_prices(raw_tomorrow, true, true, forecast);
  else if (has_prices_tomorrow) append_prices(prices_tomorrow, false, true, forecast);
  else if (has_tomorrow) append_values(tomorrow, true, forecast);
  std::stable_sort(forecast.points.begin(), forecast.points.end(), [](const PricePoint &a, const PricePoint &b) {
    if (a.tomorrow != b.tomorrow) return !a.tomorrow;
    return a.at < b.at;
  });
}

static std::string format_price(double price, int decimals) {
  char text[24];
  snprintf(text, sizeof(text), "%.*f", decimals, price);
  return text;
}

class ChartView {
 public:
  explicit ChartView(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(lv_obj_t *parent, int width, int height) {
    width_ = width;
    height_ = height;
    title_ = ui::label(parent, Font::TITLE, theme::INK);
    today_ = ui::label(parent, Font::BODY, theme::ACCENT);
    tomorrow_ = ui::label(parent, Font::BODY, theme::MARK_AMBER);
    unit_ = ui::label(parent, Font::BODY, theme::MUTED);
    for (size_t i = 0; i < kMaxPricesPerDay * 2; ++i)
      bars_.push_back(ui::block(parent, theme::ACCENT));
  }

  void draw(const Forecast &forecast) {
    lv_obj_set_size(title_, width_, ui::line_height(Font::TITLE));
    lv_obj_set_pos(title_, 0, 0);
    ui::set_text(title_, plugin_->text("forecast_title"));

    const int legend_height = ui::line_height(Font::BODY);
    const int gap = ui::px(4);
    const int chart_top = ui::line_height(Font::TITLE) + gap;
    const int chart_bottom = height_ - legend_height - gap;
    const int chart_height = std::max(0, chart_bottom - chart_top);

    ui::set_text(unit_, forecast.unit);
    const int unit_width = forecast.unit.empty() ? 0 : ui::text_width(forecast.unit, Font::BODY) + ui::px(2);
    lv_obj_set_size(unit_, unit_width, legend_height);
    lv_obj_set_pos(unit_, width_ - unit_width, height_ - legend_height);

    const int label_width = forecast.unit.empty() ? width_ : std::max(1, width_ - unit_width - gap);
    const bool both_days = forecast.has_today && forecast.has_tomorrow;
    const int today_width = both_days ? label_width / 2 : label_width;
    lv_obj_set_size(today_, forecast.has_today ? today_width : 1, legend_height);
    lv_obj_set_pos(today_, 0, height_ - legend_height);
    ui::set_text(today_, forecast.has_today ? plugin_->text("today") : "");
    const int tomorrow_width = both_days ? label_width - today_width : label_width;
    lv_obj_set_size(tomorrow_, forecast.has_tomorrow ? tomorrow_width : 1, legend_height);
    lv_obj_set_pos(tomorrow_, both_days ? today_width : 0, height_ - legend_height);
    ui::set_text(tomorrow_, forecast.has_tomorrow ? plugin_->text("tomorrow") : "");

    if (forecast.points.empty() || chart_height <= 0) {
      ui::set_text(title_, plugin_->text(forecast.unavailable ? "unavailable" : "no_prices"));
      for (auto *bar : bars_) lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    double low = forecast.points.front().value;
    double high = low;
    for (const auto &point : forecast.points) {
      low = std::min(low, point.value);
      high = std::max(high, point.value);
    }
    const double scale = std::max(std::abs(low), std::abs(high));
    const double scaled_low = scale > 0 ? low / scale : 0;
    const double scaled_range = scale > 0 ? high / scale - scaled_low : 0;
    const int gap_x = ui::px(2);
    const int bar_width = std::max(1, (width_ - gap_x * static_cast<int>(forecast.points.size() - 1)) /
                                           static_cast<int>(forecast.points.size()));

    for (size_t i = 0; i < bars_.size(); ++i) {
      if (i >= forecast.points.size()) {
        lv_obj_add_flag(bars_[i], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      const auto &point = forecast.points[i];
      const double relative = scaled_range > 0 ? (point.value / scale - scaled_low) / scaled_range : 1.0;
      const int bar_height = std::max(1, static_cast<int>((0.25 + 0.75 * relative) * chart_height));
      lv_obj_clear_flag(bars_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(bars_[i], ui::color(point.tomorrow ? theme::MARK_AMBER : theme::ACCENT), 0);
      lv_obj_set_size(bars_[i], bar_width, bar_height);
      lv_obj_set_pos(bars_[i], static_cast<int>(i) * (bar_width + gap_x), chart_bottom - bar_height);
    }
  }

  void on_theme() {
    ui::set_color(title_, theme::INK);
    ui::set_color(today_, theme::ACCENT);
    ui::set_color(tomorrow_, theme::MARK_AMBER);
    ui::set_color(unit_, theme::MUTED);
  }

 private:
  const tessera::Plugin *plugin_;
  lv_obj_t *title_{}, *today_{}, *tomorrow_{}, *unit_{};
  int width_ = 0, height_ = 0;
  std::vector<lv_obj_t *> bars_;
};

class PriceTile : public tessera::Tile {
 public:
  explicit PriceTile(ElectricityPrices *plugin) : plugin_(plugin), chart_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    entity_ = c.entity;
    tile_ = c.tile;
    chart_mode_ = c.columns > 1 || c.rows > 1;
    if (chart_mode_) {
      chart_.create(c.parent, width_, height_);
    } else {
      title_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      value_ = ui::label(c.parent, Font::VALUE, theme::INK);
      unit_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    }
  }

  void on_state(JsonObjectConst data) override {
    read_forecast(data, forecast_);
    if (chart_mode_) {
      chart_.draw(forecast_);
    } else {
      paint_current();
    }
  }

  void on_theme() override {
    if (chart_mode_) {
      chart_.on_theme();
      chart_.draw(forecast_);
    } else {
      ui::set_color(title_, theme::MUTED);
      ui::set_color(value_, theme::INK);
      ui::set_color(unit_, theme::MUTED);
    }
  }

  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "chart", entity_, tile_); }

 private:
  void paint_current() {
    ui::set_text(title_, plugin_->text("current_price"));
    lv_obj_set_width(title_, width_);
    lv_obj_set_pos(title_, 0, 0);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);

    const int title_height = ui::line_height(Font::BODY);
    if (!forecast_.has_current) {
      ui::set_text(value_, plugin_->text(forecast_.unavailable ? "unavailable" : "no_price"));
      ui::set_font(value_, Font::BODY_LARGE);
      const int value_height = ui::line_height(Font::BODY_LARGE);
      const int unit_height = ui::line_height(Font::BODY);
      const int unit_y = height_ - unit_height;
      lv_obj_set_width(value_, width_);
      lv_obj_set_height(value_, value_height);
      lv_obj_set_style_text_align(value_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(value_, 0, title_height + std::max(0, (unit_y - title_height - value_height) / 2));
      ui::set_text(unit_, forecast_.unit);
      lv_obj_set_width(unit_, width_);
      lv_obj_set_height(unit_, unit_height);
      lv_obj_set_style_text_align(unit_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(unit_, 0, unit_y);
      return;
    }

    const std::string value = format_price(forecast_.current, forecast_.decimals);
    ui::set_text(value_, value);
    Font face = Font::VALUE;
    for (Font candidate : {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE}) {
      face = candidate;
      if (ui::text_width(value, candidate) <= width_) break;
    }
    ui::set_font(value_, face);
    const int value_height = ui::line_height(face);
    const int unit_height = ui::line_height(Font::BODY);
    const int unit_y = height_ - unit_height;
    lv_obj_set_width(value_, width_);
    lv_obj_set_height(value_, value_height);
    lv_obj_set_style_text_align(value_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(value_, 0, title_height + std::max(0, (unit_y - title_height - value_height) / 2));
    ui::set_text(unit_, forecast_.unit);
    lv_obj_set_width(unit_, width_);
    lv_obj_set_height(unit_, unit_height);
    lv_obj_set_style_text_align(unit_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(unit_, 0, unit_y);
  }

  ElectricityPrices *plugin_;
  ChartView chart_;
  Forecast forecast_;
  std::string entity_;
  int width_ = 0, height_ = 0, tile_ = -1;
  bool chart_mode_ = false;
  lv_obj_t *title_{}, *value_{}, *unit_{};
};

class ForecastCard : public tessera::Card {
 public:
  explicit ForecastCard(ElectricityPrices *plugin) : plugin_(plugin), chart_(plugin) {}

  void open(const tessera::CardContext &c) override {
    chart_.create(c.parent, c.width, c.height);
    chart_.draw(forecast_);
  }

  void on_state(JsonObjectConst data) override {
    read_forecast(data, forecast_);
    chart_.draw(forecast_);
  }

  void on_theme() override {
    chart_.on_theme();
    chart_.draw(forecast_);
  }

 private:
  ElectricityPrices *plugin_;
  ChartView chart_;
  Forecast forecast_;
};

void ElectricityPrices::setup() {
  add_tile("forecast", [this]() { return new PriceTile(this); });
  add_card("chart", [this]() { return new ForecastCard(this); });
}

}  // namespace esphome::electricity_prices
