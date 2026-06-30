/* main.c — Conway's Conquerors (Game Boy) — full game
 *
 * Terrible Crow. A two-color Game of Life strategy game for the Game Boy.
 *
 * Screens: TITLE -> MENU -> (HELP | CREDITS | DIFFICULTY -> PLAY -> GAMEOVER).
 * Mechanics: place up to 4 cells per turn inside allowed territory, then the
 * board evolves one Conway generation (B3/S23, majority-color births). Win by
 * extinction or by holding more cells when the round limit is reached.
 *
 * No GBDK: direct hardware access, hand-written startup (crt0.s).
 */
#include "hardware.h"
#include "font.h"
#include "crow.h"
#include "sound.h"
#include "link.h"

typedef signed char    i8;
typedef signed short   i16;

/* ───────────────────────── Tiles ───────────────────────── */
#define T_BLANK    0
#define T_BLUE     1
#define T_RED      2
#define T_CURSOR   3
#define T_GRID     4     /* empty neutral cell */
#define T_GRIDB    5     /* empty blue-zone cell */
#define T_GRIDR    9     /* empty red-zone cell */
#define T_WALL     6
#define T_BORDER   7
#define T_STAR2    8     /* second starfield tile for the title background */
#define T_BLUEC    10
#define T_REDC     11
#define T_HBAR     12
#define T_STAR     13
#define T_SPARK    14
#define T_TGRID    15     /* starfield tile for the dark title background */
#define T_DECOL    65     /* menu ornament: left diamond terminator  */
#define T_DECOR    66     /* menu ornament: right diamond terminator */
#define T_DIAM     67     /* menu ornament: small diamond accent     */
#define T_GRIDBD   68     /* blue-zone cell on the home/neutral boundary (right divider) */
#define T_GRIDRD   69     /* red-zone cell on the neutral/home boundary (left divider)   */
#define T_GRIDH_D  70     /* DMG-only: home cell with faint stipple (no divider)         */
#define T_GRIDBD_D 71     /* DMG-only: blue boundary cell — stipple + right divider      */
#define T_GRIDRD_D 72     /* DMG-only: red  boundary cell — stipple + left  divider      */
#define T_HBARB    73     /* board's bottom border: hbar shifted 1px down to fill the gap  */
#define T_HBART    74     /* board's top border: hbar shifted 1px up to centre the board   */
#define T_FONT_BASE 16    /* font occupies 16..(16+GLYPH_COUNT-1) = 16..64 */

static const u8 patt_blank[16]   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
/* Live colonies are distinguished by SHAPE so they read on a monochrome DMG /
 * Pocket as clearly as on a Game Boy Color, not by fill density alone:
 *   BLUE = a solid filled block (ink everywhere)
 *   RED  = a hollow diamond/rhombus with a thick ink outline
 * A filled square vs. a hollow diamond is unmistakable even on a low-contrast
 * Pocket screen; on GBC each also gets its colony color on top. */
static const u8 patt_blue[16]    = {0x7E,0x7E,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7E,0x7E};
static const u8 patt_red[16]     = {0x18,0x18,0x3C,0x3C,0x66,0x66,0xC3,0xC3,0xC3,0xC3,0x66,0x66,0x3C,0x3C,0x18,0x18};
static const u8 patt_cursor[16]  = {0xFF,0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0xFF,0xFF};
/* empty-cell grid tiles: a faint frame so every square is visible. The three
 * zones get slightly different marks so territory reads at a glance:
 *   neutral home/centre — a small centre dot
 *   blue zone           — dot + a faint left edge
 *   red zone            — dot + a faint right edge
 * (low bitplane only -> light gray, so live cells and the cursor still pop) */
static const u8 patt_grid[16]    = {0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00};
static const u8 patt_gridb[16]   = {0x80,0x80,0x80,0x80,0x80,0x80,0x98,0x98,0x98,0x98,0x80,0x80,0x80,0x80,0x80,0x80};
static const u8 patt_gridr[16]   = {0x01,0x01,0x01,0x01,0x01,0x01,0x19,0x19,0x19,0x19,0x01,0x01,0x01,0x01,0x01,0x01};
/* Home/neutral boundary cells. Same faint centre dot as the zone grid, plus a
 * crisp DARK 1px vertical line on the edge that faces the neutral middle — so on
 * a monochrome DMG/Pocket the 6-wide home columns are clearly bounded (without
 * it the per-cell edges read as 5 internal divisions). The dark line uses both
 * bitplanes; the dot stays low-plane (light), so the divider stands out. On GBC
 * the line picks up the zone colour from the tile attribute, staying consistent.
 *   T_GRIDBD : blue home's last column (right divider, faces neutral)
 *   T_GRIDRD : red  home's first column (left  divider, faces neutral) */
static const u8 patt_gridbd[16]  = {0x01,0x01,0x01,0x01,0x01,0x01,0x19,0x19,0x19,0x19,0x01,0x01,0x01,0x01,0x01,0x01};
static const u8 patt_gridrd[16]  = {0x80,0x80,0x80,0x80,0x80,0x80,0x98,0x98,0x98,0x98,0x80,0x80,0x80,0x80,0x80,0x80};
/* DMG-only variants: the home zones get a faint light-gray corner stipple so the
 * three zones differ by shading as well as by the divider lines. Pure monochrome
 * has no per-tile colour, so this subtle texture is the only way to tint a home.
 * Neutral cells stay clean (plain dot). On GBC these are unused — colour already
 * separates the zones and a stipple would just add noise. */
static const u8 patt_gridh_d[16] = {0x42,0x00,0x00,0x00,0x24,0x00,0x18,0x18,0x18,0x18,0x42,0x00,0x00,0x00,0x24,0x00};
static const u8 patt_gridbd_d[16]= {0x43,0x01,0x01,0x01,0x25,0x01,0x19,0x19,0x19,0x19,0x43,0x01,0x01,0x01,0x25,0x01};
static const u8 patt_gridrd_d[16]= {0xC2,0x80,0x80,0x80,0xA4,0x80,0x98,0x98,0x98,0x98,0xC2,0x80,0x80,0x80,0xA4,0x80};
static const u8 patt_wall[16]    = {0x18,0x00,0x18,0x00,0x18,0x00,0x18,0x00,0x18,0x00,0x18,0x00,0x18,0x00,0x18,0x00};
static const u8 patt_border[16]  = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
/* horizontal frame line: a solid bar mid-tile, to close the board top & bottom */
static const u8 patt_hbar[16]    = {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00};
/* Bottom-border variant: same 2px line as patt_hbar but one pixel lower, so with
 * the 1px screen nudge it reaches the very bottom edge and fills the leftover gap
 * (the top border and menu rules keep the centered patt_hbar). */
static const u8 patt_hbarb[16]   = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00};
/* Top-border variant: the 2px line one pixel HIGHER than patt_hbar (rows 2-3),
 * so the board sits equidistant between the top and bottom border lines. Only
 * the board's top edge uses it; menu rules keep the centered patt_hbar. */
