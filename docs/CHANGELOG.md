# Changelog

History of the game, version by version.

## v21 — link cable working on real hardware, vs-CPU coin toss, UI polish, theme tweak

The big one: **link-cable multiplayer (introduced in v18) now actually works on
two physical Game Boys.** It connected and took one move before, then broke on
turn two. The root cause was timing, not the handshake: the clock master was
sending bytes back-to-back faster than the slave could re-arm between transfers,
so bytes dropped or duplicated. The fix (in `src/link.h`) adds a deliberate
inter-byte delay on the master side (`LINK_BYTE_DELAY`) and a per-move rendezvous
so both consoles wait for each other with matching patience. Verified end-to-end
on a real Game Boy Color hosting and a Game Boy Pocket joining, across a full
multi-turn match. Because the board evolution is deterministic, still only the
placements travel over the wire.

**vs-CPU coin toss.** A coin toss now decides who opens each round against the
CPU. You're always BLUE, but the CPU (RED) may move first. Both orderings give
each side the same number of turns over a game, so the toss only sets who goes
first — never who gets an extra move. Seeded from `rDIV` and stirred with menu
input, so it's unpredictable. (`start_game`/`next_turn`/`cpu_take_turn` in
`src/main.c`.)

**Pause menu & rematch.** START opens a RESUME / QUIT TO MENU pause in vs-CPU and
local hot-seat games (disabled over the cable, where a pause would desync). The
game-over screen gains a one-press **rematch** (A). A **THINKING** label shows on
the HUD while the CPU computes a move.

**Screen layout polish.** The whole frame is nudged down 1px for breathing room
above the HUD (the title is excluded so its full-bleed field still reaches the
edge), and the play grid is now vertically centered between the top and bottom
rules via dedicated border tiles (`T_HBART`/`T_HBARB`) that also fill the bottom
screen edge cleanly.

**HOW TO PLAY is now two pages.** Rules on one, controls on the other, A to
advance and B to step back, with proper spacing instead of a wall of text. A
separate page counter keeps the menu cursor where you left it on exit.

**New ACKNOWLEDGMENTS screen** (`ACK` in the menu) with a dedication and
signature; fixed a corner-diamond decoration overlapping the full-width title.
**CREDITS** reflowed for legibility — `CODE:` is now `AUTHOR:`, and the
location/year line is separated from the name. The 2 PLAYERS and DIFFICULTY
screens were rebalanced (footer rule now sits above `B: BACK`, consistent with
the others).

**Title theme tweak.** The v19 A-minor theme is kept, but recolored toward Am9:
bar 2 is now an F arpeggio with a melodic turn (`F4 A4 G4 F4 E4`) and bar 4
resolves home via a `B3→A3` leading tone instead of hanging on a suspension.
Every bar still sums to 128 frames, so the loop can't drift.

Both ROMs rebuilt (GBC `life.gb` + DMG `life-dmg.gb`), 32 KB MBC3+RAM+BATT,
header and global checksums verified.

## v20 — enemy-territory scoring, persistent stats, one-shot skip

Three gameplay/feature changes, plus a cartridge change to support saving.

**Double score behind enemy lines.** Every live cell is worth one point, but one
of your own cells standing in **enemy territory now counts double**. The on-board
HUD and the game-over screen show this weighted score (3-digit fields, since the
total can pass 99), and the round-12 winner is decided by it. Raw cell counts
are still used for extinction detection. The rule is spelled out on the HOW TO
PLAY screen (`YOUR CELL IN ENEMY LAND SCORES X2`). See `score_cells()` in
`src/main.c`.

**Persistent STATISTICS screen.** A new **STATS** entry on the main menu opens a
lifetime record: vs-CPU games played / wins / losses / draws, and Blue-won /
Red-won tallies for 2-player games. The counters live in the cartridge's
battery-backed SRAM behind a `'TC'` signature (so fresh, garbage-filled save RAM
is detected and zeroed on first boot), stored as 16-bit values, with the RAM
window enabled only around the brief load/save. Hold **SELECT** on the screen to
wipe the record. To make this possible the cartridge header is now **MBC3 with
battery RAM** (`makebin -yt 0x13 -ya 1` in `build.sh`); `src/hardware.h` gained
the MBC3 control registers.

**Skip is now one-per-game.** Previously SELECT and START both ended the turn
with no limit. START still ends a turn freely; **SELECT is now a one-shot SKIP**,
exactly one per player per game (the board still evolves). The CPU gets the same
single skip and spends it only when every legal move it has would lose ground —
Easy never skips, so beginners aren't out-thought. The HOW TO PLAY controls now
read `START: END TURN` / `SEL: SKIP (1/GAME)`. The AI change is mirrored in
`tools/ai_sim.c`; the difficulty ladder stays ordered (re-verified).

