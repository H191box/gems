#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// render texto
void draw_text(uint16_t* vram, int x, int y, const char* str, uint8_t color);

// tamaño fuente
#define FONT_WIDTH 5
#define FONT_HEIGHT 7

#endif