static const u8 patt_hbart[16]   = {0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
/* decorative star and sparkle for the title screen */
static const u8 patt_star[16]    = {0x18,0x18,0x18,0x18,0xDB,0xDB,0x7E,0x7E,0x3C,0x3C,0x7E,0x7E,0xC3,0xC3,0x00,0x00};
static const u8 patt_spark[16]   = {0x00,0x00,0x18,0x18,0x18,0x18,0x7E,0x7E,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00};
/* Title background "starfield" tiles. On the dark title palette (color 0 = field)
 * these read as a deep field scattered with faint dots (color 1) and the odd
 * bright star (color 3). Two variants are alternated across the screen so the
 * field looks scattered rather than gridded — giving the title the depth of a
 * real cartridge cover on DMG as well as on GBC. */
static const u8 patt_tgrid[16]   = {0x00,0x00,0x00,0x00,0x04,0x04,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00};
static const u8 patt_tgrid2[16]  = {0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x02,0x00,0x00,0x00,0x00};
/* Menu ornaments (black-on-white): diamond bar terminators that cap a heading's
 * underline rule, and a small solid diamond used as a list/accent mark. These
 * add a bit more drawn detail to the otherwise plain text screens. */
static const u8 patt_decol[16]   = {0x00,0x00,0x08,0x08,0x1C,0x1C,0x3E,0x3E,0x1C,0x1C,0x08,0x08,0x00,0x00,0x00,0x00};
static const u8 patt_decor[16]   = {0x00,0x00,0x10,0x10,0x38,0x38,0x7C,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,0x00,0x00};
static const u8 patt_diam[16]    = {0x00,0x00,0x18,0x18,0x3C,0x3C,0x7E,0x7E,0x3C,0x3C,0x18,0x18,0x00,0x00,0x00,0x00};
/* cursor-on-cell variants: the colony shape wrapped in a solid ink border so
 * the selected square is obvious while still reading as blue (filled) or red
 * (hollow diamond). */
static const u8 patt_bluec[16]   = {0xFF,0xFF,0x81,0x81,0xBD,0xBD,0xBD,0xBD,0xBD,0xBD,0xBD,0xBD,0x81,0x81,0xFF,0xFF};
static const u8 patt_redc[16]    = {0xFF,0xFF,0xA5,0xA5,0xC3,0xC3,0x99,0x99,0x99,0x99,0xC3,0xC3,0xA5,0xA5,0xFF,0xFF};

/* ───────────────────── Board geometry ───────────────────── */
#define BW 20
#define BH 14
#define BN (BW*BH)
#define BOARD_OX 0
#define BOARD_OY 3
#define BORDER_TOP 2     /* horizontal line above the board */
#define BORDER_BOT 17    /* horizontal line below the board */

/* zones scaled to the wider board: cols 0..5 blue, 6..13 neutral, 14..19 red */
#define ZONE_BLUE_MAX 5
#define ZONE_RED_MIN  14

#define DEAD 0
#define BLUE 1
#define RED  2

#define MAX_ROUNDS 12
#define CELLS_PER_TURN 4

static u8 board[BN];
static u8 nextb[BN];

/* ───────────────────── Game state ───────────────────── */
typedef enum { S_TITLE, S_MENU, S_HELP, S_DIFF, S_PLAY, S_GAMEOVER, S_CREDITS,
               S_2PMENU, S_LINKWAIT, S_STATS, S_PAUSE, S_ACK } State;
static State state;

static u8 menu_sel;
static u8 difficulty;
static u8 vs_cpu;
static u8 cpu_first;        /* vs-CPU: 1 if the CPU (RED) plays first each round */
static u8 cur_player;
static u8 round_no;
static u8 placed;
static u8 cx, cy;
static u8 winner;
static u8 redraw;
static u16 rng;
static u16 anim;       /* frame counter for title animations */
static u8 mel_idx;     /* current note index in the funeral march */

/* Link-cable session state. link_mode: 0 = no cable (CPU or local hot-seat);
 * 1 = two Game Boys over the serial cable. link_local is which colour THIS
 * console controls (the host plays BLUE and moves first; the joiner plays RED).
 * A move buffer collects the cells you place this turn so they can be shipped
 * to the other unit in one packet. */
static u8 link_mode;       /* 1 when playing over the cable */
static u8 link_local;      /* BLUE or RED — the colour this console plays */
static u8 link_lost;       /* set if the cable drops mid-game */
static u8 link_host_armed; /* host lobby: 1 once A pressed, retry handshake/frame */
static u8 lmx[LINK_CELLS], lmy[LINK_CELLS];  /* this turn's placements */
static u8 mel_timer;   /* frames left on the current note */
static u8 mel_playing; /* 1 while the march is playing */
static u8 win_jingle;  /* small countdown to play a win flourish */
static u8 stats_reset_hold; /* frames SELECT held on the stats screen */
static u8 help_page;        /* HOW TO PLAY: 0 = rules page, 1 = controls page */
/* One free "skip" (pass your placement, board still evolves) per player per
 * game. Indexed by colour: blue uses [0], red uses [1]. Reset at game start. */
static u8 skip_used[2];

/* ───────────────────── Persistent statistics ─────────────────────
 * Six counters kept in the cartridge's battery-backed SRAM so the record
 * survives power-off. A magic signature in the first two SRAM bytes tells a
 * fresh (garbage-filled) cart RAM apart from a previously-saved one. Counters
 * are stored as 16-bit little-endian so they can climb past 255.
 * SRAM layout (offsets from 0xA000):
 *   0,1  : magic 'TC'                            (init marker)
 *   2,3  : CPU games played
 *   4,5  : CPU wins (you, BLUE)
 *   6,7  : CPU losses
 *   8,9  : CPU draws
 *  10,11 : 2P blue wins
 *  12,13 : 2P red wins                                                       */
#define ST_MAGIC0 0xA000
#define ST_CPU_PLAYED 2
#define ST_CPU_WINS   4
#define ST_CPU_LOSS   6
#define ST_CPU_DRAW   8
#define ST_2P_BLUE   10
#define ST_2P_RED    12

static u16 st_played, st_wins, st_loss, st_draw, st_bwon, st_rwon;

static void sram_on(void)  { rRAMG = 0x0A; rRAMB = 0x00; }
static void sram_off(void) { rRAMG = 0x00; }

static u16 sram_rd(u8 off){ return (u16)SRAM[off] | ((u16)SRAM[off+1] << 8); }
static void sram_wr(u8 off, u16 v){ SRAM[off]=(u8)(v&0xFF); SRAM[off+1]=(u8)(v>>8); }

/* Load counters from SRAM into RAM mirrors. If the magic isn't present, the
 * cart RAM has never held our data: zero everything and stamp the magic. */
static void stats_load(void){
  sram_on();
  if(SRAM[0]!='T' || SRAM[1]!='C'){
    u8 i; for(i=2;i<14;i++) SRAM[i]=0;
    SRAM[0]='T'; SRAM[1]='C';
  }
  st_played=sram_rd(ST_CPU_PLAYED); st_wins=sram_rd(ST_CPU_WINS);
  st_loss  =sram_rd(ST_CPU_LOSS);   st_draw=sram_rd(ST_CPU_DRAW);
  st_bwon  =sram_rd(ST_2P_BLUE);    st_rwon=sram_rd(ST_2P_RED);
  sram_off();
}
/* Write the RAM mirrors back to battery SRAM. */
static void stats_save(void){
  sram_on();
  SRAM[0]='T'; SRAM[1]='C';
  sram_wr(ST_CPU_PLAYED,st_played); sram_wr(ST_CPU_WINS,st_wins);
  sram_wr(ST_CPU_LOSS,st_loss);     sram_wr(ST_CPU_DRAW,st_draw);
  sram_wr(ST_2P_BLUE,st_bwon);      sram_wr(ST_2P_RED,st_rwon);
  sram_off();
}
static void stats_reset(void){
  st_played=st_wins=st_loss=st_draw=st_bwon=st_rwon=0;
  stats_save();
}
/* Record one finished game. Called once at game over. In CPU mode the human is
 * BLUE: BLUE win = win, RED win = loss, 0 = draw. In 2-player, tally by colour.
 * Link-cable games that ended on a dropped cable are not counted (winner==255
 * sentinel is never used; callers skip the link-lost case). */
static void stats_record(u8 was_cpu, u8 w){
  if(was_cpu){
    st_played++;
    if(w==BLUE) st_wins++;
    else if(w==RED) st_loss++;
    else st_draw++;
  } else {
    if(w==BLUE) st_bwon++;
    else if(w==RED) st_rwon++;
    /* a 2P draw is not tallied to either colour */
  }
  stats_save();
}

/* ───────────────────── RNG ───────────────────── */
static u8 rnd(void) {
  rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8;
  return (u8)rng;
}

/* ───────────────────── VRAM helpers ───────────────────── */
static void wait_vbl(void) { while (rLY != 144) {} }

/* On real hardware, VRAM can only be written during VBlank (or with the LCD
 * off). A full-screen redraw writes far more than fits in one VBlank, so any
 * writes that spill past it are silently dropped — which showed up as missing
 * letters on real hardware (the emulator doesn't enforce the lock). The robust
 * fix: turn the LCD off, do all the drawing, then turn it back on. The blank is
 * a single frame and invisible in practice. */
#define LCDC_GAME (LCDC_ON|LCDC_BG_ON|LCDC_BG_DATA_8000)

static void lcd_off(void) {
  if (rLCDC & LCDC_ON) wait_vbl();  /* only wait if LCD is on, else LY never hits 144 */
  rLCDC = 0x00;
}
static void lcd_on(void) {
  rLCDC = LCDC_GAME;
}

static void set_tile(u8 idx, const u8 *p) {
  volatile u8 *d = VRAM_TILES + (u16)idx*16;
  u8 i; for (i=0;i<16;i++) d[i]=p[i];
}
static void set_font_tile(u8 idx, const u8 *g) {
  volatile u8 *d = VRAM_TILES + (u16)idx*16;
  u8 i; for (i=0;i<8;i++){ d[i*2]=g[i]; d[i*2+1]=g[i]; }
}
static void map_put(u8 tx, u8 ty, u8 tile) { VRAM_MAP[(u16)ty*32 + tx]=tile; }

/* ───────────────────── Game Boy Color ───────────────────── */
/* The startup code stashed the boot A value at 0xDFFF: 0x11 on a GBC/GBA.
 *
 * One source, two builds:
 *   default   — color-aware; is_gbc() detects a Game Boy Color at runtime and
 *               the game layers a color palette over the grayscale art.
 *   BUILD_DMG — pure monochrome for the original Game Boy / Pocket. is_gbc() is
 *               hard-wired to 0, so every color path is dead code the compiler
 *               drops, and the ROM is plain DMG. (build.sh sets the matching
 *               cartridge header.) */
#ifdef BUILD_DMG
static u8 is_gbc(void) { return 0; }
#else
static u8 is_gbc(void) { return (*(volatile u8 *)0xDFFF) == 0x11; }
#endif

/* Write one 4-color background palette (pal 0..7). Colors are 15-bit BGR555,
 * lowest first. BCPS auto-increments so we stream 8 bytes. */
static void set_bg_palette(u8 pal, u16 c0, u16 c1, u16 c2, u16 c3) {
  rBCPS = 0x80 | (pal*8);        /* auto-increment, start at this palette */
  rBCPD = (u8)(c0 & 0xFF); rBCPD = (u8)(c0 >> 8);
  rBCPD = (u8)(c1 & 0xFF); rBCPD = (u8)(c1 >> 8);
  rBCPD = (u8)(c2 & 0xFF); rBCPD = (u8)(c2 >> 8);
  rBCPD = (u8)(c3 & 0xFF); rBCPD = (u8)(c3 >> 8);
}

/* helper to build a BGR555 color from 5-bit r,g,b */
#define RGB5(r,g,b) ((u16)((b)<<10 | (g)<<5 | (r)))

/* set the color attribute (which palette) for a map cell. On GBC, the tile
 * attributes live in VRAM bank 1 at the same map address. */
static void set_attr(u8 tx, u8 ty, u8 pal) {
  rVBK = 1;
  VRAM_MAP[(u16)ty*32 + tx] = pal & 0x07;
  rVBK = 0;
}

/* fill a rectangular region's attributes with one palette */
static void fill_attr(u8 x0, u8 y0, u8 w, u8 h, u8 pal) {
  u8 x,y;
  rVBK = 1;
  for(y=0;y<h;y++) for(x=0;x<w;x++)
    VRAM_MAP[(u16)(y0+y)*32 + (x0+x)] = pal & 0x07;
  rVBK = 0;
}

/* set every attribute on screen to one palette (clears prior coloring) */
static void clear_attr(u8 pal) { fill_attr(0,0,20,18,pal); }

/* Build the crow's 4x3 tiles from the bitmap into VRAM at CROW_BASE.
 * If eye_open is 0, the eye pixel is filled in (blink). Darkest = both planes. */
static void build_crow(u8 eye_open) {
  u8 tx,ty,py,px;
  for(ty=0;ty<CROW_H_TILES;ty++){
    for(tx=0;tx<CROW_W_TILES;tx++){
      u8 patt[16];
      for(py=0;py<8;py++){
        u8 bits=0;
        u8 row=ty*8+py;
        for(px=0;px<8;px++){
          u8 col=tx*8+px;
          char c=CROWBMP[row][col];
          if(c=='#') bits|=(0x80>>px);
          else if(c=='E'){ if(!eye_open) bits|=(0x80>>px); } /* eye fills when blinking */
        }
        patt[py*2]=bits; patt[py*2+1]=bits;
      }
      set_tile(CROW_BASE + ty*CROW_W_TILES + tx, patt);
    }
  }
}

/* place the crow logo tiles into the map at (ox,oy) */
static void draw_crow_at(u8 ox, u8 oy) {
  u8 tx,ty;
  for(ty=0;ty<CROW_H_TILES;ty++)
    for(tx=0;tx<CROW_W_TILES;tx++)
      map_put(ox+tx, oy+ty, CROW_BASE + ty*CROW_W_TILES + tx);
}

/* Blink: rewrite only the single tile that holds the eye (16 bytes — safely
 * fits in one VBlank, so no LCD blanking needed). The eye sits in tile 64,
 * bitmap rows 0..7 -> the 'E' is on row 2. */
static void crow_blink(u8 eye_open) {
  u8 py,px;
  u8 patt[16];
  for(py=0;py<8;py++){
    u8 bits=0;
    u8 row=0*8+py;            /* top tile-row */
    for(px=0;px<8;px++){
      char c=CROWBMP[row][0*8+px];  /* left tile-column */
      if(c=='#') bits|=(0x80>>px);
      else if(c=='E'){ if(!eye_open) bits|=(0x80>>px); }
    }
    patt[py*2]=bits; patt[py*2+1]=bits;
  }
  set_tile(CROW_BASE, patt);
}
static void print_at(u8 tx, u8 ty, const char *s) {
  while (*s){
    if(*s=='\n'){ ty++; tx=0; s++; continue; }
    map_put(tx,ty,T_FONT_BASE+glyph_of(*s)); tx++; s++;
  }
}
static void clear_map(void){ u16 i; for(i=0;i<32*32;i++) VRAM_MAP[i]=T_BLANK; }

/* Big text: render a string at 2x size. Each 8x8 glyph is scaled to 16x16 (a
 * 2x2 block of tiles) by doubling every pixel, then placed on the map. Tiles
 * are generated on the fly into a pool starting at BIG_TILE_BASE. Good for a
 * chunky logo-style wordmark like the title. Call with the LCD off. */
#define BIG_TILE_BASE 96      /* VRAM pool for big-text tiles (past crow 80..91) */
static u8 big_next;           /* next free tile in the pool */

static void big_reset(void){ big_next = BIG_TILE_BASE; }

/* build the 4 scaled tiles for one glyph and place them at (tx,ty) */
static void big_char(u8 tx, u8 ty, char ch){
  const u8 *g = FONT[glyph_of(ch)];
  u8 quad; /* 0=TL 1=TR 2=BL 3=BR */
  for(quad=0; quad<4; quad++){
    u8 patt[16];
    u8 col_half = (quad & 1);      /* 0 = left 4 source cols, 1 = right 4 */
    u8 row_half = (quad >> 1);     /* 0 = top 4 source rows, 1 = bottom 4 */
    u8 r;
    for(r=0;r<8;r++){
      /* source row = row_half*4 + r/2 ; double each source pixel horizontally */
      u8 src = g[row_half*4 + (r>>1)];
      u8 nib = col_half ? (src & 0x0F) : (src >> 4);  /* 4 source bits */
      u8 outb = 0; u8 b;
      for(b=0;b<4;b++){
        if(nib & (0x08>>b)){ outb |= (0xC0 >> (b*2)); } /* each src bit -> 2 px */
      }
      patt[r*2]=outb; patt[r*2+1]=outb;
    }
    set_tile(big_next, patt);
    {
      u8 ox = tx + (quad & 1);
      u8 oy = ty + (quad >> 1);
      map_put(ox, oy, big_next);
    }
    big_next++;
  }
}

/* print a big string at (tx,ty); each char takes 2x2 tiles, advance by 2 */
static void print_big(u8 tx, u8 ty, const char *s){
  while(*s){
    if(*s!=' ') big_char(tx, ty, *s);
    tx += 2;
    s++;
  }
}

/* ───────────────────── Input ───────────────────── */
static u8 read_pad(void){
  u8 d,b;
  u8 i;
  rP1=P1_DPAD;
  /* let the hardware settle: reading P1 right after selecting is unreliable */
  for(i=0;i<6;i++){ d=rP1; }
  d=~rP1&0x0F;
  rP1=P1_BTN;
  for(i=0;i<6;i++){ b=rP1; }
  b=~rP1&0x0F;
  rP1=0x30;
  return (u8)((b<<4)|d);
}

/* ───────────────────── Board logic ───────────────────── */
static u8 zone_of(u8 x){
  if(x<=ZONE_BLUE_MAX) return BLUE;
  if(x>=ZONE_RED_MIN)  return RED;
  return 0;
}

/* Tile for an empty cell at column x. The only vertical lines on the board are
 * the two home/neutral dividers. On DMG (no per-tile colour) the home zones also
 * get a faint corner stipple so the three zones differ by shading too; on GBC the
 * homes stay clean because colour already separates them. Neutral is always the
 * plain dotted tile. */
static u8 empty_tile(u8 x){
  u8 dmg = !is_gbc();
  if(x==ZONE_BLUE_MAX) return dmg ? T_GRIDBD_D : T_GRIDBD;   /* last blue col, right divider */
  if(x==ZONE_RED_MIN)  return dmg ? T_GRIDRD_D : T_GRIDRD;   /* first red col, left divider  */
  if(dmg){
    u8 z=zone_of(x);
    if(z==BLUE || z==RED) return T_GRIDH_D;                  /* home interior: stipple */
  }
  return T_GRID;                                            /* neutral / GBC home: plain dot */
}
static void clear_board(void){ u16 i; for(i=0;i<BN;i++) board[i]=DEAD; }
static void count_cells(u8 *b, u8 *r){
  u16 i; u8 cb=0,cr=0;
  for(i=0;i<BN;i++){ if(board[i]==BLUE)cb++; else if(board[i]==RED)cr++; }
  *b=cb; *r=cr;
}
/* Weighted score: a cell standing in ENEMY territory is worth DOUBLE (an
 * incursion behind enemy lines counts for two). A cell in its own zone or in
 * the neutral middle is worth one. BLUE's enemy zone is RED's and vice versa.
 * Used for the on-screen score and for deciding the winner; raw count_cells is
 * kept for extinction detection. Clamped to 255 for the u8 return. */
static void score_cells(u8 *b, u8 *r){
  u8 x,y; u16 sb=0,sr=0;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 v=board[y*BW+x];
    if(v==BLUE) sb += (zone_of(x)==RED)  ? 2 : 1;    /* BLUE in RED land  = x2 */
    else if(v==RED) sr += (zone_of(x)==BLUE) ? 2 : 1; /* RED  in BLUE land = x2 */
  }
  if(sb>255)sb=255; if(sr>255)sr=255;
  *b=(u8)sb; *r=(u8)sr;
}
static u8 player_has_cell_in_zone(u8 player, u8 zone){
  u8 x,y;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    if(board[y*BW+x]==player){
      u8 z=zone_of(x);
      if(zone==0 && z==0) return 1;
      if(zone==BLUE && z==BLUE) return 1;
      if(zone==RED && z==RED) return 1;
    }
  }
  return 0;
}
static u8 can_place(u8 player, u8 x, u8 y){
  u8 z;
  if(board[y*BW+x]!=DEAD) return 0;
  z=zone_of(x);
  if(z==player) return 1;                            /* own home: always allowed */
  /* Expanding OUTSIDE the home — neutral or enemy — always requires holding at
   * least one cell in your own home. Without a home foothold you can only play
   * at home. */
  if(!player_has_cell_in_zone(player,player)) return 0;
  if(z==0) return 1;                                 /* neutral: home foothold is enough */
  return player_has_cell_in_zone(player,z);          /* enemy zone: also need a cell there */
}
static void step(void){
  u8 x,y; i8 dx,dy;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 n1=0,n2=0;
    for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++){
      i8 nx,ny; u8 v;
      if(!dx&&!dy)continue;
      nx=(i8)x+dx; ny=(i8)y+dy;
      if(nx<0||nx>=BW||ny<0||ny>=BH)continue;
      v=board[ny*BW+nx];
      if(v==BLUE)n1++; else if(v==RED)n2++;
    }
    {
      u8 tot=n1+n2, cur=board[y*BW+x], res;
      if(cur) res=(tot==2||tot==3)?cur:DEAD;
      else    res=(tot==3)?((n1>n2)?BLUE:RED):DEAD;
      nextb[y*BW+x]=res;
    }
  }
  { u16 i; for(i=0;i<BN;i++) board[i]=nextb[i]; }
}
static void setup_board(void){
  clear_board();
  /* board starts empty — each side builds its colony from scratch */
}

