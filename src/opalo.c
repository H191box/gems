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
    o->seed        = seed;
    o->tipo        = h & 3;
    o->patron      = (h >> 2) & 3;
    o->brillo      = 16 + ((h >> 4) & 15);
    o->saturacion  = 16 + ((h >> 8) & 15);
    o->color_offset= (h >> 12) & 255;
}

void generar_chunk(Chunk* c, uint32_t seed) {
    // Usamos un hash diferente al del ópalo para que
    // los atributos de la roca no revelen los del interior
    uint32_t h = hash32(seed ^ 0xDEADBEEF);

    c->seed    = seed;
    c->cortado = 0;

    // Peso: 1-10 (distribución algo sesgada hacia valores bajos,
    // los chunks muy pesados son más raros)
    c->peso = 1 + (h & 0xF) % 10;

    // Tamaño: 1-5
    c->tamanyo = 1 + ((h >> 4) & 0xF) % 5;

    // Grietas: 0-3, con probabilidad decreciente
    // 0: ~50%  1: ~25%  2: ~15%  3: ~10%
    uint8_t g = ((h >> 8) & 0xFF);
    if      (g < 128) c->grietas = 0;
    else if (g < 192) c->grietas = 1;
    else if (g < 230) c->grietas = 2;
    else              c->grietas = 3;

    // Pista de color solo si hay grietas
    // Usa bits distintos del hash para que no sea predecible
    if (c->grietas > 0) {
        c->pista = 1 + ((h >> 16) & 0xFF) % 4; // 1-4
    } else {
        c->pista = 0;
    }
}
