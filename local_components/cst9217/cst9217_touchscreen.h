// cst9217.h
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"

/* CST9217 registers */
#define ESP_LCD_TOUCH_CST9217_DATA_REG 0xD000
#define ESP_LCD_TOUCH_CST9217_PROJECT_ID_REG 0xD204
#define ESP_LCD_TOUCH_CST9217_CMD_MODE_REG   0xD101
#define ESP_LCD_TOUCH_CST9217_CHECKCODE_REG  0xD1FC
#define ESP_LCD_TOUCH_CST9217_RESOLUTION_REG 0xD1F8

/* CST9217 parameters */
#define CST9217_CHIP_ID 0x9217
#define CST9217_ACK_VALUE 0xAB
#define CST9217_MAX_TOUCH_POINTS 1
#define CST9217_DATA_LENGTH (CST9217_MAX_TOUCH_POINTS * 5 + 5)

namespace esphome {
namespace cst9217 {

static const char *const TAG = "cst9217.touchscreen";


class CST9217Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void update_touches() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void reset_device_();
  // Internal buffer to read touch data
  uint8_t touch_data_[CST9217_DATA_LENGTH]; // Sufficient for 2 touch points (8 bytes each) + header
  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint16_t chip_id_;
  uint16_t touch_res_x_;
  uint16_t touch_res_y_;
  uint16_t touch_project_id_;
};

} // namespace cst9217
} // namespace esphome

