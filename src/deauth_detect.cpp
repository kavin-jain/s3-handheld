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
