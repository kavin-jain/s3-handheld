#!/usr/bin/env bash
# Run every host test TWICE and summarise. Exit non-zero if anything fails.
# Usage:  bash test/run_all.sh   (from the repo root)
#
# These are the PURE-LOGIC tests (parsers, codecs, frame builders, key iteration,
# path/format helpers). They prove the brains of every feature on the host with
# g++ — no hardware. The radio/NFC/IR/USB I/O is verified separately on-device
# via docs/BRINGUP.md when you plug the board in.
#
# Built with ASan+UBSan: a pure-logic parser handling SD-loaded data (a key
# dictionary, a config line, a malformed .ir file) has no business ever reading
# or writing out of bounds, however malformed the input -- a plain "did it
# return the right answer" pass can't see that, it only ever reads what the
# test happened to hand it. This is what caught nfc_keys.h's nfc_parse_key
# reading 1 byte past its input on a real "too short" case already in this
# suite -- plain g++ had been passing it silently.
set -u
cd "$(dirname "$0")/.."

RUNS=2
pass=0; fail=0; failed=""
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for src in test/test_*.cpp; do
  name="$(basename "$src" .cpp)"
  bin="$tmp/$name"
  if ! g++ -std=c++17 -Wall -fsanitize=address,undefined -g "$src" -o "$bin" 2>"$tmp/err"; then
    fail=$((fail+1)); failed="$failed\n  BUILD  $name"; continue
  fi
  ok=1
  for r in $(seq 1 "$RUNS"); do
    if ! "$bin" >/dev/null 2>&1; then ok=0; break; fi
  done
  if [ "$ok" = 1 ]; then
    pass=$((pass+1)); printf '  ok   %-26s x%d\n' "$name" "$RUNS"
  else
    fail=$((fail+1)); failed="$failed\n  RUN    $name"
  fi
done

total=$((pass+fail))
echo "------------------------------------------------------------"
echo "host tests: $pass/$total passed  (each run ${RUNS}x)"
if [ "$fail" -ne 0 ]; then
  printf 'FAILED:%b\n' "$failed"
  exit 1
fi
echo "ALL GREEN"
