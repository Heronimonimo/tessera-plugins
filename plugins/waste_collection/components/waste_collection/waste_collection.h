#pragma once
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::waste_collection {

class UpcomingCard;

// The plugin: a tile ("next"), a card ("upcoming"), a tap action for calendar tiles ("upcoming"), an item for the top
// bar ("soon") and two rows on the screen's settings page.
class WasteCollection : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool settings(tessera::SettingsPage &page) override;
  void on_message(JsonObjectConst message) override;

  void set_message(text_sensor::TextSensor *s) { message_ = s; }
  void set_start(text_sensor::TextSensor *s) { start_ = s; }
  void set_in_bar(switch_::Switch *s) { in_bar_ = s; }
  void set_days_ahead(number::Number *n) { days_ahead_ = n; }
  // The calendar the top bar reads (plugin.yaml's CALENDAR), for a card opened from the settings page.
  std::string calendar() const { return calendar_; }
  void set_calendar(const char *entity) { calendar_ = entity; }

  UpcomingCard *open_card_ = nullptr;  // the card that is open, to hand it its answer

 protected:
  tessera::BarItem bar() const;
  text_sensor::TextSensor *message_{}, *start_{};
  switch_::Switch *in_bar_{};
  number::Number *days_ahead_{};
  std::string calendar_;
};

}  // namespace esphome::waste_collection
