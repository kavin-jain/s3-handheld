#include "storage.h"
#include "strutil.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "bus_locks.h"

// The SD shares SPI-A (HSPI) with the TFT — same SCLK/MOSI/MISO, its own CS.
// TFT_eSPI drives its own transactions; we run a separate SPIClass on the same
// pins for the SD. Each device asserts its own CS, so single-threaded access
// (all SD + all draw work on the UI core) is safe.
// ponytail: shared SPI-A bus is a HARDWARE bring-up risk — if the display glitches
// during an SD write, give the SD its own bus or wrap draws/SD in a mutex.
// See docs/BRINGUP.md (Storage). Untested without the board.
#include <TFT_eSPI.h>
// Use the SPI instance already started by TFT_eSPI so we don't fight over the bus state.
static bool s_ready = false;

static void ensure_dirs() {
  for (int k = 0; k < SAVE_KIND_N; k++) {
    const char *d = sp_dir(k);
    if (!SD.exists(d)) SD.mkdir(d);
  }
}

bool storage_begin() {
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  // Do not call SPI.begin() again since TFT_eSPI already brought up the bus.
  // We just hand the shared instance to the SD library.
  // Using 4MHz (4000000) for better stability on breadboards/modules.
  s_ready = SD.begin(PIN_SD_CS, TFT_eSPI::getSPIinstance(), 4000000);
  if (s_ready) ensure_dirs();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return s_ready;
}

bool storage_ready() { return s_ready; }

uint32_t storage_total_mb() {
  if (!s_ready) return 0;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  uint32_t mb = (uint32_t)(SD.cardSize() >> 20);
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return mb;
}

uint32_t storage_used_mb() {
  if (!s_ready) return 0;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  uint32_t mb = (uint32_t)(SD.usedBytes() >> 20);
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return mb;
}

// Scan a kind's folder ONCE, return the next free sequence (max existing + 1),
// using the host-tested sp_parse_seq. Beats probing SD.exists() up to 10000×.
static uint32_t next_seq(int kind) {
  File dir = SD.open(sp_dir(kind));
  if (!dir) return 0;
  int hi = -1;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    const char *name = e.name();
    const char *base = name;                 // some cores return a full path
    for (const char *p = name; *p; p++) if (*p == '/') base = p + 1;
    int s = sp_parse_seq(base, kind);
    if (s > hi) hi = s;
    e.close();
  }
  dir.close();
  return (uint32_t)(hi + 1);
}

const char *storage_save(int kind, const char *ext, const uint8_t *data, size_t len) {
  static char path[48];
  if (!s_ready || kind < 0 || kind >= SAVE_KIND_N || !ext) return "";
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  uint32_t seq = next_seq(kind);
  if (seq >= 10000) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return ""; }
  if (!sp_make_path(path, sizeof path, kind, ext, seq)) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return ""; }
  File f = SD.open(path, FILE_WRITE);
  if (!f) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return ""; }
  if (data && len) f.write(data, len);
  f.close();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return path;
}

bool storage_save_config(const char *text) {
  if (!s_ready || !text) return false;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  File f = SD.open("/config.txt", FILE_WRITE);   // FILE_WRITE truncates+rewrites
  if (!f) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return false; }
  f.print(text);
  f.close();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return true;
}

int storage_count_files(const char *dir, const char *ext) {
  if (!s_ready || !dir || !ext) return 0;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  File d = SD.open(dir);
  if (!d) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return 0; }
  int n = 0;
  for (File e = d.openNextFile(); e; e = d.openNextFile()) {
    if (str_ends_with(e.name(), ext)) n++;
    e.close();
  }
  d.close();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return n;
}

size_t storage_read_file(const char *path, char *out, size_t cap) {
  if (!s_ready || !path || !out || cap == 0) return 0;
  out[0] = 0;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  if (!SD.exists(path)) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return 0; }
  File f = SD.open(path, FILE_READ);
  if (!f) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return 0; }
  size_t o = 0;
  while (f.available() && o < cap - 1) {
    int c = f.read();
    if (c < 0) break;
    out[o++] = (char)c;
  }
  out[o] = 0;
  f.close();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return o;
}

bool storage_load_config(char *out, size_t cap) {
  if (!s_ready || !out || cap == 0) return false;
  out[0] = 0;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  if (!SD.exists("/config.txt")) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return false; }
  File f = SD.open("/config.txt", FILE_READ);
  if (!f) { if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex); return false; }
  size_t o = 0;
  while (f.available() && o < cap - 1) {
    int c = f.read();
    if (c < 0 || c == '\n' || c == '\r') break;
    out[o++] = (char)c;
  }
  out[o] = 0;
  f.close();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  return o > 0;
}
