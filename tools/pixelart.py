#!/usr/bin/env python3
# Authors the Death Note mascot sprites as tiny drawing programs (rects + mirror,
# not hand-typed pixel grids) and emits LVGL lv_img_dsc_t C arrays.
#
# No image-generation tool is available in this pipeline, and hand-typing ~20
# pixel-perfect ASCII grids is both slower and harder to tweak than a dozen
# rect() calls. This is the actual "artist" here — blocky, few-color, simple
# on purpose (matches the Flipper-dolphin scale the firmware wants, not the
# high-detail manga reference).
#
# Run: python3 tools/pixelart.py   (from repo root; regenerates src/sprites_*.c)
import struct
import os

W = H = 28  # character/object sprite canvas
SPLASH_W, SPLASH_H = 48, 56

# ---- palette: charcoal/slate Death Note theme (see main.cpp C_* for UI chrome) ----
INK     = (0x1a, 0x1a, 0x1a)  # near-black outline / hair base
PAPER   = (0xED, 0xE7, 0xDD)  # off-white skin
SLATE   = (0x4b, 0x5a, 0x63)  # charcoal-blue shirt
SLATE_D = (0x2c, 0x36, 0x3c)  # darker shade / collar line
DENIM   = (0x35, 0x45, 0x55)  # jeans
CREAM   = (0xF2, 0xE9, 0xD8)  # Light's shirt highlight
GOLD    = (0xC8, 0xA2, 0x30)  # Misa accent (bow, ribbon)
PINK    = (0xD8, 0x8A, 0x9E)  # Misa hair
RED     = (0xB0, 0x2E, 0x2E)  # Ryuk grin / danger accent
GLOW    = (0xE8, 0xD8, 0x40)  # Ryuk eye glow
BOOK_BG = (0x08, 0x08, 0x09)  # notebook cover — true near-black, NOT the UI's C_BG
                               # (0x14181b): they were nearly identical, making the
                               # splash sprite invisible against its own background.
BOOK_PG = (0xE8, 0xE2, 0xD4)  # notebook page sliver


def blank(w=W, h=H):
    return [[None] * w for _ in range(h)]


def rect(cv, x, y, w, h, c):
    for j in range(y, y + h):
        for i in range(x, x + w):
            if 0 <= j < len(cv) and 0 <= i < len(cv[0]):
                cv[j][i] = c


def px(cv, x, y, c):
    if 0 <= y < len(cv) and 0 <= x < len(cv[0]):
        cv[y][x] = c


