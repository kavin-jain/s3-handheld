#include "radio_cc1101.h"
#include "subghz_classify.h"
#include "subghz_replay.h"
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

// ---- Fixed-code capture & replay (async OOK) ---------------------------------
// GDO0 carries the raw demodulated bitstream in async mode; an edge interrupt
// timestamps every transition into a ring of pulse durations. The decode itself
// is the host-tested rcs_decode() in subghz_replay.h. Untested without a CC1101
// + a fob to capture — see docs/BRINGUP.md (Sub-GHz capture/replay).
#define SGX_MAX 300
static volatile uint32_t s_dur[SGX_MAX];
static volatile int      s_cnt = 0;
static volatile uint32_t s_last = 0;

static void IRAM_ATTR sgx_edge() {
  uint32_t now = micros();
  int c = s_cnt;
  if (c < SGX_MAX) { s_dur[c] = now - s_last; s_cnt = c + 1; }
  s_last = now;
}

static void sgx_async(float mhz) {
  ELECHOUSE_cc1101.setCCMode(0);              // async raw serial mode (not packet)
  ELECHOUSE_cc1101.setModulation(2);          // ASK/OOK
  ELECHOUSE_cc1101.setMHZ(mhz);
}

bool subghz_capture(float mhz, uint32_t timeout_ms,
                    uint32_t *code, uint8_t *bits, int *proto_no) {
  if (!s_present || !code || !bits || !proto_no) return false;
  const int irq = digitalPinToInterrupt(PIN_CC1101_1_GDO0);
  sgx_async(mhz);
  pinMode(PIN_CC1101_1_GDO0, INPUT);
  ELECHOUSE_cc1101.SetRx();
  s_cnt = 0; s_last = micros();
  attachInterrupt(irq, sgx_edge, CHANGE);

  bool got = false;
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (s_cnt >= 50) {                         // a fob frame is well over 50 edges
      detachInterrupt(irq);
      int n = s_cnt;
      uint32_t buf[SGX_MAX];
      for (int i = 0; i < n; i++) buf[i] = s_dur[i];
      int sync = 0;                            // rotate so the longest gap leads
      for (int i = 1; i < n; i++) if (buf[i] > buf[sync]) sync = i;
      got = rcs_decode(buf + sync, n - sync, 20, code, bits, proto_no);
      if (got) break;
      s_cnt = 0; s_last = micros();
      attachInterrupt(irq, sgx_edge, CHANGE);  // no valid frame yet, keep listening
    }
    delay(2);
  }
  detachInterrupt(irq);
  ELECHOUSE_cc1101.setSidle();
  return got;
}

bool subghz_replay(float mhz, uint32_t code, uint8_t bits, int proto_no) {
  if (!s_present || proto_no < 1 || proto_no > RCS_PROTO_COUNT || bits == 0)
    return false;
  const RcsProto &p = RCS_PROTOS[proto_no - 1];
  sgx_async(mhz);
  pinMode(PIN_CC1101_1_GDO0, OUTPUT);
  ELECHOUSE_cc1101.SetTx();
  for (int rep = 0; rep < 6; rep++) {          // real fobs repeat the frame
    for (int i = (int)bits - 1; i >= 0; i--) {
      uint32_t hi, lo;
      rcs_symbol((code >> i) & 1u, &p, &hi, &lo);
      digitalWrite(PIN_CC1101_1_GDO0, HIGH); delayMicroseconds(hi);
      digitalWrite(PIN_CC1101_1_GDO0, LOW);  delayMicroseconds(lo);
    }
    delayMicroseconds((uint32_t)p.base_us * p.sync_lo);   // inter-frame sync gap
  }
  ELECHOUSE_cc1101.setSidle();
  return true;
}
