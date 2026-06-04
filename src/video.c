#include <gba_video.h>
#include <gba_dma.h>
#include "video.h" // Aquí ya están definidos FRONT y BACK

#define BACKBUFFER 0x10
#define PAL_BG   ((volatile uint16_t*)0x05000000)
#define PAL_SIZE 256

static uint8_t page = 1;

// Paleta guardada por fade_out, usada por fade_in
static uint16_t pal_saved[PAL_SIZE];

// --------------------------------------------------
// VRAM / flip
// --------------------------------------------------

uint16_t* get_vram(void) {
    return page ? BACK : FRONT;
}

// Espera VBlank sin hacer page swap.
// Usar antes de escribir paleta o hacer flip_no_vsync().
void vsync_only(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// Page swap sin vsync propio.
// Llamar SIEMPRE después de vsync_only() para que el swap
// ocurra en el VBlank y no a mitad de frame.
void flip_no_vsync(void) {
    if (page) {
        REG_DISPCNT |=  BACKBUFFER;
        page = 0;
    } else {
        REG_DISPCNT &= ~BACKBUFFER;
        page = 1;
    }
}

// vsync + page swap atómico. Equivalente al flip() original.
// Mantener para compatibilidad con código que no necesita
// escribir paleta entre el vsync y el swap.
void flip(void) {
    vsync_only();
    flip_no_vsync();
}

// --------------------------------------------------
// Primitivas de dibujo
// --------------------------------------------------

void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    if (x0 < 0)   x0 = 0;
    if (x1 > 240) x1 = 240;
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= 160) continue;
        for (int xx = x0; xx < x1; xx += 2)
            vram[yy * 120 + xx / 2] = packed;
    }
}

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

void fill_rect(uint16_t* vram, int x, int y, int w, int h, uint8_t c) {
    rect(vram, x, y, w, h, c);
}

void clear(uint16_t* vram, uint8_t color) {
    uint16_t packed = ((uint16_t)color << 8) | color;
    for (int i = 0; i < 19200; i++)
        vram[i] = packed;
}

void clear_vram_con_color(uint16_t* vram, uint8_t color) {
    uint16_t packed = ((uint16_t)color << 8) | color;
    for (int i = 0; i < 19200; i++)
        vram[i] = packed;
}

// --------------------------------------------------
// Helpers internos
// --------------------------------------------------

static uint16_t scale_color(uint16_t col, int factor) {
    int r = (col      ) & 0x1F;
    int g = (col >>  5) & 0x1F;
    int b = (col >> 10) & 0x1F;
    r = (r * factor) >> 4;
    g = (g * factor) >> 4;
    b = (b * factor) >> 4;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

static void wait_vblanks(int n) {
    for (int i = 0; i < n; i++) {
        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
    }
}

// --------------------------------------------------
// Fade separado para transiciones entre menús
//
// Uso:
//   fade_out();                // oscurece la pantalla actual
//   // <--- aquí dibuja el nuevo menú en el back buffer, sin flip
//   wait_vblanks(60);         // pausa en negro (opcional, ponla aquí fuera)
//   fade_in();                 // aparece el nuevo menú
// --------------------------------------------------

void fade_out(void) {
    // 1. Guardar paleta
    for (int i = 0; i < PAL_SIZE; i++)
        pal_saved[i] = PAL_BG[i];

    // 2. Fade out sobre lo que hay visible — sin tocar buffers
    for (int step = 16; step >= 0; step--) {
        for (int i = 0; i < PAL_SIZE; i++)
            PAL_BG[i] = scale_color(pal_saved[i], step);
        wait_vblanks(2);
    }

    // 3. Ahora que ya estamos en negro, limpiar ambos buffers
    //    El LCD no ve nada porque la paleta es todo ceros
    for (int i = 0; i < 19200; i++) {
        FRONT[i] = 0;
        BACK[i]  = 0;
    }
}

void fade_in(void) {
    uint16_t pal_target[PAL_SIZE];
    for (int i = 0; i < PAL_SIZE; i++)
        pal_target[i] = PAL_BG[i];

    // Esperamos al VBlank, y en ese mismo VBlank ponemos negro Y hacemos flip
    // así ambos cambios ocurren antes de que empiece el siguiente frame
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
    for (int i = 0; i < PAL_SIZE; i++)
        PAL_BG[i] = 0;
    // flip manual en lugar de llamar a flip() para no consumir otro VBlank
    if (page) {
        REG_DISPCNT |=  BACKBUFFER;
        page = 0;
    } else {
        REG_DISPCNT &= ~BACKBUFFER;
        page = 1;
    }

    wait_vblanks(60);

    for (int step = 0; step <= 16; step++) {
        for (int i = 0; i < PAL_SIZE; i++)
            PAL_BG[i] = scale_color(pal_target[i], step);
        wait_vblanks(2);
    }

    for (int i = 0; i < PAL_SIZE; i++)
        PAL_BG[i] = pal_target[i];
}
