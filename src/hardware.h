/* hardware.h — Game Boy memory-mapped hardware registers (DMG) */
#ifndef HARDWARE_H
#define HARDWARE_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef signed char    i8;

/* memory-mapped I/O register accessor */
#define REG(addr) (*(volatile u8 *)(addr))

/* LCD / video */
#define rLCDC   REG(0xFF40)   /* LCD control */
#define rSTAT   REG(0xFF41)   /* LCD status */
#define rSCY    REG(0xFF42)   /* scroll Y */
#define rSCX    REG(0xFF43)   /* scroll X */
#define rLY     REG(0xFF44)   /* current scanline */
#define rLYC    REG(0xFF45)
#define rBGP    REG(0xFF47)   /* BG palette */
#define rOBP0   REG(0xFF48)   /* sprite palette 0 */
#define rOBP1   REG(0xFF49)
#define rWY     REG(0xFF4A)
#define rWX     REG(0xFF4B)
#define rIE     REG(0xFFFF)   /* interrupt enable */
#define rIF     REG(0xFF0F)
#define rDIV    REG(0xFF04)   /* divider — handy as an entropy source */

/* joypad */
#define rP1     REG(0xFF00)

/* MBC3 cartridge control (for battery-backed SRAM persistence) */
#define rRAMG   REG(0x0000)   /* write 0x0A to enable cart RAM, 0x00 to disable */
#define rROMB   REG(0x2000)   /* ROM bank select */
#define rRAMB   REG(0x4000)   /* RAM bank select */
#define SRAM    ((volatile u8 *)0xA000)   /* 8KB battery-backed cart RAM window */

/* sound — channel 1 (square wave with sweep), channel 2 (square), and master */
#define rNR10   REG(0xFF10)   /* ch1 sweep */
#define rNR11   REG(0xFF11)   /* ch1 duty + length */
#define rNR12   REG(0xFF12)   /* ch1 volume envelope */
#define rNR13   REG(0xFF13)   /* ch1 frequency low */
#define rNR14   REG(0xFF14)   /* ch1 frequency high + trigger */
#define rNR21   REG(0xFF16)   /* ch2 duty + length */
#define rNR22   REG(0xFF17)   /* ch2 volume envelope */
#define rNR23   REG(0xFF18)   /* ch2 frequency low */
#define rNR24   REG(0xFF19)   /* ch2 frequency high + trigger */
#define rNR50   REG(0xFF24)   /* master volume L/R */
#define rNR51   REG(0xFF25)   /* sound panning */
#define rNR52   REG(0xFF26)   /* sound on/off */

/* Game Boy Color registers */
#define rKEY1   REG(0xFF4D)   /* speed switch */
#define rVBK    REG(0xFF4F)   /* VRAM bank select (0/1) */
#define rBCPS   REG(0xFF68)   /* background palette spec (index + auto-inc) */
#define rBCPD   REG(0xFF69)   /* background palette data */
#define rOCPS   REG(0xFF6A)   /* object palette spec */
#define rOCPD   REG(0xFF6B)   /* object palette data */
/* the boot ROM leaves A=0x11 on a GBC/GBA, 0x01 on DMG; we capture it at start */

/* LCDC bits */
#define LCDC_ON        0x80
#define LCDC_BG_ON     0x01
#define LCDC_OBJ_ON    0x02
#define LCDC_BG_MAP_9800  0x00
#define LCDC_BG_DATA_8000 0x10  /* tile data at 0x8000, unsigned index */

/* video memory */
#define VRAM_TILES  ((volatile u8 *)0x8000)   /* tile pattern table */
#define VRAM_MAP    ((volatile u8 *)0x9800)    /* BG tile map (32x32) */

/* joypad bits (active low) */
#define P1_DPAD    0x20
#define P1_BTN     0x10
/* joypad bits in the value returned by read_pad():
 * low nibble = d-pad, high nibble = action buttons. */
#define PAD_RIGHT  0x01
#define PAD_LEFT   0x02
#define PAD_UP     0x04
#define PAD_DOWN   0x08
#define BTN_A      0x10
#define BTN_B      0x20
#define BTN_SELECT 0x40
#define BTN_START  0x80

#endif