/* ───────────────────── Drawing ───────────────────── */
/* Draw a single board cell (tile + GBC color) in place. Small enough to do
 * during VBlank with the LCD on, so moving the cursor doesn't flash the whole
 * screen. */
static void draw_cell(u8 x, u8 y) {
  u8 v = board[y*BW+x];
  u8 oncur = (state==S_PLAY && x==cx && y==cy);
  u8 t, pal;
  if(v==BLUE){ t=oncur?T_BLUEC:T_BLUE; pal=1; }
  else if(v==RED){ t=oncur?T_REDC:T_RED; pal=2; }
  else if(oncur){ t=T_CURSOR; pal=(zone_of(x)==BLUE)?1:(zone_of(x)==RED)?2:0; }
  else {
    u8 z=zone_of(x);
    t = empty_tile(x);
    pal = (z==BLUE)?1:(z==RED)?2:0;
  }
  map_put(BOARD_OX+x, BOARD_OY+y, t);
  if(is_gbc()){
    rVBK=1;
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = pal;
    rVBK=0;
  }
}

static void draw_board(void){
  u8 x,y;
  u8 gbc = is_gbc();
  /* close the board with horizontal frame lines top and bottom */
  for(x=0;x<BW;x++){
    map_put(BOARD_OX+x, BORDER_TOP, T_HBART);   /* shifted up 1px to centre the board */
    map_put(BOARD_OX+x, BORDER_BOT, T_HBARB);   /* shifted down 1px to fill the gap */
  }
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 v=board[y*BW+x];
    u8 t;
    u8 oncur=(state==S_PLAY && x==cx && y==cy);
    if(v==BLUE) t=oncur?T_BLUEC:T_BLUE;
    else if(v==RED) t=oncur?T_REDC:T_RED;
    else if(oncur) t=T_CURSOR;
    else {
      /* empty cell: plain dotted square, except the two home/neutral boundary
       * columns which carry the divider line (see empty_tile). */
      t = empty_tile(x);
    }
    map_put(BOARD_OX+x, BOARD_OY+y, t);
  }
  /* GBC: color the whole screen. HUD rows and frame neutral/paper; board cells
   * by colony/zone. Done in one bank-1 pass so nothing from a previous screen
   * (e.g. the gold title) lingers. */
  if (gbc) {
    u8 ax,ay;
    rVBK = 1;
    /* default everything to paper white */
    for(ay=0;ay<18;ay++) for(ax=0;ax<20;ax++)
      VRAM_MAP[(u16)ay*32 + ax] = 4;
    /* board cells */
    for(y=0;y<BH;y++)for(x=0;x<BW;x++){
      u8 v=board[y*BW+x];
      u8 pal;
      if(v==BLUE) pal=1;
      else if(v==RED) pal=2;
      else { u8 z=zone_of(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
      VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = pal;
    }
    /* frame lines neutral green */
    for(x=0;x<BW;x++){
      VRAM_MAP[(u16)BORDER_TOP*32 + (BOARD_OX+x)] = 0;
      VRAM_MAP[(u16)BORDER_BOT*32 + (BOARD_OX+x)] = 0;
    }
    rVBK = 0;
  }
}
static char hud0[21], hud1[21];
static void hud_compute(void){
  u8 cb,cr; char *p;
  score_cells(&cb,&cr);          /* weighted: enemy-territory cells count double */
  p=hud0;
  { const char *r="RND "; while(*r)*p++=*r++; }
  *p++='0'+(round_no/10); *p++='0'+(round_no%10);
  *p++='/'; *p++='1'; *p++='2'; *p++=' '; *p++=' ';
  if(cur_player==BLUE){ *p++='B';*p++='L';*p++='U';*p++='E'; }
  else { *p++='R';*p++='E';*p++='D';*p++=' '; }
  while(p<hud0+20)*p++=' ';
  *p=0;
  p=hud1;
  *p++='B';*p++=':';*p++='0'+(cb/100);*p++='0'+((cb/10)%10);*p++='0'+(cb%10);*p++=' ';
  *p++='R';*p++=':';*p++='0'+(cr/100);*p++='0'+((cr/10)%10);*p++='0'+(cr%10);*p++=' ';
  *p++='P';*p++='U';*p++='T';*p++=':'; *p++='0'+(CELLS_PER_TURN-placed);
  while(p<hud1+20)*p++=' ';
  *p=0;
}
/* write the precomputed HUD lines — call right after wait_vbl (fits one VBlank) */
static void draw_hud_vbl(void){
  hud_compute();
  print_at(0,0,hud0);
  print_at(0,1,hud1);
}
static void draw_hud(void){
  hud_compute();
  print_at(0,0,hud0);
  print_at(0,1,hud1);
}

/* Show "THINKING" on the HUD top row while the CPU computes its move (cpu_turn
 * is blocking, so without this the screen looks frozen on a slow turn). The HUD
 * top row is "RND xx/12 RED " — we overwrite the tail with the notice, wait one
 * VBlank so it actually latches before the CPU work begins, then the normal HUD
 * redraw after the move clears it. */
static void cpu_thinking_on(void){
  wait_vbl();
  print_at(11,0,"THINKING");
}

static void draw_title(void){
  u8 x,y;
  lcd_off();
  clear_map();
  big_reset();

  /* On DMG there are no per-tile palettes, so we flip the global BG palette to
   * an inverted ramp (color 0 = darkest, color 3 = lightest) for the title only.
   * That turns the field dark and makes the text, gliders and stars glow bright
   * on top — the same "cartridge cover" depth the GBC build gets from palette 5.
   * The normal palette is restored when we leave the title (see leave_title). */
  if(!is_gbc()) rBGP = 0x1B;

  /* dark starfield background: scatter the two star tiles so it reads as a deep
   * field with faint dots and the odd bright star, not a flat grid */
  for(y=0;y<18;y++) for(x=0;x<20;x++){
    u8 h = (u8)(x*7 + y*13);
    map_put(x,y, (h%5==0) ? T_TGRID : (h%5==2) ? T_STAR2 : T_BLANK);
  }

  /* title text up top */
  print_at(6,1,"CONWAY'S");
  print_big(0,3,"CONQUERORS");   /* one line, fills the 20-tile width */

  /* Gliders — the iconic Game of Life spaceship — flying toward the center from
   * both sides, blue from the left, red from the right. A glider is 5 cells in
   * an arrow shape; we place a few at different heights for a graceful sweep. */
  /* blue gliders (left), pointing right-ish */
  /* glider A */
  map_put(2,6,T_BLUE);  map_put(3,7,T_BLUE);  map_put(1,8,T_BLUE);  map_put(2,8,T_BLUE);  map_put(3,8,T_BLUE);
  /* glider B */
  map_put(6,9,T_BLUE);  map_put(7,10,T_BLUE); map_put(5,11,T_BLUE); map_put(6,11,T_BLUE); map_put(7,11,T_BLUE);
  /* red gliders (right), mirrored, pointing left-ish */
  /* glider C */
  map_put(17,6,T_RED);  map_put(16,7,T_RED);  map_put(16,8,T_RED);  map_put(17,8,T_RED);  map_put(18,8,T_RED);
  /* glider D */
  map_put(13,9,T_RED);  map_put(12,10,T_RED); map_put(12,11,T_RED); map_put(13,11,T_RED); map_put(14,11,T_RED);
  /* a small sparkle constellation where they head toward each other */
  map_put(9,7,T_STAR);  map_put(10,9,T_STAR);
  map_put(10,7,T_SPARK); map_put(9,9,T_SPARK);

  /* studio + badge + prompt */
  print_at(2,13,"- TERRIBLE CROW -");
  print_at(8,14,"2026");
  print_at(5,17,"PRESS START");

  /* GBC coloring: dark label look */
  if (is_gbc()) {
    clear_attr(5);                 /* dark field, bright ink everywhere */
    /* color each glider in its colony glow */
    fill_attr(1,6,3,3, 6);  fill_attr(5,9,3,3, 6);   /* blue gliders -> cyan */
    fill_attr(16,6,3,3, 7); fill_attr(12,9,3,3, 7);  /* red gliders -> red */
    /* sparkle constellation alternates */
    set_attr(9,7,6); set_attr(9,9,6);
    set_attr(10,7,7); set_attr(10,9,7);
  }
  /* The title is a full-bleed centered screen; show it without the 4px gameplay
   * nudge so its dark field reaches the top edge and PRESS START isn't clipped.
   * The offset is restored on the way out to the menu. */
  rSCY=0;
  lcd_on();
}
/* Decoration for the white menu/text screens. A heading gets an ornamented rule:
 * a diamond terminator on each end with the solid underline bar between them,
 * plus small diamond accents in the corners and a matching bar across the
 * bottom. Keeps the clean black-on-white look but gives it drawn detail rather
 * than a bare text dump. */
static void deco_header(u8 hx, u8 hy, u8 hlen){
  u8 i;
  /* ornamented underline: ◆──────◆ under the heading */
  map_put(hx-1, hy+1, T_DECOL);
  for(i=0;i<hlen;i++) map_put(hx+i, hy+1, T_HBAR);
  map_put(hx+hlen, hy+1, T_DECOR);
  /* corner accents: stars top, diamonds where they frame the screen */
  map_put(0,0,T_STAR);   map_put(19,0,T_STAR);
  map_put(0,2,T_DIAM);   map_put(19,2,T_DIAM);
}

/* a decorative bar across the bottom of a text screen: ◆──── … ────◆ */
static void deco_footer(u8 row){
  u8 x;
  map_put(0,row,T_DECOL);
  for(x=1;x<19;x++) map_put(x,row,T_HBAR);
  map_put(19,row,T_DECOR);
}

static void draw_menu(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);    /* paper: white bg, black text */
  else rBGP = 0xE4;              /* restore normal palette (title inverts it) */
  print_at(5,1,"MAIN MENU");
  deco_header(5,1,9);
  print_at(4,4,"VS CPU");
  print_at(4,6,"2 PLAYERS");
  print_at(4,8,"HOW TO PLAY");
  print_at(4,10,"STATS");
  print_at(4,12,"CREDITS");
  print_at(4,14,"ACK");
  /* small diamond bullets beside each option */
  map_put(3,4,T_DIAM); map_put(3,6,T_DIAM); map_put(3,8,T_DIAM);
  map_put(3,10,T_DIAM); map_put(3,12,T_DIAM); map_put(3,14,T_DIAM);
  deco_footer(16);
  { u8 ty=(menu_sel==0)?4:(menu_sel==1)?6:(menu_sel==2)?8:(menu_sel==3)?10:(menu_sel==4)?12:14;
    print_at(2,ty,">"); }
  lcd_on();
}

