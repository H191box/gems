/*
 * patron_data.c
 * Catálogo de patrones visuales — Gacha de Ópalos GBA
 *
 * CAMBIOS v2: añadido PINFIRE (id 6, rareza 3, complejidad 1).
 */

#include "patron_data.h"
#include "plasma.h"

const PatronDef patrones[NUM_PATRONES] = {
    /* id 0 */ { "Nebula",    0, 1 },
    /* id 1 */ { "Venas",     1, 2 },
    /* id 2 */ { "Matrix",    4, 4 },  /* NULL en fns — necesita Gema* */
    /* id 3 */ { "Mosaico",   2, 3 },
    /* id 4 */ { "Chaos",     3, 2 },
    /* id 5 */ { "Harlequin", 4, 3 },
    /* id 6 */ { "Pinfire",   3, 1 },
};

const PatronPixelFn patron_pixel_fns[NUM_PATRONES] = {
    /* 0 NEBULA    */ pixel_nebula,
    /* 1 VENAS     */ pixel_venas,
    /* 2 MATRIX    */ 0,              /* caso especial — plasma.c */
    /* 3 MOSAICO   */ pixel_mosaico,
    /* 4 CHAOS     */ pixel_chaos,
    /* 5 HARLEQUIN */ pixel_harlequin,
    /* 6 PINFIRE   */ pixel_pinfire,
};

const PatronDef *patron_def(uint8_t id)
{
    if (id >= NUM_PATRONES) return 0;
    return &patrones[id];
}

const char *patron_nombre(uint8_t id)
{
    if (id >= NUM_PATRONES) return "Desconocido";
    return patrones[id].nombre;
}

uint8_t patron_rareza(uint8_t id)
{
    if (id >= NUM_PATRONES) return 0;
    return patrones[id].rareza;
}