The title music (v19) and the loss cue are untouched.

## v19 — title music: original theme, in time and in tune

A full rework of the looping title music. The old cue was a *Gymnopédie* rendition
whose phrases didn't quite connect; it's been replaced with an **original A-minor
theme** built for direction and coherence rather than drift.

What changed, in `src/sound.h`:

- **Clear harmonic arc.** Eight bars — Am · F · C · G · Am · F · E(dominant) · Am —
  so the loop travels somewhere and comes home. The line is built from one motif
  that's transformed across the phrases (a through-line instead of unrelated
  fragments), swelling to a sustained A4 peak in bar 3 and resolving through a
  G&#9839;→A leading tone over the E-dominant cadence.
- **Walking bass.** Channel&nbsp;1 now plays chord roots with passing tones that step
  between them, turning over in time with the melody, instead of a static drone.
- **A dynamic emphasis curve.** A new per-note volume table (`GYMNO_V[]`) makes
  the theme breathe — soft statement, a swell into the peak, a settle, then a
  build back into the cadence — rather than playing at one flat level.
- **Strict 4/4, no drift.** Earlier passes used free durations that didn't line up
  to a bar, so the pulse slipped at the first bar line. Every note is now on a
  4/4 grid (quarter = 32 frames, ~112&nbsp;BPM) and every bar sums to exactly 128
  frames, verified, so the loop stays locked in time.
- **Per-cartridge timbre, one melody.** The GBC and DMG builds share a single note
  table and differ only in sound palette (a fuller 50%/25% pulse on the GBC, a
  leaner 25%/12.5% one on the DMG) via a `BUILD_DMG` switch, so the two can't
  drift apart.

