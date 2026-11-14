# Frontend

`frontend/` is the game-independent SDL2 frontend shared by the arcade
projects (Kaiser Knuckle, Dragon Blaze, …). A game plugs in through `fe_game`
(`frontend/fe.h`): ROM validation and loading, reset, DIP words, input bits,
one-frame execution with audio, rendering, state save/validate/load, ROM
fingerprint and optional verified gadgets. For Street Fighter II the frontend
runs a **suite**: one `fe_game` per version, grouped by `fe_suite`
(`ports/sdl2/main.cpp` is the adapter). The changes made for suites are listed
in [MODIFIED_FILES.md](MODIFIED_FILES.md).

| File | Contents |
| --- | --- |
| `fe.h` | public adapter interface (`fe_game`, `fe_suite`) |
| `fe_app.c` | window, loop, settings, menus, version selection, input, audio, states, rewind, movies, gadgets |
| `fe_draw.c` | embedded font atlas (Inter, OFL) and shapes |
| `fe_platform.c` | UTF-8 paths on Windows/POSIX, directory listing, atomic writes, INI, PNG, log |
| `fe_profile.c`, `fe_profile.h` | JSON game profiles (see below) |

The look is original (no game or third-party artwork): dark gradient
background, one accent colour, animated focus bar, a help line and a controls
legend, laid out for 1280×800 (Steam Deck) and 16:9. The header shows
"Street Fighter II · *version*".

## Versions

| Profile id | Version | MAME set | Accent |
|---|---|---|---|
| `sf2_world_warrior` | The World Warrior | `sf2` | Azure |
| `sf2_champion_edition` | Champion Edition | `sf2ce` | Amber |
| `sf2_hyper_fighting` | Hyper Fighting | `sf2hf` | Violet |

**Selecting.** Main menu > **Select Version** lists the three versions with
their state (*selected*, *ready*, *ROM not set*, *ROM not usable* and why).
**Start Game** and **Continue** always show which version they will use. The
selection is remembered (`[ui] version=` in `settings.ini`); `--version ID`
overrides it for one start. Choosing a version whose ROM set is missing or
invalid opens the ROM browser for *that* version — another version is never
used in its place.

**Independent configuration.** Per version: ROM path, DIP switches, save-state
slots and autosave, screenshots, input movies, and the accent colour (when
Display > Accent = *Per profile*, the default). Shared: display, audio,
controls, hotkeys and gadgets layout. Any configured version runs on its own;
the others may stay unconfigured.

**Switching while playing.** Quick menu > **Switch Version** (or Select
Version from the main menu during a game):

1. asks for confirmation (the default answer is *No*); the screen also offers
   **Save State First**, which opens the slot list of the running version;
2. ends the session — with an autosave when "autosave on main menu" is on
   (default);
3. stops the board and releases everything of the old version (the adapter's
   `unload()` frees the machine, ROM images and decoded graphics);
4. loads the new version's ROM set, DIPs and profile, applies its accent;
5. resets inputs (held buttons are ignored until released), flushes the audio
   queue, clears rewind history, stops any movie recording, and starts the new
   version's board from power-on.

Save states are bound to their version: the slot list only shows the running
version's files, and a state file from another version (e.g. passed with
`--state`) is refused with "state belongs to another game or version (*title*)".

## Menus

Main menu: Resume (during a game), Start Game / Start New Game (with the
version), Select Version, Continue (autosave of this version), Load State,
Save State (during a game), ROM Configuration, Display, Audio, Controller,
Gameplay Settings, Gadgets, Extras, About, Exit.

Quick menu (`Esc`, or Back + Start on a controller): Resume, Restart Current
Version, Save State, Load State, Display, Audio, Controller Configuration,
Gadgets, Extras, Switch Version, Return to Main Menu, Exit. The game is paused
while any menu is open, audio is paused and flushed, and game inputs are
cleared. Destructive actions ask first, with *No* preselected.

## Display

