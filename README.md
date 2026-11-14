# Street Fighter II — native CPS-1 port (three versions in one application)

One Windows / Linux / Steam Deck application that runs **Street Fighter II:
The World Warrior**, **Street Fighter II′: Champion Edition** and **Street
Fighter II′: Hyper Fighting** on a native rebuild of Capcom's CPS-1 arcade
board, with a version selector, per-version ROM configuration, save states,
display/audio/controller settings and verified on-screen gadgets.

You need your own MAME 0.264 ROM sets (`sf2.zip`, `sf2ce.zip`, `sf2hf.zip`;
any one of them is enough to play that version). No ROM data, graphics or
sound is in this repository or in any package.

## The three versions

| Version | MAME set | Revision | Main CPU | CPS-B | Notes |
|---|---|---|---|---|---|
| The World Warrior | `sf2` | World 910522 | 68000 @ 10 MHz | CPS-B-11, STF29 graphics mapper | 8 playable characters |
| Champion Edition | `sf2ce` | World 920513 | 68000 @ 12 MHz | CPS-B-21, S9263B mapper | bosses playable, mirror matches |
| Hyper Fighting | `sf2hf` | World 921209 | 68000 @ 12 MHz | CPS-B-21, S9263B mapper | faster game speed, new moves |

All three are MAME *parent* sets (no parent zip needed). They share the Z80
sound hardware, the OKI samples and the PAL dumps; Champion Edition and
Hyper Fighting also share their graphics and Z80 sound program. Every main
program is different. Details: [docs/ROM_SETUP.md](docs/ROM_SETUP.md),
[docs/HARDWARE.md](docs/HARDWARE.md).

## What this is (and is not)

| Technique | Used here? |
|---|---|
| **Hardware reimplementation / emulation of the board** | **Yes — this is what the project is.** The CPS-1 board is rebuilt after MAME 0.264: its scheduler and timing, MAME's cycle-exact 68000 core, MAME's Z80 core, the YM2151 through ymfm (the library MAME uses), the OKI MSM6295, and the CPS-A/CPS-B video from `cps1_v.cpp`. The CPU cores are *interpreters* (MAME's). |
| **Static recompilation** (game code translated to C/C++ ahead of time) | **No, not in this release.** See "Recompilation status" below. |
| **Dynamic recompilation (JIT)** | No. |
| **Decompilation** (readable source of the game logic) | No. |

### Recompilation status

The translator used in the Gaia Crusaders and Violent Storm projects turns
each 68000 instruction into C with a per-instruction cycle model. It was
evaluated for this board and not adopted for 0.1.0, because:

1. the goal here was to match MAME frame for frame, and that requires the
   68000's exact cycle-level timing (MAME's cycle-exact core), which the
   per-instruction model only approximates;
2. Champion Edition runs code it copies into graphics RAM at 0x900000, so a
   translated build needs a run-time fallback for RAM code anyway;
3. on MAME's cycle-exact core, a "dispatch" translation like the one in the
   Kaiser Knuckle port would remove nothing measurable (one table lookup per
   instruction), so it would be recompilation in name only.

A real static recompilation of the three 68000 programs is **planned**: it
needs a translator that reproduces the cycle-exact core's bus timing, and it
must be checked against the existing MAME references (the tools for that are in
`tools/` and `ports/headless/`). The board runs at about 25× real time
already, so this is about the project's goal, not about speed.

## Status

| Area | Status |
|---|---|
| ROM validation (zip or folder; every file by name or CRC32; wrong-version and incomplete sets reported, never substituted) | implemented + tested |
| Board: 68000, Z80, YM2151, OKI6295, CPS-A/B video, inputs, DIP switches, coin lockout | implemented + tested against MAME 0.264 |
| Accuracy | **68000 work RAM and graphics RAM byte-identical to MAME at every compared frame** (6,750 frames over 9 runs: attract mode and one- and two-player games in all three versions) and **screen pixel-identical at all 162 compared snapshots**. The Z80 sound CPU's RAM matches at most frames; at some it is one instruction (≈2 µs) off MAME (see [docs/ACCURACY.md](docs/ACCURACY.md)) |
| Audio | implemented; against MAME's 61 s attract-mode recordings of The World Warrior and Champion Edition: level within 0.2 % in every second, waveform correlation 0.997 |
| Save states | implemented + tested: save, 300 other frames, load — the run continues identical to MAME; damaged files refused; per-version slots; cross-version states refused |
| One application, three versions | implemented + tested (Xvfb): version selector, remembered last version, per-version ROM paths, DIPs, states, screenshots and movies, safe switching with autosave, works with only one version configured |
| Controls | 2 players × (8-way stick + 6 buttons) on keyboard and controllers, remappable; keyboard tested under Xvfb, **no physical controller tested** |
| Gadgets | version, round timer, both players' scores (each address checked against the HUD in MAME runs of every version); FPS, frame time, input display |
| Windows x64 zip, Linux / Steam Deck AppImage | built; Windows build run under Wine (identical results and byte-identical save states to Linux), AppImage run under Xvfb; **not run on Windows 10/11 hardware or on a Steam Deck** |
| Gameplay | exercised with scripted and random inputs (headless) and keyboard input in the frontend; **not played by a person** |

## Quick start

1. Start the program. Open **ROM Configuration** and pick the zip (or
   unzipped folder) of each version you own.
2. **Select Version**, then **Start Game**. Coin `5`, Start `Enter`, stick =
   arrow keys, punches `A S D`, kicks `Z X C`. `Esc` (or Back + Start on a
   controller) opens the quick menu, which also has **Switch Version**.
   See [docs/CONTROLS.md](docs/CONTROLS.md).

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/StreetFighterII
./build/bin/sf2_headless --version sf2ce --rom /path/to/sf2ce.zip --frames 3000 --png 3000:shot.png
```

No ROM is needed to build. The CMake files ship in the source package attached
to each release (they are not kept in the git repository). Details:
[docs/BUILD_LINUX.md](docs/BUILD_LINUX.md), [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md),
[docs/STEAM_DECK.md](docs/STEAM_DECK.md).

## Documentation

- [docs/ROM_SETUP.md](docs/ROM_SETUP.md) — the three sets, files, checks
- [docs/CONTROLS.md](docs/CONTROLS.md) — mappings, opposite directions, service mode, DIP switches
- [docs/FRONTEND.md](docs/FRONTEND.md) — menus, version switching, display/audio, profiles, gadgets
- [docs/SAVE_STATES.md](docs/SAVE_STATES.md) — what a state contains and how it is checked
- [docs/HARDWARE.md](docs/HARDWARE.md) — the board model and the version differences
- [docs/ACCURACY.md](docs/ACCURACY.md) — how the port was compared with MAME, with numbers
- [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) — limitations and remaining work
- [docs/MODIFIED_FILES.md](docs/MODIFIED_FILES.md) — reused code and what changed

## Licences

This project's own code is GPL-2.0 (`LICENSE`). The 68000 and Z80 cores, the
CPS-1 video and the sound devices are MAME code (BSD-3-Clause); the YM2151 is
ymfm (BSD-3-Clause); see `LICENSES/`. SDL2 (zlib), miniz (MIT), Inter font
(OFL). *Street Fighter II* is © Capcom; this project is not affiliated with or
endorsed by Capcom.
