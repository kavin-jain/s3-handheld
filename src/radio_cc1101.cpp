#include "radio_cc1101.h"
#include "subghz_classify.h"
#include "subghz_replay.h"
#include "power_ctl.h"
#include "power_level.h"
#include "pins.h"
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "bus_locks.h"

// ponytail: single-radio bring-up. SPI-B is shared by 4 radios; when the 2nd
// CC1101 / NRF24s join, add per-device CS arbitration before each transaction.
// Untested without hardware — see docs/BRINGUP.md (Sub-GHz).
static bool s_present = false;

bool cc1101_begin() {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ELECHOUSE_cc1101.setSpiPin(PIN_SPIB_SCLK, PIN_SPIB_MISO, PIN_SPIB_MOSI, PIN_CC1101_1_CS);
  ELECHOUSE_cc1101.setGDO0(PIN_CC1101_1_GDO0);
  ELECHOUSE_cc1101.Init();
  s_present = ELECHOUSE_cc1101.getCC1101();   // reads VERSION/PARTNUM — false if absent
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return s_present;
}

bool cc1101_present() { return s_present; }

uint8_t cc1101_version() {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  uint8_t v = ELECHOUSE_cc1101.SpiReadStatus(CC1101_VERSION);
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return v;
}

int cc1101_rssi_at(float mhz) {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ELECHOUSE_cc1101.setMHZ(mhz);
  ELECHOUSE_cc1101.SetRx();
  delay(6);                                   // let the AGC settle before reading
  int rssi = ELECHOUSE_cc1101.getRssi();
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return rssi;
}

int cc1101_sweep(const float *freqs, int n, int *rssi_out) {
  if (!s_present || !freqs || !rssi_out || n <= 0) return -1;
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  for (int i = 0; i < n; i++) rssi_out[i] = cc1101_rssi_at(freqs[i]);
  ELECHOUSE_cc1101.setSidle();                // park the radio after the sweep
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return sg_peak(rssi_out, n);
}

// ---- Fixed-code capture & replay (async OOK) ---------------------------------
// GDO0 carries the raw demodulated bitstream in async mode; an edge interrupt
// timestamps every transition into a ring of pulse durations. The decode itself
// is the host-tested rcs_decode() in subghz_replay.h. Untested without a CC1101
// + a fob to capture — see docs/BRINGUP.md (Sub-GHz capture/replay).
#define SGX_MAX 300
static portMUX_TYPE sgx_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_dur[SGX_MAX];
static volatile int      s_cnt = 0;
static volatile uint32_t s_last = 0;

static void IRAM_ATTR sgx_edge() {
  uint32_t now = micros();
  portENTER_CRITICAL_ISR(&sgx_mux);
  int c = s_cnt;
  if (c < SGX_MAX) { s_dur[c] = now - s_last; s_cnt = c + 1; }
  s_last = now;
  portEXIT_CRITICAL_ISR(&sgx_mux);
}

static void sgx_async(float mhz) {
  ELECHOUSE_cc1101.setCCMode(0);              // async raw serial mode (not packet)
  ELECHOUSE_cc1101.setModulation(2);          // ASK/OOK
  ELECHOUSE_cc1101.setMHZ(mhz);
  ELECHOUSE_cc1101.setPA(pwr_cc1101_dbm(power_level()));   // intensity dial (max +12 dBm)
  // setPA is frequency-dependent, so it must follow setMHZ. NOTE: +12 dBm can
  // exceed local ISM ERP limits (433/868/915) — legal to run only where allowed.
}

void subghz_capture_begin(float mhz) {
  if (!s_present) return;
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  sgx_async(mhz);
  pinMode(PIN_CC1101_1_GDO0, INPUT);
  ELECHOUSE_cc1101.SetRx();
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  s_cnt = 0; s_last = micros();
  attachInterrupt(digitalPinToInterrupt(PIN_CC1101_1_GDO0), sgx_edge, CHANGE);
}

bool subghz_capture_poll(uint32_t *code, uint8_t *bits, int *proto_no) {
  if (!s_present || !code || !bits || !proto_no) return false;
  int n = 0;
  uint32_t buf[SGX_MAX];
  portENTER_CRITICAL(&sgx_mux);
  if (s_cnt >= 50) {
    n = s_cnt;
    for (int i = 0; i < n; i++) buf[i] = s_dur[i];
    s_cnt = 0; s_last = micros();
  }
  portEXIT_CRITICAL(&sgx_mux);

  if (n >= 50) {
    int sync = 0;
    for (int i = 1; i < n; i++) if (buf[i] > buf[sync]) sync = i;
    return rcs_decode(buf + sync, n - sync, 20, code, bits, proto_no);
  }
  return false;
}

void subghz_capture_end() {
  if (!s_present) return;
  detachInterrupt(digitalPinToInterrupt(PIN_CC1101_1_GDO0));
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ELECHOUSE_cc1101.setSidle();
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
}

bool subghz_capture(float mhz, uint32_t timeout_ms,
                    uint32_t *code, uint8_t *bits, int *proto_no) {
  if (!s_present || !code || !bits || !proto_no) return false;
  subghz_capture_begin(mhz);
  bool got = false;
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (subghz_capture_poll(code, bits, proto_no)) {
      got = true;
      break;
    }
    delay(10);
  }
  subghz_capture_end();
  return got;
}

bool subghz_replay(float mhz, uint32_t code, uint8_t bits, int proto_no) {
  if (!s_present || proto_no < 1 || proto_no > RCS_PROTO_COUNT || bits == 0)
    return false;
  const RcsProto &p = RCS_PROTOS[proto_no - 1];
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  sgx_async(mhz);
  pinMode(PIN_CC1101_1_GDO0, OUTPUT);
  ELECHOUSE_cc1101.SetTx();
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  for (int rep = 0; rep < 6; rep++) {          // real fobs repeat the frame
    for (int i = (int)bits - 1; i >= 0; i--) {
      uint32_t hi, lo;
      rcs_symbol((code >> i) & 1u, &p, &hi, &lo);
      digitalWrite(PIN_CC1101_1_GDO0, HIGH); delayMicroseconds(hi);
      digitalWrite(PIN_CC1101_1_GDO0, LOW);  delayMicroseconds(lo);
    }
    delayMicroseconds((uint32_t)p.base_us * p.sync_lo);   // inter-frame sync gap
  }
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ELECHOUSE_cc1101.setSidle();
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return true;
}
