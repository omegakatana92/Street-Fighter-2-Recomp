# Building on Linux

Requirements: CMake ≥ 3.20, Ninja (or make), C11 and C++17 compilers, SDL2
development files (`libsdl2-dev`) for the frontend. No ROM and no Python are
needed to build. The CMake files ship in the release source package (they are
not kept in the git repository).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cp -r profiles build/bin/
./build/bin/StreetFighterII
```

Smoke tests with your own ROM sets (any subset):

```bash
scripts/build-local.sh --sf2 ~/roms/sf2.zip --sf2ce ~/roms/sf2ce.zip --sf2hf ~/roms/sf2hf.zip
```

This configures with `-DSF2_ROM_SF2=…` etc. and runs `ctest`: per version a
600-frame boot and a save-state round trip. The paths only end up in your local
build directory.

## Headless runner

`sf2_headless --version sf2|sf2ce|sf2hf --rom PATH --frames N` runs the board
without a window.

| Option | |
|---|---|
| `--inputs FILE` | lines `FRAME MASK` (MASK = `sf2::IN_*` bits from `runtime/include/sf2/machine.h`), applied from FRAME on |
| `--mame-latency` | apply inputs one frame later, as MAME applies Lua `set_value()` |
| `--dsw A,B,C` | DIP switch bytes (default 255,252,159) |
| `--hash-every N` | print a hash of 68000 work RAM, graphics RAM and Z80 RAM every N frames |
| `--png FRAME:FILE` | the 384×224 screen after FRAME (repeatable) |
| `--wav FILE` | the board's mono output at 55,930 Hz |
| `--compare DIR` | compare RAM with MAME dumps `DIR/rNNNNN.bin` (see below) |
| `--state-test FRAME` | save, run 300 frames with other inputs, load, continue; also checks re-save and truncated-file refusal |
| `--save-state FRAME:FILE`, `--load-state FILE` | state files (a loaded state continues at its own frame number) |
| `--trace-z80 FILE`, `--event-log FILE` | development traces of the sound CPU |

## Comparing with MAME

```bash
mkdir -p ref/h && cd ref
SF2_FRAMES=7200 SF2_EVERY=10 SF2_SNAP=600 SF2_INPUTS=../inputs.txt \
  mame sf2ce -nodrc -video none -sound none -nothrottle \
       -autoboot_script ../tools/mame_reference.lua
cd .. && sf2_headless --version sf2ce --rom sf2ce.zip --frames 7200 \
  --inputs inputs.txt --mame-latency --compare ref/h --png 600:ours0600.png
```

The Lua script writes one RAM image every `SF2_EVERY` frames (68000 work
RAM, graphics RAM as big-endian words, Z80 RAM) and a snapshot every
`SF2_SNAP` frames (`snap/sf2ce/NNNN.png`, 384×224). The runner reports, per
differing frame, how many bytes differ in each region. Results:
[ACCURACY.md](ACCURACY.md).

## Release build (portable binary for SteamOS and older distributions)

The release binary links SDL2 statically and targets glibc 2.31 (Steam Runtime
"sniper" / Debian 11 baseline); SDL2 loads X11, Wayland, PulseAudio
(PipeWire's pulse server on the Deck) and ALSA from the system at run time.
The packages were built with zig as the C/C++ compiler:

```bash
pip install ziglang
mkdir -p ~/zigcc && for t in cc c++; do cat > ~/zigcc/$t <<EOS
#!/bin/sh
exec python3 -m ziglang $t -target x86_64-linux-gnu.2.31 -idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu "\$@"
EOS
done
printf '#!/bin/sh\nexec python3 -m ziglang ar "$@"\n' > ~/zigcc/ar
printf '#!/bin/sh\nexec python3 -m ziglang ranlib "$@"\n' > ~/zigcc/ranlib
chmod +x ~/zigcc/*

# static SDL2 2.30.9 (video/audio backends loaded dynamically)
CC=~/zigcc/cc cmake -S SDL2-2.30.9 -B sdl-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_AR=$HOME/zigcc/ar -DCMAKE_RANLIB=$HOME/zigcc/ranlib \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_STATIC_PIC=ON -DSDL_TEST=OFF -DCMAKE_INSTALL_PREFIX=$PWD/sdl-static
cmake --build sdl-build && cmake --install sdl-build

CC=~/zigcc/cc CXX=~/zigcc/c++ cmake -S . -B build/linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_AR=$HOME/zigcc/ar -DCMAKE_RANLIB=$HOME/zigcc/ranlib \
  -DSDL2_DIR=$PWD/sdl-static/lib/cmake/SDL2
cmake --build build/linux-release
APPIMAGETOOL=/path/to/appimagetool ./scripts/build-appimage.sh
```

The result is `dist/StreetFighterII-SteamDeck-x86_64.AppImage`. It contains
the program, the three profiles, the icon, licences and the user docs; no ROM
data.

Check: `ldd build/linux-release/bin/StreetFighterII` lists only libc, libm,
libpthread and libdl (plus the loader); `objdump -T … | grep GLIBC_` tops out
at 2.29.

## Verified (2026-11-14, Ubuntu 24.04 container)

- Development build (GCC 13) and release build (zig 0.17.1 `cc`/`c++`,
  glibc 2.31 target, SDL2 2.30.9 static): all MAME comparisons in
  ACCURACY.md pass with the release build.
- The release binary needs GLIBC_2.29 at most and links only libc, libm,
  libpthread and libdl.
- The AppImage runs under Xvfb (Mesa, OpenGL renderer) from a clean home
  folder: main menu, Hyper Fighting from `--version`/`--rom`, coin and start,
  player select, quick menu with Switch Version, quick save, the version
  selector showing unconfigured versions, configuring Champion Edition
  through the ROM browser and switching to it (accent changes, per-version
  ROM paths in `settings.ini`).
- Not run on physical Linux desktop hardware or a Steam Deck.
