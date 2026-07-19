#include "nrf24_radio.h"
#include "nrf_band.h"
#include "power_ctl.h"
#include "power_level.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// ponytail: NRF24 shares SPI-B with the CC1101. The SmartRC lib makes its own
// HSPI instance and RF24 uses this one — on real hardware only one radio may
// drive the bus at a time (each has its own CS). If they fight, unify onto a
// single shared SPIClass + per-device CS. Lazy init keeps them off the bus at
// boot. Untested without hardware — see docs/BRINGUP.md (NRF24).
static SPIClass nrfSPI(HSPI);
static RF24 radio(PIN_NRF24_1_CE, PIN_NRF24_1_CS);
static bool s_begun = false, s_present = false, s_scanned = false;
static uint8_t s_counts[NRF_CHAN];

static void ensure() {
  if (s_begun) return;
  s_begun = true;
  nrfSPI.begin(PIN_SPIB_SCLK, PIN_SPIB_MISO, PIN_SPIB_MOSI, PIN_NRF24_1_CS);
  s_present = radio.begin(&nrfSPI) && radio.isChipConnected();
  if (s_present) {
    radio.setPALevel((rf24_pa_dbm_e)pwr_nrf24_pa(power_level()));  // intensity dial
    radio.setDataRate(RF24_2MBPS);           // full throughput; drop to 250KBPS for range
    radio.setAutoAck(false);                 // don't ACK — raw scan/inject
    radio.setCRCLength(RF24_CRC_DISABLED);   // promiscuous: accept everything
  }
}

bool nrf_present() { ensure(); return s_present; }

void nrf_scan() {
  ensure();
  if (!s_present) return;
  for (int ch = 0; ch < NRF_CHAN; ch++) {
    s_counts[ch] = 0;
    radio.setChannel(ch);
    for (int r = 0; r < 20; r++) {           // 20 samples/channel
      radio.startListening();
      delayMicroseconds(130);                // let the receiver settle
      radio.stopListening();
      if (radio.testCarrier()) s_counts[ch]++;
    }
  }
  s_scanned = true;
}

bool    nrf_scanned() { return s_scanned; }
int     nrf_busiest_ch() { return nrf_busiest(s_counts, NRF_CHAN); }
uint8_t nrf_activity(int ch) { return (ch >= 0 && ch < NRF_CHAN) ? s_counts[ch] : 0; }
