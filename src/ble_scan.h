// BLE recon — native scan + tracker (AirTag/Tile/SmartTag) flagging.
// Tracker heuristics live in ble_track.h (host-tested).
#pragma once

void        ble_begin();          // BLEDevice::init (lazy; ble_scan calls it too)
int         ble_scan(int seconds); // blocking scan; returns device count (cached)
int         ble_count();
const char *ble_name(int i);      // "" if unnamed
const char *ble_addr(int i);
int         ble_rssi(int i);
bool        ble_is_tracker(int i);
