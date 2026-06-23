#include "cst9217_touchscreen.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst9217 {

void CST9217Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST9217 Touchscreen...");

  // Reset + initialize the controller. Same sequence is reused by the runtime
  // self-heal in update_touches() if the chip stops responding.
  if (!this->init_device_()) {
    ESP_LOGE(TAG, "CST9217 Chip ID mismatch, expected 0x%04X, got 0x%04X", CST9217_CHIP_ID, this->chip_id_);
    this->status_set_error(LOG_STR("CST9217 Chip ID mismatch"));
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Chip Type: 0x%04X, ProjectID: 0x%04X", this->chip_id_, this->touch_project_id_);

  // Attach an interrupt pin if provided
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    ESP_LOGV(TAG, "Attached Interrupt Pin: %d", this->interrupt_pin_);
  }

  // At this point we're all good.
  ESP_LOGV(TAG, "CST9217 Touchscreen initialized successfully");
}

// Reset the controller and run the register init sequence. Returns true if the
// chip ID reads back correctly. Does NOT mark_failed() — callers decide (setup
// fails hard, the runtime watchdog just keeps retrying).
bool CST9217Touchscreen::init_device_() {
  // Register operations adapted from Waveshare's esp_lcd_touch_cst9217 driver.
  uint8_t data[4] = {0};

  // Hardware reset (pulses the reset pin if configured).
  this->reset_device_();

  // Enter command mode
  uint8_t cmd_mode[2] = { 0xD1, 0x01 };
  this->write_register16(ESP_LCD_TOUCH_CST9217_CMD_MODE_REG, cmd_mode, sizeof(cmd_mode));
  vTaskDelay(pdMS_TO_TICKS(10));

  // Read the checkcodes
  this->read_register16(ESP_LCD_TOUCH_CST9217_CHECKCODE_REG, data, sizeof(data));
  ESP_LOGV(TAG, "Checkcode: 0x%02X%02X%02X%02X", data[0], data[1], data[2], data[3]);

  // Get the touchscreen resolution
  this->read_register16(ESP_LCD_TOUCH_CST9217_RESOLUTION_REG, data, sizeof(data));
  this->touch_res_x_ = (data[1] << 8) | data[0];
  this->touch_res_y_ = (data[3] << 8) | data[2];
  this->x_raw_min_ = 0;
  this->y_raw_min_ = 0;
  this->x_raw_max_ = this->touch_res_x_;
  this->y_raw_max_ = this->touch_res_y_;
  ESP_LOGV(TAG, "Resolution X: %d, Y: %d", this->touch_res_x_, this->touch_res_y_);

  // Get the chip ID and project ID
  this->read_register16(ESP_LCD_TOUCH_CST9217_PROJECT_ID_REG, data, sizeof(data));
  this->chip_id_ = (data[3] << 8) | data[2];
  this->touch_project_id_ = (data[1] << 8) | data[0];

  return this->chip_id_ == CST9217_CHIP_ID;
}

void CST9217Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST9217 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Resolution X: %d, Y: %d", this->touch_res_x_, this->touch_res_y_);
  ESP_LOGCONFIG(TAG, "  Chip Type: 0x%04X, ProjectID: 0x%04X", this->chip_id_, this->touch_project_id_);
  ESP_LOGCONFIG(TAG, "  CST9217 Touchscreen config dump complete");
}

void CST9217Touchscreen::update_touches() {
  uint8_t data[CST9217_DATA_LENGTH] = {0};

  // A poll is "good" only if the I2C read succeeds AND the controller returns
  // its ACK byte. A failed read or a missing ACK both mean it's unresponsive.
  i2c::ErrorCode err = this->read_register16(ESP_LCD_TOUCH_CST9217_DATA_REG, data, sizeof(data));
  bool ok = (err == i2c::ErrorCode::NO_ERROR) && (data[6] == CST9217_ACK_VALUE);

  if (!ok) {
    // Log once on the first failure of a streak, then stay quiet to avoid floods.
    if (this->fail_count_ == 0) {
      ESP_LOGW(TAG, "CST9217 not responding (i2c err=%d, ack=0x%02X) — watching for hang", (int) err, data[6]);
    }
    this->fail_count_++;

    // Sustained unresponsiveness (~2s) → the touch controller has hung. Pulse
    // reset and re-init in place so the panel self-heals without a full reboot.
    if (this->fail_count_ >= CST9217_RECOVER_AFTER) {
      ESP_LOGW(TAG, "CST9217 hung — resetting touch controller");
      this->init_device_();
      this->fail_count_ = 0;
    }
    this->status_set_warning(LOG_STR("CST9217 unresponsive"));
    return;
  }

  if (this->fail_count_ != 0) {
    ESP_LOGI(TAG, "CST9217 recovered");
    this->fail_count_ = 0;
  }
  this->status_clear_warning();

  ESP_LOGV(TAG, "Touch data dump: %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

  uint8_t points = data[5] & 0x7F;
  points = (points > CST9217_MAX_TOUCH_POINTS) ? CST9217_MAX_TOUCH_POINTS : points;
  for (int i = 0; i < points; i++) {
    uint8_t *p = &data[i * 5 + (i ? 2 : 0)];
    uint8_t status = p[0] & 0x0F;

    if (status == 0x06) {
      int16_t x = ((p[1] << 4) | (p[3] >> 4));
      int16_t y = ((p[2] << 4) | (p[3] & 0x0F));
      this->add_raw_touch_position_(i, x, y);
      ESP_LOGV(TAG, "Point %d: X=%d, Y=%d", i, x, y);
    }
  }
}

void CST9217Touchscreen::reset_device_() {
  if (this->reset_pin_ != nullptr) {
    ESP_LOGD(TAG, "Resetting CST9217 Touchscreen...");
    this->reset_pin_->digital_write(false);
    vTaskDelay(pdMS_TO_TICKS(10));
    this->reset_pin_->digital_write(true);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGD(TAG, "CST9217 Touchscreen reset complete");
  } else {
    ESP_LOGD(TAG, "No reset pin configured, skipping reset");
  }
}

} // namespace cst9217
} // namespace esphome

