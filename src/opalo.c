#include "opalo.h"
#include "mina.h"
#include "data.h"



int COLOR_PRUEBA = 2;

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

// ----------------------------------------------------
// BONUS REGIONAL
// ----------------------------------------------------

void aplicar_bonus_region(Opalo* o, Chunk* c) {
    switch (ciudad_actual_idx) {
        case 0: // VALLE CENIZA
            if ((o->seed % 100) < 40) o->tipo = OPALO_FUEGO;
            o->brillo += 2;
            break;
        case 1: // COSTA ROSA
            if (o->tipo == OPALO_BLANCO) o->pureza += 10;
            break;
        case 2: // LLANURA CRISTAL
            if (o->tipo == OPALO_CRISTAL) o->saturacion += 4;
            break;
    }
    if (o->pureza > 100) o->pureza = 100;
}
// ----------------------------------------------------
// QUILATES
// ----------------------------------------------------
static uint16_t calcular_quilates_exacto(uint32_t h, int bonus) {
    uint32_t r = h % 1000000;

    // MINA T1 (bonus 0): rango 1-50, mayoría entre 5-30
    if (bonus == 0) {
        if (r < 200000) return 1  + (h % 5);
        if (r < 600000) return 5  + (h % 26);
        if (r < 900000) return 30 + (h % 16);
        return                45  + (h % 6);
    }

    // MINA T2 (bonus 15): rango 1-90, mayoría entre 5-30
    if (bonus == 15) {
        if (r < 100000) return 1  + (h % 5);
        if (r < 600000) return 5  + (h % 26);
        if (r < 850000) return 30 + (h % 31);
        if (r < 970000) return 60 + (h % 21);
        return                80  + (h % 11);
    }

    // MINA T3 (bonus 30): rango 1-150, mayoría entre 30-90
    if (bonus == 30) {
        if (r < 50000)  return 1   + (h % 5);
        if (r < 200000) return 5   + (h % 26);
        if (r < 550000) return 30  + (h % 41);
        if (r < 800000) return 70  + (h % 31);
        if (r < 950000) return 100 + (h % 21);
        if (r < 995000) return 120 + (h % 21);
        return                140  + (h % 11);
    }

    // Fallback
    return 1 + (h % 10);
}
// ----------------------------------------------------
// OPALO
// ----------------------------------------------------

void generar_opalo(Opalo* o, uint32_t seed) {
    uint32_t h = hash32(seed);

    o->seed = seed;
    o->tipo = h & 3;

    // Patrón: 5 opciones con probabilidades distintas
    // Usamos 8 bits: 0-127 = nebula(50%), 128-191 = venas(25%),
    // 192-223 = mosaico(12.5%), 224-239 = chaos(6.25%),
    // 240-251 = matrix(4.7%), 252-255 = harlequin(1.5%)
    {
        uint8_t pr = (h >> 2) & 255;
        if      (pr < 128) o->patron = PATRON_NEBULA;
        else if (pr < 192) o->patron = PATRON_VENAS;
        else if (pr < 224) o->patron = PATRON_MATRIX;
        else if (pr < 240) o->patron = PATRON_MOSAICO;
        else if (pr < 252) o->patron = PATRON_CHAOS;
        else               o->patron = PATRON_HARLEQUIN;
    }

    o->brillo       = 16 + (((h >> 4)  & 15) * ((h >> 4)  & 15) / 15);
    o->saturacion   = 16 + (((h >> 8)  & 15) * ((h >> 8)  & 15) / 15);
    o->iridiscencia = 16 + (((h >> 13) & 15) * ((h >> 13) & 15) / 15);

    {
        uint32_t p = (h >> 20) & 1023;
        if (p < 500)
            o->pureza = 40 + (p % 20);
        else if (p < 850)
            o->pureza = 60 + (p % 25);
        else if (p < 980)
            o->pureza = 85 + (p % 10);
        else
            o->pureza = 95 + (p % 6);
    }

    o->color_offset = (h >> 17) & 255;
    o->quilates = 0;
}

// ----------------------------------------------------
// CHUNK
// ----------------------------------------------------

void generar_chunk(Chunk* c, uint32_t seed) {
    uint32_t h = hash32(seed ^ 0xDEADBEEF);
    int bonus = mina_obtener_bonus();

    c->seed    = seed;
    c->cortado = 0;

    {
        uint32_t engaño = (h >> 5) & 15;
        c->tamanyo = 1 + ((h >> 2) & 3);
        if (engaño < 4) {
            if (engaño & 1) { if (c->tamanyo > 1) c->tamanyo--; }
            else            { if (c->tamanyo < 4) c->tamanyo++; }
        }
    }

    c->quilates          = calcular_quilates_exacto(h, bonus);
    c->grietas           = ((h >> 8) & 255) > 120 ? 1 : 0;
    c->pista             = 1 + ((h >> 16) & 3);
    c->intensidad_grieta = 1 + ((h >> 22) & 3);
    c->forma_grieta      = (h >> 24) & 3;
    c->pureza_aprox      = 30 + ((h >> 27) & 63);
}

// ----------------------------------------------------
// VALOR
// ----------------------------------------------------

uint32_t calcular_valor_opalo(const Opalo* o) {
    uint32_t base = (uint32_t)o->quilates * o->quilates / 20;

    uint32_t mult_tipo =
        (o->tipo == OPALO_NEGRO)   ? 50 :
        (o->tipo == OPALO_FUEGO)   ? 30 :
        (o->tipo == OPALO_CRISTAL) ? 20 : 10;

    uint32_t b =
        (o->brillo - 16) +
        (o->saturacion - 16) +
        (o->iridiscencia - 16);

    uint32_t factor_belleza = 10 + b;

    uint32_t valor = (base * mult_tipo) / 10;
    valor = (valor * factor_belleza) / 10;
    valor = (valor * (50 + o->pureza)) / 100;

    // Bonus por patrón
    if (o->patron == PATRON_CHAOS)
        valor = (valor * 3) / 2;  // +50%

    if (o->patron == PATRON_HARLEQUIN)
        valor = (valor * 5) / 2;  // +150% — el más raro y caro

    if (o->patron == PATRON_MATRIX) {
        // Multiplicador variable 0.2..2.5 según color_offset
        // color_offset 0..255 → factor 4..50 (dividido entre 20)
        // Esto da una distribución muy amplia de calidad
        uint32_t factor_matrix = 4 + ((uint32_t)o->color_offset * 46) / 255;
        valor = (valor * factor_matrix) / 20;
    }

    if (valor < 1) valor = 1;
    return valor;
}
