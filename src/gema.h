/*
 * gema.h
 * Entidad persistente Gema — Gacha de Ópalos GBA
 *
 * FILOSOFÍA:
 *   El precio por quilate se determina mediante atributos de fase y rareza,
 *   y posteriormente se somete a un multiplicador exponencial basado en
 *   los quilates.
 *
 * TAMAÑO: 8 bytes exactos. La SRAM solo almacena seed + quilates + etapa + flags.
 * Todo lo demás (tipo, patrón, brillo, valor…) se reconstruye proceduralmente.
 *
 * MIGRACIÓN (Fase 1 + Fase 2):
 *   - Ya no incluye opalo.h. Los tipos TipoOpalo / PatronOpalo se han
 *     convertido en índices uint8_t respaldados por opalo_data.h y
 *     patron_data.h respectivamente.
 *   - Chunk ha sido eliminado definitivamente. crear_gema_desde_chunk()
 *     ya no forma parte de la API.
 *   - opalo_to_gema() y gema_a_opalo_temp() se mantienen temporalmente
 *     para compatibilidad con código heredado que aún use Opalo como
 *     estructura temporal de UI. Se eliminarán en Fase 4.
 */

#ifndef GEMA_H
#define GEMA_H

#include <stdint.h>
#include "opalo_data.h"   /* TipoOpaloDef, NUM_TIPOS_OPALO, TIPO_OPALO_* */
#include "patron_data.h"  /* PatronDef,    NUM_PATRONES,    PATRON_ID_*  */

/* ------------------------------------------------------------------ */
/* Alias de tipo para compatibilidad con código existente             */
/* El render y plasma.c usan TipoOpalo y PatronOpalo como uint8_t.    */
/* Aquí los definimos como aliases explícitos en lugar de enums,      */
/* para que NUM_TIPOS_OPALO y NUM_PATRONES vengan del catálogo.       */
/* ------------------------------------------------------------------ */

typedef uint8_t TipoOpalo;    /* Índice en tipos_opalo[]  de opalo_data.h  */
typedef uint8_t PatronOpalo;  /* Índice en patrones[]     de patron_data.h */

/* Constantes de tipo nombradas — compatibilidad con switch/case existentes */
#define OPALO_NEGRO    TIPO_OPALO_NEGRO
#define OPALO_CRISTAL  TIPO_OPALO_CRISTAL
#define OPALO_FUEGO    TIPO_OPALO_FUEGO
#define OPALO_BLANCO   TIPO_OPALO_BLANCO
#define OPALO_ROSA     TIPO_OPALO_ROSA
#define OPALO_GRIS     TIPO_OPALO_GRIS

/* Constantes de patrón nombradas — compatibilidad con switch/case existentes */
#define PATRON_NEBULA    PATRON_ID_NEBULA
#define PATRON_VENAS     PATRON_ID_VENAS
#define PATRON_MATRIX    PATRON_ID_MATRIX
#define PATRON_MOSAICO   PATRON_ID_MOSAICO
#define PATRON_CHAOS     PATRON_ID_CHAOS
#define PATRON_HARLEQUIN PATRON_ID_HARLEQUIN

/* ------------------------------------------------------------------ */
/* Estructura temporal de ópalo para UI/render                        */
/* NO se serializa. Se rellena al vuelo desde la seed de una Gema.   */
/* Se mantendrá hasta Fase 4, donde se eliminará junto con opalo.c.   */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t    seed;
    TipoOpalo   tipo;
    PatronOpalo patron;
    uint16_t    quilates;
    uint8_t     brillo;
    uint8_t     saturacion;
    uint8_t     iridiscencia;
    uint8_t     pureza;
    uint8_t     color_offset;
} Opalo;

/* ------------------------------------------------------------------ */
/* Etapas de evolución                                                */
/* ------------------------------------------------------------------ */

#define ETAPA_BRUTA    0  /* Fase 1: Bruto   */
#define ETAPA_CORTADA  1  /* Fase 2: Cabujón */
#define ETAPA_PULIDA   2  /* Fase 3: Pulido  */

/* ------------------------------------------------------------------ */
/* Máscaras de campo visible                                          */
/* ------------------------------------------------------------------ */

