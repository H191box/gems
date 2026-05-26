#include "opalo.h"
#include "mina.h"

static uint32_t hash32(uint32_t x) {

    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;

    return x;
}

// ----------------------------------------------------
// DISTRIBUCION DE QUILATES
// ----------------------------------------------------

static uint16_t calcular_quilates_exacto(uint32_t h, int bonus) {

    uint32_t r = h % 1000000;

    // ---------------------------------------------
    // BONUS MINA
    // ---------------------------------------------
    //
    // T1 = 0
    // T2 = 15
    // T3 = 30
    //
    // Empuja probabilidades hacia zonas raras
    //
    // cuanto menor es r -> mas raro
    //
    // ---------------------------------------------

    if (bonus > 0) {

        r = (r * (100 - bonus)) / 100;
    }

    // 30% : 1-5

    if (r < 300000)
        return 1 + (h % 5);

    // 20% : 5-10

    if (r < 500000)
        return 5 + (h % 6);

    // 10% : 10-25

    if (r < 600000)
        return 10 + (h % 16);

    // 5% : 25-50

    if (r < 650000)
        return 25 + (h % 26);

    // 2.5% : 50-100

    if (r < 675000)
        return 50 + (h % 51);

    // 1% : 100-110

    if (r < 685000)
        return 100 + (h % 11);

    // EXTREMOS : 111-225

    return 111 + (uint16_t)(((r - 685000) * 114) / 315000);
}

// ----------------------------------------------------
// OPALO
// ----------------------------------------------------

void generar_opalo(Opalo* o, uint32_t seed) {

    uint32_t h = hash32(seed);

    int bonus = mina_obtener_bonus();

    o->seed = seed;

    o->tipo = h & 3;

    o->patron = (h >> 2) & 3;

    o->brillo =
        16 + (uint8_t)(((h >> 4) & 15) * ((h >> 4) & 15) / 15);

    o->saturacion =
        16 + (uint8_t)(((h >> 8) & 15) * ((h >> 8) & 15) / 15);

    o->iridiscencia =
        16 + (uint8_t)(((h >> 13) & 15) * ((h >> 13) & 15) / 15);

    o->color_offset = (h >> 17) & 255;

    o->quilates =
        calcular_quilates_exacto(hash32(h), bonus);
}

// ----------------------------------------------------
// CHUNK
// ----------------------------------------------------

void generar_chunk(Chunk* c, uint32_t seed) {

    uint32_t h = hash32(seed ^ 0xDEADBEEF);

    int bonus = mina_obtener_bonus();

    c->seed = seed;

    c->cortado = 0;

    // ---------------------------------------------
    // TAMAÑO
    // ---------------------------------------------

    uint32_t tam = 1 + ((h >> 4) & 3);

    // T2

    if (bonus >= 15 && tam < 4 && (h & 7) == 0) {
        tam++;
    }

    // T3

    if (bonus >= 30 && tam < 4 && (h & 3) == 0) {
        tam++;
    }

    c->tamanyo = tam;

    // ---------------------------------------------
    // QUILATES
    // ---------------------------------------------

    c->quilates =
        calcular_quilates_exacto(h, bonus);

    // ---------------------------------------------
    // GRIETAS
    // ---------------------------------------------

    c->grietas =
        ((h >> 8) & 0xFF) > 200 ? 1 : 0;

    c->pista =
        (c->grietas > 0)
        ? 1 + ((h >> 16) & 0xFF) % 4
        : 0;
}

// ----------------------------------------------------
// VALOR
// ----------------------------------------------------

uint32_t calcular_valor_opalo(const Opalo* o) {

    // Base exponencial

    uint32_t base =
        (uint32_t)o->quilates * o->quilates / 20;

    // Tipo

    uint32_t mult_tipo =
        (o->tipo == OPALO_NEGRO)
        ? 50
        : (o->tipo == OPALO_FUEGO
        ? 30
        : (o->tipo == OPALO_CRISTAL
        ? 20
        : 10));

    // Belleza

    uint32_t b =
        (o->brillo - 16) +
        (o->saturacion - 16) +
        (o->iridiscencia - 16);

    uint32_t factor_belleza = 10 + b;

    // Valor

    uint32_t valor =
        (base * mult_tipo) / 10;

    valor =
        (valor * factor_belleza) / 10;

    // Bonus patrón

    if (o->patron == PATRON_CHAOS) {

        valor = (valor * 3) / 2;
    }

    return (valor < 1) ? 1 : valor;
}
