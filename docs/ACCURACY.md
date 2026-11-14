# Accuracy

The port was compared with **MAME 0.264** running the same ROM sets with the
same inputs. MAME ran `tools/mame_reference.lua`, which feeds scripted inputs
and writes a RAM image every 10 frames (68000 work RAM, graphics RAM, Z80 RAM)
and a screenshot every 300 or 600 frames; `sf2_headless --compare` runs the
port with the same inputs and compares (see BUILD_LINUX.md). The figures
below are from the Linux release build of 0.1.0 (zig/clang); the development
build (GCC) gives identical results.

## Runs

| Script | Frames | What happens |
|---|---|---|
| attract | 3,600 (1 min) | power-on, warning screen, title, attract demo fights, no input |
| 2-player | 7,200 (2 min) | coins and both starts, character selection, a versus fight with scripted random inputs for both players |
| 1-player | 9,000 (2.5 min) | coin and start, character selection, fights against the computer with random inputs |

Each script was run on all three versions: 9 runs, 6,750 compared RAM images,
162 compared screenshots.

## Results

| | The World Warrior | Champion Edition | Hyper Fighting |
|---|---|---|---|
| 68000 work RAM (64 KB) | identical at all 2,250 frames | identical at all 2,250 | identical at all 2,250 |
| graphics RAM (192 KB) | identical at all 2,250 | identical at all 2,250 | identical at all 2,250 |
| screenshots (384×224) | 54 of 54 pixel-identical | 54 of 54 | 54 of 54 |
| Z80 RAM (2 KB) | differs at 532 of 2,250 frames | at 335 | at 680 |

So everything the game logic does — positions, health, timers, scores, AI
decisions, random numbers, every sprite and layer — matches MAME exactly over
these runs, including the attract demo and two-player fights.

### The Z80 difference

Where Z80 RAM differs, it is by 1–12 bytes (usually 2–4), mostly in the
sound driver's stack and work variables near the top of its RAM (e.g. `D7E7`,
`D7EF`, `D7FB`), and the values are ones MAME has a moment earlier or later. Traced cause: the Z80 accepts some YM2151 timer
interrupts 7 Z80 cycles (≈2 µs) later than in MAME, which shifts where the
sound program happens to be when the RAM image is taken. The YM2151 core
(ymfm), its timer arithmetic, the scheduler and the stream counts were each
ruled out; the remaining suspect is a one-cycle difference in how the Z80
core's cycle budget is reconciled with the scheduler. It does not reach the
68000 side (the 68000 only writes the sound latches; the board has no path
back), which is why main and graphics RAM stay identical.

## Save states

`--state-test` at frame 3,000 of the 2-player run and 5,000 of the 1-player
run, all versions: after saving, running 300 frames with other inputs and
loading, the run continues identical to MAME (main and graphics RAM) to the
end. See SAVE_STATES.md.

## Audio

MAME 0.264 `-wavwrite` recordings (61 s of attract mode, `-nocompressor`,
48 kHz) against the port's output (resampled from 55,930 Hz), for The World
Warrior and Champion Edition:

| | The World Warrior | Champion Edition |
|---|---|---|
| level ratio, every second with sound (45 s) | 0.9995–1.0018 | 0.9999–1.0018 |
| waveform correlation | 0.9966 | 0.9973 |
| residual (RMS of difference / RMS of MAME) | 8.3 % | 7.4 % |
| alignment | 2 samples at 48 kHz | 2 samples |

The residual is mostly the resampling (MAME's 48 kHz resampler against a
linear resampler in the comparison script) plus the Z80 timing difference
above, which can move a note by microseconds. Hyper Fighting uses the same
sound board and program as Champion Edition and was not recorded separately.

## Speed

Headless, one core of the test machine: The World Warrior 1,783 frames/s
without rendering, 1,512 with rendering (about 25× real time); Hyper Fighting
1,456 / 1,307. Not measured on a Steam Deck.

## What was not compared

- Physical controllers, real Windows PCs and the Steam Deck (not available).
- Long sessions beyond 2.5 minutes, the service menu, every character and
  stage, and the endings.
- The frontend's own output (scaling, CRT effects) — it is a presentation
  layer over the frames above.
