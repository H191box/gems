// 2.0
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
    o->seed         = seed;
    o->tipo         = h & 3;
    o->patron       = (h >> 2) & 3;
    o->brillo       = 16 + ((h >> 4) & 15);
    o->saturacion   = 16 + ((h >> 8) & 15);
    o->color_offset = (h >> 12) & 255;
}

void generar_chunk(Chunk* c, uint32_t seed) {
    uint32_t h = hash32(seed ^ 0xDEADBEEF);

    c->seed    = seed;
    c->cortado = 0;

    // Tamaño: distribución exponencial decreciente
    // 1(S):~50%  2(M):~25%  3(L):~13%  4(XL):~7%  5(XXL):~5%
    uint8_t t = (h >> 4) & 0xFF;
    if      (t < 128) c->tamanyo = 1;
    else if (t < 192) c->tamanyo = 2;
    else if (t < 225) c->tamanyo = 3;
    else if (t < 243) c->tamanyo = 4;
    else              c->tamanyo = 5;

    // Peso: derivado del tamaño + variación aleatoria
    // Tam 1 → 1-3,  Tam 2 → 3-5,  Tam 3 → 5-7,  Tam 4 → 7-9,  Tam 5 → 9-10
    int peso_base = (c->tamanyo - 1) * 2 + 1;
    int peso_var  = (int)((h >> 12) & 0x3); // 0-3 extra
    c->peso = peso_base + peso_var;
    if (c->peso > 10) c->peso = 10;

    // Grietas: 0 o 1
    uint8_t g = (h >> 8) & 0xFF;
    c->grietas = (g < 128) ? 0 : 1;

    // Pista solo si hay grieta
    if (c->grietas > 0)
        c->pista = 1 + ((h >> 16) & 0xFF) % 4;
    else
        c->pista = 0;
}

uint16_t calcular_valor_chunk(const Chunk* c) {
    uint16_t valor = 0;
    valor += c->peso * 3;
    valor += c->tamanyo * 5;
    valor += c->grietas * 12;
    return valor;
}
