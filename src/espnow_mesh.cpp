#include "espnow_mesh.h"
#include "espnow_msg.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

static bool     s_on = false;
static uint32_t s_rx = 0;
static char     s_last[64] = {0};
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void handle(const uint8_t *data, int len) {
  char t[64];
  if (espnow_parse(data, (size_t)len, t, sizeof t)) {
    strncpy(s_last, t, sizeof s_last - 1);
    s_last[sizeof s_last - 1] = 0;
    s_rx++;
  }
}

// Recv-callback signature changed between Arduino-ESP32 2.x and 3.x.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void on_recv(const esp_now_recv_info_t *, const uint8_t *data, int len) { handle(data, len); }
#else
static void on_recv(const uint8_t *, const uint8_t *data, int len) { handle(data, len); }
#endif

bool espnow_begin() {
  if (s_on) return true;
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(on_recv);
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof peer);
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  s_on = true;
  return true;
}

bool        espnow_active() { return s_on; }
uint32_t    espnow_rx()     { return s_rx; }
const char *espnow_last()   { return s_last; }

void espnow_broadcast(const char *text) {
  if (!s_on) return;
  uint8_t f[256];
  size_t n = espnow_frame(text, f, sizeof f);
  if (n) esp_now_send(BCAST, f, n);
}
