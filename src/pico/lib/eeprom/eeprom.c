#include "eeprom/eeprom.h"
#include "controller/guitar_includes.h"
#include "hardware/flash.h"
#include "pico/stdlib.h"
#include "util/util.h"
#include <string.h>
const Configuration_t default_config = DEFAULT_CONFIG;
const uint8_t *flash_target_contents =
    (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
// Round to nearst 256 (FLASH_PAGE_SIZE)
uint8_t newConfig[((sizeof(Configuration_t) >> 8) + 1) << 8];
void loadConfig(Configuration_t* config) {
  memcpy(config, flash_target_contents, sizeof(Configuration_t));
  if (config->main.signature != ARDWIINO_DEVICE_TYPE) {
    memcpy(config, &default_config, sizeof(Configuration_t));
    writeConfigBlock(0, (uint8_t *)config, sizeof(Configuration_t));
    return;
  }
  // We made a change to simplify the guitar config, but as a result whammy is
  // now flipped
  if (config->main.version < 9 && isGuitar(config->main.subType)) {
    config->pins.r_x.inverted = !config->pins.r_x.inverted;
  }
  if (config->main.version < 12) {
    memcpy(&config->axisScale, &default_config.axisScale,
           sizeof(default_config.axisScale));
  }
  if (config->main.version < 13) {
    memcpy(&config->debounce, &default_config.debounce,
           sizeof(default_config.debounce));
  }
  if (config->main.version < 14) {
    switch (config->axis.mpu6050Orientation) {
    case NEGATIVE_X:
    case POSITIVE_X:
      config->axis.mpu6050Orientation = X;
      break;
    case NEGATIVE_Y:
    case POSITIVE_Y:
      config->axis.mpu6050Orientation = Y;
      break;
    case NEGATIVE_Z:
    case POSITIVE_Z:
      config->axis.mpu6050Orientation = Z;
      break;
    }
  }
  if (config->main.version < 15) {
    config->debounce.combinedStrum = false;
  }
  if (config->main.version < 16) {
    config->neck.gh5Neck = false;
    config->neck.gh5NeckBar = false;
    config->neck.wtNeck = false;
    config->neck.wiiNeck = false;
    config->neck.ps2Neck = false;
  }

  if (config->main.version < 17 && config->main.subType > XINPUT_ARCADE_PAD) {
    config->main.subType += XINPUT_TURNTABLE - XINPUT_ARCADE_PAD;
    if (config->main.subType > PS3_GAMEPAD) {
      config->main.subType += 2; 
    }
    if (config->main.subType > WII_ROCK_BAND_DRUMS) {
      config->main.subType += 1; 
    }
  }
  if (config->main.version < 18) {
    config->deque = false;
    config->debounce.buttons *= 10;
    config->debounce.strum *= 10;
  }
  if (config->main.version < CONFIG_VERSION) {
    config->main.version = CONFIG_VERSION;
    writeConfigBlock(0, (uint8_t *)config, sizeof(Configuration_t));
  }
}
void writeConfigBlock(uint16_t offset, const uint8_t *data, uint16_t len) {
  memcpy(newConfig + offset, data, len);
  uint32_t saved_irq;
  if (offset + len >= sizeof(Configuration_t)) {
    saved_irq = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, newConfig, sizeof(newConfig));
    restore_interrupts(saved_irq);
  }
}
void readConfigBlock(uint16_t offset, uint8_t *data, uint16_t len) {
  memcpy(data, flash_target_contents + offset, len);
}

void resetConfig(void) {
  writeConfigBlock(0, (uint8_t *)&default_config, sizeof(Configuration_t));
}