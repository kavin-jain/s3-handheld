// Central pin map — single source of truth for the ESP32-S3 handheld.
// Every value here is from the verified v2 wiring contract. Anything that
// touches a GPIO includes this; never hard-code a pin number elsewhere.
//
// Board: Edgehax S3-PRO (ESP32-S3-WROOM-1-N16R8).
// Reserved, never use: GPIO 26-32 (flash), 33-37 (octal PSRAM).
#pragma once

// ---- Display + onboard microSD share SPI-A (HSPI) ----
// TFT pins go to TFT_eSPI via platformio.ini build flags (CS41 DC14 RST21).
// The onboard microSD's CS is fixed by the carrier board's own PCB traces --
// verified against the official Edgehax pinout (github.com/edgehax/esp32-s3-
// wroom1-n16r8): the SD slot is wired to FSPIHD(9)/FSPICS0(10)/FSPID(11)/
// FSPICLK(12)/FSPIQ(13). GPIO10 is FSPICS0 == the real CS, NOT GPIO9 (that's
// HD/Hold) -- the old PIN_SD_CS=9 was toggling the wrong pin, so the card
// could never be selected. TFT_CS was ALSO on GPIO10 (platformio.ini),
// colliding with the SD's hardwired CS -- moved TFT_CS to GPIO41 (was an
// unused PIN_PI_TX; every other GPIO on this chip is already claimed).
#define PIN_SPIA_SCLK     12
#define PIN_SPIA_MOSI     11
#define PIN_SPIA_MISO     13
#define PIN_SD_CS         10     // onboard microSD chip-select (FSPICS0, per official pinout)

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
// 100kHz, not 400kHz: PN532 clock-stretches the bus during its I2C wakeup
// (see Adafruit_PN532::wakeup() comment), and that's flaky at Fast Mode on a
// marginal/soldered connection -- bus-scan ACKs (single byte) can pass at
// 400kHz while the multi-byte getFirmwareVersion() handshake fails, which
// matches what's happening on this board (0x28 shows in the scan, but
// nfc_begin() still fails). 100kHz costs nothing measurable for MCP23017 or
// MPU6050 either.
#define I2C_FREQ_HZ       100000

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
// PIN_PI_TX (GPIO41) removed -- reclaimed as PIN_TFT_CS above (nothing used it,
// the Pi-companion feature was never implemented). GPIO42 is still free if a Pi
// link is ever built, but per the official pinout it's also GRN_LED -- expect
// it to flicker if driven as a UART line.
#define PIN_PI_RX         42      // ESP RX <- Pi TXD (also GRN_LED per official pinout)

// ---- Onboard status LEDs (per official Edgehax pinout: ORNG=40 WHT=41 GRN=42) ----
#define PIN_LED_WHITE     40
