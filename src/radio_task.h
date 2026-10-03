// Dedicated FreeRTOS task, pinned to core 0, that owns every blocking SPI-B
// radio sweep. This board's Arduino loopTask (setup/loop, LVGL, every tool's
// render callback) runs pinned to core 1 (CONFIG_ARDUINO_RUNNING_CORE=1 in the
// installed qio_opi sdkconfig) -- core 0 is free.
//
// Why this exists: three times now, a tool called a multi-channel/multi-second
// radio function straight from its render path with no async wrapper, freezing
// the UI (ble_scan/wifi_scan, fixed twice already as one-off async+poll wrappers;
// nrf_scan, fixed by moving the sweep here instead of writing a fourth one-off).
// Any future SPI-B sweep (e.g. a cc1101 multi-frequency scan, if one is ever
// wired up) gets its own *_service() call added to radio_task_fn below, instead
// of repeating the mistake in a new tool.
//
// spi_b_mutex (bus_locks.h) is a real FreeRTOS recursive semaphore, already
// safe for genuine cross-task access -- nrf_scan() takes it itself, unchanged.
#pragma once

void radio_task_start();   // call once from setup(), after bus_locks_init()