def mirror(cv):
    w = len(cv[0])
    for row in cv:
        for i in range(w // 2):
            if row[i] is not None and row[w - 1 - i] is None:
                row[w - 1 - i] = row[i]
    return cv


def to_rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def emit(name, frames, alpha=True):
    """frames: list of canvases (list[list[color-or-None]]) -> src/sprites_<name>.c"""
    h = len(frames[0])
    w = len(frames[0][0])
    lines = ['#include "sprites.h"\n']
    descs = []
    for fi, cv in enumerate(frames):
        data = bytearray()
        for row in cv:
            for c in row:
                rgb = to_rgb565(c if c is not None else (0, 0, 0))
                data += struct.pack("<H", rgb)
                if alpha:
                    data.append(255 if c is not None else 0)
        arr = f"{name}_{fi}_map"
        lines.append(f"static const uint8_t {arr}[] = {{")
        lines.append(",".join(str(b) for b in data))
        lines.append("};")
        cf = "LV_IMG_CF_TRUE_COLOR_ALPHA" if alpha else "LV_IMG_CF_TRUE_COLOR"
        lines.append(f"const lv_img_dsc_t {name}_{fi} = {{")
        lines.append("  .header.always_zero = 0,")
        lines.append(f"  .header.w = {w},")
        lines.append(f"  .header.h = {h},")
        lines.append(f"  .header.cf = {cf},")
        lines.append(f"  .data_size = sizeof({arr}),")
        lines.append(f"  .data = {arr},")
        lines.append("};")
        descs.append(f"&{name}_{fi}")
    lines.append(f"const lv_img_dsc_t *const {name}_frames[] = {{{','.join(descs)}}};")
    lines.append(f"const uint8_t {name}_frame_count = {len(frames)};")
    out = os.path.join(os.path.dirname(__file__), "..", "src", f"sprites_{name}.c")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {out} ({len(frames)} frame(s), {w}x{h})")


# ------------------------------------------------------------------ characters
def build_l(bob):
    """L: spiky black hair, hunched pale figure, knees drawn up. bob=0/1 breathing."""
    cv = blank()
    rect(cv, 9, 2, 5, 1, INK)
    rect(cv, 7, 3, 8, 2, INK)
    rect(cv, 6, 5, 10, 3, INK)
    mirror(cv)
    rect(cv, 6, 8, 3, 2, INK)
    mirror(cv)
    rect(cv, 9, 8, 10, 7, PAPER)
    px(cv, 11, 11, INK)
    y0 = 15 + bob
    rect(cv, 6, y0, 8, 10, PAPER)
    mirror(cv)
    rect(cv, 6, y0, 16, 2, SLATE_D)
    rect(cv, 8, y0 + 4, 5, 6, DENIM)
    rect(cv, 15, y0 + 4, 5, 6, DENIM)
    return cv


def build_light(sweep):
    """Light Yagami: swept hair, straight posture, writing-arm sweep 0/1/2."""
    cv = blank()
    rect(cv, 8, 3, 12, 2, INK)
    rect(cv, 7, 5, 6, 2, INK)  # hair swept to one side
    rect(cv, 9, 8, 10, 7, PAPER)
    px(cv, 11, 11, INK)
    px(cv, 16, 11, INK)
    rect(cv, 6, 15, 16, 10, CREAM)
    rect(cv, 13, 15, 2, 4, SLATE_D)  # necktie
    # writing arm: sweeps left/center/right across frames
    ax = 4 + sweep * 6
    rect(cv, ax, 19, 4, 2, PAPER)
    return cv


def build_ryuk(frame):
    """Ryuk: tall wild black hair, glowing eyes, red grin. frame=0/1 alert flash."""
    cv = blank()
    for i, wgt in enumerate([2, 3, 4, 5, 4]):
        rect(cv, 13 - wgt // 2, 0 + i, wgt, 1, INK)
    rect(cv, 6, 5, 16, 5, INK)
    mirror(cv)
    rect(cv, 8, 9, 12, 6, SLATE_D)
    eye = GLOW if frame == 1 else (0x80, 0x78, 0x20)
    rect(cv, 10, 11, 2, 2, eye)
    rect(cv, 16, 11, 2, 2, eye)
    rect(cv, 10, 15, 8, 1, RED)  # grin
    rect(cv, 8, 15, 12, 12, SLATE)
    return cv


def build_misa(frame):
    """Misa Amane: blonde twin-tails + bow, bright dress. frame 0/1/2 = bounce."""
    cv = blank()
    dy = [0, -2, 0][frame]
    rect(cv, 3, 6 + dy, 4, 12, PINK)
    mirror(cv)
    rect(cv, 8, 3 + dy, 12, 6, PINK)
    rect(cv, 9, 9 + dy, 10, 6, PAPER)
    px(cv, 11, 11 + dy, INK)
    px(cv, 16, 11 + dy, INK)
    rect(cv, 12, 13 + dy, 4, 1, RED)  # smile
    rect(cv, 6, 4 + dy, 3, 2, GOLD)  # bow
    rect(cv, 7, 15 + dy, 14, 10, GOLD)
    return cv


def build_nfc_card(frame):
    """A card sliding down toward the reader; frame 2 = contact glow."""
    cv = blank()
    y = 2 + frame * 6
    rect(cv, 5, y, 18, 12, SLATE)
    rect(cv, 7, y + 2, 8, 3, PAPER)
    rect(cv, 4, 22, 20, 3, SLATE_D)  # reader plate
    if frame == 2:
        rect(cv, 3, 20, 22, 1, GOLD)
        rect(cv, 3, 26, 22, 1, GOLD)
    return cv


def build_ir_beam(frame):
    """A remote silhouette pulsing 0/1/2 signal bars toward the device."""
    cv = blank()
    rect(cv, 3, 8, 6, 16, SLATE_D)
    rect(cv, 4, 10, 4, 2, PAPER)
    for bar in range(frame + 1):
        rect(cv, 12 + bar * 5, 13 - bar * 2, 3, 4 + bar * 4, GOLD)
    return cv


def build_notebook(open_):
    cv = blank(SPLASH_W, SPLASH_H)
    rect(cv, 2, 2, SPLASH_W - 4, SPLASH_H - 4, PAPER)   # bright edge — pops on any bg
    rect(cv, 4, 4, SPLASH_W - 8, SPLASH_H - 8, BOOK_BG)
    rect(cv, 8, 10, SPLASH_W - 16, 4, RED)               # title-bar accent
    if open_:
        rect(cv, SPLASH_W - 16, 14, 10, SPLASH_H - 24, BOOK_PG)
        rect(cv, SPLASH_W - 13, 18, 4, 1, INK)
        rect(cv, SPLASH_W - 13, 22, 4, 1, INK)
    return cv


if __name__ == "__main__":
    emit("l", [build_l(0), build_l(1)])
    emit("light", [build_light(0), build_light(1), build_light(2)])
    emit("ryuk", [build_ryuk(0), build_ryuk(1)])
    emit("misa", [build_misa(0), build_misa(1), build_misa(2)])
    emit("nfc_card", [build_nfc_card(0), build_nfc_card(1), build_nfc_card(2)])
    emit("ir_beam", [build_ir_beam(0), build_ir_beam(1), build_ir_beam(2)])
    emit("notebook", [build_notebook(False), build_notebook(True)], alpha=False)