| Setting | Values | Default |
| --- | --- | --- |
| Mode | Windowed / borderless fullscreen | windowed (fullscreen on Steam Deck) |
| Window size | 1×–8× native height | 3× (reduced to fit the desktop) |
| Scaling | Fit / Integer | Fit |
| Aspect | Original 4:3 / square pixels | 4:3 |
| Filtering | Nearest / bilinear | Nearest |
| VSync | on / off | on |
| Frame pacing | Exact game rate (59.637 Hz) / Display sync (one frame per refresh on a 59–61 Hz display; about 0.6 % fast at 60 Hz) | Exact |
| Rotation | 0 / 90 / 180 / 270 | 0 (horizontal monitor) |
| Scanlines | 0–100 % | 0 |
| Phosphor mask | off / aperture grille / shadow mask / slot mask, strength | off |
| Brightness, gamma | 50–150 %, 0.50–2.00 | 100 %, 1.00 |
| Background | black / gradient / ambient glow | gradient |
| Accent | Azure, Amber, Crimson, Jade, Violet, Silver, Per profile | Per profile |

The board's picture is 384×224 pixels shown at 4:3, refreshed at
8 MHz / (512 × 262) = 59.637 Hz, the CPS-1 rate MAME uses. No widescreen or
stretch-to-fill mode is offered: the game draws a fixed 4:3 picture.
Curvature is not implemented (SDL_Renderer has no shaders).

## Audio

Master volume, mute, output device, latency target (16–250 ms). The board's
mono output (YM2151 + OKI MSM6295, mixed as MAME mixes them) is produced at
the YM2151's rate, 3.579545 MHz / 64 = 55,930 Hz, and resampled by SDL to the
output device. If the queue grows beyond twice the target after a stall, the
excess is dropped and counted rather than letting delay build up; underruns
and drops are shown in Audio > Diagnostics. Music and effects cannot be
separated: the game mixes them on the sound board.

## Gadgets

| Gadget | Source |
|---|---|
| Current Version | the running version's title |
| Round Timer | work RAM, BCD: `FF8ACE` (World Warrior), `FF8ABE` (Champion Edition, Hyper Fighting) |
| Score (Player 1) | work RAM, 4 BCD bytes: `FF8660` (WW), `FF8658` (CE, HF) |
| Score (Player 2) | work RAM, 4 BCD bytes: `FF8960` (WW), `FF8958` (CE, HF) |
| FPS, Frame time, Input display | frontend |

Each address was matched against the numbers the game itself draws, in MAME
0.264 runs of one- and two-player games of every version (the timer at 9
frames, Player 1's score at 7 and Player 2's at 3 per version). The last digit
of the stored score is the game's continue counter, which the HUD also
shows. Health bars, round wins and character ids are **not** offered: their
addresses were not verified, and a gadget showing wrong values is worse than
none. Each gadget can be enabled, anchored in one of six places, offset and
sized.

## Game profiles (JSON)

`profiles/<version id>.json` (three files) give each version its title,
subtitle, manufacturer and year, ROM hint, MAME set, player count, display
aspect, controls description, the frontend features it asks for, and its
accent (`theme.accent`). The frontend looks, in order, at `--profile FILE`,
`<data folder>/profiles/`, `profiles/` beside the executable, and
`../share/sf2/profiles/` (AppImage layout).

A profile never enables anything by itself: each requested feature is
combined with what the adapter implements, and the result is logged and shown
on the About screen. The SF2 profiles request save states, rewind, input
movies, screenshots, gadgets and input display; they set `nvram: false`
because the CPS-1 board has no non-volatile memory (settings are DIP
switches, high scores are lost at power-off, as on the cabinet).

## Extras

Rewind (bounded memory), autosave on exit / on return to the main menu,
pause when unfocused, record / play input movies, screenshot, data folder
location, reset all settings.

## Files

The data folder (`%APPDATA%\OmegaKatana\Street Fighter II\` on Windows,
`~/.local/share/OmegaKatana/Street Fighter II/` on Linux, or beside the
executable in portable mode) holds `settings.ini`, `frontend.log`, `states/`,
`screenshots/` and `movies/`. Set `FE_PROFILE=1` to log loop, present and
frame times every 120 frames.

## Command line

```
--version ID      sf2_world_warrior | sf2_champion_edition | sf2_hyper_fighting
--rom PATH        ROM zip or folder for that version (also: drop it on the window)
--fullscreen / --windowed
--scale N         window multiple
--window WxH      exact window size (1280x800 previews the Deck)
--portable        data beside the executable
--state FILE      load a save state at start (must belong to the version)
--no-menu         start the game immediately
--rotate N        picture rotation in quarter turns for this session
--frames N        quit after N frames (automation)
--screenshot FILE PNG of the last frame at exit
--profile FILE    use this profile instead of profiles/<id>.json
```
