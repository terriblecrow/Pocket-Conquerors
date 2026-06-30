# Conway's Conquerors — Game Boy

A two-color Game of Life strategy game for the Nintendo Game Boy, by **[Terrible Crow](https://github.com/terriblecrow)**.

A complete game: a starfield title screen, lightly-decorated menus, vs-CPU and
2-player play (local hot-seat *or* head-to-head over the link cable), a
turn-based cell-placement mechanic, Conway evolution between turns, win
conditions, music and sound effects, and a Game Boy Color palette — plus a
purpose-built monochrome edition for original hardware.

Written in C and compiled with [SDCC](https://sdcc.sourceforge.net/) — **no
GBDK**. The startup code, cartridge header, and all hardware access are
hand-written.

---

## Play it

Two ROMs are provided — pick the one for your console:

| ROM | For | Color |
|-----|-----|-------|
| **[`conways-conquerors-gbc.gb`](conways-conquerors-gbc.gb)**         | Game Boy **Color** / Advance (also boots on a classic GB) | full color palette |
| **[`conways-conquerors-dmg.gb`](conways-conquerors-dmg.gb)** | original Game Boy / Game Boy **Pocket** | pure monochrome |

Load either in an emulator ([SameBoy](https://sameboy.github.io/),
[BGB](https://bgb.bircd.org/), [Emulicious](https://emulicious.net/),
[PyBoy](https://github.com/Baekalfen/PyBoy)) or flash it to a cartridge. Both are
32 KB MBC3 ROMs with battery-backed save RAM, so the **STATS** record persists
across power-offs (in an emulator, keep the generated `.sav`/`.ram` file).

Both editions get the same cartridge-cover title: a dark starfield with the
CONQUERORS wordmark and the two colony gliders sweeping in. **The two colonies
are distinguished by shape, not just color**, so they're easy to tell apart on
every Game Boy:

- **Blue** colony = a **solid filled block**
- **Red** colony = a **hollow diamond** with a thick outline

A filled square versus a hollow diamond reads instantly even on a low-contrast
Game Boy Pocket screen. On a Game Boy Color, each colony also gets its own vivid
color (electric blue vs. pure red) on top of the shape, and the three board zones
are tinted (blue home / green neutral / red home).

## How to play

You and an opponent share one board split into three zones: your home, a neutral
middle, and the enemy home. Each turn:

1. **Move the cursor** with the D-pad and **place up to 4 cells** with **A**,
   inside territory you're allowed to use.
2. Press **START** to end your turn. The board then **evolves one Conway
   generation** (B3/S23; births take the majority neighbour color), and play
   passes to the other side. **SELECT** ends the turn the same way but is a
   one-shot **SKIP** — each player gets exactly one per game (the CPU spends its
   skip only when every move it could make would lose ground).

**Territory rules:** you can always place in your home zone; in the neutral zone
once you have any live cell; in the enemy zone only once one of your cells has
been *born* there by an evolution.

**Scoring:** every live cell is worth one point, but one of **your** cells
standing in **enemy territory is worth double** — an incursion behind enemy
lines counts for two.

**Win** by wiping out the opponent (extinction) or by leading on score when
round 12 ends.

### Controls

| Context             | Button        | Action                       |
|---------------------|---------------|------------------------------|
| Menus               | D-pad / A / B | navigate / select / back     |
| Title / game over   | START         | continue                     |
| In game             | D-pad         | move cursor                  |
| In game             | A             | place a cell                 |
| In game             | SELECT        | skip turn (once per game)    |
| In game             | START         | end turn (evolve)            |

### Menus

- **VS CPU** — pick Easy / Normal / Hard, then play against the AI.
- **2 PLAYERS** — opens a submenu:
  - **Same Game Boy** — hot-seat on one console, taking turns.
  - **Link: Host** — host a cable game (you play **Blue** and move first).
  - **Link: Join** — join a hosted cable game (you play **Red**).
- **HOW TO PLAY** — the rules, on-screen.
- **STATS** — lifetime record kept in the cartridge's battery-backed save RAM,
  so it survives power-off: games played / wins / losses / draws vs the CPU, and
  Blue-won / Red-won tallies for 2-player games. Hold **SELECT** on this screen
  to reset the record.
- **CREDITS**

### Link cable (2 players, two Game Boys)

Head-to-head on two consoles connected by a Game Boy Link Cable. It works on
the original **Game Boy / Pocket (DMG)** and the **Game Boy Color**, and the two
units can be mixed (a DMG can play a GBC). The serial link runs at normal clock
speed for maximum compatibility.

To start a game:

1. Connect the two consoles with the link cable **before** entering the lobby.
2. On one console, choose **2 PLAYERS → Link: Host**. It plays **Blue** and
   moves first.
3. On the other, choose **2 PLAYERS → Link: Join**. It plays **Red** and waits.
4. On the host, press **A** to connect. Once the handshake succeeds, both drop
   into the game.

Turns alternate just like the local game: the active player places up to four
cells and ends the turn; the move is sent to the other console, the board
evolves identically on both, and play passes to the other player. Because the
evolution is deterministic, only the placements travel over the wire — never the
whole board.

If the cable is unplugged or the partner stops responding, the game ends with a
**CABLE LOST** screen rather than freezing. Press **B** in the lobby to cancel.

---

## Build it yourself

You need **SDCC** (it includes the `sm83` Game Boy target plus `sdasgb`,
`sdldgb`, and `makebin`):

```bash
# Debian / Ubuntu
sudo apt-get install sdcc

# macOS
brew install sdcc

./build.sh        # compiles src/ into ./conways-conquerors-gbc.gb AND ./conways-conquerors-dmg.gb
```

The build script builds **both** ROMs from the one source tree: the color build
(`conways-conquerors-gbc.gb`) and the monochrome build (`conways-conquerors-dmg.gb`, compiled with `-DBUILD_DMG`,
which compiles out every color path and sets a DMG-only cartridge header). Both
are stamped as **MBC3 with battery-backed RAM** (`makebin -yt 0x13 -ya 1`) so the
STATS save persists. Intermediate artifacts land in `build/` (git-ignored); the
finished ROMs are written to the repo root.

### One source, two builds

Color and monochrome share `src/main.c`. The single switch is `is_gbc()`: in the
DMG build it's hard-wired to `0`, so every `if (is_gbc()) { … }` color block
becomes unreachable and is never executed. There's no second copy of the game to
keep in sync — a fix in `main.c` lands in both ROMs at once.

## Project layout

```
.
├── conways-conquerors-gbc.gb              color ROM — Game Boy Color / Advance (boots on DMG too)
├── conways-conquerors-dmg.gb          monochrome ROM — original Game Boy / Pocket
├── build.sh             the build script (builds both ROMs with SDCC)
├── .gitignore           ignores build/, emulator saves, the ai_sim binary
├── src/
│   ├── main.c           the whole game: state machine, board, rules, AI, screens, stats
│   ├── link.h           serial / link-cable driver and the 2-player wire protocol
│   ├── sound.h          square-wave SFX + the music driver (original theme / Chopin)
│   ├── hardware.h       Game Boy registers + MBC3 SRAM control for the save
│   ├── font.h           a minimal 8x8 font (A–Z, 0–9, symbols)
│   ├── crow.h           the Terrible Crow crow logo bitmap
│   ├── crt0.s           hand-written Game Boy startup + cartridge header
│   └── main_proto_backup.c   the earlier automaton-only prototype, for reference
├── docs/
│   └── CHANGELOG.md     the version-by-version history
├── tools/
│   ├── ai_sim.c         desktop harness that measures AI difficulty strength
│   └── README.md        how to use the harness
└── .github/workflows/
    └── build.yml        CI: builds both ROMs (and runs the AI regression) on every push
```

## How it works

The DMG background map is 32×32 tiles; the visible window is 20×18. The board is
one cell per background tile. Rendering is tile-based — each cell state maps to a
tile, and on Game Boy Color each tile also gets a palette attribute from VRAM
bank 1.

**The title screen** uses a dark starfield. On Game Boy Color that comes from a
dark palette; on a DMG, which has no per-tile palettes, the title flips the global
BG palette to an inverted ramp so the field goes dark and the text, gliders and
stars glow on top — then restores the normal palette on the way into the menus and
game. The white menu/text screens get a light touch of decoration (an underline
rule under each heading and small star accents) so they frame the text without
clutter.

**VRAM timing** is the tricky part. On real hardware, VRAM can only be written
during VBlank (or with the LCD off); writes during active drawing are silently
dropped. Full-board redraws build tile/color buffers in RAM, then blit them with
the LCD off for the shortest possible window — synced to VBlank so the blank is
barely perceptible, with a soft "evolution tick" sound fired right at the blank so
the flash reads as a pulse of the simulation rather than a glitch. Small updates
(cursor moves, single placements, the crow's blink) fit inside a single VBlank
with the LCD on, so they never flash.

**Saving.** The cartridge is an MBC3 with battery-backed RAM, and the lifetime
statistics live in the first few bytes of that save RAM behind a small `'TC'`
signature (so a fresh, garbage-filled save is recognised and zeroed on first
boot). Counters are 16-bit and the RAM window is only enabled around the brief
load/save, then disabled again, which is the usual guard against corrupting the
save if the console loses power mid-write.

### The AI

The AI scores candidate placements by their impact on the board (net
own-vs-enemy population, plus its own growth), placing up to 4 cells per turn near
live cells. The three difficulties are genuinely ordered:

- **Easy** — heavy scoring noise and a slightly slower tempo (sometimes places
  one fewer cell), so it frequently makes sub-optimal moves but can still win.
- **Normal** — a light noise term over a clean 1-generation evaluation.
- **Hard** — a true 2-generation lookahead (it *replaces* the shallow score
  rather than adding to it, so the deeper horizon actually drives the choice),
  biased slightly toward immediately solid plays.

Normal and Hard also hold the one-per-game **skip**: if every legal placement a
turn offers would lose ground (best impact below zero), the AI passes that turn
instead of being forced into a bad move. Easy never skips, so beginners aren't
out-thought.

Measured over ~800-game round-robins (sides swapped to cancel first-move bias):
Hard beats Normal ~56%, Normal beats Easy ~65%, Hard beats Easy ~68% — a clear
ladder where no level wins or loses every game. See `tools/ai_sim.c`.

## Notes & limitations

- The CPU takes a beat to think on each of its turns (a "CPU" label shows
  top-right). This is the AI running on a 4 MHz CPU; it's playable but not
  instant.
- 2-player can be hot-seat on one console or head-to-head over the link cable
  (see [Link cable](#link-cable-2-players-two-game-boys) above).
- The link protocol is verified in simulation against the real driver code, but
  has not yet been exercised on physical hardware. If you test it on two
  consoles (or two linked emulator instances) and hit a hang or desync, the most
  likely knobs are the handshake retry and the per-byte timeout budget
  (`LINK_BYTE_SPINS` in `src/link.h`).

## License

Released under the [MIT License](LICENSE). The Game Boy is a trademark of
Nintendo; this is an unofficial homebrew project, not affiliated with or endorsed
by Nintendo.

The title carries an **original, somber A-minor theme** written for the Game
Boy's two square-wave channels — a melody over a walking bass line, with a clear
8-bar harmonic arc (Am · F · C · G · Am · F · E · Am) that swells to a peak and
resolves through an E-dominant cadence. It's quantised to a strict 4/4 grid so
the loop never drifts, and each cartridge gets its own timbre (a fuller pulse on
the GBC, a leaner one on the DMG) from the same shared note data. The loss cue
is a rendition of a public-domain classic, Chopin's *Marche funèbre*, arranged as
two voices for the same channels.