/* Pause overlay: ask whether to quit to the main menu. menu_sel 0 = resume,
 * 1 = quit. Drawn as a clean text screen (the board is restored on resume by
 * S_PLAY's redraw path). */
static void draw_pause(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  print_at(6,3,"PAUSED");
  deco_header(6,3,6);
  print_at(2,7,"BACK TO MENU?");
  print_at(4,10,"RESUME");
  print_at(4,12,"QUIT TO MENU");
  map_put(3,10,T_DIAM); map_put(3,12,T_DIAM);
  deco_footer(15);
  print_at(0,16,"A: SELECT  B: RESUME");
  { u8 ty=(menu_sel==0)?10:12; print_at(2,ty,">"); }
  lcd_on();
}
static void draw_diff(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  print_at(3,2,"DIFFICULTY");
  deco_header(3,2,10);
  print_at(5,7,"EASY");
  print_at(5,9,"NORMAL");
  print_at(5,11,"HARD");
  map_put(4,7,T_DIAM); map_put(4,9,T_DIAM); map_put(4,11,T_DIAM);
  print_at(3,(u8)(7+menu_sel*2),">");
  deco_footer(14);
  print_at(2,16,"B: BACK");
  lcd_on();
}
static void draw_help(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  if(help_page==0){
    /* Page 1 — the rules, grouped with blank lines for breathing room. */
    print_at(0,1,"HOW TO PLAY  1/2");
    deco_header(0,1,11);
    print_at(0,4,"PLACE 4 CELLS,");
    print_at(0,5,"THEN IT EVOLVES:");
    print_at(0,7,"LIVE: 2-3 NEIGHBORS");
    print_at(0,8,"BORN: EMPTY WITH 3");
    print_at(0,10,"HOME: PLAY FREELY");
    print_at(0,11,"MID: NEED HOME CELL");
    print_at(0,12,"FOE: HOME + A CELL");
    print_at(0,13,"THERE. FOE LAND X2");
    print_at(0,16,"A: NEXT   B: BACK");
  } else {
    /* Page 2 — controls and win condition. */
    print_at(0,1,"HOW TO PLAY  2/2");
    deco_header(0,1,11);
    print_at(0,3,"DPAD: MOVE CURSOR");
    print_at(0,4,"A: PLACE A CELL");
    print_at(0,6,"4TH CELL ENDS TURN");
    print_at(0,8,"SELECT: SKIP TURN");
    print_at(0,9,"(ONCE PER GAME)");
    print_at(0,11,"START: PAUSE MENU");
    print_at(0,13,"WIN: WIPE OR LEAD");
    print_at(0,14,"AT ROUND 12");
    print_at(0,16,"A: BACK   B: MENU");
  }
  lcd_on();
}
static void draw_credits(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  print_at(4,1,"CREDITS");
  deco_header(4,1,7);
  print_at(0,3,"A GAME BY");
  print_at(2,4,"TERRIBLE CROW");
  print_at(0,6,"BASED ON CONWAYS");
  print_at(0,7,"GAME OF LIFE 1970");
  print_at(0,9,"AUTHOR: AGUSTIN");
  print_at(2,10,"'TANO' MATTIOLI");
  print_at(2,12,"ARGENTINA, 2026");
  print_at(0,14,"TERRIBLECROW.COM");
  map_put(0,4,T_DIAM);           /* accent by the studio name */
  deco_footer(15);
  print_at(2,16,"B: BACK");
  lcd_on();
}
static void draw_ack(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  deco_header(0,1,15);
  print_at(0,1,"ACKNOWLEDGMENTS");
  map_put(19,1,T_BLANK);   /* drop the right corner diamond: title is full-width */
  print_at(0,3,"THIS WORK IS");
  print_at(0,4,"ESPECIALLY DEDICATED");
  print_at(0,5,"TO THE ANONYMOUS FEW");
  print_at(0,6,"WHO CHOOSE TO TURN");
  print_at(0,7,"THEIR BACKS ON THIS");
  print_at(0,8,"HORRIBLE WORLD TO");
  print_at(0,9,"CREATE THEIR OWN.");
  print_at(0,11,"TO THEM, MY DEEPEST");
  print_at(0,12,"RESPECT.");
  print_at(0,14,"- AGUSTIN 'TANO'");
  print_at(2,15,"MATTIOLI");
  print_at(13,16,"B: BACK");
  lcd_on();
}
/* print a u16 right-aligned in a 3-char field at (tx,ty). Values are clamped
 * for display at 999 so the field never overflows. */
static void print_num3(u8 tx, u8 ty, u16 v){
  char b[4];
  if(v>999) v=999;
  b[0]='0'+(u8)(v/100);
  b[1]='0'+(u8)((v/10)%10);
  b[2]='0'+(u8)(v%10);
  b[3]=0;
  print_at(tx,ty,b);
}
static void draw_stats(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4);
  else rBGP = 0xE4;
  print_at(4,1,"STATISTICS");
  deco_header(4,1,10);
  print_at(0,3,"VS CPU");
  print_at(0,4,"  PLAYED");   print_num3(15,4,st_played);
  print_at(0,5,"  WINS");     print_num3(15,5,st_wins);
  print_at(0,6,"  LOSSES");   print_num3(15,6,st_loss);
  print_at(0,7,"  DRAWS");    print_num3(15,7,st_draw);
  print_at(0,9,"2 PLAYERS");
  print_at(0,10,"  BLUE WON"); print_num3(15,10,st_bwon);
  print_at(0,11,"  RED WON");  print_num3(15,11,st_rwon);
  deco_footer(13);
  print_at(0,15,"B: BACK");
  print_at(0,16,"SEL: RESET (HOLD)");
  lcd_on();
}
static void draw_gameover(void){
  u8 cb,cr; char buf[20]; char *p;
  score_cells(&cb,&cr);          /* weighted final score */
  lcd_off();
  clear_map();
  if(!is_gbc()) rBGP = 0xE4;     /* restore normal palette (title inverts it) */
  print_at(4,4,"GAME OVER");
  deco_header(4,4,9);
  if(link_mode && link_lost){
    print_at(3,8,"CABLE LOST");
    print_at(2,11,"START: MENU");
    if(is_gbc()) clear_attr(4);
    lcd_on();
    return;
  }
  if(winner==BLUE) print_at(4,8,"BLUE WINS!");
  else if(winner==RED) print_at(5,8,"RED WINS!");
  else print_at(6,8,"DRAW!");
  p=buf;
  *p++='B';*p++=':';*p++='0'+(cb/100);*p++='0'+((cb/10)%10);*p++='0'+(cb%10);*p++=' ';
  *p++='R';*p++=':';*p++='0'+(cr/100);*p++='0'+((cr/10)%10);*p++='0'+(cr%10);
  *p=0; print_at(4,11,buf);
  /* Rematch is offered only in single-console modes (vs-CPU and local hot-seat).
   * Over the cable a rematch would need its own two-sided handshake, so the
   * link path stays MENU-only here and players re-enter the lobby to replay. */
  if(link_mode){
    print_at(2,15,"START: MENU");
  } else {
    print_at(2,14,"A: REMATCH");
    print_at(2,15,"START: MENU");
  }
  if(is_gbc()) clear_attr(4);
  lcd_on();
}

/* ── funeral march driver ──
 * Start it on a loss; tick it once per frame. Plays the melody on channel 1,
 * note by note, without blocking the main loop. */
static void funeral_start(void) {
  mel_idx = 0;
  mel_timer = 0;
  mel_playing = 1;
}
static void funeral_tick(void) {
  if (!mel_playing) return;
  if (mel_timer == 0) {
    if (mel_idx >= FUNERAL_LEN) { mel_stop(); mel_playing = 0; return; }
    mel_note(FUNERAL_N[mel_idx], 0xF2);          /* melody: full, slow decay */
    mel_bass(FUNERAL_B[mel_idx], 0x83);          /* bass: under the melody */
    mel_timer = FUNERAL_D[mel_idx];
    mel_idx++;
  }
  mel_timer--;
}

/* Title-screen Gymnopédie: two-voice, looping, at a soft volume so it sits
 * under the scene. */
