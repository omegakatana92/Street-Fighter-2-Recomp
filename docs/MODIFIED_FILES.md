# Reused code and what changed

## From MAME 0.264 (BSD-3-Clause)

| Here | From | Changes |
|---|---|---|
| `runtime/m68000/` | `src/devices/cpu/m68000/m68000*.{h,cpp}` (cycle-exact core, Olivier Galibert) | device framework replaced by `m68000_shim.h` (16-bit bus callbacks, CPU-space flag for interrupt acknowledge) and the kemu execute interface; instruction code unchanged. Shared with the earlier ports. |
| `runtime/z80/` | `src/devices/cpu/z80/z80.{h,cpp}` (Juergen Buchmueller) | byte-bus callbacks and kemu execute interface; daisy chain, NSC800, debugger and state-table glue removed; explicit save/load. |
| `runtime/video/cps1_video.*` | `src/mame/capcom/cps1_v.cpp` (Paul Leaman and others) | only the CPS-B-11 / CPS-B-21 configurations and the STF29 / S9263B mappers used by the three sets; bootleg, QSound-board and CPS-2 paths removed; renders into a 16-bit pen bitmap like MAME's, converted through the palette; explicit save/load. |
| `runtime/video/mvideo.*` | `src/emu/drawgfx.cpp`, `tilemap.cpp`, `src/lib/util/bitmap.h` | the minimal subset the CPS-1 renderer needs. |
| `runtime/devices/okim6295.*` | `src/devices/sound/okim6295.cpp`, `okiadpcm.cpp` | sound-stream plumbing replaced by a `generate()` call and callbacks for stream updates and rate changes. |
| `runtime/devices/ym2151.*` | `src/devices/sound/ymopm.cpp`, `ymfm_mame.h` | MAME's `ym_generic_device` behaviour (timers via `from_ticks`, `synchronize()` for mode writes and interrupt checks, busy flag, stream update before each access) mapped onto kemu. |
| `runtime/emu/kemu.*` | `src/emu/attotime.h`, `schedule.cpp`, `diexec.cpp` | reimplemented subset (written for Kaiser Knuckle, shared); comments updated for CPS-1. |
| `runtime/src/machine.cpp` | `src/mame/capcom/cps1.cpp` | the board: memory maps, inputs, DIPs, coin control, sound latches, clocks and the `cps1_12MHz` sound mix, as described in HARDWARE.md. |
| `runtime/src/roms.cpp` | `ROM_START(sf2)`, `(sf2ce)`, `(sf2hf)` in `cps1.cpp` | file names, sizes, CRC32s and load layout. |

## Third-party libraries

| Here | Version | Changes |
|---|---|---|
| `runtime/third_party/ymfm/` | as in MAME 0.264 `3rdparty/ymfm` (checked byte for byte) | `ymfm_fm.ipp`: `fm_engine_base::save_restore` also stores the prepare bookkeeping and on restore rebuilds the derived caches instead of invalidating them, so a restored chip produces exactly the samples the saved one would have (local change from the Dragon Blaze port). All other files unchanged. |
| `runtime/third_party/miniz/` | 11.3.2 (as reported by `MZ_VERSION`) | none |
| SDL2 | 2.30.9 | none (linked statically on Linux, `SDL2.dll` on Windows) |

## Frontend (`frontend/`, shared with the other arcade projects)

Changes made for this project, all backwards compatible with single-game
adapters:

- `fe.h`: `fe_suite` (id, title, about, variants) and `fe_run_suite()`;
  `FE_MAX_VARIANTS` = 8.
- `fe_app.c`:
  - **Select Version** and **ROM Configuration** screens; per-version status
    and error text; Start Game, Continue and the header name the version.
  - Per-version ROM paths (`[paths] rom_<id>`), DIP switches
    (`[gameplay.<id>]`) and remembered selection (`[ui] version`); the data
    folder is named after the suite.
  - Safe switching (`apply_variant`): confirm, optional "Save State First",
    autosave, unload, reload, input/audio/rewind/movie reset.
  - Quick menu: "Restart Current Version" and "Switch Version".
  - State container version 2 with a 32-byte id (version 1 still read);
    another version's state is refused by name. Movies and screenshots carry
    the version id; "Play Latest Movie" only picks the running version's.
  - "Use this folder" in the ROM browser (unzipped sets); the browser
    recognises another version's set and names it.
  - Confirmation dialogs preselect *No*.
  - `--version ID` on the command line.
  - Accent *Per profile* (profile key `theme.accent`).
  - Profile search also in `../share/<suite id>/profiles/`.
- `fe_profile.*`: `theme.accent`.

## Project files

`ports/sdl2/main.cpp` (adapter), `ports/headless/main.cpp`,
`runtime/include/sf2/machine.h`, `runtime/src/state.cpp`, `profiles/`,
`packaging/`, `scripts/`, `tools/mame_reference.lua` are original to this
project (GPL-2.0).
