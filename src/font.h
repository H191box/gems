#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// tamaño fuente
#define FONT_WIDTH 5
#define FONT_HEIGHT 7

// render texto normal
void draw_text(uint16_t* vram, int x, int y, const char* str, uint8_t color);

// render texto con sombra para mejorar legibilidad
void draw_text_shadow(uint16_t* vram, int x, int y, const char* str, uint8_t color, uint8_t shadow_color);

#endif
