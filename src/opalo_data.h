/*
 * opalo_data.h
 * Catálogo de tipos de ópalo — Gacha de Ópalos GBA
 *
 * FILOSOFÍA (Plan de Migración, Fase 1):
 *   Ópalo ya NO es una entidad persistente. Es únicamente un catálogo
 *   de definiciones estáticas. Añadir un tipo nuevo = añadir una fila
 *   a tipos_opalo[]. Cero modificaciones en gema.c ni en render.
 *
 * LAYOUT DE TipoOpaloDef (4 bytes):
 *   nombre[16]   — etiqueta legible para UI/debug
 *   rareza       — 0=común … 4=legendario (controla pesos base de loot)
 *   color_base   — índice de paleta de referencia para el render (0–5)
 *   flags        — bits de comportamiento especial (ver OPALO_FLAG_*)
 *
 * COMPATIBILIDAD CON SRAM:
 *   La Gema solo almacena un índice de tipo derivado proceduralmente
 *   de la seed. Nada de esto se serializa.
 */

#ifndef OPALO_DATA_H
#define OPALO_DATA_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Flags de tipo                                                       */
/* ------------------------------------------------------------------ */

#define OPALO_FLAG_IRIDISCENTE  0x01  /* Activa capa de iridiscencia en render  */
#define OPALO_FLAG_TRANSLUCIDO  0x02  /* Permite ver el núcleo en ETAPA_BRUTA   */
#define OPALO_FLAG_FOSFORESCENTE 0x04 /* Brilla en oscuridad (efecto UI)        */

/* ------------------------------------------------------------------ */
/* IDs de tipo — índices en tipos_opalo[]                             */
/* Mantener el orden original para no romper seeds ya generadas.      */
/* ------------------------------------------------------------------ */

#define TIPO_OPALO_NEGRO     0
#define TIPO_OPALO_CRISTAL   1
#define TIPO_OPALO_FUEGO     2
#define TIPO_OPALO_BLANCO    3
#define TIPO_OPALO_ROSA      4
#define TIPO_OPALO_GRIS      5
/* IDs 6–99: reservados para expansión futura                         */

/* ------------------------------------------------------------------ */
/* Número total de tipos registrados                                  */
/* ------------------------------------------------------------------ */

#define NUM_TIPOS_OPALO  6   /* Actualizar al añadir tipos nuevos      */

/* ------------------------------------------------------------------ */
/* Definición de tipo                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char    nombre[16];   /* Etiqueta legible                          */
    uint8_t rareza;       /* 0=común, 1=poco común, 2=raro,
                             3=épico, 4=legendario                     */
    uint8_t color_base;   /* Índice lógico de color (0–5, ver render)  */
    uint8_t flags;        /* OPALO_FLAG_*                              */
} TipoOpaloDef;

/* ------------------------------------------------------------------ */
/* Tabla global (definida en opalo_data.c)                            */
/* ------------------------------------------------------------------ */

extern const TipoOpaloDef tipos_opalo[NUM_TIPOS_OPALO];

/* ------------------------------------------------------------------ */
/* Utilidades de consulta                                             */
/* ------------------------------------------------------------------ */

/* Devuelve puntero a la definición o NULL si id >= NUM_TIPOS_OPALO   */
const TipoOpaloDef *opalo_def(uint8_t id);

/* Nombre legible seguro (nunca NULL)                                 */
const char *opalo_nombre(uint8_t id);

/* Rareza [0, 4] con guarda de rango                                 */
uint8_t opalo_rareza(uint8_t id);

#endif /* OPALO_DATA_H */
