/*
 * patron_data.h
 * Catálogo de patrones visuales de ópalo — Gacha de Ópalos GBA
 *
 * FILOSOFÍA (Plan de Migración, Fase 2):
 *   Un patrón es una definición de datos, no un case en un switch.
 *   Añadir un patrón nuevo = añadir una fila a patrones[].
 *   plasma.c registra una función de render por cada patrón mediante
 *   una tabla de punteros a función — ningún if/switch en el render.
 *
 * LAYOUT DE PatronDef (3 bytes + padding):
 *   nombre[16]    — etiqueta legible para UI/debug
 *   rareza        — 0=común … 4=legendario
 *   complejidad   — coste visual relativo [1–5]; útil para LOD futuro
 *
 * COMPATIBILIDAD CON SRAM:
 *   La Gema almacena la seed; el índice de patrón se deriva en tiempo
 *   de ejecución mediante gema_patron(). Nada se serializa aquí.
 */

#ifndef PATRON_DATA_H
#define PATRON_DATA_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* IDs de patrón — índices en patrones[]                              */
/* El orden debe coincidir con la lógica de derivación en gema.c      */
/* para que seeds existentes sigan produciendo el mismo patrón.       */
/* ------------------------------------------------------------------ */

#define PATRON_ID_NEBULA     0
#define PATRON_ID_VENAS      1
#define PATRON_ID_MATRIX     2
#define PATRON_ID_MOSAICO    3
#define PATRON_ID_CHAOS      4
#define PATRON_ID_HARLEQUIN  5
/* IDs 6–99: reservados para expansión futura                         */

/* ------------------------------------------------------------------ */
/* Número total de patrones registrados                               */
/* ------------------------------------------------------------------ */

#define NUM_PATRONES  6   /* Actualizar al añadir patrones nuevos      */

/* ------------------------------------------------------------------ */
/* Definición de patrón                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char    nombre[16];   /* Etiqueta legible                          */
    uint8_t rareza;       /* 0=común … 4=legendario                    */
    uint8_t complejidad;  /* Coste visual [1–5]; 1=barato, 5=caro      */
} PatronDef;

/* ------------------------------------------------------------------ */
/* Tabla global (definida en patron_data.c)                           */
/* ------------------------------------------------------------------ */

extern const PatronDef patrones[NUM_PATRONES];

/* ------------------------------------------------------------------ */
/* Puntero a función de render de patrón                              */
/* Firma: (x, y, off) → índice de paleta [16, 254]                   */
/* Usada por plasma.c para despachar sin switch.                      */
/* PATRON_MATRIX no figura aquí porque necesita (Gema*) extra;       */
/* plasma.c lo trata como caso especial documentado.                  */
/* ------------------------------------------------------------------ */

typedef uint8_t (*PatronPixelFn)(int x, int y, uint8_t off);

/* Tabla de funciones de render indexada por PATRON_ID_*              */
/* Definida en patron_data.c; plasma.c la importa.                   */
extern const PatronPixelFn patron_pixel_fns[NUM_PATRONES];

/* ------------------------------------------------------------------ */
/* Utilidades de consulta                                             */
/* ------------------------------------------------------------------ */

/* Devuelve puntero a la definición o NULL si id >= NUM_PATRONES      */
const PatronDef *patron_def(uint8_t id);

/* Nombre legible seguro (nunca NULL)                                 */
const char *patron_nombre(uint8_t id);

/* Rareza [0, 4] con guarda de rango                                 */
uint8_t patron_rareza(uint8_t id);

#endif /* PATRON_DATA_H */
