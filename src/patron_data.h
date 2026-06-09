/*
 * patron_data.h
 * Catálogo de patrones visuales de ópalo — Gacha de Ópalos GBA
 *
 * CAMBIOS v2:
 *   - Añadido PATRON_ID_PINFIRE (id 6)
 *   - NUM_PATRONES actualizado a 7
 *   - Orden recomendado por el Plan de Evolución:
 *     NEBULA, VENAS, MOSAICO, CHAOS, HARLEQUIN, PINFIRE, MATRIX
 *
 * NOTA sobre el orden de IDs:
 *   Los IDs 0-5 originales NO se han reordenado para no romper seeds
 *   existentes. PINFIRE se añade como id 6 (nuevo). MATRIX permanece
 *   en id 2 aunque visualmente sea el más premium.
 */

#ifndef PATRON_DATA_H
#define PATRON_DATA_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* IDs de patrón                                                      */
/* ------------------------------------------------------------------ */

#define PATRON_ID_NEBULA     0
#define PATRON_ID_VENAS      1
#define PATRON_ID_MATRIX     2
#define PATRON_ID_MOSAICO    3
#define PATRON_ID_CHAOS      4
#define PATRON_ID_HARLEQUIN  5
#define PATRON_ID_PINFIRE    6
/* IDs 7–99: reservados para expansión futura                         */

#define NUM_PATRONES  7

/* ------------------------------------------------------------------ */
/* Definición de patrón                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char    nombre[16];
    uint8_t rareza;       /* 0=común … 4=legendario                    */
    uint8_t complejidad;  /* Coste visual [1–5]                        */
} PatronDef;

extern const PatronDef patrones[NUM_PATRONES];

typedef uint8_t (*PatronPixelFn)(int x, int y, uint8_t off);
extern const PatronPixelFn patron_pixel_fns[NUM_PATRONES];

const PatronDef *patron_def(uint8_t id);
const char      *patron_nombre(uint8_t id);
uint8_t          patron_rareza(uint8_t id);

#endif /* PATRON_DATA_H */