static void gymno_start(void) {
  mel_idx = 0; mel_timer = 0; mel_playing = 1;
}
static void gymno_tick(void) {
  if (!mel_playing) return;
  if (mel_timer == 0) {
    if (mel_idx >= GYMNO_LEN) { mel_idx = 0; }   /* loop */
    mel_note(GYMNO_N[mel_idx], GYMNO_V[mel_idx]); /* melody: per-note dynamic arc */
    mel_bass(GYMNO_B[mel_idx], 0x62);            /* bass: well under the melody */
    mel_timer = GYMNO_D[mel_idx];
    mel_idx++;
  }
  mel_timer--;
}

/* ───────────────────── CPU AI (depth-1) ───────────────────── */
/* Evaluate placing one cell at (px,py) for `player`. We simulate the placement
 * and run `gens` generations inside a local window, then score by the net
 * population swing (own cells minus enemy cells) plus how much the player grew.
 * This rewards formations that survive and expand, and (via the enemy term)
 * plays that disrupt the opponent — much stronger than counting raw survivors.
 *
 * Works on a small scratch grid so the real board is untouched. */
#define WIN 5                 /* odd window size centered on the placement */
static u8 scratchA[WIN*WIN];
static u8 scratchB[WIN*WIN];

static i16 place_impact_n(u8 player, u8 px, u8 py, u8 gens){
  i8 dx,dy; u8 i,g;
  u8 enemy = (player==BLUE)?RED:BLUE;
  u8 half = WIN/2;
  i16 own_before=0, enemy_before, score;
  /* load the window around (px,py) into scratchA, with the new cell placed */
  for(dy=0;dy<WIN;dy++)for(dx=0;dx<WIN;dx++){
    i8 bx=(i8)px-half+dx, by=(i8)py-half+dy;
    u8 v=0;
    if(bx>=0&&bx<BW&&by>=0&&by<BH) v=board[by*BW+bx];
    if(dx==half && dy==half) v=player;   /* the candidate cell */
    scratchA[dy*WIN+dx]=v;
  }
  /* count starting populations in the window */
  enemy_before=0;
  for(i=0;i<WIN*WIN;i++){ if(scratchA[i]==player)own_before++; else if(scratchA[i]==enemy)enemy_before++; }

  /* run `gens` generations of Life on the scratch window */
  for(g=0; g<gens; g++){
    for(dy=0;dy<WIN;dy++)for(dx=0;dx<WIN;dx++){
      u8 n1=0,n2=0; i8 a,b;
      for(a=-1;a<=1;a++)for(b=-1;b<=1;b++){
        i8 nx=dx+b, ny=dy+a; u8 v;
        if((!a&&!b))continue;
        if(nx<0||nx>=WIN||ny<0||ny>=WIN)continue;
        v=scratchA[ny*WIN+nx];
        if(v==BLUE)n1++; else if(v==RED)n2++;
      }
      {
        u8 tot=n1+n2, cur=scratchA[dy*WIN+dx], res;
        if(cur) res=(tot==2||tot==3)?cur:DEAD;
        else    res=(tot==3)?((n1>n2)?BLUE:RED):DEAD;
        scratchB[dy*WIN+dx]=res;
      }
    }
    for(i=0;i<WIN*WIN;i++) scratchA[i]=scratchB[i];
  }
  /* score: net own-vs-enemy population after, plus growth of own cells */
  {
    i16 own_after=0, enemy_after=0;
    for(i=0;i<WIN*WIN;i++){ if(scratchA[i]==player)own_after++; else if(scratchA[i]==enemy)enemy_after++; }
    score = (own_after - enemy_after) + (own_after - own_before);
    /* tiny extra credit if we reduced the enemy in the window */
    score += (enemy_before - enemy_after);
  }
  return score;
}
/* Precomputed per-turn flags so the inner search doesn't rescan the whole
 * board for every candidate cell (that was far too slow on a 4MHz CPU). */
static u8 cpu_can_neutral;     /* may this player place in neutral? */
static u8 cpu_can_enemy_col[BW]; /* may place in enemy zone column x? */

static void cpu_prep(u8 player) {
  u8 x, y;
  u8 enemy_zone = (player==BLUE) ? RED : BLUE;
  /* A home foothold (>=1 cell in own home) is required to expand anywhere
   * outside the home — both neutral and enemy. Matches can_place for the human. */
  u8 has_home = player_has_cell_in_zone(player, player);
  cpu_can_neutral = has_home;
  /* enemy column reachable iff the player holds a home foothold AND already
   * holds a cell in that column's (enemy) zone — precompute once per turn */
  for (x=0;x<BW;x++) cpu_can_enemy_col[x]=0;
  {
    u8 has_in_enemy = 0;
    for (y=0;y<BH;y++) for(x=0;x<BW;x++)
      if (board[y*BW+x]==player && zone_of(x)==enemy_zone) { has_in_enemy=1; }
    if (has_home && has_in_enemy)
      for (x=0;x<BW;x++) if (zone_of(x)==enemy_zone) cpu_can_enemy_col[x]=1;
  }
}

static u8 cpu_can_place(u8 player, u8 x, u8 y) {
  u8 z;
  if (board[y*BW+x]!=DEAD) return 0;
  z = zone_of(x);
  if (z==player) return 1;
  if (z==0) return cpu_can_neutral;
  return cpu_can_enemy_col[x];
}

/* A cell is a useful candidate only if it touches at least one live cell —
 * isolated placements just die. This cuts the search from ~150 cells to a few
 * dozen, which is the difference between instant and unplayable on real hardware. */
static u8 has_live_neighbor(u8 x, u8 y) {
  i8 dx,dy;
  for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++){
    i8 nx,ny;
    if(!dx&&!dy)continue;
    nx=(i8)x+dx; ny=(i8)y+dy;
    if(nx<0||nx>=BW||ny<0||ny>=BH)continue;
    if(board[ny*BW+nx]!=DEAD) return 1;
  }
  return 0;
}

static void cpu_turn(u8 player){
  u8 n;
  u8 cb,cr; count_cells(&cb,&cr);
  u8 own = (player==BLUE)?cb:cr;

  /* Opening: if we have almost nothing, don't search the whole empty board
   * (slow and pointless). Drop a known growing seed in our home: a small
   * cluster that survives and spreads. We place up to CELLS_PER_TURN of it. */
  if(own < 2){
    /* a "blinker + block" style seed near home, placed cell by cell */
    u8 hx = (player==BLUE) ? 3 : (BW-4);
    u8 hy = BH/2;
    /* candidate seed offsets (a compact cluster that lives & grows) */
    static const i8 seed_dx[8] = {0,1,0,1, 2,2,1,0};
    static const i8 seed_dy[8] = {0,0,1,1, 0,1,2,2};
    u8 placed_here=0, k;
    for(k=0; k<8 && placed_here<CELLS_PER_TURN; k++){
      i8 sx=(i8)hx+seed_dx[k], sy=(i8)hy+seed_dy[k];
      if(sx<0||sx>=BW||sy<0||sy>=BH) continue;
      if(cpu_can_place(player,(u8)sx,(u8)sy)){
        board[sy*BW+sx]=player; placed_here++;
      }
    }
    return;
  }

  /* Skip option (once per game): if we still hold our free skip and EVERY
   * legal placement this turn would be a net loss, pass instead of being forced
   * into a bad move — the board still evolves on its own. Only Normal/Hard are
   * shrewd enough to use it; Easy always plays so beginners aren't out-thought.
   * Mirrors the 1-gen scoring used in the placement loop below. */
  {
    u8 si = (player==BLUE)?0:1;
    if(!skip_used[si] && difficulty>0){
      i16 best_eval=-30000; u8 x,y;
      cpu_prep(player);
      for(y=0;y<BH;y++)for(x=0;x<BW;x++){
        if(!cpu_can_place(player,x,y))continue;
        if(!has_live_neighbor(x,y)) continue;
        {
          i16 s = place_impact_n(player, x, y, 1);
          if(zone_of(x)==0) s += 2;
          if(player==RED && zone_of(x)==BLUE) s += 1;
          if(player==BLUE && zone_of(x)==RED) s += 1;
          if(s>best_eval) best_eval=s;
        }
      }
      /* best_eval<0 means our strongest move still costs us ground: skip it. */
      if(best_eval < 0){ skip_used[si]=1; return; }
    }
  }

  /* Easy often places one fewer cell — a weaker tempo without crippling it.
   * Normal and Hard always use the full allotment. */
  {
  u8 cells = CELLS_PER_TURN;
  if(difficulty==0 && (rnd()%2==0)) cells=3;
  for(n=0;n<cells;n++){
    i16 best=-30000; u8 bx=255,by=255; u8 x,y;
    cpu_prep(player);

    for(y=0;y<BH;y++)for(x=0;x<BW;x++){
      if(!cpu_can_place(player,x,y))continue;
      if(!has_live_neighbor(x,y)) continue;   /* only near live cells */
      {
        i16 s = place_impact_n(player, x, y, 1);
        if(zone_of(x)==0) s += 2;                  /* contest the middle */
        if(player==RED && zone_of(x)==BLUE) s += 1;
        if(player==BLUE && zone_of(x)==RED) s += 1;/* reward invading */
        if(difficulty==0){
          /* Easy: heavy scoring noise — frequently picks a sub-optimal cell, so
           * beginners win a fair share and the gap to Normal is clear. */
          s += (i16)(rnd()%19) - 9;
        } else if(difficulty==1){
          /* Normal: slight noise so it's not perfectly predictable. */
          s += (i16)(rnd()%3) - 1;
        } else {
          /* Hard: a true 2-generation lookahead REPLACES the 1-gen score (so the
           * horizon is genuinely deeper and the scale stays consistent), with a
           * small 1-gen bias to break ties toward immediately solid plays. */
          i16 s2 = place_impact_n(player, x, y, 2);
          s = s2 + (s>>2);
        }
        if(s>best){ best=s; bx=x; by=y; }
      }
    }

    /* fallback: build near home if nothing adjacent scored */
    if(bx==255){
      u8 hx=(player==BLUE)?2:(BW-3);
      for(y=0;y<BH && bx==255;y++) for(x=0;x<BW;x++)
        if(cpu_can_place(player,x,y) && (x==hx||has_live_neighbor(x,y))){ bx=x;by=y;break; }
      if(bx==255)
        for(y=0;y<BH && bx==255;y++) for(x=0;x<BW;x++)
          if(cpu_can_place(player,x,y)){ bx=x;by=y;break; }
    }
    if(bx==255) break;
    board[by*BW+bx]=player;
  }
  }
}

/* ───────────────────── Turn / round flow ───────────────────── */
static void check_end(void){
  u8 cb,cr; count_cells(&cb,&cr);
  /* extinction only counts from round 2 on — at the very start both sides are
   * empty by design, so round 1 is the build-up and can't end the game. */
  if(round_no>=2 && (cb==0||cr==0)){
    winner=(cb>cr)?BLUE:(cr>cb)?RED:0; state=S_GAMEOVER; redraw=1; return;
  }
  if(round_no>=MAX_ROUNDS){
    u8 sb,sr; score_cells(&sb,&sr);    /* winner by WEIGHTED score */
    winner=(sb>sr)?BLUE:(sr>sb)?RED:0; state=S_GAMEOVER; redraw=1; return;
  }
}
/* redraw the whole play screen (board + HUD) and hold it for `frames` so the
 * player can actually see each step of a turn unfold. */
static void show_board_for(u8 frames){
  u8 i;
  lcd_off(); clear_map(); draw_hud(); draw_board(); lcd_on();
  for(i=0;i<frames;i++){ wait_vbl(); while(rLY==144){} }
}

/* Update only the board cells that differ from `prev` (a snapshot of the board
 * before the change). Far fewer tile writes than a full redraw, so it fits in
 * VBlank with the LCD ON — no screen-blanking flash. Call right after VBlank.
 * Also keeps the cursor cell drawn so it never vanishes. */
static u8 prevboard[BN];
static void snapshot_board(void){ u16 i; for(i=0;i<BN;i++) prevboard[i]=board[i]; }

