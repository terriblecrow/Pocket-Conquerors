/* link.h — Game Boy serial (link cable) support for Conway's Conquerors.
 *
 * Two-player over the link cable for the game's ALTERNATING turn flow. The
 * active player places up to CELLS_PER_TURN cells locally; both Game Boys then
 * run one symmetric packet exchange so each learns the other's placements. The
 * board evolution is deterministic and identical on both units, so we only ever
 * exchange the raw placements — never the whole board.
 *
 * Hardware (works on DMG, Pocket and GBC):
 *   - Serial is two registers: SB (0xFF01 data shift) and SC (0xFF02 control).
 *     SC=0x81 starts an INTERNAL-clock transfer (this GB is MASTER, drives the
 *     wire); SC=0x80 starts an EXTERNAL-clock transfer (SLAVE, waits to be
 *     clocked). A transfer is 8 bits and full-duplex: both SB registers swap.
 *   - We POLL the SC start bit instead of using the serial interrupt, so the
 *     integration with the existing main loop stays simple and timeouts are
 *     trivial. This module enables no interrupts.
 *   - GBC supports a high-speed clock (SC bit 1); we deliberately leave it OFF.
 *     Normal speed is the most compatible across DMG<->GBC and cheap cables,
 *     and a GBC linked to a DMG must run at normal speed regardless.
 *
 * SYMMETRIC PROTOCOL (the important design choice):
 *   Both consoles call the SAME functions and perform the SAME number of byte
 *   exchanges in the SAME order. There is no separate "send" vs "receive" path
 *   that could slip out of phase. Each turn is ONE link_swap_move(): the active
 *   player passes its real placements, the waiting player passes an empty move
 *   (count 0); both read the other's packet in the same pass. Because the turns
 *   strictly alternate, exactly one side has a real move each exchange. This was
 *   verified in a host simulation of a full multi-turn game with no drift.
 *
 * Robustness: every blocking wait is bounded by a spin-count timeout, so a
 * yanked cable or absent partner returns a failure code instead of hanging.
 */
#ifndef LINK_H
#define LINK_H

#include "hardware.h"

#ifndef rSB
#define rSB REG(0xFF01)   /* serial transfer data    */
#define rSC REG(0xFF02)   /* serial transfer control */
#endif

/* SC control bits */
#define SC_START   0x80   /* set to begin a transfer; auto-clears when done   */
#define SC_FAST    0x02   /* GBC high clock speed (intentionally unused)       */
#define SC_MASTER  0x01   /* 1 = internal clock (master), 0 = external (slave) */

/* Protocol bytes */
#define LINK_HELLO_M 0xC3 /* master's handshake byte */
#define LINK_HELLO_S 0x3C /* slave's  handshake byte */
#define LINK_PAD     0xFF /* filler for unused cell slots in a packet */

/* Result codes */
#define LINK_OK      0
#define LINK_TIMEOUT 1
#define LINK_DESYNC  2

/* Timeout budget for one byte exchange, in spin iterations. Generous enough
 * that a healthy slave is never dropped, short enough that a disconnected cable
 * surfaces an error within a fraction of a second. */
#ifndef LINK_BYTE_SPINS
#define LINK_BYTE_SPINS  45000u
#endif

/* Cells exchanged per move; must equal CELLS_PER_TURN in the game. */
#ifndef LINK_CELLS
#define LINK_CELLS 4
#endif
#define LINK_PKT (1 + 2*LINK_CELLS)

/* Session role, set once during the lobby handshake. Each console has its own
 * copy — this is NOT shared state between the two units. */
static u8 link_is_master = 0;

/* Inter-byte delay the MASTER must observe after each transfer. Per the Pan
 * Docs: "The master Game Boy should always execute a small delay after each
 * transfer, in order to ensure that the other Game Boy has enough time to
 * prepare itself for the next transfer" — i.e. the slave must re-arm its
 * SC=0x80 start bit before the master clocks the next byte. Without this the
 * master streams bytes back-to-back; a Pocket/GBC slave can't re-arm fast
 * enough, so bytes get dropped or duplicated and the packet desyncs. This was
 * the real cause of the turn-2 break between a GBC host and a Pocket joiner.
 * ~3000 cycles ≈ 3ms at 1.05MHz — far longer than the slave's re-arm path,
 * negligible to gameplay. */
#ifndef LINK_BYTE_DELAY
#define LINK_BYTE_DELAY 3000u
#endif

/* ── Exchange a single byte, full-duplex. ─────────────────────────────────────
 * Returns LINK_OK with the partner's byte in *in, or LINK_TIMEOUT.
 *   Master: write SB, SC=0x81, poll until the start bit auto-clears (we drive
 *     the clock so this finishes as fast as the wire allows), THEN delay so the
 *     slave can re-arm before the next byte.
 *   Slave: write SB, SC=0x80, poll until the start bit clears — which only
 *     happens once the master clocks us. No master -> we time out, not hang. */
