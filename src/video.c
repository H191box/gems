#include <gba_video.h>
#include "video.h"

#define FRONT ((uint16_t*)0x06000000)
#define BACK  ((uint16_t*)0x0600A000)

#define BACKBUFFER 0x10

// page=1 significa que FRONT es el visible y BACK es donde dibujamos.
static uint8_t page = 1;

uint16_t* get_vram(void) {
    return page ? BACK : FRONT;
}

void flip(void) {
    // Esperar al V-Blank para evitar parpadeo (tearing)
    while (REG_VCOUNT < 160);

    if (page) {
        REG_DISPCNT |= BACKBUFFER;  // mostrar BACK
        page = 0;                   // próxima vez dibujamos en FRONT
    } else {
        REG_DISPCNT &= ~BACKBUFFER; // mostrar FRONT
        page = 1;                   // próxima vez dibujamos en BACK
    }
}

// Función compartida para dibujar rectángulos en cualquier buffer
void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color) {
    // Combinamos dos píxeles de 8 bits en un solo valor de 16 bits para eficiencia
    uint16_t packed = (color << 8) | color;
    
    // Aseguramos alineación de 16 bits para evitar problemas de acceso
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    
    // Límites de seguridad para evitar escribir fuera de memoria
    if (x0 < 0) x0 = 0;
    if (x1 > 240) x1 = 240;

    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= 160) continue; // Salto si está fuera de pantalla vertical
        for (int xx = x0; xx < x1; xx += 2) {
            vram[yy * 120 + xx / 2] = packed;
        }
    }
} 



void fundido_a_negro(void) {
    // 1. Configurar mezcla para oscurecer (MODO 2)
    // BLDCNT: Bits 0-5 capas, bits 6-7 modo (10 = oscurecer)
    REG_BLDCNT = 0x0040 | 0x000F; 
    
    for (int i = 0; i < 16; i++) {
        REG_BLDY = i; 
        
        // Esperar V-Blank 2 veces para que sea más lento y visible
        for (int v = 0; v < 4; v++) {
            while (REG_VCOUNT < 160);
        }
    }
}

void clear(uint16_t* vram, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    for (int i = 0; i < 19200; i++) { // 240 * 160 / 2 píxeles
        vram[i] = packed;
    }
}

