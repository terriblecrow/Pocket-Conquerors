/* sound.h — Game Boy sound: menu/game SFX and the funeral march on a loss.
 *
 * Uses the two square-wave channels. Channel 2 plays short sound effects;
 * channel 1 plays the melody. Frequencies use the Game Boy formula:
 *   reg = 2048 - (131072 / freq_hz)
 * We precompute note period values directly.
 */
#ifndef SOUND_H
#define SOUND_H

#include "hardware.h"

/* Game Boy 11-bit frequency register values for musical notes.
 * value = 2048 - 131072/freq_hz.  Higher value = higher pitch.
 * Full chromatic set so we can write proper minor-key melodies. */
#define N_A2   856
/* lower notes for the bass / accompaniment voice */
#define N_D2   263
#define N_E2   458
#define N_F2   547
#define N_G2   711
#define N_GS2  786
#define N_AS2  923
#define N_B2   986
#define N_C3  1047
#define N_D3  1156
#define N_DS3 1207
#define N_E3  1254
#define N_F3  1299
#define N_G3  1379
#define N_GS3 1418
#define N_A3  1452
#define N_AS3 1485
#define N_B3  1517
#define N_C4  1548
#define N_CS4 1575
#define N_D4  1602
#define N_DS4 1627
#define N_E4  1651
#define N_F4  1672
#define N_FS4 1694
#define N_G4  1714
#define N_GS4 1732
#define N_A4  1750
#define N_AS4 1767
#define N_B4  1783
#define N_C5  1797
#define N_CS5 1811
#define N_D5  1825
#define N_DS5 1837
#define N_E5  1849
#define N_F5  1860
#define N_FS5 1871
#define N_G5  1881
#define N_REST 0

static void sound_init(void) {
  rNR52 = 0x80;          /* sound on */
  rNR51 = 0xFF;          /* all channels to both speakers */
  rNR50 = 0x77;          /* max volume L/R */
  rNR10 = 0x00;          /* channel 1 sweep off */
}

/* play a short blip on channel 2 at the given frequency reg value.
 * The note isn't gated here — the envelope fades it out so callers don't
 * need to manage duration. */
static void sfx_tone(u16 freq, u8 envelope) {
  rNR21 = 0x80;                 /* 50% duty */
  rNR22 = envelope;             /* volume + decay */
  rNR23 = (u8)(freq & 0xFF);
  rNR24 = 0x80 | ((freq >> 8) & 0x07);  /* trigger */
}
/* same, but with a chosen duty (0x00=12.5%, 0x40=25%, 0x80=50%, 0xC0=75%);
 * lower duty reads as a thinner, "blippier" timbre */
static void sfx_tone_duty(u16 freq, u8 envelope, u8 duty) {
  rNR21 = duty | 0x3F;          /* duty + length data (length unused, no enable) */
  rNR22 = envelope;
  rNR23 = (u8)(freq & 0xFF);
  rNR24 = 0x80 | ((freq >> 8) & 0x07);
}

/* menu navigation: a soft high blip */
static void sfx_move(void) { sfx_tone(N_A4, 0xA2); }
/* selection / confirm: a brighter two-step feel via a single bright blip */
static void sfx_select(void) { sfx_tone(N_C5, 0xF3); }
/* place a valid cell: a short, dry, high "blip" — a thin 12.5%-duty high note
 * with a fast decay, clearly distinct from the start/select note */
static void sfx_place(void) { sfx_tone_duty(N_E5, 0xD1, 0x00); }
/* back / cancel: a low blip */
static void sfx_back(void) { sfx_tone(N_C3, 0x92); }
/* illegal placement: a harsh low ALARM. Two quick low buzzes via the noise-ish
 * low square; played as a short descending pair to read as "no". The caller
 * triggers it once; we set a low frequency with a slow-ish decay so it buzzes. */
static void sfx_error(void) {
  rNR21 = 0xC0;          /* 75% duty -> buzzier tone */
  rNR22 = 0xA4;          /* mid volume, slow decay = sustained buzz */
  rNR23 = (u8)(N_C3 & 0xFF);
  rNR24 = 0x80 | ((N_C3 >> 8) & 0x07);
}
/* a win flourish: bright rising note */
static void sfx_win(void) { sfx_tone(N_C5, 0xF4); }

/* evolution tick: a soft, short mid blip that plays on each board-redraw flash.
 * 25% duty for a thin timbre and a fast decay so it fades quickly — reads as a
 * gentle "tick"/"pop" that cushions the LCD blank instead of a bare glitch. */
static void sfx_evolve(void) { sfx_tone_duty(N_G4, 0x73, 0x40); }

/* ── melody playback on channel 1 ──
 * Plays a note for `frames`/60 seconds (caller advances frames via VBlank).
 * Because we don't have a timer loop here, the funeral march is played by a
 * small driver that the main loop ticks; see funeral_* below.
 */
