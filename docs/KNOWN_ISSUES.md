# Known issues and limitations (0.1.0)

## Not a static recompilation yet

The 68000 programs run on MAME's cycle-exact 68000 interpreter, not on
translated code (README, "What this is"). Static recompilation of the three
programs is planned work, to be checked against the existing MAME references.

## Not tested on the target hardware

- **Windows 10/11:** the Windows package was built by cross-compiling and run
  under Wine on Linux only.
- **Steam Deck:** the AppImage was run under Xvfb on a Linux PC only. Speed,
  Game Mode, suspend/resume and the built-in controls are unchecked.
- **Controllers:** no physical controller was used. Controller mappings,
  hot-plugging and the Back + Start shortcut are implemented with SDL's
  GameController API, as in the earlier projects, but untested here.
- **Audio output:** sound was checked as data (WAV against MAME). Listening
  through a real audio device, latency and underrun behaviour on real hardware
  are unchecked.
- **Human play:** gameplay was exercised by scripts and random inputs, not by
  a person playing.

## Emulation

- **Z80 timing:** some YM2151 timer interrupts reach the sound CPU 7 cycles
  (≈2 µs) later than in MAME. Z80 RAM therefore differs from MAME at some
  frames; the game itself (68000 work RAM, graphics RAM, picture) is
  unaffected in all comparisons. Audible effect: not expected (2 µs), not
  ruled out. See ACCURACY.md.
- Only the World revisions in MAME 0.264 format (`sf2`, `sf2ce`, `sf2hf`)
  are supported; other regions, revisions and hacks are refused.
- Only 2.5 minutes of play per version were compared with MAME; later
  stages, all characters, endings and the service menu were not compared.

## Frontend

- The CPS-1 has no NVRAM: high scores reset when the program is closed,
  as on the cabinet (save states keep them).
- No widescreen or CRT curvature.
- Hotkeys and controller mappings are shared by the three versions (same
  cabinet); DIP switches, ROM paths and states are per version.
- `--version` takes the profile ids (`sf2_world_warrior`,
  `sf2_champion_edition`, `sf2_hyper_fighting`), not the MAME set names.
- Two copies of the program running at the same time share one
  `settings.ini`; whichever exits last writes it.