static u8 link_xfer(u8 out, u8 *in){
  u16 spins = LINK_BYTE_SPINS;
  rSB = out;
  rSC = link_is_master ? (SC_START | SC_MASTER) : (SC_START);
  while (rSC & SC_START) {
    if (--spins == 0) { *in = LINK_PAD; return LINK_TIMEOUT; }
  }
  *in = rSB;
  if (link_is_master) {                 /* give the slave time to re-arm */
    volatile u16 d = LINK_BYTE_DELAY;
    while (d) d--;
  }
  return LINK_OK;
}

/* ── Lobby handshake (two-phase, mutually confirmed). ─────────────────────────
 * Host calls link_begin(1) (clock master); joiner calls link_begin(0).
 *
 * Why two phases: the old single-byte handshake declared LINK_OK as soon as one
 * byte went out. But the master's internal clock ALWAYS completes its transfer
 * whether or not a slave is really listening, so a master with no synced slave
 * still "succeeded", read 0xFF, and (depending on framing) could slip into the
 * game while the joiner — caught in the gap between its per-frame slave arming
 * windows — timed out and reported a dropped cable. Result: host playing solo,
 * joiner ejected. The fix is a handshake where NEITHER side returns LINK_OK
 * until it has seen the OTHER side's correct reply.
 *
 * Phase 1: master sends HELLO_M, slave (when clocked) reads it and loads HELLO_S
 *          into its SB so the SAME transfer hands HELLO_S back to the master.
 * Phase 2: a second exchange lets each side confirm the partner's byte.
 *
 * Master retries the whole handshake several times per call. Each retry is a
 * fast pair of byte exchanges; the slave only needs to be armed during ONE of
 * them, which closes the dead-window race that the single-shot version had.
 * Master returns LINK_DESYNC (stay in lobby, no solo start) unless it actually
 * reads HELLO_S back. Slave returns LINK_TIMEOUT until it is genuinely clocked
 * by a master sending HELLO_M, then confirms with a second exchange. */

#ifndef LINK_HS_RETRIES
#define LINK_HS_RETRIES 8u   /* master inner attempts per link_begin() call */
#endif

/* Master spin-wait BETWEEN inner retries, so the burst spans long enough for
 * the joiner — which re-arms its slave transfer roughly once per video frame —
 * to be caught listening by at least one attempt. ~one frame's worth of spins
 * per retry means a single host A-press reliably overlaps the joiner's arming
 * window instead of firing 8 attempts inside one frame and missing. */
#ifndef LINK_HS_GAP_SPINS
#define LINK_HS_GAP_SPINS 12000u
#endif
static void link_gap(void){ volatile u16 g = LINK_HS_GAP_SPINS; while (g) { g--; } }

static u8 link_begin(u8 as_master){
  u8 a, b;
  link_is_master = as_master ? 1 : 0;

  if (link_is_master) {
    u8 try;
    for (try = 0; try < LINK_HS_RETRIES; try++) {
      /* Phase 1: send HELLO_M, expect HELLO_S echoed back by a real slave.
       * A master's internal clock always completes, so link_xfer never times
       * out here; a missing/unsynced slave just yields 0xFF in `a`. */
      link_xfer(LINK_HELLO_M, &a);
      if (a == LINK_HELLO_S) {
        /* Phase 2: confirm with a second exchange. Two clean HELLO_S in a row
         * means a real, stable slave is on the wire. */
        link_xfer(LINK_HELLO_M, &b);
        if (b == LINK_HELLO_S) return LINK_OK;
      }
      link_gap();                               /* let the slave re-arm, retry */
    }
    return LINK_DESYNC;                          /* no confirmed partner */
  } else {
    /* Slave: block (timeout-bounded) until the master clocks phase 1. Preload
     * HELLO_S so the master receives it in the very same full-duplex transfer. */
    if (link_xfer(LINK_HELLO_S, &a) == LINK_TIMEOUT) return LINK_TIMEOUT;
    if (a != LINK_HELLO_M) return LINK_DESYNC;   /* clocked, but not our master */
    /* Phase 2: answer the master's confirming byte the same way. */
    if (link_xfer(LINK_HELLO_S, &b) == LINK_TIMEOUT) return LINK_TIMEOUT;
    if (b != LINK_HELLO_M) return LINK_DESYNC;
    return LINK_OK;
  }
}

/* Per-move sync byte. Before every data exchange both consoles rendez-vous on
 * this byte so the swap proceeds only once BOTH are present. */
#define LINK_SYNC_M 0xA5   /* master's "I'm here, are you?" */
#define LINK_SYNC_S 0x5A   /* slave's  "yes, go" */

/* How long EITHER side will keep trying to rendez-vous before a move. The move
 * exchange happens INSIDE next_turn — and the side whose turn it ISN'T enters it
 * immediately, then has to wait for the human on the other console to place four
 * cells. So the waiting budget must comfortably exceed human think-time. Both
 * roles below are tuned to ~25-30s of patience so neither side gives up first.
 *
 * Critically, master and slave budgets must be COMPARABLE. An earlier version
 * gave the master ~5s (600 rounds * an 8000-cycle spacer) but the slave ~27s
 * (600 rounds * a 45000-cycle timeout). On the turn where the HOST is the
 * passive side, the host-master entered sync first and ran out in ~5s while the
 * joiner's human was still placing — the master bailed, the streams slipped, and
 * the host fell through to game-over ("BLUE WINS") while the joiner saw a dead
 * link. Equal budgets fix that. */
