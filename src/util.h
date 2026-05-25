#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

// Borra la línea: void clear(uint16_t* vram, uint16_t color);

// Mantén solo las que NO están en video.h
void vline(uint16_t* vram, int x, int y, int h, uint8_t c);
void fill_rect(uint16_t* vram, int x, int y, int w, int h, uint8_t c);
void taller_recargar(void);

#endif
