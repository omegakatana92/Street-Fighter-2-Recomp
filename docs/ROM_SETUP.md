# ROM setup

You need your own ROM sets, in **MAME 0.264** format. None are included in the
source, the repository or any package, and the program never downloads,
copies or extracts them anywhere: it reads them from where you keep them.

| Version | MAME set | Files the program checks | Shares with |
|---|---|---|---|
| The World Warrior (World 910522) | `sf2.zip` | 8 main program, 12 graphics, 1 Z80, 2 OKI = **23** | OKI samples with the others |
| Champion Edition (World 920513) | `sf2ce.zip` | 3 main program, 12 graphics, 1 Z80, 2 OKI = **18** | graphics, Z80 program and OKI samples with Hyper Fighting |
| Hyper Fighting (World 921209) | `sf2hf.zip` | 3 main program, 12 graphics, 1 Z80, 2 OKI = **18** | graphics, Z80 program and OKI samples with Champion Edition |

All three are MAME *parent* sets, so each zip is complete on its own; no
parent zip is needed. MAME's sets also list PAL/GAL dumps (`buf1`, `ioa1`,
`prg1`, `rom1`, `sou1`, `stf29.1a` / `s9263b.1a`, `iob1…`, `bprg1.11d`,
`ioc1.ic7`, `c632.ic1`). They are not used by the board model (the mapper
logic they implement is in the code), so they may be present or absent.

Any one version is enough: a version without a ROM set simply shows
"ROM not set" in the version selector, and the others run normally.

## Choosing the files

Main menu > **ROM Configuration** lists the three versions with their status.
Pick a version and browse to:

- a **zip archive** (`sf2.zip`, `sf2ce.zip`, `sf2hf.zip`; the file name
  itself does not matter), or
- a **folder** with the unzipped files (choose "Use this folder" in the
  browser).

Paths may contain any Unicode characters (Windows paths are opened through the
wide-character API). Each version's path is stored separately in
`settings.ini` (`[paths] rom_sf2_world_warrior=…`, etc.).

You can also drop a zip on the window (it is assigned to the version being
configured, or the selected one), or start with
`--version sf2ce --rom /path/to/sf2ce.zip`.

On first start, the program looks for `sf2.zip`, `sf2ce.zip` and `sf2hf.zip`
next to the executable, in the data folder and its `roms/` subfolder, in
`~/Downloads`, in `~/roms` and in the current folder, and validates anything
it finds the same way. Nothing found means nothing configured; it never
guesses.

## What is checked

Every file is matched **by name, or by size and CRC32** (so a renamed file
is still found, as in MAME). A version is usable only if every one of its
files is present with the right size and CRC32. Otherwise the message names
what is wrong:

- *Missing:* the files that are not there;
- *Wrong size/CRC:* files that are there but different (a bad dump, another
  revision, a hacked set);
- **another version's set** in the wrong place, e.g. `sf2hf.zip` chosen for
  Champion Edition: "This holds Street Fighter II′: Hyper Fighting (MAME set
  'sf2hf'), not Champion Edition. Choose it under Hyper Fighting, or pick the
  'sf2ce' set here." The program **never substitutes one version for
  another**: the version you selected runs only from its own set.

Other regional or revision sets (`sf2ua`, `sf2ceua`, `sf2t`, …) are not
supported in 0.1.0 and are reported as incomplete or wrong.

## ROM fingerprint

After loading, the program computes a CRC32 over the assembled main program
(the *fingerprint*): The World Warrior `490A0CCA`, Champion Edition
`06D02579`, Hyper Fighting `572B5040` for the sets above. Save states,
screenshots and movies are filed under the version id and this fingerprint,
so files from different versions or revisions never mix.

## Assembly (as in MAME's ROM_START)

| Region | The World Warrior | Champion Edition / Hyper Fighting |
|---|---|---|
| main program (2 MB space) | 8 × 128 KB, byte-interleaved pairs (`ROM_LOAD16_BYTE`) | 3 × 512 KB, word-swapped (`ROM_LOAD16_WORD_SWAP`) |
| graphics (6 MB) | 12 × 512 KB, interleaved by 16-bit words in groups of four (`ROM_LOAD64_WORD`) | same |
| Z80 (64 KB ROM) | first 32 KB at 0, second 32 KB at 0x10000 (the banked window) | same |
| OKI samples (256 KB) | 2 × 128 KB | same |
