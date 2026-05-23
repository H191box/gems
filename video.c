#include <gba_video.h>

#define FRONT ((uint16_t*)0x06000000)
#define BACK  ((uint16_t*)0x0600A000)

#define BACKBUFFER 0x10

// FIX: page=1 significa que FRONT es el visible y BACK es donde dibujamos.
// Asi el primer get_vram() devuelve BACK, flip() muestra BACK,
// y el ciclo queda siempre: dibujar en oculto -> flip -> visible.
static uint8_t page = 1;

uint16_t* get_vram(void) {
    return page ? BACK : FRONT;
}

void flip(void) {
    while (REG_VCOUNT < 160);

    if (page) {
        REG_DISPCNT |= BACKBUFFER;  // mostrar BACK
        page = 0;                   // proxima vez dibujamos en FRONT
    } else {
        REG_DISPCNT &= ~BACKBUFFER; // mostrar FRONT
        page = 1;                   // proxima vez dibujamos en BACK
    }
}
