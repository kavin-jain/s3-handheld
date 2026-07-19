// "Save everything" storage layer — onboard microSD, Flipper-style folders.
// Pure path logic is in storage_paths.h (host-tested); this is the hardware side.
#pragma once
#include "storage_paths.h"
#include <stddef.h>
#include <stdint.h>

// Mount the SD (SPI-A, shared with the TFT) and ensure the capture folders exist.
// Returns false if no card / mount failed — the UI degrades gracefully.
bool storage_begin();
bool storage_ready();
uint32_t storage_total_mb();   // 0 if not mounted
uint32_t storage_used_mb();

// Save a blob under the next free slot for a kind (e.g. SAVE_NFC, ext "nfc").
// Returns the path written, or "" on failure.
const char *storage_save(int kind, const char *ext, const uint8_t *data, size_t len);

// Persist / load the settings line at /config.txt (see config.h). Bring-up.
bool storage_save_config(const char *text);
bool storage_load_config(char *out, size_t cap);   // false if absent/empty
