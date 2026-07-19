#include "storage.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// The SD shares SPI-A (HSPI) with the TFT — same SCLK/MOSI/MISO, its own CS.
// TFT_eSPI drives its own transactions; we run a separate SPIClass on the same
// pins for the SD. Each device asserts its own CS, so single-threaded access
// (all SD + all draw work on the UI core) is safe.
// ponytail: shared SPI-A bus is a HARDWARE bring-up risk — if the display glitches
// during an SD write, give the SD its own bus or wrap draws/SD in a mutex.
// See docs/BRINGUP.md (Storage). Untested without the board.
static SPIClass sdSPI(HSPI);
static bool s_ready = false;

static void ensure_dirs() {
  for (int k = 0; k < SAVE_KIND_N; k++) {
    const char *d = sp_dir(k);
    if (!SD.exists(d)) SD.mkdir(d);
  }
}

bool storage_begin() {
  sdSPI.begin(PIN_SPIA_SCLK, PIN_SPIA_MISO, PIN_SPIA_MOSI, PIN_SD_CS);
  // Keep SD clock modest on a shared bus; raise after it's proven on hardware.
  s_ready = SD.begin(PIN_SD_CS, sdSPI, 20000000);
  if (s_ready) ensure_dirs();
  return s_ready;
}

bool storage_ready() { return s_ready; }

uint32_t storage_total_mb() { return s_ready ? (uint32_t)(SD.cardSize() >> 20) : 0; }
uint32_t storage_used_mb()  { return s_ready ? (uint32_t)(SD.usedBytes() >> 20) : 0; }

const char *storage_save(int kind, const char *ext, const uint8_t *data, size_t len) {
  static char path[48];
  if (!s_ready || kind < 0 || kind >= SAVE_KIND_N || !ext) return "";
  // First unused slot for this kind (bounded scan).
  for (uint32_t seq = 0; seq < 10000; seq++) {
    if (!sp_make_path(path, sizeof path, kind, ext, seq)) return "";
    if (!SD.exists(path)) {
      File f = SD.open(path, FILE_WRITE);
      if (!f) return "";
      if (data && len) f.write(data, len);
      f.close();
      return path;
    }
  }
  return "";  // 10000 slots full for this kind
}
