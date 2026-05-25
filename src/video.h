#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

uint16_t* get_vram(void);
void flip(void);
void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color);
void clear(uint16_t* vram, uint8_t color); // Declaramos clear aquí
void fundido_a_negro(void);

#endif
