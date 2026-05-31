/*
 * gema.h
 * Entidad persistente Gema — Gacha de Ópalos GBA
 *
 * La seed solo se utiliza UNA VEZ en crear_gema_desde_chunk().
 * Todos los atributos se almacenan aquí y nunca se recalculan.
 *
 * Depende de opalo.h para los tipos TipoOpalo, PatronOpalo y Chunk.
 */

#ifndef GEMA_H
#define GEMA_H

#include <stdint.h>
#include "opalo.h"   /* TipoOpalo, PatronOpalo, Chunk */

/* ------------------------------------------------------------------ */
/* Etapas de evolución                                                */
/* ------------------------------------------------------------------ */

#define ETAPA_BRUTA    0   /* Piedra sin cortar — solo pistas visibles  */
#define ETAPA_CORTADA  1   /* Cabujón — forma y colores parciales       */
#define ETAPA_PULIDA   2   /* Ópalo final — todo revelado               */

/* ------------------------------------------------------------------ */
/* Máscaras de campo visible                                          */
/* ------------------------------------------------------------------ */

#define CAMPO_TIPO     0x01
#define CAMPO_PATRON   0x02
#define CAMPO_BRILLO   0x04
#define CAMPO_PUREZA   0x08
#define CAMPO_VALOR    0x10

/* ------------------------------------------------------------------ */
/* Identificador nulo                                                 */
/* ------------------------------------------------------------------ */

#define GEMA_ID_NULO   0xFFFFFFFFu

/* ------------------------------------------------------------------ */
/* Estructura principal  (20 bytes, alineada para GBA)                 */
/* ------------------------------------------------------------------ */

typedef struct Gema {

    /* --- Identidad ----------------------------------------------- */
    uint32_t id;            /* ID único global. */

    /* --- Estado de evolución ------------------------------------- */
    uint8_t  etapa;         /* ETAPA_BRUTA / CORTADA / PULIDA           */

    /* --- Atributos reales ---------------------------------------- */
    uint8_t  tipo_real;     /* TipoOpalo                                */
    uint8_t  patron_real;   /* PatronOpalo                              */
    uint8_t  brillo_real;   /* 0–255                                    */
    uint8_t  pureza_real;   /* 0–255                                    */
    uint8_t  iridiscencia;  /* 0–255                                    */
    uint8_t  saturacion;    /* 0–255                                    */

    /* --- Pistas visibles en etapa bruta -------------------------- */
    uint8_t  pista_color;       /* Color dominante de la grieta (0–5)    */
    uint8_t  pista_intensidad;  /* Intensidad de la grieta (0–255)       */
    uint8_t  pista_patron;      /* Patrón aproximado visible en grieta   */

    /* --- Peso ---------------------------------------------------- */
    uint16_t quilates;      /* En centésimas de quilate                 */

    /* --- Semilla visual ------------------------------------------ */
    uint32_t seed_visual;

    /* Padding explícito — struct = 20 bytes */
    uint8_t  _pad[2];

} Gema;

/* ------------------------------------------------------------------ */
/* API pública                                                        */
/* ------------------------------------------------------------------ */

void crear_gema_desde_chunk(Gema *g, const Chunk *chunk,
                            uint32_t id, uint8_t bioma);

void gema_init(Gema *g);
int gema_es_valida(const Gema *g);
int gema_campo_visible(const Gema *g, uint8_t campo);
int gema_evolucionar(Gema *g);
uint32_t gema_valor_real(const Gema *g);
uint32_t gema_valor_estimado(const Gema *g);
int gema_serializar(const Gema *g, uint8_t *buf);
int gema_deserializar(Gema *g, const uint8_t *buf);



void opalo_to_gema(const Opalo* o, Gema* g);

/* Puente Gema → Opalo temporal */
void gema_a_opalo_temp(Opalo *o, const Gema *g);

#endif /* GEMA_H */
