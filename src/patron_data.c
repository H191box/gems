/*
 * patron_data.c
 * Catálogo de patrones visuales — Gacha de Ópalos GBA
 *
 * Para añadir un patrón nuevo:
 *   1. Incrementar NUM_PATRONES en patron_data.h
 *   2. Añadir un #define PATRON_ID_XXX con el siguiente índice libre
 *   3. Implementar pixel_xxx() en plasma.c
 *   4. Añadir la fila en patrones[] y el puntero en patron_pixel_fns[]
 *   Nada más. gema.c no requiere cambios.
 *
 * NOTA sobre PATRON_MATRIX (id 2):
 *   Su función de render necesita un puntero Gema* adicional, por lo
 *   que no encaja en la firma PatronPixelFn. Se registra como NULL en
 *   patron_pixel_fns[] y plasma.c lo despacha como caso especial
 *   documentado. Esta es la única excepción aceptada al patrón data-driven.
 */

#include "patron_data.h"
#include "plasma.h"   /* pixel_nebula, pixel_venas, etc. */

/* ------------------------------------------------------------------ */
/* Tabla de definiciones                                             */
/* El orden de filas fija los PATRON_ID_* para siempre.              */
/* ------------------------------------------------------------------ */

const PatronDef patrones[NUM_PATRONES] = {
    /* id 0 */ { "Nebula",    0, 1 },
    /* id 1 */ { "Venas",     1, 2 },
    /* id 2 */ { "Matrix",    2, 4 },  /* NULL en fns — ver nota arriba  */
    /* id 3 */ { "Mosaico",   3, 3 },
    /* id 4 */ { "Chaos",     4, 2 },
    /* id 5 */ { "Harlequin", 5, 3 },
    /*
     * Ejemplo de expansión futura (descomentar y ajustar NUM_PATRONES):
     * { "Dendritic", 3, 3 },
     * { "Pinfire",   4, 4 },
     */
};

/* ------------------------------------------------------------------ */
/* Tabla de punteros a función de render                             */
/* Indexada por PATRON_ID_*. NULL = caso especial (ver Matrix arriba) */
/* ------------------------------------------------------------------ */

const PatronPixelFn patron_pixel_fns[NUM_PATRONES] = {
    /* 0 NEBULA    */ pixel_nebula,
    /* 1 VENAS     */ pixel_venas,
    /* 2 MATRIX    */ 0,            /* necesita Gema* — despachado por plasma.c */
    /* 3 MOSAICO   */ pixel_mosaico,
    /* 4 CHAOS     */ pixel_chaos,
    /* 5 HARLEQUIN */ pixel_harlequin,
};

/* ------------------------------------------------------------------ */
/* Utilidades                                                         */
/* ------------------------------------------------------------------ */

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
