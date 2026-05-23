#include "opalo.h"

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

void generar_opalo(Opalo* o, uint32_t seed) {

    uint32_t h = hash32(seed);

    o->seed = seed;

    // derivados deterministas del seed
    o->tipo = h & 3;
    o->patron = (h >> 2) & 3;

    o->brillo = 16 + ((h >> 4) & 15);
    o->saturacion = 16 + ((h >> 8) & 15);

    o->color_offset = (h >> 12) & 255;
}
