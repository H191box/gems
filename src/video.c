#include <gba_video.h>
#include "video.h"

#define FRONT ((uint16_t*)0x06000000)
#define BACK  ((uint16_t*)0x0600A000)
#define BACKBUFFER 0x10

static uint8_t page = 1;

uint16_t* get_vram(void) {
    return page ? BACK : FRONT;
}

void flip(void) {
    while (REG_VCOUNT < 160);
    if (page) {
        REG_DISPCNT |= BACKBUFFER;
        page = 0;
    } else {
        REG_DISPCNT &= ~BACKBUFFER;
        page = 1;
    }
}

// Dibuja un rectángulo (tu función original)
void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    
    if (x0 < 0) x0 = 0;
    if (x1 > 240) x1 = 240;

    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= 160) continue;
        for (int xx = x0; xx < x1; xx += 2) {
            vram[yy * 120 + xx / 2] = packed;
        }
    }
}

// Dibuja una línea vertical
void vline(uint16_t* vram, int x, int y, int h, uint8_t c) {
    if (x < 0 || x >= 240) return;
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= 160) continue;
        int idx = yy * 120 + x / 2;
        if (x & 1) 
            vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)c << 8);
        else       
            vram[idx] = (vram[idx] & 0xFF00) | c;
    }
}

// Rellena un área (wrapper de rect)
void fill_rect(uint16_t* vram, int x, int y, int w, int h, uint8_t c) {
    rect(vram, x, y, w, h, c);
}

// Limpiar VRAM completa
void clear(uint16_t* vram, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    for (int i = 0; i < 19200; i++) {
        vram[i] = packed;
    }
}

void fundido_a_negro(void) {
    REG_BLDCNT = 0x0040 | 0x000F; 
    for (int i = 0; i < 16; i++) {
        REG_BLDY = i; 
        for (int v = 0; v < 4; v++) {
            while (REG_VCOUNT < 160);
        }
    }
}
