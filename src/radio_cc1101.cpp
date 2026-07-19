#include "radio_cc1101.h"
#include "subghz_classify.h"
#include "pins.h"
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// ponytail: single-radio bring-up. SPI-B is shared by 4 radios; when the 2nd
// CC1101 / NRF24s join, add per-device CS arbitration before each transaction.
// Untested without hardware — see docs/BRINGUP.md (Sub-GHz).
static bool s_present = false;

bool cc1101_begin() {
  ELECHOUSE_cc1101.setSpiPin(PIN_SPIB_SCLK, PIN_SPIB_MISO, PIN_SPIB_MOSI, PIN_CC1101_1_CS);
  ELECHOUSE_cc1101.setGDO0(PIN_CC1101_1_GDO0);
  ELECHOUSE_cc1101.Init();
  s_present = ELECHOUSE_cc1101.getCC1101();   // reads VERSION/PARTNUM — false if absent
  return s_present;
}

bool cc1101_present() { return s_present; }

uint8_t cc1101_version() { return ELECHOUSE_cc1101.SpiReadStatus(CC1101_VERSION); }

int cc1101_rssi_at(float mhz) {
  ELECHOUSE_cc1101.setMHZ(mhz);
  ELECHOUSE_cc1101.SetRx();
  delay(6);                                   // let the AGC settle before reading
  return ELECHOUSE_cc1101.getRssi();
}

int cc1101_sweep(const float *freqs, int n, int *rssi_out) {
  if (!s_present || !freqs || !rssi_out || n <= 0) return -1;
  for (int i = 0; i < n; i++) rssi_out[i] = cc1101_rssi_at(freqs[i]);
  ELECHOUSE_cc1101.setSidle();                // park the radio after the sweep
  return sg_peak(rssi_out, n);
}
