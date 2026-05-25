#ifndef VIAJAR_H
#define VIAJAR_H

#include <stdint.h>

void viajar_init(void);

// Ahora devuelve int en lugar de void
int viajar_input(uint16_t keys); 

void render_viaje(int cursor);

#endif