#define CAMPO_TIPO     0x01
#define CAMPO_PATRON   0x02
#define CAMPO_BRILLO   0x04
#define CAMPO_PUREZA   0x08
#define CAMPO_VALOR    0x10

/* ------------------------------------------------------------------ */
/* Flags persistentes (empaquetados en 1 byte)                        */
/* ------------------------------------------------------------------ */

#define GEMA_FLAG_FAVORITA   0x01
#define GEMA_FLAG_BLOQUEADA  0x02
#define GEMA_FLAG_GRIETAS    0x04  /* Fallo crítico en pulido — persistente */

/* ------------------------------------------------------------------ */
/* Entidad persistente principal                                      */
/* ------------------------------------------------------------------ */

typedef struct Gema {
    uint32_t seed;      /* ciudad_id[31:24] | dia[23:16] | rand[15:0]  */
    uint16_t quilates;
    uint8_t  etapa;
    uint8_t  flags;
} Gema;

typedef struct {
    /* Atributos reales (potencial máximo, inmutables) */
    uint8_t  brillo_real;
    uint8_t  fuego_real;
    uint8_t  saturacion_real;
    uint8_t  pureza_real;
    uint16_t quilates;

    /* Atributos aparentes (percepción del jugador y del mercado) */
    uint8_t  brillo_aparente;
    uint8_t  fuego_aparente;
    uint8_t  calidad_aparente;
    int8_t   sesgo_visual;
} AtributosGema;

/* ------------------------------------------------------------------ */
/* Accesores de los campos empaquetados en seed                       */
/* ------------------------------------------------------------------ */

uint8_t  gema_ciudad_id(const Gema *g);
uint8_t  gema_dia(const Gema *g);
uint16_t gema_rand(const Gema *g);

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                      */
/* ------------------------------------------------------------------ */

void gema_init(Gema *g);
int  gema_es_valida(const Gema *g);
int  gema_campo_visible(const Gema *g, uint8_t campo);

/* Control de transiciones de fase */
int gema_cortar(Gema *g);
int gema_pulir(Gema *g, uint8_t random_roll, uint8_t umbral_fallo);

/* ------------------------------------------------------------------ */
/* Construcción procedural                                            */
/* ------------------------------------------------------------------ */

/*
 * Crea una Gema a partir de ciudad + día + entropía externa.
 * Reemplaza a crear_gema_desde_chunk(), que dependía de Chunk
 * (ya eliminado del proyecto según Plan de Migración).
 */
void crear_gema(Gema *g, uint8_t ciudad_id, uint8_t dia_actual, uint16_t entropia);

/* ------------------------------------------------------------------ */
/* Atributos derivados                                                */
/* ------------------------------------------------------------------ */

TipoOpalo   gema_tipo(const Gema *g);
PatronOpalo gema_patron(const Gema *g);
uint8_t     gema_brillo(const Gema *g);
uint8_t     gema_pureza(const Gema *g);
uint8_t     gema_iridiscencia(const Gema *g);
uint8_t     gema_saturacion(const Gema *g);

void gema_calcular_atributos(const Gema *g, AtributosGema *out_attr);

/* ------------------------------------------------------------------ */
/* Pistas (información parcial para el jugador)                       */
/* ------------------------------------------------------------------ */

uint8_t gema_pista_color(const Gema *g);
uint8_t gema_pista_patron(const Gema *g);
uint8_t gema_pista_intensidad(const Gema *g);

/* ------------------------------------------------------------------ */
/* Economía exponencial                                               */
/* ------------------------------------------------------------------ */

uint32_t gema_valor_real(const Gema *g);
uint32_t gema_valor_estimado(const Gema *g);

/* ------------------------------------------------------------------ */
/* Persistencia                                                       */
/* ------------------------------------------------------------------ */

int gema_serializar(const Gema *g, uint8_t *buf);
int gema_deserializar(Gema *g, const uint8_t *buf);

/* ------------------------------------------------------------------ */
/* Conversión temporal con Opalo (heredado — eliminar en Fase 4)      */
/* ------------------------------------------------------------------ */

void opalo_to_gema(const Opalo *o, Gema *g);
void gema_a_opalo_temp(Opalo *o, const Gema *g);

#endif /* GEMA_H */
