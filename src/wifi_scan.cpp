#include "wifi_scan.h"
#include "power_ctl.h"
#include "power_level.h"
#include <WiFi.h>
#include <esp_wifi.h>

// ponytail: synchronous scan blocks ~2 s (freezes the UI once per entry). Fine for
// a first cut; move to async WiFi.scanNetworks(true) + a refresh timer if it annoys.
static bool s_begun = false;
static int  s_count = 0;

static void ensure_begin() {
  if (!s_begun) {
    WiFi.mode(WIFI_STA); WiFi.disconnect();
    esp_wifi_set_max_tx_power(pwr_wifi_qdbm(power_level()));   // intensity dial
    s_begun = true;
  }
}

void wifi_begin() { ensure_begin(); }

int wifi_scan() {
  ensure_begin();
  s_count = WiFi.scanNetworks(false);
  return s_count;
}

void wifi_scan_async() {
  ensure_begin();
  WiFi.scanNetworks(true);
}

int wifi_scan_complete() {
  int n = WiFi.scanComplete();
  if (n >= 0) s_count = n;
  return n;
}

int wifi_count() { return s_count; }

const char *wifi_ssid(int i) { static String s; s = WiFi.SSID(i); return s.c_str(); }
int wifi_rssi(int i) { return WiFi.RSSI(i); }
int wifi_chan(int i) { return WiFi.channel(i); }
int wifi_enc(int i)  { return (int)WiFi.encryptionType(i); }