#ifndef LINK_SYNC_ROUNDS
#define LINK_SYNC_ROUNDS 4000u
#endif
/* Master spacer between sync attempts. The master loop is much cheaper per round
 * than the slave's (no timeout wait), so to reach the SAME wall-clock patience
 * as the slave (~170s at 4000 rounds * 45000-cycle timeout) the master needs a
 * larger spacer. ~44000 cycles brings 4000 master rounds to ~170s too, so on the
 * turn where the host is passive it waits as long as the joiner's slave would —
 * removing the asymmetry that dropped the link on turn 2. */
#ifndef LINK_SYNC_MGAP
#define LINK_SYNC_MGAP 44000u
#endif

/* ── Rendez-vous before a move. ───────────────────────────────────────────────
 * Master: repeatedly clock LINK_SYNC_M until it reads LINK_SYNC_S back (slave
 *   present and armed). Each attempt is one fast master xfer; retry many times.
 * Slave: repeatedly arm and wait to be clocked; when it reads LINK_SYNC_M it
 *   has answered LINK_SYNC_S in the same transfer, so it's synced. Each attempt
 *   is one timeout-bounded slave xfer; retry up to LINK_SYNC_ROUNDS.
 * Both budgets are sized to the same wall-clock patience so the side that
 * arrives first never gives up before the other (human-paced) side shows.
 * Returns LINK_OK once both meet, or LINK_TIMEOUT if the partner never shows. */
static u8 link_sync(void){
  u16 round;
  u8 in;
  if (link_is_master) {
    for (round = 0; round < LINK_SYNC_ROUNDS; round++) {
      link_xfer(LINK_SYNC_M, &in);          /* master clock always completes */
      if (in == LINK_SYNC_S) return LINK_OK;
      /* slave not armed this instant; small spacer, then try again */
      { volatile u16 g = LINK_SYNC_MGAP; while (g) g--; }
    }
    return LINK_TIMEOUT;
  } else {
    for (round = 0; round < LINK_SYNC_ROUNDS; round++) {
      /* Preload our answer so the master gets SYNC_S in the very same transfer
       * that delivers SYNC_M to us. If no master clocks us this round, the slave
       * xfer times out and we simply re-arm on the next round. */
      if (link_xfer(LINK_SYNC_S, &in) == LINK_OK && in == LINK_SYNC_M)
        return LINK_OK;
    }
    return LINK_TIMEOUT;
  }
}

/* ── Symmetric move exchange. ─────────────────────────────────────────────────
 * Both consoles call this once per turn. The active player passes its real
 * placements (out_cnt, ox[], oy[]); the waiting player passes out_cnt == 0.
 * On return, *in_cnt / ix[] / iy[] hold the partner's placements.
 *
 * A per-move link_sync() rendez-vous runs FIRST, so the two sides need not
 * reach this call at the same instant — the one that arrives early waits for
 * the other (covering human think-time and unsynced animations) instead of
 * clocking into the void and dropping the link.
 *
 * Packet layout (fixed length, no negotiation):
 *     [count][x0][y0][x1][y1]...   unused slots are LINK_PAD.
 *
 * Returns LINK_OK, LINK_TIMEOUT (cable lost) or LINK_DESYNC (bad count -> the
 * streams slipped, treat as a dropped link). */
static u8 link_swap_move(u8 out_cnt, const u8 *ox, const u8 *oy,
                         u8 *in_cnt,       u8 *ix,       u8 *iy){
  u8 i, inb;
  u8 op[LINK_PKT], ip[LINK_PKT];
  if (link_sync() == LINK_TIMEOUT) return LINK_TIMEOUT;   /* rendez-vous first */
  op[0] = out_cnt;
  for (i = 0; i < LINK_CELLS; i++) {
    if (i < out_cnt) { op[1 + i*2] = ox[i]; op[2 + i*2] = oy[i]; }
    else             { op[1 + i*2] = LINK_PAD; op[2 + i*2] = LINK_PAD; }
  }
  for (i = 0; i < LINK_PKT; i++) {
    if (link_xfer(op[i], &inb) == LINK_TIMEOUT) return LINK_TIMEOUT;
    ip[i] = inb;
  }
  if (ip[0] > LINK_CELLS && ip[0] != LINK_PAD) return LINK_DESYNC;
  *in_cnt = (ip[0] == LINK_PAD) ? 0 : ip[0];
  for (i = 0; i < LINK_CELLS; i++) { ix[i] = ip[1 + i*2]; iy[i] = ip[2 + i*2]; }
  return LINK_OK;
}

#endif /* LINK_H */
