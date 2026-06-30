;; crt0.s — Game Boy startup + cartridge header for SDCC/sm83 (hand-written)

        .module crt0
        .globl  _main
        .globl  l__INITIALIZER
        .globl  s__INITIALIZER
        .globl  s__INITIALIZED

        .area _HEADER0 (ABS)
        .org    0x0040
        reti
        .org    0x0048
        reti
        .org    0x0050
        reti
        .org    0x0058
        reti
        .org    0x0060
        reti

        .org    0x0100
        nop
        jp      _start

        .org    0x0104
        .db 0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D
        .db 0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99
        .db 0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E

        .org    0x0134
        .ascii  "TC LIFE"
        .org    0x013B
        .db 0,0,0,0,0,0,0,0
        .org    0x0143
        .db 0x80                ; CGB flag (makebin -yc also sets this)
        .org    0x0144
        .db 0x00,0x00
        .db 0x00
        .db 0x00
        .db 0x00
        .db 0x00
        .db 0x01
        .db 0x33
        .db 0x00
        .db 0x00
        .db 0x00,0x00

        .org    0x0150
_start:
        di
        ld      sp,#0xFFFE

        ;; the boot ROM leaves A=0x11 on GBC/GBA, 0x01 on DMG.
        ;; stash it at 0xDFFF (top of WRAM, away from program data) so C code
        ;; can read whether we're on a GBC.
        ld      (0xDFFF), a

        ld      bc, #l__INITIALIZER
        ld      a, b
        or      a, c
        jr      z, init_done
        ld      hl, #s__INITIALIZER
        ld      de, #s__INITIALIZED
copy_loop:
        ld      a, (hl+)
        ld      (de), a
        inc     de
        dec     bc
        ld      a, b
        or      a, c
        jr      nz, copy_loop
init_done:

        call    gsinit
        call    _main
hang:
        halt
        jr      hang

        .area _CODE
        .area _HOME
        .area _GSINIT
        .area _GSFINAL
        .area _DATA
        .area _INITIALIZED
        .area _INITIALIZER
        .area _BSS
        .area _HEAP

        .area _GSINIT
gsinit::
        .area _GSFINAL
        ret
