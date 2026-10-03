#include "radio_task.h"
#include "nrf24_radio.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Untested without hardware: task-switch latency, SPI-B contention under real
// preemption (vs. today's single-task cooperative access), and whether a
// sweep now overlaps cleanly with LVGL's render cadence are all unverified --
// see docs/BRINGUP.md. Compiles + the pure-logic nrf_band.h tests passing is
// gate 2 of this project's own 3-gate maturity model, not gate 3.
static void radio_task_fn(void *) {
  for (;;) {
    nrf_scan_service();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void radio_task_start() {
  xTaskCreatePinnedToCore(radio_task_fn, "radio", 4096, NULL, 1, NULL, 0);
}