The driver in `src/main.c` (`gymno_tick`) reads the new volume table per note;
nothing else in the game changed. The loss cue (Chopin's *Marche funèbre*) is
untouched.

## v18 — link cable two-player (DMG + GBC)

Head-to-head over the Game Boy link cable, on top of the existing alternating
turn flow. New "2 PLAYERS" submenu: *Same Game Boy* (the old local hot-seat),
*Link: Host* and *Link: Join*. The host plays BLUE and moves first, the joiner
plays RED; a short lobby runs a handshake, then both consoles step through the
game exchanging only their placements each turn (the deterministic board
evolution runs identically on both, so the whole board never needs sending).

Implementation in `src/link.h`: a small, interrupt-free serial driver that polls
the SC transfer bit, works on DMG/Pocket and GBC at normal clock speed (most
compatible, and required for DMG<->GBC), and bounds every wait with a timeout so
a yanked cable surfaces a "CABLE LOST" game-over instead of hanging. The wire
protocol is fully *symmetric*: both consoles call the same `link_swap_move()`
the same number of times in the same order — the active player passes a real
move, the waiting player an empty one — so the two streams cannot slip out of
phase. Verified end-to-end against the real `link.h` code in a two-process host
harness (each process holding its own master/slave state, like two real units)
across a full multi-turn game with no desync.

## v17 — AI tuning pass for beta

A stability/QA pass on the CPU AI ahead of the beta. Extended the offline harness
(`tools/ai_sim.c`) with new checks: each level was played against a random-legal
opponent (a proxy for a novice) and instrumented for placement efficiency. Key
findings — the AI never lets itself be wiped out early (0 extinction losses), it
leads on population every round (avg ~33 vs ~21 cells), and it never skips a turn
or wastes a placement (0 skipped turns in 300 games). An experimental
global-population-awareness term was prototyped but measured equivalent in
head-to-head play, so it was left out to keep the AI simple and fast on the 4&nbsp;MHz
CPU. The one shipped change: **Easy is slightly weaker** (a touch more scoring
noise, and it drops a cell a bit more often) so beginners win a fairer share and
the gap between Easy / Normal / Hard reads more clearly.

## v16 — credits: location & year

The credits now read **CODE: AGUSTIN 'TANO' MATTIOLI / ARGENTINA, 2026** on both
the color and monochrome builds. A comma glyph was added to the font to set it
(the font grew from 48 to 49 glyphs); the crow and big-wordmark VRAM tile pools
were shifted up to make room, with no visible change to the title.

## v15 — two-voice music, richer menus, credit update

**Music.** The title theme and the funeral march were upgraded from a single
flat melody line to **two voices**: the melody on channel 2 plus a soft
bass/accompaniment voice on channel 1 (25% duty, sweep off) holding the harmonic
root under each phrase. Notes are now re-triggered explicitly so repeated notes
sound as separate pulses instead of one held tone, and each note has a gentle
decay envelope rather than a flat blare. The bass sits well under the melody
(melody/bass energy ratio ~7:1) so the tune still sings on top. Verified by
capturing the emulator's audio and confirming two simultaneous voices in the
spectrum.

**Menus.** More drawn detail in black on the white screens: each heading now
gets an ornamented rule (a diamond terminator on each end of the underline bar),
small diamond bullets beside menu options, diamond accents framing the screen,
and a decorative bar across the bottom — on both the color and monochrome builds.

**Credits.** "Agustin Mattioli" is now "Agustin 'Tano' Mattioli".

## v14 — monochrome cartridge-cover title + decorated menus

The DMG title screen now gets the same "real game cover" treatment as the color
build. Before, the monochrome title sat on a flat white background; now it uses a
**dark starfield**:

- On a DMG there are no per-tile palettes, so the title flips the global BG
  palette to an inverted ramp — the field goes dark and the CONQUERORS wordmark,
  the colony gliders, and the stars glow bright on top. The normal palette is
  restored on the way into the menus and the game, so nothing else is affected.
- The starfield itself was reworked (two scattered star tiles alternated across
  the screen) so it reads as a deep field rather than a regular dot grid — this
  also improves the color title.

The white menu / text screens got a **light touch of decoration**: a thin
underline rule under each heading and small star accents in the corners, so they
frame the text without clutter. Applies to the menu, difficulty, help, credits,
and game-over screens, on both builds.

## v13 — stronger cell contrast on every Game Boy

The two colonies are now distinguished by **shape**, not fill density alone, so
they're unmistakable on a monochrome DMG / Pocket as well as on a Game Boy Color:

- **Blue** = a solid filled block.
- **Red** = a **hollow diamond** with a thick outline (was a faint gray dither).

A filled square vs. a hollow diamond reads instantly even on a low-contrast
Pocket screen — verified by rendering the actual tile data and by emulator
capture. The cursor-on-cell variants were redrawn to match.

On Game Boy Color the colony palettes were pushed to vivid, well-separated hues
(electric blue vs. pure red): the distance between the two colony inks roughly
doubled, so they stay distinct even on a dim screen.

## v12 — monochrome (DMG) build + AI difficulty rebalance

Added a pure-monochrome build for the original Game Boy / Pocket alongside the
color build, both from the same `src/main.c` (the DMG build hard-wires
`is_gbc()` to `0`). And fixed the AI difficulties, which weren't actually ordered
by strength — the old "Hard" was the weakest of the three. Reworked so Hard does
a true 2-generation lookahead, Easy is reliably beatable, and the ladder is
clean: Hard > Normal (~56%), Normal > Easy (~62%), Hard > Easy (~66%). Added
`tools/ai_sim.c`.

## v11 — evolution tick

The brief screen flash on every board redraw now has a soft sound tied to it,
fired from inside the LCD-blank routine exactly when the screen goes dark, so the
flash reads as a pulse of the simulation rather than a glitch.

## v10 — Game Boy Color + title fix

Fixed the title wordmark's N glyph, and made the cartridge GBC-aware (CGB flag,
still boots on a classic GB): colored board zones, a gold title wordmark, colored
gliders, clean black-on-white menus, with color attributes reset per screen.

## v9 — elaborate title screen

A proper logo screen: a big CONQUERORS wordmark (the 8×8 font scaled 2×), the
blinking crow flanked by the two colony gliders, CONWAY'S above, the studio name
below, and twinkling stars and sparkles.

## v8 — distinct place blip + richer intro loop

A short, thin, high blip on placement (distinct from START), and a fuller,
longer title melody so the intro feels less repetitive.

## v7 — better SFX + Satie on the title

A crisp chime on valid placement, a harsh buzz on an illegal one, and Erik
Satie's Gymnopédie No. 1 looping softly under the title.

## v6 — sound

Square-wave SFX for menus, placement, and wins; Chopin's funeral march on a loss
(SFX on channel 2, the march on channel 1 via a non-blocking driver).

## v5 — framed board + crow

The board is closed top and bottom with frame lines; the title gained the
Terrible Crow crow, flanked by the colony gliders, blinking every couple seconds.

## v4 — bigger board + glider title

The board fills the screen at 20×14 cells with rescaled zones; the title gained
two gliders, one per colony, facing off.

## v3 — visible grid

Empty cells show a faint square so the matrix is visible, with the three zones
marked differently so territory reads at a glance.

## v2 — hardware VRAM fix

Real-hardware testing revealed dropped VRAM writes during active drawing. Fix:
every full-screen redraw turns the LCD off, draws, and turns it back on.

## v1 — full game

The automaton prototype (`src/main_proto_backup.c`) grown into a complete game:
state machine, the turn-based placement mechanic, two-color Conway evolution,
territory rules, the vs-CPU AI, and 2-player hot-seat.
