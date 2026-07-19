// Pure RCSwitch-style fixed-code protocol codec — host-testable.
// Decodes/encodes the OOK symbol timings used by cheap 315/433 MHz gate &
// remote fobs. The RF front-end (CC1101 in async mode, edge capture on GDO0)
// is bring-up; this file is just the timing<->bits math and is unit-tested.
//
// A protocol is defined the way the rc-switch library defines it: one base
// pulse length in microseconds, and each symbol (sync / zero / one) expressed
// as a {high, low} count of base pulses.  E.g. protocol 1: base 350us,
// one = {3,1} -> 1050us high then 350us low.
#pragma once
#include <stdint.h>
#include <stdio.h>

struct RcsProto {
  uint16_t base_us;                 // one base pulse in microseconds
  uint8_t  sync_hi, sync_lo;        // sync gap, in base pulses
  uint8_t  zero_hi, zero_lo;
  uint8_t  one_hi,  one_lo;
};

// The five classic rc-switch protocols. Covers the vast majority of fixed-code
// fobs / gates seen in the wild.
static const RcsProto RCS_PROTOS[] = {
  {350, 1, 31, 1, 3, 3, 1},   // 1
  {650, 1, 10, 1, 2, 2, 1},   // 2
  {100, 30, 71, 4, 11, 9, 6}, // 3
  {380, 1, 6,  1, 3, 3, 1},   // 4
  {500, 6, 14, 1, 2, 2, 1},   // 5
};
static const int RCS_PROTO_COUNT = sizeof(RCS_PROTOS) / sizeof(RCS_PROTOS[0]);

// Is `measured` within tol% of `expected`? Integer-only, tol in percent.
static inline bool rcs_near(uint32_t measured, uint32_t expected, int tol_pct) {
  uint32_t slack = expected * (uint32_t)tol_pct / 100u;
  uint32_t lo = expected > slack ? expected - slack : 0;
  return measured >= lo && measured <= expected + slack;
}

// Classify one symbol against a protocol using an explicit base pulse length
// (so a capture whose base has drifted from spec still decodes). Returns 1 for
// a "one" bit, 0 for a "zero" bit, -1 if neither template fits.
static inline int rcs_bit_base(uint32_t hi_us, uint32_t lo_us,
                               const RcsProto *p, uint32_t base, int tol_pct) {
  if (rcs_near(hi_us, base * p->one_hi, tol_pct) &&
      rcs_near(lo_us, base * p->one_lo, tol_pct))
    return 1;
  if (rcs_near(hi_us, base * p->zero_hi, tol_pct) &&
      rcs_near(lo_us, base * p->zero_lo, tol_pct))
    return 0;
  return -1;
}

// Same, at the protocol's spec base pulse length.
static inline int rcs_bit(uint32_t hi_us, uint32_t lo_us,
                          const RcsProto *p, int tol_pct) {
  return rcs_bit_base(hi_us, lo_us, p, p->base_us, tol_pct);
}

// Timings to transmit one bit — inverse of rcs_bit, used by the replay path.
static inline void rcs_symbol(int bit, const RcsProto *p,
                              uint32_t *hi_us, uint32_t *lo_us) {
  *hi_us = (uint32_t)p->base_us * (bit ? p->one_hi : p->zero_hi);
  *lo_us = (uint32_t)p->base_us * (bit ? p->one_lo : p->zero_lo);
}

// Recover the base pulse length from a measured sync gap, so a capture can be
// decoded even when the exact base drifts from spec. 0 if the gap can't match.
static inline uint32_t rcs_base_from_sync(uint32_t sync_lo_us, const RcsProto *p) {
  return p->sync_lo ? sync_lo_us / p->sync_lo : 0;
}

// Decode a captured OOK frame. `d` is a run of pulse durations (us): d[0] is the
// long sync-low gap, then alternating high,low durations per data bit. Tries each
// protocol, recovering its base pulse from the sync gap. On the first protocol
// that decodes cleanly (>=8 bits, every symbol valid) fills code/bits/proto_no
// (1-based) and returns true.
static inline bool rcs_decode(const uint32_t *d, int n, int tol_pct,
                              uint32_t *code_out, uint8_t *bits_out,
                              int *proto_no) {
  if (!d || n < 5) return false;                 // sync + a couple of bits minimum
  for (int pi = 0; pi < RCS_PROTO_COUNT; ++pi) {
    const RcsProto &p = RCS_PROTOS[pi];
    uint32_t base = rcs_base_from_sync(d[0], &p);
    if (base < 50 || base > 2000) continue;      // implausible -> not this protocol
    uint32_t code = 0;
    int bits = 0;
    bool ok = true;
    for (int i = 1; i + 1 < n; i += 2) {
      int b = rcs_bit_base(d[i], d[i + 1], &p, base, tol_pct);
      if (b < 0) { ok = false; break; }
      code = (code << 1) | (uint32_t)b;
      ++bits;
    }
    if (ok && bits >= 8) {
      *code_out = code;
      *bits_out = (uint8_t)bits;
      *proto_no = pi + 1;
      return true;
    }
  }
  return false;
}

// Format a decoded code for the screen: "0x0015F3  (24 bit, P1)".
static inline void rcs_fmt(uint32_t code, uint8_t bits, int proto_no,
                           char *out, size_t n) {
  snprintf(out, n, "0x%06lX  (%u bit, P%d)",
           (unsigned long)code, (unsigned)bits, proto_no);
}
