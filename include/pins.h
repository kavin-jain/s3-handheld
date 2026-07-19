// Central pin map — single source of truth for the ESP32-S3 handheld.
// Every value here is from the verified v2 wiring contract. Anything that
// touches a GPIO includes this; never hard-code a pin number elsewhere.
//
// Board: Edgehax S3-PRO (ESP32-S3-WROOM-1-N16R8).
// Reserved, never use: GPIO 26-32 (flash), 33-37 (octal PSRAM).
#pragma once

// ---- Display + onboard microSD share SPI-A (HSPI) ----
// TFT pins go to TFT_eSPI via platformio.ini build flags (CS10 DC14 RST21).
// The SD sits on the same bus with its own chip-select (GPIO9). Bus pins:
#define PIN_SPIA_SCLK     12
#define PIN_SPIA_MOSI     11
#define PIN_SPIA_MISO     13
#define PIN_SD_CS         9      // onboard microSD chip-select (leave to the board)

// ---- Backlight (power) ----
// Panel "LED/BL" ships tied to 3V3 (always on). To enable dimming/auto-off,
// rewire LED -> BL_PWM through a small MOSFET. This pin is a no-op until then.
#define PIN_BL_PWM        46      // free strapping pin; safe to drive after boot
#define BL_LEDC_CH        0
#define BL_LEDC_FREQ      5000    // Hz — above audible, no coil whine
#define BL_LEDC_BITS      8       // 0..255 duty

// ---- Rotary encoder (EC11) — native GPIO, interrupt-driven ----
#define PIN_ENC_A         38      // CLK
#define PIN_ENC_B         39      // DT
// Encoder click (SW) is on the MCP23017, not a GPIO — see MCP_ENC_SW below.

// ---- I2C bus (sensors + expander) ----
#define PIN_I2C_SDA       1
#define PIN_I2C_SCL       2
#define I2C_FREQ_HZ       400000

// ---- MCP23017 I/O expander (buttons, click, buzzer, motor) ----
#define MCP_ADDR          0x20    // A0/A1/A2 -> GND
#define PIN_MCP_INT       3       // INTA — one IRQ/wake line for all buttons+click
// Port-A pin indices as the Adafruit_MCP23X17 lib numbers them (GPA0..7 = 0..7):
#define MCP_BTN_BACK      0       // GPA0
#define MCP_BTN_HOME      1       // GPA1
#define MCP_BTN_ACTION    2       // GPA2  (silk "SELECT"; click already selects)
#define MCP_ENC_SW        3       // GPA3  — encoder push
#define MCP_BUZZER        5       // GPA5  — active buzzer via BC547
#define MCP_MOTOR         6       // GPA6  — vibration motor via BC547 + flyback

// PN532 is I2C (addr 0x24); its IRQ/RST pads are left OPEN on this board and every
// real GPIO is allocated, so the driver's required irq/reset args point at the unused
// onboard LED pin (harmless to toggle). We poll, not IRQ. ponytail: if I2C polling
// misbehaves on hardware, the PN532 may need its IRQ/RST actually wired — see BRINGUP.
#define PIN_PN532_IRQ     40
#define PIN_PN532_RST     40

// ---- IR ----
#define PIN_IR_TX         47      // Adafruit 5639 emitter Signal (onboard FET)
#define PIN_IR_RX         48      // TSOP38238 / VS1838B OUT

// ---- SPI-B (four radios share SCLK/MOSI/MISO; own CS/CE each) ----
#define PIN_SPIB_SCLK     4
#define PIN_SPIB_MOSI     5
#define PIN_SPIB_MISO     6
#define PIN_CC1101_1_CS   7
#define PIN_CC1101_1_GDO0 15
#define PIN_CC1101_2_CS   16
#define PIN_CC1101_2_GDO0 17
#define PIN_NRF24_1_CE    8
#define PIN_NRF24_1_CS    18
#define PIN_NRF24_2_CE    45      // strap — add 10k pulldown
#define PIN_NRF24_2_CS    0       // strap — keep BOOT pull-up

// ---- UART ----
#define PIN_GPS_TX        43      // ESP TX -> GPS RX
#define PIN_GPS_RX        44      // ESP RX <- GPS TX
#define PIN_PI_TX         41      // ESP TX -> Pi RXD
#define PIN_PI_RX         42      // ESP RX <- Pi TXD

// ---- Onboard status LEDs (active-driven; also UART activity) ----
#define PIN_LED_WHITE     40
