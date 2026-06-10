#include "sacos.h"
#include "save.h"
#include "gema.h"

/* Variables externas */
extern uint8_t ciudad_actual_idx;
extern uint8_t dia_actual;
extern uint8_t mes_actual;

/*
 * 🧠 SACOS REEQUILIBRADOS
 * Diseñados alrededor de la DISTRIBUCIÓN REAL de quilates,
 * no del máximo (200).
 *
 * Objetivo:
 * - early game rentable
 * - mid game estable
 * - late game volátil pero controlado
 */

const Saco SACOS[4] = {
    /* nombre, precio, cantidad, tier */
    {"SACO POBRE",             500,  3, 1},
    {"SACO MINERO",          10000,  5, 2},
    {"SACO INDUSTRIAL",     100000,  7, 3},
    {"MEGA CARGAMENTO",    1000000, 10, 4}
};

/* Semilla global */
static uint32_t saco_seed = 0x12345678;
static uint32_t contador_global = 0;

/* --------------------------------------------------------- */
/* Caps de quilates por tier (hard visual cap = 200)        */
/* --------------------------------------------------------- */
static const uint16_t CAP_QUILATES[5] = {
    0,
    45,   /* tier 1 */
    95,   /* tier 2 */
    155,  /* tier 3 */
    200,  /* tier 4 */
};

/* --------------------------------------------------------- */
/* Generación de quilates                                    */
/* --------------------------------------------------------- */
static uint16_t generar_quilates(uint32_t seed, uint8_t tier)
{
    uint16_t cap = (tier <= 4) ? CAP_QUILATES[tier] : 200;

    uint16_t base, spread;

    switch (tier)
    {
        case 1: base = 8;   spread = 30;  break;
        case 2: base = 25;  spread = 55;  break;
        case 3: base = 60;  spread = 85;  break;
        case 4: base = 95;  spread = 95;  break;
        default: base = 10;  spread = 20;  break;
    }

    /* distribución pseudo-no uniforme */
    uint16_t r1 = seed & 0xFF;
    uint16_t r2 = (seed >> 8) & 0xFF;
    uint16_t r3 = (seed >> 4) & 0xFF;

    uint16_t q = base + ((r1 * 2 + r2 + r3) % spread);

    /* -------------------------------------------------- */
    /* JACKPOTS CONTROLADOS (alineados a distribución real) */
    /* -------------------------------------------------- */
    uint32_t r = (seed >> 12) % 10000;

    if (r == 0)
    {
        q = cap; /* ultra raro (0.01%) */
    }
    else if (r < 20)
    {
        q = (uint16_t)(cap * 90 / 100); /* 0.2% */
    }
    else if (r < 150)
    {
        q = (uint16_t)(cap * 80 / 100); /* 1.3% */
    }
    else if (r < 600)
    {
        q = (uint16_t)(cap * 65 / 100); /* 4.5% */
    }

    if (q > cap) q = cap;

    return q;
}

/* --------------------------------------------------------- */
/* Compra de saco                                            */
/* --------------------------------------------------------- */
void comprar_saco(int idx)
{
    if (idx < 0 || idx >= 4)
        return;

    const Saco* s = &SACOS[idx];

    if (obtener_dinero() < (int32_t)s->precio)
        return;

    modificar_dinero(-(int32_t)s->precio);

    for (int i = 0; i < s->cantidad_chunks; i++)
    {
        Gema nueva_gema;

        /* Entropía */
        uint16_t rand_part =
            (uint16_t)((saco_seed + contador_global++ +
            (i * 0x9E3779B9u)) & 0xFFFFu);

        if (rand_part == 0 && ciudad_actual_idx == 0 && dia_actual == 0)
            rand_part = 1;

        /* Seed empaquetada */
        nueva_gema.seed =
            ((uint32_t)ciudad_actual_idx << 24) |
            ((uint32_t)dia_actual << 16) |
            (uint32_t)rand_part;

        /* Quilates con sistema físico */
        nueva_gema.quilates =
            generar_quilates(nueva_gema.seed, s->tier);

        /* Estado inicial */
        nueva_gema.etapa = ETAPA_BRUTA;
        nueva_gema.flags = 0;

        guardar_gema_pool(&nueva_gema);
    }

    saco_seed += 0x9E3779B9u;
}
