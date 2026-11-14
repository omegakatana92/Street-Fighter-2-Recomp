# Changelog

## 0.1.0 — 2026-11-14

First release.

- One application for **Street Fighter II: The World Warrior** (`sf2`, World
  910522), **Champion Edition** (`sf2ce`, World 920513) and **Hyper Fighting**
  (`sf2hf`, World 921209), with a version selector, per-version ROM
  configuration, DIP switches and save states, and safe switching between
  versions (confirm, optional save, autosave, full unload and reload).
- CPS-1 board rebuilt after MAME 0.264 on MAME's cycle-exact 68000 core, MAME's
  Z80 core, ymfm (YM2151), the OKI MSM6295 and the CPS-A/CPS-B video; MAME's
  scheduler timing. **Not a static recompilation** (see README).
- Compared with MAME 0.264: 68000 work RAM and graphics RAM identical at all
  6,750 compared frames, 162 of 162 screenshots pixel-identical, over attract,
  one-player and two-player runs of every version. Known difference: Z80 RAM
  at some frames (sound CPU interrupt timing, ≈2 µs).
- ROM validation by name or CRC32, from zip archives or folders, with
  messages for missing, bad and wrong-version files; Unicode paths.
- Save states with version label, ROM fingerprint, format version and
  timestamp; incompatible and damaged states refused without touching the
  running game; rewind; input movies.
- Two-player six-button controls on keyboard and controllers, remappable;
  opposing directions cancel as in MAME.
- Gadgets: current version, round timer, both players' scores (addresses
  verified per version), FPS, frame time, input display.
- Windows x64 zip and Linux / Steam Deck AppImage.
