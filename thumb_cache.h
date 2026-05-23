#ifndef THUMB_CACHE_H
#define THUMB_CACHE_H

#include <stdint.h>

#include "opalo.h"

#define THUMB_W 52
#define THUMB_H 64

typedef struct {

    uint8_t pixels[THUMB_W * THUMB_H];

    uint16_t palette[16];

    uint8_t ready;

} ThumbCache;

// --------------------------------------------------
// Genera thumbnail cacheado desde una seed
// --------------------------------------------------

void thumb_generar(
    ThumbCache* t,
    uint32_t seed
);

// --------------------------------------------------
// Dibuja thumbnail en VRAM
// --------------------------------------------------

void thumb_dibujar(
    ThumbCache* t,
    uint16_t* vram,
    int ox,
    int oy
);

#endif
