/*
 * opalo_data.c
 * Catálogo de tipos de ópalo — Gacha de Ópalos GBA
 *
 * Para añadir un tipo nuevo:
 *   1. Incrementar NUM_TIPOS_OPALO en opalo_data.h
 *   2. Añadir un #define TIPO_OPALO_XXX con el siguiente índice libre
 *   3. Añadir la fila correspondiente al final de tipos_opalo[]
 *   Nada más. gema.c, plasma.c y render no requieren cambios.
 */

#include "opalo_data.h"

/* ------------------------------------------------------------------ */
/* Tabla de tipos                                                     */
/* Orden fijo: los índices 0–5 deben coincidir con TIPO_OPALO_*       */
/* para que las seeds existentes sigan derivando el tipo correcto.    */
/* ------------------------------------------------------------------ */

const TipoOpaloDef tipos_opalo[NUM_TIPOS_OPALO] = {
    /* id 0 */ { "Negro",   3, 0, OPALO_FLAG_IRIDISCENTE                              },
    /* id 1 */ { "Cristal", 2, 1, OPALO_FLAG_IRIDISCENTE | OPALO_FLAG_TRANSLUCIDO     },
    /* id 2 */ { "Fuego",   2, 2, OPALO_FLAG_IRIDISCENTE                              },
    /* id 3 */ { "Blanco",  1, 3, OPALO_FLAG_TRANSLUCIDO                              },
    /* id 4 */ { "Rosa",    1, 4, 0                                                   },
    /* id 5 */ { "Gris",    0, 5, 0                                                   },
    /*
     * Ejemplo de expansión futura (descomentar y ajustar NUM_TIPOS_OPALO):
     * { "Boulder",  2, 6, OPALO_FLAG_IRIDISCENTE },
     * { "Etíope",   3, 7, OPALO_FLAG_FOSFORESCENTE | OPALO_FLAG_IRIDISCENTE },
     */
};

/* ------------------------------------------------------------------ */
/* Utilidades                                                         */
/* ------------------------------------------------------------------ */

const TipoOpaloDef *opalo_def(uint8_t id)
{
    if (id >= NUM_TIPOS_OPALO) return 0;
    return &tipos_opalo[id];
}

const char *opalo_nombre(uint8_t id)
{
    if (id >= NUM_TIPOS_OPALO) return "Desconocido";
    return tipos_opalo[id].nombre;
}

uint8_t opalo_rareza(uint8_t id)
{
    if (id >= NUM_TIPOS_OPALO) return 0;
    return tipos_opalo[id].rareza;
}
