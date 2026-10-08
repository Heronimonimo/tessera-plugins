#include "electricity_prices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <string>
#include <vector>

namespace esphome::electricity_prices {

using tessera::Font;
namespace ui = tessera::ui;

class PriceTile : public tessera::Tile {
 public:
  explicit PriceTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    title_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    value_ = ui::label(c.parent, Font::VALUE, theme::INK);
    unit_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    for (size_t i = 0; i < kMaxPrices; ++i) bars_.push_back(ui::block(c.parent, theme::ACCENT));
    paint();
  }

  void on_state(JsonObjectConst data) override {
    prices_.clear();
    unit_text_ = "";
    unavailable_ = false;

    JsonObjectConst attributes = data["attributes"].as<JsonObjectConst>();
    decimals_ = std::clamp(attributes["suggested_display_precision"] | 3, 0, 4);
    JsonArrayConst rows = attributes["raw_tomorrow"].as<JsonArrayConst>();
    bool nordpool = !rows.isNull() && rows.size() > 0;
    if (!nordpool) rows = attributes["prices_tomorrow"].as<JsonArrayConst>();

    const size_t count = std::min(rows.size(), kMaxPrices);
    for (size_t i = 0; i < count; ++i) {
      JsonObjectConst row = rows[i].as<JsonObjectConst>();
      const JsonVariantConst value = row[nordpool ? "value" : "price"];
      double price = 0;
      if (read_price(value, price)) prices_.push_back(price);
    }

    unit_text_ = attributes["unit_of_measurement"] | "";
    if (prices_.empty()) {
      const std::string state = data["state"] | "";
      unavailable_ = state == "unavailable" || state == "unknown";
    }
    paint();
  }

  void on_theme() override {
    ui::set_color(title_, theme::MUTED);
    ui::set_color(value_, theme::INK);
    ui::set_color(unit_, theme::MUTED);
    for (auto *bar : bars_) lv_obj_set_style_bg_color(bar, ui::color(theme::ACCENT), 0);
    paint();
  }

 private:
  static constexpr size_t kMaxPrices = 16;

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

  std::string format_price(double price) const {
    char text[24];
    snprintf(text, sizeof(text), "%.*f", decimals_, price);
    return text;
  }

  void paint() {
    ui::set_text(title_, plugin_text("tomorrow"));
    lv_obj_set_width(title_, width_);
    lv_obj_set_pos(title_, 0, 0);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    if (prices_.empty()) {
      ui::set_text(value_, plugin_text(unavailable_ ? "unavailable" : "no_prices"));
      const Font face = Font::BODY_LARGE;
      ui::set_font(value_, face);
      lv_obj_set_width(value_, width_);
      lv_obj_set_height(value_, ui::line_height(face));
      lv_obj_set_style_text_align(value_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(value_, 0, std::max(ui::line_height(Font::BODY), (height_ - ui::line_height(face)) / 2));
      ui::set_text(unit_, "");
      for (auto *bar : bars_) lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    const double average = std::accumulate(prices_.begin(), prices_.end(), 0.0,
                                           [count = prices_.size()](double total, double price) {
                                             return total + price / count;
                                           });
    const double low = *std::min_element(prices_.begin(), prices_.end());
    const double high = *std::max_element(prices_.begin(), prices_.end());

    ui::set_text(value_, format_price(average));
    Font face = Font::VALUE;
    for (Font candidate : {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
      face = candidate;
      if (ui::text_width(format_price(average), candidate) <= width_) break;
    }
    ui::set_font(value_, face);
    const int title_height = ui::line_height(Font::BODY);
    const int value_height = ui::line_height(face);
    const int unit_height = ui::line_height(Font::BODY);
    const int gap = ui::px(4);
    const bool show_chart = width_ >= ui::px(130) && height_ >= ui::px(100);
    const int chart_top = title_height + value_height + 2 * gap;
    const int chart_bottom = height_ - unit_height - gap;

    lv_obj_set_width(value_, width_);
    lv_obj_set_height(value_, value_height);
    lv_obj_set_style_text_align(value_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(value_, 0, title_height);
    std::string unit = unit_text_;
    ui::set_text(unit_, unit);
    lv_obj_set_width(unit_, width_);
    lv_obj_set_height(unit_, unit_height);
    lv_obj_set_style_text_align(unit_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(unit_, 0, height_ - unit_height);

    if (!show_chart || chart_bottom <= chart_top) {
      for (auto *bar : bars_) lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    const int gap_x = ui::px(2);
    const int bar_width = std::max(1, (width_ - gap_x * static_cast<int>(prices_.size() - 1)) /
                                           static_cast<int>(prices_.size()));
    const int chart_height = chart_bottom - chart_top;
    const double scale = std::max(std::abs(low), std::abs(high));
    const double scaled_low = scale > 0 ? low / scale : 0;
    const double scaled_range = scale > 0 ? high / scale - scaled_low : 0;
    for (size_t i = 0; i < bars_.size(); ++i) {
      if (i >= prices_.size()) {
        lv_obj_add_flag(bars_[i], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      const double relative = scaled_range > 0 ? (prices_[i] / scale - scaled_low) / scaled_range : 1.0;
      const int bar_height = std::max(1, static_cast<int>((0.25 + 0.75 * relative) * chart_height));
      lv_obj_clear_flag(bars_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(bars_[i], bar_width, bar_height);
      lv_obj_set_pos(bars_[i], static_cast<int>(i) * (bar_width + gap_x), chart_bottom - bar_height);
    }
  }

  std::string plugin_text(const char *key) const {
    return plugin_ ? plugin_->text(key) : "";
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, decimals_ = 3;
  bool unavailable_ = false;
  std::string unit_text_;
  std::vector<double> prices_;
  std::vector<lv_obj_t *> bars_;
  lv_obj_t *title_{}, *value_{}, *unit_{};
};

void ElectricityPrices::setup() {
  add_tile("forecast", [this]() { return new PriceTile(this); });
}

}  // namespace esphome::electricity_prices
