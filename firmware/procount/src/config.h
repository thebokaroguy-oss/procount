#pragma once
#include <Arduino.h>
// Example ESP32 DevKit pin map only. Audit this against your exact board/modules.
// RC522 and MAX7219 share SPI clock/data; their separate chip-selects keep traffic apart.
static constexpr uint8_t MOTOR_PINS[4] = {16, 17, 18, 19};
static constexpr uint8_t BUS_SCK = 21, BUS_MOSI = 23, BUS_MISO = 34;
static constexpr uint8_t MATRIX_DIN = BUS_MOSI, MATRIX_CLK = BUS_SCK, MATRIX_CS = 5;
// GPIO0 replaces GPIO12 for RFID reset; keep it HIGH during normal boot.
static constexpr uint8_t RFID_SS = 22, RFID_RST = 4;
static constexpr bool KEYPAD_ENABLED = false;
static constexpr uint8_t KEYPAD_ROWS[4] = {13, 14, 25, 26};
static constexpr uint8_t KEYPAD_COLS[4] = {27, 32, 33, 4};
// GPIO 2 and 15 are boot-strapping pins on classic ESP32. Confirm board behavior before connecting.
// Confirm the display uses TM1637 before using this driver.
static constexpr uint8_t SEG_CLK = 15, SEG_DIO = 2;
// Replace these example pins after checking boot-strapping behavior and your light module.
static constexpr int8_t LED_RED = -1, LED_YELLOW = -1, LED_GREEN = -1;
// Zero is intentional: set a measured calibration value before enabling dispensing.
static constexpr float STEPS_PER_GRAM = 0.0f;
static constexpr uint16_t MAX_TARGET_GRAMS = 200;
static constexpr uint16_t STEP_INTERVAL_MS = 4;
static constexpr char TOPIC_ROOT[] = "procount/device/";
