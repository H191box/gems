#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

// Gestión de buffers
uint16_t* get_vram(void);
void flip(void);

// Funciones de dibujo
void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color);
void vline(uint16_t* vram, int x, int y, int h, uint8_t c);
void fill_rect(uint16_t* vram, int x, int y, int w, int h, uint8_t c);
void clear(uint16_t* vram, uint8_t color);

// Efectos

void fade_out(void);
void fade_in(void);

#endif
