#include "ble_scan.h"
#include "ble_track.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Results are copied into fixed arrays right after each scan so we never hold
// bluedroid objects (and can clearResults to free them between scans).
#define BLE_MAX 20
static bool s_begun = false;
static int  s_count = 0;
static char s_name[BLE_MAX][24];
static char s_addr[BLE_MAX][18];
static int  s_rssi[BLE_MAX];
static bool s_track[BLE_MAX];

static void ensure() {
  if (!s_begun) { BLEDevice::init(""); s_begun = true; }
}

void ble_begin() { ensure(); }

int ble_scan(int seconds) {
  ensure();
  BLEScan *sc = BLEDevice::getScan();
  sc->setActiveScan(true);
  BLEScanResults res = sc->start(seconds, false);
  int n = res.getCount();
  if (n > BLE_MAX) n = BLE_MAX;
  for (int i = 0; i < n; i++) {
    BLEAdvertisedDevice d = res.getDevice(i);
    snprintf(s_name[i], sizeof s_name[i], "%s", d.getName().c_str());
    snprintf(s_addr[i], sizeof s_addr[i], "%s", d.getAddress().toString().c_str());
    s_rssi[i] = d.getRSSI();
    bool tr = false;
    if (d.haveManufacturerData()) {
      auto m = d.getManufacturerData();      // String or std::string per core version
      tr = ble_is_airtag((const uint8_t *)m.c_str(), m.length());
    }
    s_track[i] = tr;
  }
  s_count = n;
  sc->clearResults();
  return s_count;
}

int         ble_count() { return s_count; }
const char *ble_name(int i) { return (i >= 0 && i < s_count) ? s_name[i] : ""; }
const char *ble_addr(int i) { return (i >= 0 && i < s_count) ? s_addr[i] : ""; }
int         ble_rssi(int i) { return (i >= 0 && i < s_count) ? s_rssi[i] : 0; }
bool        ble_is_tracker(int i) { return (i >= 0 && i < s_count) ? s_track[i] : false; }
