# Save states

Save states capture the complete running board and restore it exactly. The
CPS-1 board has no non-volatile memory, so there is nothing else to keep.

## Using them

- Each version has its own **slots 1–10** and **Autosave**. Save/Load State
  shows each slot's thumbnail, date and time, and play length.
- **Quick save / quick load** (`F5` / `F9`) use the current slot (marked `*`);
  `F6` / `F7` change it. Quick save overwrites without asking; saving over a
  slot from the menu asks first.
- Loading while a game is running asks first.
- **Autosave** is written when you return to the main menu, exit or switch
  version (each can be turned off in Extras). **Continue** resumes the
  selected version's autosave.
- Delete a slot with Delete / X in the slot list.

## Where they are stored

`<data folder>/states/<version id>-<ROM fingerprint>/slotNN.fest` and
`autosave.fest`, e.g. `states/sf2_hyper_fighting-572B5040/slot01.fest`. The
data folder is `%APPDATA%\OmegaKatana\Street Fighter II\` on Windows,
`~/.local/share/OmegaKatana/Street Fighter II/` on Linux and Steam Deck, or
the executable's folder in portable mode (`--portable` or a `portable.txt`
beside the executable; ignored inside the read-only AppImage).

## What a state file contains

A container written atomically (`.tmp`, then rename):

| Field | Purpose |
| --- | --- |
| `FESTATE1`, container version 2 | file format |
| version id (e.g. `sf2_champion_edition`), runtime version | the version label; other versions' states are refused |
| ROM fingerprint (CRC32 of the program) | refuses another revision or a modified set |
| timestamp, frame count | shown in the slot list |
| 192×112 thumbnail | preview |
| core state | the board, see below |
| CRC32 of the whole file | detects damage |

Container version 1 (16-byte id, used by the earlier projects) is still read.
Version 2 was introduced here because the version ids are longer than 15
characters.

The core state (`runtime/src/state.cpp`, magic `SF2S`, format version 1)
repeats the check: build-layout signature, version and ROM fingerprint must
all match before anything is restored. It holds:

- **Scheduler:** base time, quantum, both CPUs' execution state (local time,
  suspend, cycle eating, queued interrupt-line events), every timer, the order
  of the active timer list, and pending `synchronize()` callbacks (sound-latch
  writes, YM2151 mode and interrupt updates) and input-line flushes.
- **CPUs:** the cycle-exact 68000 (including an instruction in progress), the
  Z80.
- **Sound:** YM2151 (ymfm's full state, busy flag, timers), OKI MSM6295
  (voices, ADPCM state, command state, pin 7), stream positions.
- **Memory and latches:** 68000 work RAM, graphics RAM, CPS-A and CPS-B
  registers, Z80 RAM and bank, both sound latches, coin lockouts, the DIP
  switches the game was started with, VBLANK times, frame counter.
- **Video:** the renderer's frame state (buffered sprite list, the current
  palette, raster-interrupt counters, flip); decoded graphics come from the ROM.

No ROM data is stored in a state (a state is about 290 KB, the same bytes on Windows and Linux).

## Restoring safely

The frontend checks the container (magic, CRC, version id, fingerprint)
before the board is touched; the core then checks its own header. While the
core loads, it keeps a copy of the running board: if anything is wrong
(truncated or oversized data, an impossible timer index), the copy is put
back and the game continues as before. After a load, queued audio is flushed,
held inputs must be released before they count again, and the picture is
redrawn from the restored state.

## Verified (2026-11-14)

- `sf2_headless --state-test FRAME` saves the board, runs 300 frames with
  different inputs, loads the state and continues. With the 2-player script
  (state at frame 3000) and the 1-player script (frame 5000) the run then
  stays identical to MAME 0.264 in main and graphics RAM at every compared
  frame, for all three versions; saving again right after loading gives a
  byte-identical state; a truncated copy is refused and the running game is
  unaffected. These are the `*_save_state_round_trip` CTests.
- Frontend (Xvfb): F5/F9, slot save/load, autosave on switch, Continue,
  per-version state folders, and refusal of another version's state with
  `--state`.
