# Steam Deck

Artifact: `StreetFighterII-SteamDeck-x86_64.AppImage` (native Linux, no
Proton). It contains no ROM data.

**Status: built, and run under Xvfb on a Linux PC; not yet run on a Steam Deck.**

## Install

1. Desktop Mode: copy the AppImage and your ROM zips into one folder, e.g.
   `~/Games/StreetFighterII/`. Right-click the AppImage > Properties >
   Permissions > "Is executable" (or `chmod +x`).
2. Run it once from Desktop Mode. Open ROM Configuration and pick each
   version's zip (touchscreen, trackpad or controller). Versions you do not
   own can stay "not set".
3. Steam > Add a Non-Steam Game > browse to the AppImage. Optional launch
   options, e.g. `--version sf2_champion_edition` to always start with
   Champion Edition.
4. Launch from Game Mode.

## Behaviour on the Deck

- Starts in borderless fullscreen when the `SteamDeck` environment variable is
  present (Steam sets it); menus are laid out for 1280×800.
- The game is 4:3: on the Deck's 16:10 screen it has narrow side borders
  (Display > Background: gradient or ambient glow fills them).
- Game Mode: gamescope provides XWayland, so `AppRun` selects SDL's X11
  driver there; Desktop Mode leaves SDL's default.
- Controls arrive as a standard controller through Steam Input: X / Y / R1 =
  light / medium / heavy punch, A / B / R2 = light / medium / heavy kick,
  View = coin, Menu = start, View + Menu = quick menu (Switch Version is
  there). Player 2 uses the next controller. The test switch is on the
  keyboard (`F2`) or can be mapped to a spare button. The Steam button stays
  with Steam.
- Settings, saves, screenshots and movies:
  `~/.local/share/OmegaKatana/Street Fighter II/` (outside the read-only
  AppImage, kept across updates).
- Audio goes through PulseAudio/PipeWire; default latency target 64 ms.
- The game runs at 59.637 Hz. Frame pacing = Exact (default) keeps that rate
  on the 60 Hz panel; Display sync runs one frame per refresh, 0.6 % fast.

## Packaging details

The binary links SDL2 2.30.9 statically and needs glibc ≥ 2.29 only; SDL
loads X11/Wayland/PulseAudio/ALSA from SteamOS at run time. See
`docs/BUILD_LINUX.md` ("Release build") and `scripts/build-appimage.sh`.

## Things to check on real hardware

- Speed: the board needs about 4 % of one desktop core in real time
  (1,500 frames/s headless on the build machine); the Deck was not measured.
- Game Mode launch, fullscreen, focus when the Steam overlay opens and closes
  (focus loss pauses by default).
- The built-in controls, the R2 trigger as heavy kick, an external pad as
  Player 2.
- Suspend/resume, then the quick menu.
