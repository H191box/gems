#ifndef PLASMA_H
#define PLASMA_H

#include <stdint.h>

#include "opalo.h"

// ----------------------------------------------------
// PALETA
// ----------------------------------------------------

void generar_paleta(Opalo* o);

// ----------------------------------------------------
// RENDER FULLSCREEN
// ----------------------------------------------------

void renderizar_opalo(Opalo* o);

// ----------------------------------------------------
// CORE PROCEDURAL
// Usado por thumbnails/cache/render parcial
// ----------------------------------------------------

uint8_t plasma_pixel(
    int x,
    int y,
    uint8_t off,
    const Opalo* o
);

// ----------------------------------------------------
// UI FX
// ----------------------------------------------------

void flash_guardado(void);

#endif
