#include "tvbgone.h"
#include "ir_remote.h"
#include <Arduino.h>

// Blast every known TV power code with a gap between each. Starter table — expand
// from the Flipper-IRDB TV folder for wider coverage. Untested without an emitter.
void tvbgone_fire_all(int gap_ms) {
  TvCode c;
  for (int i = 0; i < tvb_count(); i++) {
    if (tvb_get(i, &c)) {
      ir_send(c.proto, c.value, c.bits);
      delay(gap_ms);
    }
  }
}
