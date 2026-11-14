# Controls

All three versions use the same cabinet controls: per player an 8-way stick
and **six buttons** — three punches (top row) and three kicks (bottom row) —
plus Start and Coin (MAME's `sf2` input definition, the same for `sf2`,
`sf2ce` and `sf2hf`). Both players are fully supported on keyboard and
controllers.

Everything can be remapped in **Controller Settings** (two keyboard keys and
one controller button/axis per input), with "restore defaults". Controllers
are hot-plugged. Mappings are shared by the three versions (the cabinet is the
same); DIP switches are kept per version (see below).

Menu input is never passed to the game: while a menu is open the game is
paused, and buttons held when it closes are ignored until released.

## Default keyboard

| Input | Player 1 | Player 2 |
|---|---|---|
| Up / Down / Left / Right | Arrow keys | I / K / J / L |
| Light Punch (Jab) | A | R |
| Medium Punch (Strong) | S | T |
| Heavy Punch (Fierce) | D | Y |
| Light Kick (Short) | Z | F |
| Medium Kick (Forward) | X | G |
| Heavy Kick (Roundhouse) | C | H |
| Start | Enter (or 1) | 2 |
| Coin | 5 | 6 |

Cabinet: Service coin `9`, Service Mode (test switch) `F2`.

Frontend hotkeys (remappable under Controller Settings > Hotkeys): `Esc` quick
menu, `F5` quick save, `F9` quick load, `F6` / `F7` previous / next slot,
`F12` screenshot, `F11` fullscreen, `Backspace` (hold) rewind, `Pause` or `P`
pause, `F10` FPS counter, `F3` reset.

## Default controller (Xbox / PlayStation / Steam Deck / arcade stick)

Each player uses the controller assigned under Controller Settings > Player
Controllers (automatic: 1st controller = P1, 2nd = P2).

| Input | Button | |
|---|---|---|
| Move | D-pad (left stick also works) | |
| Light / Medium / Heavy Punch | X / Y / RB | Square / Triangle / R1 |
| Light / Medium / Heavy Kick | A / B / RT | Cross / Circle / R2 |
| Start | Start / Menu | Options |
| Coin | Back / View | Share |
| Quick menu | Back + Start held together (configurable) | |

This is the usual "punches on top, kicks below" layout of a six-button pad.

## Opposing directions

A real 8-way stick cannot press left and right (or up and down) at the same
time. As in MAME (whose default is to disallow "contradictory" joystick
input), the frontend cancels both directions of a pair while both are held,
for each player. Keyboard users therefore cannot send an impossible input.

## Service mode (Test Menu)

Press `F2` while the game is running, or set **Gameplay Settings > Game
Mode = Test** (the cabinet's DIP switch C-8), and the board shows Capcom's
*TEST MENU* (Color, DIP Switch, I/O Test, Dot Cloth, Character…). Both ways
were checked to reach the menu in headless runs; the menu itself was not
walked through or compared with MAME.

## DIP switches (Gameplay Settings)

Each version keeps its own DIP settings (`[gameplay.<version id>]` in
`settings.ini`). The defaults are MAME's.

| Setting | Values | Default |
|---|---|---|
| Coin A, Coin B | 4 coins/1 credit … 1 coin/6 credits | 1 coin / 1 credit |
| 2 Coins to Start, 1 to Continue | Off / On | Off |
| Difficulty | 0 (easiest) … 7 (hardest) | 3 (Normal) |
| Free Play | Off / On | Off |
| Freeze | Off / On | Off |
| Flip Screen | Off / On | Off |
| Demo Sounds | Off / On | On |
| Allow Continue | No / Yes | Yes |
| Game Mode | Game / Test | Game |

Changes are applied when the game is next started or restarted; while a game
is running, **Restart Now to Apply** does that (after asking). A save state
carries the switch settings it was made with.