/* Melody playback. We use channel 2 (no sweep unit, and proven reliable here)
 * for melodies too; SFX and melody don't overlap in time on the title or
 * game-over screens, so sharing the channel is fine. */
/* ── Two-voice melody playback ──
 * Channel 2 (square) carries the melody; channel 1 (square, sweep disabled)
 * carries a softer bass / accompaniment voice underneath. Each note is
 * re-triggered explicitly so repeated notes sound as separate pulses rather
 * than one held tone, and a gentle volume envelope gives every note a shape
 * (soft attack-less pluck with a slow decay) instead of a flat blare. */
/* ── Per-build timbre palette ──
 * The MELODY and HARMONY are identical across both cartridges (one shared note
 * table below); only the sound palette differs, chosen to flatter each console:
 *   GBC  — melody 50%% duty (full, round), bass 25%% duty (warm pad). The richer
 *          envelopes read well through the GBC's cleaner mix.
 *   DMG  — melody 25%% duty (thinner, more plaintive — the "classic Game Boy"
 *          voice), bass 12.5%% duty (reedy). Same notes, leaner palette so the
 *          original hardware sounds characterful rather than muddy.
 * Duty bits live in the high two bits of NRx1 (0x00=12.5,0x40=25,0x80=50,0xC0=75). */
#ifdef BUILD_DMG
#define MEL_DUTY  0x40    /* DMG melody: 25%% — thin, plaintive */
#define BASS_DUTY 0x00    /* DMG bass:   12.5%% — reedy */
#else
#define MEL_DUTY  0x80    /* GBC melody: 50%% — full, round */
#define BASS_DUTY 0x40    /* GBC bass:   25%% — warm pad */
#endif

static void mel_note(u16 freq, u8 vol) {
  if (freq == N_REST) { rNR22 = 0x00; rNR24 = 0x80; return; }  /* silence ch2 */
  rNR21 = MEL_DUTY;             /* duty per build palette */
  rNR22 = vol;                  /* volume + decay envelope */
  rNR23 = (u8)(freq & 0xFF);
  rNR24 = 0x80 | ((freq >> 8) & 0x07);   /* trigger */
}
/* bass / harmony voice on channel 1 (sweep off). Duty per build palette so it
 * sits under the melody as a warm pad rather than competing. */
static void mel_bass(u16 freq, u8 vol) {
  if (freq == N_REST) { rNR12 = 0x00; rNR14 = 0x80; return; }  /* silence ch1 */
  rNR10 = 0x00;                 /* sweep off */
  rNR11 = BASS_DUTY;            /* duty per build palette */
  rNR12 = vol;
  rNR13 = (u8)(freq & 0xFF);
  rNR14 = 0x80 | ((freq >> 8) & 0x07);   /* trigger */
}
static void mel_stop(void) {
  rNR22 = 0x00; rNR24 = 0x80;   /* melody off */
  rNR12 = 0x00; rNR14 = 0x80;   /* bass off */
}

/* Funeral march (Chopin, Marche funèbre — the famous "dum dum da-dum") opening.
 * Stored as two parallel flat arrays (note, duration) rather than an array of
 * structs: SDCC reads flat const arrays from ROM reliably, whereas indexing
 * struct members in a const array can miscompile on the sm83 target. */
static const u16 FUNERAL_N[] = {
  N_B3,N_B3,N_B3,N_B3, N_D4,N_C4,N_C4,N_B3, N_B3,N_A3,N_B3,N_REST,
  N_B3,N_B3,N_B3,N_B3, N_D4,N_C4,N_C4,N_B3, N_A3,N_B3,N_A3,N_REST,
};
static const u8 FUNERAL_D[] = {
  45,18,18,45, 30,12,30,12, 30,12,60,20,
  45,18,18,45, 30,12,30,12, 30,12,80,10,
};
/* Bass voice for the march: the funereal low B / tonic drone under each beat,
 * sinking with the melody. One entry per melody note (N_REST = let it ring). */
static const u16 FUNERAL_B[] = {
  N_B2,N_REST,N_REST,N_B2, N_B2,N_REST,N_E2,N_REST, N_GS2,N_REST,N_E2,N_REST,
  N_B2,N_REST,N_REST,N_B2, N_B2,N_REST,N_E2,N_REST, N_F2,N_REST,N_E2,N_REST,
};
#define FUNERAL_LEN (sizeof(FUNERAL_D)/sizeof(FUNERAL_D[0]))

/* Erik Satie — Gymnopédie No.1, the opening melodic line. Slow, in 3/4, the
 * unmistakable wistful phrase. Played softly on the title screen, looping. */
/* Erik Satie — Gymnopédie No.1. A fuller take on the opening: the rising
 * statement, its answer, and a descending resolution that flows back into the
 * loop. Long notes "float" (3/4 feel); short pickup notes lead between them.
 * Tuned so the end glides back to the start without a hard seam. */
