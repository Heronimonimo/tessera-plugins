#include "sc202cs.h"

#include "esphome/core/log.h"

namespace esphome::sc202cs {

static const char *const TAG = "sc202cs";

// The registers of esp_cam_sensor's sc202cs_regs.h and its start tables.
static constexpr uint16_t REG_ID_HIGH = 0x3107;
static constexpr uint16_t REG_ID_LOW = 0x3108;
static constexpr uint16_t SC202CS_ID = 0xEB52;
static constexpr uint16_t REG_BLC_TARGET_HIGH = 0x3907;  // bits 4:0
static constexpr uint16_t REG_BLC_TARGET_LOW = 0x3908;

// In 10-bit counts: the sensor starts at 0x41, which is 16 of 255 in its 8-bit picture. 4 is 1 of 255, black.
static constexpr uint16_t BLC_TARGET = 0x0004;

void SC202CS::setup() {
  uint8_t high = 0, low = 0;
  if (this->read_register16(REG_ID_HIGH, &high, 1) != i2c::ERROR_OK ||
      this->read_register16(REG_ID_LOW, &low, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "No sensor answers at 0x%02X", this->address_);
    return;
  }
  if ((uint16_t(high) << 8 | low) != SC202CS_ID) {
    ESP_LOGD(TAG, "Sensor id 0x%02X%02X is not the SC202CS: black level left alone", high, low);
    return;
  }
  this->found_ = true;
  const uint8_t target_high = (BLC_TARGET >> 8) & 0x1F;
  const uint8_t target_low = BLC_TARGET & 0xFF;
  this->set_ = this->write_register16(REG_BLC_TARGET_HIGH, &target_high, 1) == i2c::ERROR_OK &&
               this->write_register16(REG_BLC_TARGET_LOW, &target_low, 1) == i2c::ERROR_OK;
  if (!this->set_)
    ESP_LOGW(TAG, "Could not set the black level target; the picture keeps a grey, pink cast in its shadows");
}

void SC202CS::dump_config() {
  ESP_LOGCONFIG(TAG, "SC202CS black level:");
  if (!this->found_) {
    ESP_LOGCONFIG(TAG, "  No SC202CS on this screen");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Target: %u (10-bit), %s", BLC_TARGET, this->set_ ? "set" : "NOT set");
}

}  // namespace esphome::sc202cs
