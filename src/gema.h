/*
 * gema.h
 * Entidad persistente Gema — Gacha de Ópalos GBA
 *
 * FILOSOFÍA:
 *   La Gema almacena únicamente información persistente.
 *   Todo atributo visual y comercial se deriva dinámicamente desde seed.
 *
 * LAYOUT DE SEED (32 bits):
 *
 *   [ 31..24 ] ciudad_id  — 8 bits, hasta 255 ciudades
 *   [ 23..16 ] dia        — 8 bits, día absoluto de juego (0-255)
 *   [ 15.. 0 ] rand       — 16 bits de entropía visual
 *
 *   La parte rand se genera aleatoriamente en crear_gema_desde_chunk().
 *   ciudad_id y dia se leen directamente desde la seed — no se almacenan
 *   por separado.
 *
 * TAMAÑO: 8 bytes exactos.
 */

#ifndef GEMA_H
#define GEMA_H

#include <stdint.h>
#include "opalo.h"

/* ------------------------------------------------------------------ */
/* Etapas de evolución                                                */
/* ------------------------------------------------------------------ */

#define ETAPA_BRUTA    0
#define ETAPA_CORTADA  1
#define ETAPA_PULIDA   2

/* ------------------------------------------------------------------ */
/* Máscaras de campo visible                                          */
/* ------------------------------------------------------------------ */

#define CAMPO_TIPO     0x01
#define CAMPO_PATRON   0x02
#define CAMPO_BRILLO   0x04
#define CAMPO_PUREZA   0x08
#define CAMPO_VALOR    0x10

/* ------------------------------------------------------------------ */
/* Flags persistentes (bits 0-1 del byte flags)                      */
/* ------------------------------------------------------------------ */

#define GEMA_FLAG_FAVORITA   0x01
#define GEMA_FLAG_BLOQUEADA  0x02

/* ------------------------------------------------------------------ */
/* Estructura persistente — 8 bytes                                  */
/*                                                                    */
/*   seed      4   [ ciudad_id(8) | dia(8) | rand(16) ]              */
/*   quilates  2                                                      */
/*   etapa     1                                                      */
/*   flags     1                                                      */
/* ------------------------------------------------------------------ */

typedef struct Gema {
    uint32_t seed;
    uint16_t quilates;
    uint8_t  etapa;
    uint8_t  flags;
} Gema;

/* ------------------------------------------------------------------ */
/* Accesores de los campos empaquetados en seed                       */
/* ------------------------------------------------------------------ */

uint8_t  gema_ciudad_id(const Gema *g);   /* bits 31..24 */
uint8_t  gema_dia(const Gema *g);         /* bits 23..16 */
uint16_t gema_rand(const Gema *g);        /* bits 15.. 0 */

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                      */
/* ------------------------------------------------------------------ */

void gema_init(Gema *g);
int  gema_es_valida(const Gema *g);

void crear_gema_desde_chunk(
    Gema          *g,
    const Chunk   *chunk,
    uint8_t        ciudad_id,
    uint8_t        dia_actual
);

/* ------------------------------------------------------------------ */
/* Evolución                                                          */
/* ------------------------------------------------------------------ */

int gema_evolucionar(Gema *g);
int gema_campo_visible(const Gema *g, uint8_t campo);

/* ------------------------------------------------------------------ */
/* Atributos derivados                                                */
/* ------------------------------------------------------------------ */

TipoOpalo   gema_tipo(const Gema *g);
PatronOpalo gema_patron(const Gema *g);
uint8_t     gema_brillo(const Gema *g);
uint8_t     gema_pureza(const Gema *g);
uint8_t     gema_iridiscencia(const Gema *g);
uint8_t     gema_saturacion(const Gema *g);

/* ------------------------------------------------------------------ */
/* Pistas                                                             */
/* ------------------------------------------------------------------ */

uint8_t gema_pista_color(const Gema *g);
uint8_t gema_pista_patron(const Gema *g);
uint8_t gema_pista_intensidad(const Gema *g);

/* ------------------------------------------------------------------ */
/* Economía                                                           */
/* ------------------------------------------------------------------ */

uint32_t gema_valor_real(const Gema *g);
uint32_t gema_valor_estimado(const Gema *g);

/* ------------------------------------------------------------------ */
/* Persistencia                                                       */
/* ------------------------------------------------------------------ */

int gema_serializar(const Gema *g, uint8_t *buf);
int gema_deserializar(Gema *g, const uint8_t *buf);

/* ------------------------------------------------------------------ */
/* Conversión temporal                                                */
/* ------------------------------------------------------------------ */

void opalo_to_gema(const Opalo *o, Gema *g);
void gema_a_opalo_temp(Opalo *o, const Gema *g);

#endif /* GEMA_H */