/* Title theme — "Conquerors" main loop. A-minor, slow and somber but with a
 * clear EPIC/STRATEGIC arc rather than a meandering line. The whole melody is
 * built from ONE 4-note motif (rise–turn) that is transformed across four
 * phrases, so the tune has a through-line ("hilo conductor") instead of unrelated
 * fragments. Harmonic plan — each phrase lands on a real chord so the loop
 * travels somewhere and comes home:
 *     Phrase A: Am -> F     (state the motif, an aching rise)
 *     Phrase B: C  -> G     (motif up a step, climbs to the yearning peak A4)
 *     Phrase C: Am -> F     (motif inverted/descending — the strategic weight)
 *     Phrase D: E(dom) -> Am (G#->A leading tone: the epic arrival that re-loops)
 * The same note table is used on both GBC and DMG; only the timbre differs
 * (see MEL_DUTY / BASS_DUTY above). Plays softly under the title. */
static const u16 GYMNO_N[] = {
  /* Built on a STRICT 4/4 grid: quarter = 32 frames (~112 BPM), so every bar
   * sums to exactly 128 frames and the pulse never drifts. Harmony per bar:
   * Am | F | C | G | Am | F | E(dom) | Am. The opening A3-C4-E4-D4 is kept. */
  N_A3, N_C4, N_E4, N_D4, N_E4,    /* c1  Am */
  N_F4, N_A4, N_G4, N_F4, N_E4,    /* c2  F  (F arpeggio up + melodic turn down) */
  N_E4, N_G4, N_A4, N_G4,          /* c3  C  (A4 = the half-note peak) */
  N_F4, N_E4, N_D4, N_B3, N_A3,    /* c4  G->A (B3->A3 leading tone resolves home) */
  N_C4, N_B3, N_A3, N_C4, N_D4,    /* c5  Am */
  N_D4, N_C4, N_B3, N_C4,          /* c6  F  */
  N_E4, N_D4, N_C4, N_B3, N_GS3,   /* c7  E  (G# leading tone) */
  N_A3,                            /* c8  Am (whole-note resolution) */
};
static const u8 GYMNO_D[] = {
  /* Durations on the 4/4 grid (16=eighth, 32=quarter, 64=half, 128=whole).
   * Each bar sums to 128 — verified — so it cannot desync at a bar line. */
  32, 16, 16, 32, 32,   /* c1 = 128 */
  16, 16, 32, 32, 32,   /* c2 = 128 */
  32, 16, 64, 16,       /* c3 = 128 */
  16, 16, 32, 32, 32,   /* c4 = 128 */
  32, 16, 16, 32, 32,   /* c5 = 128 */
  16, 16, 64, 32,       /* c6 = 128 */
  32, 16, 16, 32, 32,   /* c7 = 128 */
  128,                  /* c8 = 128 (whole note) */
};
#define GYMNO_LEN (sizeof(GYMNO_D)/sizeof(GYMNO_D[0]))

/* Per-note volume — the dynamic arc (high nibble = volume 9..F, low = decay).
 * Soft statement, SWELL to the A4 peak in bar 3 (0xF3), settle, then BUILD into
 * the G#->A cadence (0xF3). One entry per melody note. */
static const u8 GYMNO_V[] = {
  0x93,0xA3,0xB3,0xA3,0x93,   /* c1 */
  0xA3,0xB3,0xA3,0xA3,0x93,   /* c2  arpeggio lift then settle */
  0xC3,0xD3,0xF3,0xD3,        /* c3  peak */
  0xC3,0xB3,0xA3,0xA3,0x93,   /* c4  descend, soft landing home */
  0xA3,0xB3,0xA3,0xA3,0xA3,   /* c5 */
  0xA3,0xA3,0x93,0xA3,        /* c6 */
  0xC3,0xB3,0xB3,0xC3,0xD3,   /* c7  build */
  0xF3,                       /* c8  resolution */
};

/* Bass / harmony — chord root per bar with a passing tone, one entry per melody
 * note so the harmony turns over in step with the tune. Am | F | C | G | Am | F
 * | E(E-G#-B dominant) | Am. N_REST = let the bass ring. */
static const u16 GYMNO_B[] = {
  N_A2, N_REST, N_E2, N_A2, N_REST,   /* c1  Am */
  N_F2, N_REST, N_C3, N_F2, N_REST,   /* c2  F  */
  N_C3, N_REST, N_G2, N_C3,           /* c3  C  */
  N_G2, N_REST, N_D3, N_E2, N_A2,     /* c4  G->A (cadence resolves to A) */
  N_A2, N_REST, N_E2, N_A2, N_REST,   /* c5  Am */
  N_F2, N_REST, N_C3, N_F2,           /* c6  F  */
  N_E2, N_REST, N_GS2, N_B2, N_E2,    /* c7  E dom */
  N_A2,                               /* c8  Am */
};

#endif
