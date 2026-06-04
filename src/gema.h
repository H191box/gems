/*
 * gema.h
 * Entidad persistente Gema — Gacha de Ópalos GBA
 *
 * FILOSOFÍA:
 * El precio por quilate se determina mediante atributos de fase y rareza,
 * y posteriormente se somete a un multiplicador exponencial basado en los quilates.
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

#define ETAPA_BRUTA    0  /* Fase 1: Bruto */
#define ETAPA_CORTADA  1  /* Fase 2: Cabujón */
#define ETAPA_PULIDA   2  /* Fase 3: Pulido */

/* ------------------------------------------------------------------ */
/* Máscaras de campo visible                                          */
/* ------------------------------------------------------------------ */

#define CAMPO_TIPO     0x01
#define CAMPO_PATRON   0x02
#define CAMPO_BRILLO   0x04
#define CAMPO_PUREZA   0x08
#define CAMPO_VALOR    0x10

/* ------------------------------------------------------------------ */
/* Flags persistentes (Empaquetados en 1 byte)                        */
/* ------------------------------------------------------------------ */

#define GEMA_FLAG_FAVORITA   0x01
#define GEMA_FLAG_BLOQUEADA  0x02
#define GEMA_FLAG_GRIETAS    0x04  /* Excepción: Almacena de forma persistente el Fallo Crítico */

/* ------------------------------------------------------------------ */
/* Estructuras de Datos                                               */
/* ------------------------------------------------------------------ */

typedef struct Gema {
    uint32_t seed;
    uint16_t quilates;
    uint8_t  etapa;
    uint8_t  flags;
} Gema;

typedef struct {
    /* ATRIBUTOS REALES (Inmutables, potencial máximo) */
    uint8_t brillo_real;
    uint8_t fuego_real;       
    uint8_t saturacion_real;
    uint8_t pureza_real;
    uint16_t quilates;

    /* ATRIBUTOS APARENTES (Percepción del jugador y mercado) */
    uint8_t brillo_aparente;
    uint8_t fuego_aparente;
    uint8_t calidad_aparente; 
    
    int8_t sesgo_visual;      
} AtributosGema;

/* ------------------------------------------------------------------ */
/* Accesores de los campos empaquetados en seed                       */
/* ------------------------------------------------------------------ */

uint8_t  gema_ciudad_id(const Gema *g);   
uint8_t  gema_dia(const Gema *g);         
uint16_t gema_rand(const Gema *g);        

/* ------------------------------------------------------------------ */
/* Ciclo de vida y Evolución de Fases                                 */
/* ------------------------------------------------------------------ */

void gema_init(Gema *g);
int  gema_es_valida(const Gema *g);

void crear_gema_desde_chunk(
    Gema          *g,
    const Chunk   *chunk,
    uint8_t        ciudad_id,
    uint8_t        dia_actual
);

/* Control de transiciones de fase */
int gema_cortar(Gema *g);
int gema_pulir(Gema *g, uint8_t random_roll, uint8_t umbral_fallo);
int gema_campo_visible(const Gema *g, uint8_t campo);

/* ------------------------------------------------------------------ */
/* Atributos derivados y Simulación Procedural                        */
/* ------------------------------------------------------------------ */

TipoOpalo   gema_tipo(const Gema *g);
PatronOpalo gema_patron(const Gema *g);
uint8_t     gema_brillo(const Gema *g);
uint8_t     gema_pureza(const Gema *g);
uint8_t     gema_iridiscencia(const Gema *g);
uint8_t     gema_saturacion(const Gema *g);

void gema_calcular_atributos(const Gema *g, AtributosGema *out_attr);

/* ------------------------------------------------------------------ */
/* Pistas                                                             */
/* ------------------------------------------------------------------ */

uint8_t gema_pista_color(const Gema *g);
uint8_t gema_pista_patron(const Gema *g);
uint8_t gema_pista_intensidad(const Gema *g);

/* ------------------------------------------------------------------ */
/* Economía Exponencial                                               */
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
