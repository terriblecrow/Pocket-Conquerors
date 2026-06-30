#!/bin/bash
# build.sh — compile Conway's Conquerors into Game Boy ROMs.
#
# Produces TWO ROMs from the one source tree:
#   life.gb       — color build (Game Boy Color palette, boots on DMG too)
#   life-dmg.gb   — pure monochrome build for the original Game Boy / Pocket
#
# The only difference is the BUILD_DMG compile flag (which compiles out every
# color path) and the cartridge header's CGB flag. Requires SDCC (sm83 target:
# sdasgb, sdldgb, makebin). No GBDK.
set -e
cd "$(dirname "$0")"

SRC=src
OUT=build
mkdir -p "$OUT"

# build_rom <out.gb> <extra-cflags> <makebin-cgb-flag> <title>
build_rom() {
  local gb="$1" cflags="$2" cgb="$3" title="$4"
  local tag="${gb%.gb}"

  echo "  assembling crt0.s"
  sdasgb -o "$OUT/${tag}_crt0.rel" "$SRC/crt0.s"

  echo "  compiling main.c ($cflags)"
  sdcc -msm83 $cflags -I"$SRC" -c "$SRC/main.c" -o "$OUT/${tag}_main.rel"

  echo "  linking"
  sdldgb -n -i -m -w -b _CODE=0x0200 -b _DATA=0xC000 \
    -k /usr/share/sdcc/lib/sm83 -l sm83 \
    -o "$OUT/${tag}.ihx" "$OUT/${tag}_crt0.rel" "$OUT/${tag}_main.rel"

  echo "  stripping RAM data from IHX"
  python3 - "$OUT/${tag}" <<'PY'
import sys
base = sys.argv[1]
out = []
for line in open(base + ".ihx"):
    s = line.strip()
    if not s.startswith(':'):
        out.append(line); continue
    addr = int(s[3:7], 16); typ = int(s[7:9], 16)
    if typ == 0 and addr >= 0x8000:      # RAM region — keep out of the ROM image
        continue
    out.append(line)
open(base + "_rom.ihx", "w").writelines(out)
PY

  echo "  building $gb"
  # -yc sets the CGB-compatible flag; omit it for the pure-DMG ROM.
  # -yt 0x13 = MBC3+RAM+BATTERY, -ya 1 = one 8KB RAM bank (battery-backed) for
  # the persistent statistics. Both must be set or SRAM saves won't stick.
  makebin -Z $cgb -yo 2 -yt 0x13 -ya 1 -yj -yn "$title" "$OUT/${tag}_rom.ihx" "$gb" >/dev/null 2>&1
  echo "  done: $gb ($(stat -c%s "$gb") bytes)"
}

echo "[color]  Game Boy Color build"
build_rom life.gb     ""           "-yc" "TCLIFE"

echo "[mono]   Game Boy (DMG) build"
build_rom life-dmg.gb "-DBUILD_DMG" ""    "TCLIFEBW"

echo "all done."
