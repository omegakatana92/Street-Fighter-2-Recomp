# Hardware model

All three versions run on Capcom's **CPS-1** (CP System) board. The model
follows MAME 0.264 (`src/mame/capcom/cps1.cpp`, `cps1_v.cpp`) and is built
from MAME's own cores where MAME has them.

| Part | Here | MAME 0.264 source |
|---|---|---|
| Main CPU | Motorola 68000, **10 MHz** (`sf2`) or **12 MHz** (`sf2ce`, `sf2hf`) — MAME's cycle-exact core | `cpu/m68000/m68000*.cpp` |
| Sound CPU | Zilog Z80, 3.579545 MHz — MAME's core | `cpu/z80/z80.cpp` |
| FM | Yamaha YM2151, 3.579545 MHz (55,930 Hz output) — ymfm, as in MAME | `sound/ymopm.cpp`, `3rdparty/ymfm` |
| ADPCM | OKI MSM6295, 1 MHz, pin 7 high (7,575 Hz) | `sound/okim6295.cpp` |
| Video | CPS-A + CPS-B: three scroll layers, sprites, layer priority masks, palette with brightness | `capcom/cps1_v.cpp` |
| CPS-B | **CPS-B-11** with the **STF29** graphics mapper (`sf2`); **CPS-B-21** (default configuration) with the **S9263B** mapper (`sf2ce`, `sf2hf`) | `cps1_v.cpp` config table |
| Screen | 8 MHz pixel clock, 512 × 262 total, 384 × 224 visible (x 64–447, y 16–239): 59.637 Hz | `cps1.cpp` `set_raw` |
| Scheduler | MAME's timeslice scheduler: attotime, timers, `synchronize()`, input-line synchronisation | `emu/schedule.cpp`, `diexec.cpp` |

## Main CPU map

| Address | |
|---|---|
| `000000–3FFFFF` | program ROM (2 MB used) |
| `800000–800007` | IN1: both players' stick and punches |
| `800018–80001F` | IN0 (coins, starts, service, test), DIP switches A, B, C (high byte) |
| `800030–800037` | coin control: coin counters and **coin lockouts** (applied to the coin inputs as MAME's `-coin_lockout` does) |
| `800100–80013F` | CPS-A registers |
| `800140–80017F` | CPS-B registers; the kick buttons of both players (IN2) are read through CPS-B |
| `800180`, `800188` | sound latches 1 and 2, written through `synchronize()` like `generic_latch_8` |
| `900000–92FFFF` | graphics RAM (layers, sprites, palette, and code that Champion Edition copies there and runs) |
| `FF0000–FFFFFF` | work RAM |

Interrupts: IRQ 2 (VBLANK, asserted at the start of line 240 and cleared by
the interrupt acknowledge, which also clears IRQ 4), autovectored, as in
MAME's `cpu_space_map` for CPS-1.

## Sound CPU map

| Address | |
|---|---|
| `0000–7FFF` | ROM |
| `8000–BFFF` | banked ROM, 16 KB pages from `0x10000` |
| `D000–D7FF` | RAM |
| `F000–F001` | YM2151 |
| `F002` | OKI MSM6295 |
| `F004` | bank select |
| `F006` | OKI pin 7 (sample rate) |
| `F008`, `F00A` | sound latches 1 and 2 |

The YM2151's interrupt drives the Z80 IRQ; its timers, mode writes and
interrupt updates go through the scheduler exactly where MAME's
`ym_generic_device` uses timers and `synchronize()`.

## Audio mix

As MAME mixes the `cps1_12MHz` sound section: both YM2151 outputs at 0.35
and the OKI at 0.30 into one mono speaker. Each stream advances on its own
device clock (MAME's `time_to_buffer_index` with rounding up), and the OKI is
delayed by MAME's resampler latency (one OKI sample minus two YM samples) so
that the two line up as in a MAME recording.

## Differences between the versions

| | The World Warrior | Champion Edition | Hyper Fighting |
|---|---|---|---|
| 68000 clock | 10 MHz | 12 MHz | 12 MHz |
| Program | 8 × 1 Mbit EPROMs, byte pairs | 3 × 4 Mbit, word-swapped | 3 × 4 Mbit, word-swapped |
| CPS-B / mapper | CPS-B-11 / STF29 | CPS-B-21 / S9263B | CPS-B-21 / S9263B |
| Graphics, Z80 program | own | shared with HF | shared with CE |
| OKI samples | same ROMs in all three | | |
| Inputs, DIP switches | identical (MAME's `sf2` input ports) | | |

The CPS-B model's differences (ID register, layer-enable and priority
register positions) come from MAME's configuration
table; the mapper decides which graphics ROM ranges each layer may use.
