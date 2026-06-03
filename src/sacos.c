#include "sacos.h"
#include "save.h"
#include "gema.h"

/* Variables externas */
extern uint8_t ciudad_actual_idx;
extern uint8_t dia_actual;
extern uint8_t mes_actual;

const Saco SACOS[4] = {
    {"SACO POBRE",       50,  3, 1},
    {"SACO MINERO",     200,  6, 2},
    {"SACO INDUSTRIAL", 800, 12, 3},
    {"MEGA CARGAMENTO",3000, 24, 4}
};

/* Semilla global */
static uint32_t saco_seed = 0x12345678;
static uint32_t contador_global = 0;

/* --------------------------------------------------------- */
/* Caps de quilates por tier                                 */
/*   Tier 1 (POBRE)      → max  40                          */
/*   Tier 2 (MINERO)     → max 100                          */
/*   Tier 3 (INDUSTRIAL) → max 225                          */
/*   Tier 4 (MEGA)       → max 225                          */
/* --------------------------------------------------------- */
static const uint16_t CAP_QUILATES[5] = {
    0,    /* unused (tier 0) */
    40,
    100,
    225,
    225,
};

/* --------------------------------------------------------- */
/* Generación de quilates                                    */
/* Unidad: centésimas de quilate — 100 = 1.00 ct            */
/* El cap se aplica al final para que jackpots sean         */
/* sorprendentes pero nunca desborden la fórmula de valor.  */
/* --------------------------------------------------------- */
static uint16_t generar_quilates(uint32_t seed, uint8_t tier)
{
    uint16_t base;
    uint16_t rango;

    switch (tier)
    {
        default:
        case 1: /* normal: 10 - 35 */
            base  = 10;
            rango = 25;
            break;

        case 2: /* normal: 30 - 85 */
            base  = 30;
            rango = 55;
            break;

        case 3: /* normal: 80 - 200 */
            base  = 80;
            rango = 120;
            break;

        case 4: /* normal: 80 - 200 (mismo techo, jackpot lo diferencia) */
            base  = 80;
            rango = 120;
            break;
    }

    uint16_t q = base + (uint16_t)(seed % rango);

    /* -------------------------------------------------- */
    /* JACKPOTS — llevan al límite del cap del tier       */
    /* -------------------------------------------------- */
    uint16_t cap = (tier <= 4) ? CAP_QUILATES[tier] : 225;

    uint32_t r = (seed >> 8) % 10000;

    if (r == 0)
    {
        /* 1 entre 10000: quilates al máximo del cap */
        q = cap;
    }
    else if (r < 10)
    {
        /* 9 entre 10000: 90% del cap */
        q = (uint16_t)(cap * 9 / 10);
    }
    else if (r < 100)
    {
        /* 90 entre 10000: 75% del cap */
        q = (uint16_t)(cap * 3 / 4);
    }

    /* Seguridad: nunca superar el cap del tier */
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

        /* 1. Entropía visual de 16 bits */
        uint16_t rand_part = (uint16_t)((saco_seed + contador_global++ + (i * 0x9E3779B9u)) & 0xFFFFu);

        if (rand_part == 0 && ciudad_actual_idx == 0 && dia_actual == 0)
            rand_part = 1;

        /* 2. Seed empaquetada: ciudad | dia | entropia */
        nueva_gema.seed = ((uint32_t)ciudad_actual_idx << 24)
                        | ((uint32_t)dia_actual        << 16)
                        | (uint32_t)rand_part;

        /* 3. Quilates con cap por tier */
        nueva_gema.quilates = generar_quilates(nueva_gema.seed, s->tier);

        /* 4. Estado inicial */
        nueva_gema.etapa = ETAPA_BRUTA;
        nueva_gema.flags = 0;

        guardar_gema(&nueva_gema);
    }

    saco_seed += 0x9E3779B9u;
}