/* Repaint the color attribute of every board cell (GBC). A cell's palette is
 * its colony color if a cell lives there, otherwise its zone color. This is the
 * single source of truth for board color, so calling it leaves the whole board
 * correct regardless of what incremental updates happened. Writes only bank-1
 * attributes (no tiles), but there are BW*BH of them so callers run it with the
 * LCD off (it's only used on full redraws, not during smooth animation). */
static void paint_board_colors(void){
  u8 x,y;
  if(!is_gbc()) return;
  /* VRAM bank 1 can only be written reliably during VBlank or with the LCD off.
   * This repaints all BW*BH cells, too many for one VBlank, so we briefly blank
   * the LCD. Callers use this on full refreshes, not during smooth animation. */
  lcd_off();
  rVBK=1;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 v=board[y*BW+x], pal;
    if(v==BLUE) pal=1; else if(v==RED) pal=2;
    else { u8 z=zone_of(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = pal;
  }
  for(x=0;x<BW;x++){
    VRAM_MAP[(u16)BORDER_TOP*32 + (BOARD_OX+x)] = 0;
    VRAM_MAP[(u16)BORDER_BOT*32 + (BOARD_OX+x)] = 0;
  }
  rVBK=0;
  lcd_on();
}

static void redraw_changes(void){
  u8 x,y;
  u8 gbc = is_gbc();
  /* Single pass: for each changed cell write its tile (bank 0) and, on GBC, its
   * color (bank 1). Toggling rVBK per cell is cheaper overall than two full
   * scans, and keeps the work inside one VBlank for a typical evolution. */
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 cur=board[y*BW+x];
    if(cur!=prevboard[y*BW+x] || oncur){
      u8 t, pal;
      if(cur==BLUE){ t=oncur?T_BLUEC:T_BLUE; pal=1; }
      else if(cur==RED){ t=oncur?T_REDC:T_RED; pal=2; }
      else if(oncur){ t=T_CURSOR; u8 z=zone_of(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
      else { u8 z=zone_of(x); t=empty_tile(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
      VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = t;   /* bank 0 (tile) */
      if(gbc){
        rVBK=1;
        VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = pal; /* bank 1 (color) */
        rVBK=0;
      }
    }
  }
}

/* animate one step: snapshot, evolve, then over `frames` frames redraw only the
 * changed cells (no LCD blanking) and hold. */
/* Redraw only the tiles (bank 0) of changed cells — no color. Bank 0 writes
 * with the LCD on are fine for the small number of tiles a frame touches, and
 * we spread color separately. Always (re)draw the cursor cell's tile. */
static void redraw_tiles_only(void){
  u8 x,y;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 cur=board[y*BW+x];
    if(cur!=prevboard[y*BW+x] || oncur){
      u8 t;
      if(cur==BLUE) t=oncur?T_BLUEC:T_BLUE;
      else if(cur==RED) t=oncur?T_REDC:T_RED;
      else if(oncur) t=T_CURSOR;
      else { t=empty_tile(x); }
      map_put(BOARD_OX+x, BOARD_OY+y, t);
    }
  }
}

/* Paint one chunk of rows: both tiles (bank 0) and colors (bank 1), for the
 * rows [start_row, start_row+COLOR_CHUNK). Small enough to complete inside a
 * single VBlank, so it's safe on real hardware and never needs the LCD off.
 * Must be called right at the start of VBlank. */
#define COLOR_CHUNK 2            /* rows painted per VBlank (tiles+colors fit) */
/* Per-cell color attribute buffer, rebuilt in RAM (fast, no VRAM timing rules)
 * then blitted to VRAM bank 1 in one tight VBlank pass. A cell's color is its
 * colony if alive, else its zone — so dying cells always revert to zone color
 * (no stale tint), and it all lands in one VBlank (no flash, hardware-safe). */
static u8 colbuf[BN];
static void build_colbuf(void){
  u8 x,y;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 v=board[y*BW+x], pal;
    if(v==BLUE) pal=1; else if(v==RED) pal=2;
    else { u8 z=zone_of(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
    colbuf[y*BW+x]=pal;
  }
}
/* Blit colbuf -> VRAM bank 1, must be called at VBlank start. Tight loop so all
 * BN writes finish within one VBlank window. */
static void blit_colbuf(void){
  u8 x,y;
  if(!is_gbc()) return;
  rVBK=1;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++)
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = colbuf[y*BW+x];
  rVBK=0;
}

/* Build the tile buffer changes and blit tiles to VRAM bank 0 at VBlank. */
static void blit_tiles(void){
  u8 x,y;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 v=board[y*BW+x], t;
    if(v==BLUE) t=oncur?T_BLUEC:T_BLUE;
    else if(v==RED) t=oncur?T_REDC:T_RED;
    else if(oncur) t=T_CURSOR;
    else { t=empty_tile(x); }
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = t;
  }
}

static void paint_chunk(u8 start_row){
  u8 x,y, end=start_row+COLOR_CHUNK;
  u8 gbc=is_gbc();
  if(end>BH) end=BH;
  /* tiles first */
  for(y=start_row;y<end;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 v=board[y*BW+x], t;
    if(v==BLUE) t=oncur?T_BLUEC:T_BLUE;
    else if(v==RED) t=oncur?T_REDC:T_RED;
    else if(oncur) t=T_CURSOR;
    else { t=empty_tile(x); }
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = t;
  }
  /* then colors for the same rows */
  if(gbc){
    rVBK=1;
    for(y=start_row;y<end;y++)for(x=0;x<BW;x++){
      u8 v=board[y*BW+x], pal;
      if(v==BLUE) pal=1; else if(v==RED) pal=2;
      else { u8 z=zone_of(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
      VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = pal;
    }
    rVBK=0;
  }
}

/* Repaint the whole board (tiles+colors) over several VBlanks, then keep holding
 * for the remaining frames. Nothing is ever written outside VBlank and the LCD
 * is never turned off, so there is no flash and no hardware glitch. */
/* List of changed cells, computed in RAM (no VRAM timing limits), then written
 * to VRAM during VBlank. Each entry stores the VRAM offset, the tile, and the
 * color. Only changed cells are written, so the VBlank pass is short and fits.
 * Max changes in one evolution is well under BN. */
static u16 chg_off[BN];
static u8  chg_tile[BN];
static u8  chg_pal[BN];
static u16 chg_count;

static void build_changes(void){
  u8 x,y;
  chg_count=0;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 cur=board[y*BW+x];
    if(cur!=prevboard[y*BW+x] || oncur){
      u8 t, pal;
      if(cur==BLUE){ t=oncur?T_BLUEC:T_BLUE; pal=1; }
      else if(cur==RED){ t=oncur?T_REDC:T_RED; pal=2; }
      else if(oncur){ u8 z=zone_of(x); t=T_CURSOR; pal=(z==BLUE)?1:(z==RED)?2:0; }
      else { u8 z=zone_of(x); t=empty_tile(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
      chg_off[chg_count]=(u16)(BOARD_OY+y)*32 + (BOARD_OX+x);
      chg_tile[chg_count]=t;
      chg_pal[chg_count]=pal;
      chg_count++;
    }
  }
}

/* Write up to `limit` queued changes starting at *idx, at VBlank. Writes tile
 * (bank 0) and color (bank 1) for each. Returns having advanced *idx. */
static void blit_changes(u16 *idx, u8 limit){
  u8 n=0; u8 gbc=is_gbc();
  while(*idx<chg_count && n<limit){
    u16 off=chg_off[*idx];
    VRAM_MAP[off]=chg_tile[*idx];        /* bank 0 (tile) */
    if(gbc){
      rVBK=1;
      VRAM_MAP[off]=chg_pal[*idx];       /* bank 1 (color) */
      rVBK=0;
    }
    (*idx)++; n++;
  }
}

/* Full-board buffers, built in RAM, then blitted to VRAM with the LCD off for
 * the shortest possible time: two tight contiguous passes (all tiles, then all
 * colors) with no per-cell rVBK toggling and no recompute inside the blank.
 * This guarantees correct color on real hardware; the blank is so short (a
 * couple hundred microseconds) it reads as a barely-perceptible flicker. */
static u8 tilebuf[BN];
static u8 palbuf[BN];
static void build_buffers(void){
  u8 x,y;
  for(y=0;y<BH;y++)for(x=0;x<BW;x++){
    u8 oncur=(x==cx && y==cy);
    u8 v=board[y*BW+x], t, pal;
    if(v==BLUE){ t=oncur?T_BLUEC:T_BLUE; pal=1; }
    else if(v==RED){ t=oncur?T_REDC:T_RED; pal=2; }
    else if(oncur){ u8 z=zone_of(x); t=T_CURSOR; pal=(z==BLUE)?1:(z==RED)?2:0; }
    else { u8 z=zone_of(x); t=empty_tile(x); pal=(z==BLUE)?1:(z==RED)?2:0; }
    tilebuf[y*BW+x]=t;
    palbuf[y*BW+x]=pal;
  }
}
static void blit_buffers(void){
  u8 x,y; u8 gbc=is_gbc();
  /* Sync to VBlank first, THEN turn the LCD off, so the off-period starts at the
   * blanking interval and is as short as possible — just the blit time. */
  wait_vbl();
  sfx_evolve();                  /* soft evolution tick: fires on every board
                                  * redraw flash (evolution + CPU placement),
                                  * cushioning the brief LCD blank so the flash
                                  * reads as a pulse, not a glitch. */
  rLCDC = 0x00;                  /* LCD off (already at VBlank, minimal blank) */
  print_at(0,0,hud0);
  print_at(0,1,hud1);
  for(y=0;y<BH;y++)for(x=0;x<BW;x++)
    VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = tilebuf[y*BW+x];
  if(gbc){
    rVBK=1;
    for(y=0;y<BH;y++)for(x=0;x<BW;x++)
      VRAM_MAP[(u16)(BOARD_OY+y)*32 + (BOARD_OX+x)] = palbuf[y*BW+x];
    rVBK=0;
  }
  rLCDC = LCDC_GAME;             /* LCD back on */
}

static void redraw_board_spread(u8 frames){
  u8 i;
  /* Build everything in RAM, then do the quick blits with the LCD off only for
   * that moment. Correct colors on hardware, with the smallest possible
   * flicker. */
  build_buffers();
  hud_compute();                 /* prepare HUD strings (no VRAM yet) */
  blit_buffers();
  for(i=0;i<frames;i++){ wait_vbl(); while(rLY==144){} }
}

static void anim_step_and_hold(u8 frames){
  snapshot_board();
  step();
  redraw_board_spread(frames);   /* all VBlank-timed, no LCD off, no flash */
}

/* show freshly-placed cells (already on the board) vs a prior snapshot, holding
 * for `frames`, again without blanking. Caller must snapshot before placing. */
static void anim_show_placed(u8 frames){
  redraw_board_spread(frames);   /* same VBlank-timed path, no flash */
}

/* Play one full CPU (RED) turn: think, place, animate, evolve. Returns with the
 * board evolved; the caller advances cur_player/round. Factored out so the CPU
 * can also take the OPENING turn when the start-of-game coin toss picks it. */
static void cpu_take_turn(void){
  snapshot_board();
  cpu_thinking_on();
  cpu_turn(RED);
  anim_show_placed(45);          /* ~0.75s: see what the CPU placed */
  anim_step_and_hold(35);        /* evolve after the CPU's placements */
}

static void next_turn(void){
  if(link_mode){
    /* ── Cable turn. ONE generation per turn, applied IDENTICALLY on both
     * consoles, in this fixed order so the two boards never diverge:
     *   1. (already done in S_PLAY) the active side placed its real cells onto
     *      its own board; the passive side placed nothing yet.
     *   2. Exchange placements over the wire (sync-gated, so the side that
     *      arrives first waits for the other instead of dropping the link).
     *   3. The PASSIVE side now applies the active side's placements to its
     *      board. The active side already has them. Both boards are now equal.
     *   4. BOTH evolve exactly one Conway generation.
     *   5. Advance turn / round.
     * Note: unlike the old code there is NO pre-exchange evolve — placing then
     * evolving is the canonical single-generation turn, and doing the evolve
     * only AFTER both boards hold the same placements is what keeps them in
     * lockstep. */
    u8 in_cnt, ix[LINK_CELLS], iy[LINK_CELLS];
    u8 r, i;
    u8 active = (cur_player == link_local);

    if(active){
      r = link_swap_move(placed, lmx, lmy, &in_cnt, ix, iy);
    } else {
      r = link_swap_move(0, lmx, lmy, &in_cnt, ix, iy);
    }
    if(r != LINK_OK){ link_lost=1; winner=0; state=S_GAMEOVER; redraw=1; return; }

    if(!active){
      /* apply the remote player's placements (snapshot first so the animation
       * shows only the newly-arrived cells, matching the local placement feel) */
      snapshot_board();
      for(i=0;i<in_cnt;i++){
        u8 px=ix[i], py=iy[i];
        if(px<BW && py<BH && can_place(cur_player,px,py)) board[py*BW+px]=cur_player;
      }
      anim_show_placed(45);
    }

    /* both boards are now identical -> evolve ONE generation on both */
    anim_step_and_hold(35);
    check_end();
    if(state==S_GAMEOVER) return;

    if(cur_player==BLUE){ cur_player=RED; placed=0; }
    else { cur_player=BLUE; round_no++; placed=0; }
    check_end();
    return;
  }

  /* ── Non-link (vs-CPU / hot-seat): evolve after the player's placements. */
  /* 1) evolve after the current player's placements — animated, no flash */
  anim_step_and_hold(35);          /* ~0.6s: watch your move take effect */
  check_end();
  if(state==S_GAMEOVER) return;

  if(vs_cpu){
    if(cpu_first){
      /* CPU opened this round (in S_PLAY); the human just played second, so the
       * round is complete. Advance the round and hand back to RED, who opens the
       * next round from S_PLAY. (next_turn is only called here for the human's
       * turn, since the CPU turn is driven from S_PLAY in this mode.) */
      cur_player=RED; round_no++; placed=0;
    } else {
      /* Player (BLUE) opened; now the CPU (RED) replies, then the round ends. */
      cur_player=RED; placed=0;
      cpu_take_turn();
      check_end();
      if(state==S_GAMEOVER) return;
      cur_player=BLUE; round_no++; placed=0;
    }
  } else {
    /* local hot-seat: simple alternation */
    if(cur_player==BLUE){ cur_player=RED; placed=0; }
    else { cur_player=BLUE; round_no++; placed=0; }
  }
  check_end();
}
static void start_game(void){
  mel_stop(); mel_playing=0;     /* silence the menu music — gameplay is SFX only */
  if(!is_gbc()) rBGP = 0xE4;     /* restore normal palette (title inverts it) */
  link_mode=0;                   /* plain CPU / local hot-seat game */
  setup_board();
  round_no=1; placed=0; winner=0;
  skip_used[0]=0; skip_used[1]=0;   /* each side gets one skip this game */
  cx=3; cy=BH/2;
  /* vs-CPU: coin toss for who opens. You're always BLUE; the CPU (RED) may move
   * first each round — like the original. When the CPU opens, the game starts on
   * RED's turn and S_PLAY drives the CPU move (after the board's first paint, so
   * it never animates onto a blank screen). Both orders give 12 turns each. */
  cpu_first = (vs_cpu && (rnd()&1));
  cur_player = cpu_first ? RED : BLUE;
  state=S_PLAY; redraw=1;
}

/* Begin a cable game once the handshake has succeeded. The host plays BLUE and
 * moves first; the joiner plays RED. Both call this with the same board setup
 * so the two simulations start identical. */
static void start_link_game(void){
  mel_stop(); mel_playing=0;
  if(!is_gbc()) rBGP = 0xE4;
  link_mode=1; link_lost=0;
  link_local = link_is_master ? BLUE : RED;
  vs_cpu=0;
  setup_board();
  cur_player=BLUE; round_no=1; placed=0; winner=0;
  skip_used[0]=0; skip_used[1]=0;   /* each side gets one skip this game */
  cx = (link_local==BLUE) ? 3 : (BW-4);   /* start the cursor in your own zone */
  cy = BH/2;
  state=S_PLAY; redraw=1;
}

/* The 2-player submenu: local hot-seat, host a cable game, or join one. */
static void draw_2pmenu(void){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4); else rBGP = 0xE4;
  print_at(4,2,"2 PLAYERS");
  deco_header(4,2,9);
  print_at(5,7,"SAME GAME BOY");
  print_at(5,9,"LINK: HOST");
  print_at(5,11,"LINK: JOIN");
  map_put(4,7,T_DIAM); map_put(4,9,T_DIAM); map_put(4,11,T_DIAM);
  print_at(3,(u8)(7+menu_sel*2),">");
  deco_footer(14);
  print_at(2,16,"B: BACK");
  lcd_on();
}

/* Lobby / handshake screen. The joiner sits here waiting to be clocked; the
 * host attempts the handshake and, on success, both jump into the game. */
static void draw_linkwait(u8 as_host){
  lcd_off();
  clear_map();
  if(is_gbc()) clear_attr(4); else rBGP = 0xE4;
  print_at(4,2,"LINK CABLE");
  deco_header(4,2,10);
  if(as_host){
    print_at(2,6,"HOSTING...");
    print_at(2,8,"JOINER: PRESS JOIN");
    print_at(2,10,"THEN PRESS A HERE");
  } else {
    print_at(2,6,"JOINING...");
    print_at(2,8,"WAITING FOR HOST");
  }
  deco_footer(16);
  print_at(2,14,"B: CANCEL");
  lcd_on();
}

/* ───────────────────── Init ───────────────────── */
static void load_tiles(void){
  u8 i;
  set_tile(T_BLANK,patt_blank);
  set_tile(T_BLUE,patt_blue);
  set_tile(T_RED,patt_red);
  set_tile(T_CURSOR,patt_cursor);
  set_tile(T_GRID,patt_grid);
  set_tile(T_GRIDB,patt_gridb);
  set_tile(T_GRIDR,patt_gridr);
  set_tile(T_GRIDBD,patt_gridbd);
  set_tile(T_GRIDRD,patt_gridrd);
  set_tile(T_GRIDH_D,patt_gridh_d);
  set_tile(T_GRIDBD_D,patt_gridbd_d);
  set_tile(T_GRIDRD_D,patt_gridrd_d);
  set_tile(T_WALL,patt_wall);
  set_tile(T_BORDER,patt_border);
  set_tile(T_STAR,patt_star);
  set_tile(T_TGRID,patt_tgrid);
  set_tile(T_STAR2,patt_tgrid2);
  set_tile(T_SPARK,patt_spark);
  set_tile(T_HBAR,patt_hbar);
  set_tile(T_HBARB,patt_hbarb);
  set_tile(T_HBART,patt_hbart);
  set_tile(T_BLUEC,patt_bluec);
  set_tile(T_REDC,patt_redc);
  set_tile(T_DECOL,patt_decol);
  set_tile(T_DECOR,patt_decor);
  set_tile(T_DIAM,patt_diam);
  for(i=0;i<GLYPH_COUNT;i++) set_font_tile(T_FONT_BASE+i,FONT[i]);
}

/* Define the color palettes (GBC only). Tiles are drawn with 2 bitplanes, so
 * each tile uses colors 0..3 of its palette. Our 1bpp art uses color 0 (light/
 * background) and color 3 (ink); we set 1 and 2 to in-between shades.
 *   pal 0 — neutral: pale green board (classic GB tint) with dark ink
 *   pal 1 — blue: light blue field, deep blue ink (blue colony & home)
 *   pal 2 — red:  light red field, deep red ink (red colony & home)
 *   pal 3 — gold: title accent (crow/logo highlight)
 *   pal 4 — paper: white field for menus/text */
static void init_palettes(void) {
  if (!is_gbc()) return;
  /* RGB5(r,g,b), 0..31 each. Color 0 = field (light), color 3 = ink (dark). */
  set_bg_palette(0, RGB5(20,26,16), RGB5(13,20,10), RGB5(6,12,5),  RGB5(2,5,2));    /* green board */
  /* Blue & red colonies pushed to vivid, well-separated hues so they're easy to
   * tell apart even on a dim screen: pale tinted field (color 0) up to a bright
   * saturated colony ink (color 3) — electric blue vs. pure red, ~2x further
   * apart in color than before. */
  set_bg_palette(1, RGB5(21,27,31), RGB5(10,17,31), RGB5(3,9,31),   RGB5(0,4,24));   /* blue */
  set_bg_palette(2, RGB5(31,24,20), RGB5(31,12,9),  RGB5(31,3,3),   RGB5(22,0,0));   /* red */
  set_bg_palette(3, RGB5(13,14,18), RGB5(20,21,25), RGB5(26,27,30), RGB5(31,31,31)); /* soft light-grey highlight */
  set_bg_palette(4, RGB5(31,31,31), RGB5(21,21,21), RGB5(10,10,10),RGB5(0,0,0));    /* paper */
  /* dark "cartridge label" palettes for the title screen:
   * color 0 = near-black field, 3 = bright ink, so text/art glows on dark. */
  set_bg_palette(5, RGB5(2,2,5),   RGB5(7,7,12),   RGB5(16,16,22), RGB5(30,30,31)); /* dark + white ink */
  set_bg_palette(6, RGB5(2,3,7),   RGB5(6,12,26),  RGB5(12,20,31), RGB5(22,28,31)); /* dark + cyan/blue glow */
  set_bg_palette(7, RGB5(6,2,4),   RGB5(24,6,6),   RGB5(31,12,8),  RGB5(31,22,18)); /* dark + red/orange glow */
}

void main(void){
  u8 pad, prev=0, pressed;

  rLCDC=0x00;
  rBGP=0xE4;
  load_tiles();
  init_palettes();
  sound_init();
  clear_map();
  stats_load();          /* pull the persistent record from battery SRAM */

  rng=0x1234;
  rng ^= ((u16)rDIV<<8)|rDIV|1;

  state=S_TITLE; redraw=1;
  /* Nudge the whole frame down 1px: SCY wraps, so 256-1 shows 1 blank px at the
   * top (a touch of air above the HUD) and trims 1px off the bottom border line,
   * which is just decorative. clear_map blanks the full 32x32 map, so the
   * wrapped-in row above the content is clean (no garbage). */
  rSCX=0; rSCY=0xFF;
  rLCDC=LCDC_ON|LCDC_BG_ON|LCDC_BG_DATA_8000;

  for(;;){
    wait_vbl();
    pad=read_pad();
    pressed=(u8)(pad & ~prev);
    prev=pad;
    rng += pad; rng ^= rng<<3;

    switch(state){
      case S_TITLE:
        if(redraw){ anim=0; draw_title(); gymno_start(); redraw=0; }
        anim++;
        gymno_tick();                          /* Silent-Hill-style theme, looping */
        /* twinkle the sparkle constellation between the gliders */
        if(anim==20){ map_put(9,7,T_SPARK); map_put(10,9,T_SPARK);
                      map_put(10,7,T_STAR); map_put(9,9,T_STAR); }
        else if(anim==40){ map_put(9,7,T_STAR);  map_put(10,9,T_STAR);
                           map_put(10,7,T_SPARK); map_put(9,9,T_SPARK); anim=0; }
        if(pressed & BTN_START){ sfx_select(); rSCY=0xFF; state=S_MENU; menu_sel=0; redraw=1; }
        break;
      case S_MENU:
        if(redraw){ draw_menu(); if(!mel_playing) gymno_start(); redraw=0; }
        gymno_tick();
        {
          u8 oldsel=menu_sel, moved=0;
          if(pressed & PAD_UP){ if(menu_sel>0)menu_sel--; sfx_move(); moved=1; }
          if(pressed & PAD_DOWN){ if(menu_sel<5)menu_sel++; sfx_move(); moved=1; }
          if(moved && menu_sel!=oldsel){
            /* move the '>' marker only — plain menu, no color to change */
            u8 oy=(u8)(4+oldsel*2), ny=(u8)(4+menu_sel*2);
            wait_vbl();
            map_put(2,oy,T_BLANK);                       /* erase old marker */
            map_put(2,ny,T_FONT_BASE+glyph_of('>'));     /* draw new marker */
          }
        }
        if(pressed & BTN_A){
          sfx_select();
          if(menu_sel==0){ vs_cpu=1; menu_sel=1; state=S_DIFF; redraw=1; }
          else if(menu_sel==1){ menu_sel=0; state=S_2PMENU; redraw=1; }
          else if(menu_sel==2){ help_page=0; state=S_HELP; redraw=1; }
          else if(menu_sel==3){ stats_reset_hold=0; state=S_STATS; redraw=1; }
          else if(menu_sel==4){ state=S_CREDITS; redraw=1; }
          else { state=S_ACK; redraw=1; }
        }
        break;
      case S_2PMENU:
        if(redraw){ draw_2pmenu(); redraw=0; }
        gymno_tick();
        {
          u8 oldsel=menu_sel, moved=0;
          if(pressed & PAD_UP){ if(menu_sel>0)menu_sel--; sfx_move(); moved=1; }
          if(pressed & PAD_DOWN){ if(menu_sel<2)menu_sel++; sfx_move(); moved=1; }
          if(moved && menu_sel!=oldsel){
            u8 oy=(u8)(7+oldsel*2), ny=(u8)(7+menu_sel*2);
            wait_vbl();
            map_put(3,oy,T_BLANK);
            map_put(3,ny,T_FONT_BASE+glyph_of('>'));
          }
        }
        if(pressed & BTN_A){
          sfx_select();
          if(menu_sel==0){ vs_cpu=0; start_game(); }              /* local hot-seat */
          else if(menu_sel==1){ menu_sel=1; link_host_armed=0; state=S_LINKWAIT; redraw=1; }  /* host */
          else { menu_sel=2; link_host_armed=0; state=S_LINKWAIT; redraw=1; }                 /* join */
        }
        if(pressed & BTN_B){ sfx_back(); state=S_MENU; menu_sel=0; redraw=1; }
        break;
      case S_LINKWAIT:
        {
          u8 as_host = (menu_sel==1);
          if(redraw){ draw_linkwait(as_host); redraw=0; }
          gymno_tick();
          /* Host kicks off the handshake when it presses A (after telling the
           * joiner to be ready). The joiner attempts the handshake every frame,
           * parking on the slave xfer until the host's clock arrives. */
          if(as_host){
            /* Press A once to ARM hosting; from then on we attempt the
             * handshake every frame (each link_begin already retries internally
             * and is timeout-bounded). This removes the original single-shot
             * race where one mistimed A-press, with the joiner momentarily not
             * armed, failed and left the host idle. B still cancels. */
            if(pressed & BTN_A){ link_host_armed=1; }
            if(link_host_armed){
              u8 r = link_begin(1);          /* we are master/host */
              if(r==LINK_OK){ sfx_select(); start_link_game(); }
              /* not OK yet: stay armed and keep trying next frame, silently */
            }
          } else {
            /* joiner: try once per frame; the call is timeout-bounded so it
             * returns control to the loop (and the B-to-cancel check) quickly */
            u8 r = link_begin(0);            /* we are slave/joiner */
            if(r==LINK_OK){ sfx_select(); start_link_game(); }
          }
          if(pressed & BTN_B){ sfx_back(); link_host_armed=0; state=S_2PMENU; menu_sel=0; redraw=1; }
        }
        break;
      case S_DIFF:
        if(redraw){ draw_diff(); redraw=0; }
        gymno_tick();
        {
          u8 oldsel=menu_sel, moved=0;
          if(pressed & PAD_UP){ if(menu_sel>0)menu_sel--; sfx_move(); moved=1; }
          if(pressed & PAD_DOWN){ if(menu_sel<2)menu_sel++; sfx_move(); moved=1; }
          if(moved && menu_sel!=oldsel){
            u8 oy=(u8)(7+oldsel*2), ny=(u8)(7+menu_sel*2);
            wait_vbl();
            map_put(3,oy,T_BLANK);
            map_put(3,ny,T_FONT_BASE+glyph_of('>'));
          }
        }
        if(pressed & BTN_A){ sfx_select(); difficulty=menu_sel; start_game(); }
        if(pressed & BTN_B){ sfx_back(); state=S_MENU; menu_sel=0; redraw=1; }
        break;
      case S_HELP:
        if(redraw){ draw_help(); redraw=0; }
        gymno_tick();
        if(pressed & BTN_A){
          if(help_page==0){ sfx_select(); help_page=1; redraw=1; }  /* page 1 -> 2 */
          else { sfx_back(); state=S_MENU; redraw=1; }              /* page 2 -> menu */
        }
        if(pressed & BTN_B){
          if(help_page==1){ sfx_back(); help_page=0; redraw=1; }    /* page 2 -> 1 */
          else { sfx_back(); state=S_MENU; redraw=1; }              /* page 1 -> menu */
        }
        break;
      case S_CREDITS:
        if(redraw){ draw_credits(); redraw=0; }
        gymno_tick();
        if(pressed & BTN_B){ sfx_back(); state=S_MENU; menu_sel=0; redraw=1; }
        break;
      case S_ACK:
        if(redraw){ draw_ack(); redraw=0; }
        gymno_tick();
        if(pressed & BTN_B){ sfx_back(); state=S_MENU; menu_sel=5; redraw=1; }
        break;
      case S_STATS:
        if(redraw){ draw_stats(); redraw=0; }
        gymno_tick();
        /* hold SELECT ~1.5s to wipe the record; release early cancels */
        if(pad & BTN_SELECT){
          stats_reset_hold++;
          if(stats_reset_hold==90){
            stats_reset();
            sfx_error();              /* audible confirmation of the wipe */
            redraw=1;                 /* repaint with zeroed counters */
          }
        } else {
          stats_reset_hold=0;
        }
        if(pressed & BTN_B){ sfx_back(); state=S_MENU; menu_sel=0; redraw=1; }
        break;
      case S_PAUSE:
        if(redraw){ draw_pause(); redraw=0; }
        if(pressed & (PAD_UP|PAD_DOWN)){
          menu_sel ^= 1;            /* toggle between RESUME(0) and QUIT(1) */
          sfx_move();
          redraw=1;
        }
        if(pressed & BTN_B){         /* B = resume immediately */
          sfx_back();
          state=S_PLAY; redraw=1;    /* S_PLAY redraw repaints the board */
        }
        if(pressed & BTN_A){
          if(menu_sel==0){           /* RESUME */
            sfx_select();
            state=S_PLAY; redraw=1;
          } else {                   /* QUIT TO MENU */
            sfx_select();
            mel_stop(); mel_playing=0;
            state=S_MENU; menu_sel=0; redraw=1;
          }
        }
        break;
      case S_PLAY:
        if(redraw){
          lcd_off();
          clear_map();
          draw_hud();
          draw_board();                  /* draws tiles and (on GBC) all colors */
          lcd_on();
          redraw=0;
        }
        /* If the CPU opens the round (coin toss), let RED play first now that the
         * board is painted, then hand control to the player as BLUE. Runs at the
         * start of every round when cpu_first is set; cleared after each CPU move
         * and re-armed by next_turn for the next round. */
        if(cpu_first && vs_cpu && cur_player==RED){
          cpu_take_turn();
          check_end();
          if(state==S_GAMEOVER){ break; }
          cur_player=BLUE; placed=0;
          wait_vbl(); draw_cell(cx,cy); draw_hud();
        }
        {
          u8 ocx=cx, ocy=cy, moved=0;
          /* ── Link game: if it isn't this console's turn, don't read the pad.
           * Run the symmetric exchange to receive the remote player's move
           * (next_turn handles applying it and advancing). Both consoles reach
           * their matching link_swap_move call this way. */
          if(link_mode && cur_player!=link_local){
            next_turn();
            if(state==S_PLAY){ redraw=1; }   /* remote move applied; refresh */
            break;
          }
          if(pressed & PAD_LEFT){ if(cx>0)cx--; moved=1; }
          if(pressed & PAD_RIGHT){ if(cx<BW-1)cx++; moved=1; }
          if(pressed & PAD_UP){ if(cy>0)cy--; moved=1; }
          if(pressed & PAD_DOWN){ if(cy<BH-1)cy++; moved=1; }
          if(pressed & BTN_A){
            if(placed<CELLS_PER_TURN && can_place(cur_player,cx,cy)){
              board[cy*BW+cx]=cur_player; 
              /* in a link game, remember this placement so we can ship the
               * whole turn to the other Game Boy when the player ends it */
              if(link_mode){ lmx[placed]=cx; lmy[placed]=cy; }
              placed++;
              sfx_place();
              wait_vbl();
              draw_cell(cx,cy);          /* just this cell, no screen flash */
              draw_hud();                /* HUD is small text, safe in VBlank */
              /* Cells can't be repositioned once placed, so there's nothing to
               * confirm: placing the last cell auto-ends the turn. (SELECT can
               * still skip remaining placements, once per game.) */
              if(placed>=CELLS_PER_TURN){
                next_turn();
                if(state==S_PLAY){ wait_vbl(); draw_cell(cx,cy); draw_hud(); }
              }
            } else {
              sfx_error();   /* can't place here — alarm */
            }
          }
          if(pressed & BTN_SELECT){
            /* SKIP: end the turn early, once per game per player. The board
             * still evolves (next_turn handles it); the skip just forgoes any
             * remaining placements and is capped at one use. */
            u8 si = (cur_player==BLUE)?0:1;
            if(!skip_used[si]){
              skip_used[si]=1;
              sfx_select();
              next_turn();
              if(state==S_PLAY){ wait_vbl(); draw_cell(cx,cy); draw_hud(); }
            } else {
              sfx_error();             /* skip already spent this game */
            }
          }
          /* START opens the pause menu (return-to-menu prompt). Not in link
           * games: pausing one console would desync the cable. The turn still
           * auto-advances on the 4th placement, so START is free to repurpose. */
          if((pressed & BTN_START) && !link_mode){
            sfx_select();
            state=S_PAUSE; menu_sel=0; redraw=1;
          }
          if(moved){
            /* redraw only the old cell (cursor gone) and the new cell (cursor
             * here). Wait for VBlank first so the 2 bank-1 color writes land
             * reliably on hardware — only two tiles, so no flicker. */
            wait_vbl();
            draw_cell(ocx,ocy);
            draw_cell(cx,cy);
          }
        }
        break;
      case S_GAMEOVER:
        if(redraw){
          draw_gameover();
          redraw=0;
          /* tally this finished game into the persistent record (skip games
           * voided by a dropped cable). vs_cpu still reflects the mode here. */
          if(!(link_mode && link_lost)){
            stats_record(vs_cpu, winner);
          }
          /* music: in vs-CPU you are BLUE, so a non-blue result is a loss ->
           * funeral march. In 2-player there's always a loser, so the march
           * plays unless it's a draw. A blue win (vs CPU) gets a short flourish. */
          if(link_mode && link_lost){ /* cable dropped: silence */ }
          else if(winner==0){ /* draw: silence */ }
          else if(link_mode){
            /* each console plays its own colour: your win = flourish, else march */
            if(winner==link_local){ sfx_win(); win_jingle=0; }
            else { funeral_start(); }
          }
          else if(vs_cpu){
            if(winner==BLUE){ sfx_win(); win_jingle=0; }
            else { funeral_start(); }   /* you lost — cue Chopin */
          } else {
            funeral_start();            /* 2P: someone lost, play the march */
          }
        }
        funeral_tick();
        if(pressed & BTN_START){
          mel_stop(); mel_playing=0;     /* cut the music */
          sfx_select(); state=S_MENU; menu_sel=0; redraw=1;
        }
        else if((pressed & BTN_A) && !link_mode){
          /* Rematch: only in vs-CPU / local hot-seat. start_game() keeps the
           * current vs_cpu and difficulty, so it replays the same matchup. */
          mel_stop(); mel_playing=0;
          sfx_select(); start_game();
        }
        break;
      default: break;
    }

    while(rLY==144){}
  }
}
