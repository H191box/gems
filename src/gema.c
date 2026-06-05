/*
 * gema.c
 * Implementación de la entidad Gema — Gacha de Ópalos GBA
 *
 * MIGRACIÓN (Fase 1 + Fase 2 del Plan de Migración):
 *
 *   ELIMINADO:
 *     - #include "opalo.h"  → sustituido por opalo_data.h (vía gema.h)
 *     - Tablas de pesos hardcodeadas de BIOMA_AFIN y CICLO[] locales
 *     - Enum TipoOpalo / PatronOpalo locales (ahora en opalo_data.h / patron_data.h)
 *     - crear_gema_desde_chunk() → depende de Chunk (eliminado del proyecto)
 *     - Toda referencia a Chunk / chunk.h
 *
 *   AÑADIDO:
 *     - crear_gema() → construye Gema desde ciudad_id + dia + entropía directa
 *     - derivar_tipo() ahora consulta tipos_opalo[] de opalo_data.h
 *     - gema_patron() ahora consulta patrones[]    de patron_data.h
 *
 *   SIN CAMBIOS DE COMPORTAMIENTO:
 *     Las funciones de hash, los pesos de bioma, los multiplicadores de
 *     economía y toda la lógica de atributos son idénticos al original.
 *     Las seeds existentes producen exactamente los mismos resultados.
 */

#include "gema.h"
#include "opalo_data.h"
#include "patron_data.h"
#include "ciudades.h"

/* ------------------------------------------------------------------ */
/* Hash e indexación interna                                           */
/* ------------------------------------------------------------------ */

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/*
 * slot() — índice pseudo-aleatorio en [0, rango)
 * rango es uint16_t para soportar 256 de forma segura y evitar
 * división por cero en el procesador ARM7TDMI.
 */
static uint8_t slot(uint32_t seed, uint8_t s, uint16_t rango)
{
    uint32_t entropy = seed & 0xFFFFu;
    return (uint8_t)(hash32(entropy ^ ((uint32_t)s * 0x9e3779b9u)) % rango);
}

/* ------------------------------------------------------------------ */
/* Accesores de campos empaquetados en seed                           */
/* ------------------------------------------------------------------ */

uint8_t  gema_ciudad_id(const Gema *g) { return (uint8_t)((g->seed >> 24) & 0xFF); }
uint8_t  gema_dia(const Gema *g)       { return (uint8_t)((g->seed >> 16) & 0xFF); }
uint16_t gema_rand(const Gema *g)      { return (uint16_t)(g->seed & 0xFFFF); }

/* ------------------------------------------------------------------ */
/* Derivación de tipo por bioma (Fase 1)                              */
/*                                                                    */
/* Los pesos base parten de tipos_opalo[i].rareza, de modo que tipos  */
/* más raros tienen menor probabilidad base. El ajuste por bioma      */
/* opera sobre un afín y su opuesto exactamente como antes.           */
/* Al añadir tipos nuevos en opalo_data.h los pesos se adaptan solos. */
/* ------------------------------------------------------------------ */

/*
 * Tabla de afinidad bioma→tipo: bioma_id [0,4] → TIPO_OPALO_*
 * Mantiene el mismo mapeo que el BIOMA_AFIN[] original para que
 * las seeds existentes produzcan los mismos resultados.
 */
static const uint8_t BIOMA_AFIN[5] = {
    TIPO_OPALO_CRISTAL,  /* bioma 0 */
    TIPO_OPALO_BLANCO,   /* bioma 1 */
    TIPO_OPALO_ROSA,     /* bioma 2 */
    TIPO_OPALO_NEGRO,    /* bioma 3 */  /* era índice 4 en CICLO[] original */
    TIPO_OPALO_FUEGO,    /* bioma 4 */  /* era índice 3 en CICLO[] original */
};

static TipoOpalo derivar_tipo(uint32_t seed, uint8_t bioma)
{
    uint32_t h = hash32(seed & 0xFFFFu);

    /* Peso base proporcional a (4 - rareza): cuanto más raro, menos peso */
    int pesos[NUM_TIPOS_OPALO];
    int i;
    for (i = 0; i < NUM_TIPOS_OPALO; i++) {
        uint8_t rareza = tipos_opalo[i].rareza;
        pesos[i] = (rareza < 4) ? (4 - rareza) * 5 : 1;
    }

    /* Bonificación/penalización por bioma — solo sobre los 5 primeros tipos */
    if (bioma < 5) {
        int afin       = (int)BIOMA_AFIN[bioma];
        int opuesto    = (afin + 2) % 5;
        int adj_afin_a = (afin + 1) % 5;
        int adj_afin_b = (afin + 4) % 5;
        int adj_op_a   = (opuesto + 1) % 5;
        int adj_op_b   = (opuesto + 4) % 5;

        pesos[afin]       += 30;
        pesos[adj_afin_a] += 10;
        pesos[adj_afin_b] += 10;
        pesos[opuesto]    -= 15;
        pesos[adj_op_a]   -= 5;
        pesos[adj_op_b]   -= 5;

        for (i = 0; i < NUM_TIPOS_OPALO; i++) {
            if (pesos[i] < 1) pesos[i] = 1;
        }
    }

    int total = 0;
    for (i = 0; i < NUM_TIPOS_OPALO; i++) total += pesos[i];

    int r = (int)((h >> 4) % (uint32_t)total);
    for (i = 0; i < NUM_TIPOS_OPALO; i++) {
        r -= pesos[i];
        if (r < 0) return (TipoOpalo)i;
    }

    return (TipoOpalo)(NUM_TIPOS_OPALO - 1);  /* fallback al último tipo */
}

/* ------------------------------------------------------------------ */
/* Visibilidad por etapa                                              */
/* ------------------------------------------------------------------ */

static const uint8_t VISIBILIDAD[3] = {
    0x00,
    CAMPO_BRILLO | CAMPO_PATRON,
    CAMPO_TIPO | CAMPO_PATRON | CAMPO_BRILLO | CAMPO_PUREZA | CAMPO_VALOR,
};

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                      */
/* ------------------------------------------------------------------ */

void gema_init(Gema *g)
{
    g->seed     = 0;
    g->quilates = 0;
    g->etapa    = ETAPA_BRUTA;
    g->flags    = 0;
}

int gema_es_valida(const Gema *g) { return (g->seed != 0); }

int gema_campo_visible(const Gema *g, uint8_t campo)
{
    if (g->etapa > ETAPA_PULIDA) return 0;
    return (VISIBILIDAD[g->etapa] & campo) != 0;
}

/* ------------------------------------------------------------------ */
/* Gestión del Taller de Evolución                                    */
/* ------------------------------------------------------------------ */

int gema_cortar(Gema *g)
{
    if (g->etapa != ETAPA_BRUTA) return 0;
    g->etapa = ETAPA_CORTADA;
    return 1;
}

int gema_pulir(Gema *g, uint8_t random_roll, uint8_t umbral_fallo)
{
    if (g->etapa != ETAPA_CORTADA) return 0;
    g->etapa = ETAPA_PULIDA;
    if (random_roll < umbral_fallo) {
        g->flags |= GEMA_FLAG_GRIETAS;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Atributos derivados                                                */
/* ------------------------------------------------------------------ */

TipoOpalo gema_tipo(const Gema *g)
{
    uint8_t ciudad = gema_ciudad_id(g);
    uint8_t bioma  = 0;
    if (ciudad < NUM_TOTAL_CIUDADES) bioma = ciudades[ciudad].bioma_id;
    return derivar_tipo(g->seed, bioma);
}

/*
 * gema_patron() — Fase 2
 * Deriva el índice de patrón y lo valida contra NUM_PATRONES.
 * Así, si en el futuro NUM_PATRONES crece, los patrones nuevos
 * pueden aparecer sin tocar este código.
 */
PatronOpalo gema_patron(const Gema *g)
{
    /* Mismos umbrales que el original para preservar seeds existentes */
    uint8_t pr = slot(g->seed, 11, 255);
    uint8_t id;
    if      (pr < 128) id = PATRON_ID_NEBULA;
    else if (pr < 192) id = PATRON_ID_VENAS;
    else if (pr < 224) id = PATRON_ID_MATRIX;
    else if (pr < 240) id = PATRON_ID_MOSAICO;
    else if (pr < 252) id = PATRON_ID_CHAOS;
    else               id = PATRON_ID_HARLEQUIN;

    /* Guarda de rango: si el id supera el catálogo, cae a Nebula */
    if (id >= NUM_PATRONES) id = PATRON_ID_NEBULA;
    return (PatronOpalo)id;
}

uint8_t gema_brillo(const Gema *g)       { uint8_t v = slot(g->seed, 12, 16); return (uint8_t)(16 + (v * v / 15)); }
uint8_t gema_saturacion(const Gema *g)   { uint8_t v = slot(g->seed, 13, 16); return (uint8_t)(16 + (v * v / 15)); }
uint8_t gema_iridiscencia(const Gema *g) { uint8_t v = slot(g->seed, 14, 16); return (uint8_t)(16 + (v * v / 15)); }

uint8_t gema_pureza(const Gema *g)
{
    uint32_t p = hash32((g->seed & 0xFFFFu) ^ ((uint32_t)15 * 0x9e3779b9u)) & 1023u;
    if      (p < 500) return (uint8_t)(40 + (p % 20));
    else if (p < 850) return (uint8_t)(60 + (p % 25));
    else if (p < 980) return (uint8_t)(85 + (p % 10));
    else              return (uint8_t)(95 + (p % 6));
}

/* ------------------------------------------------------------------ */
/* Simulación procedural de Apariencia                                */
/* ------------------------------------------------------------------ */

void gema_calcular_atributos(const Gema *g, AtributosGema *attr)
{
    if (!g || !attr) return;

    attr->brillo_real     = slot(g->seed, 12, 256);
    attr->fuego_real      = slot(g->seed, 14, 256);
    attr->saturacion_real = slot(g->seed, 13, 256);
    attr->pureza_real     = slot(g->seed, 15, 256);
    attr->quilates        = g->quilates;

    int32_t raw_sesgo  = slot(g->seed, 30, 81);
    attr->sesgo_visual = (int8_t)(raw_sesgo - 40);

    if (g->etapa == ETAPA_BRUTA) {
        float factor_calidad = attr->brillo_real / 255.0f;
        int32_t sesgo_aplicado = (int32_t)(attr->sesgo_visual * (0.5f + factor_calidad));
        int32_t b_ap = (int32_t)attr->brillo_real - 40 + sesgo_aplicado;
        int32_t f_ap = (int32_t)attr->fuego_real  - 50 + sesgo_aplicado;
        attr->brillo_aparente = (uint8_t)(b_ap < 0 ? 0 : (b_ap > 255 ? 255 : b_ap));
        attr->fuego_aparente  = (uint8_t)(f_ap < 0 ? 0 : (f_ap > 255 ? 255 : f_ap));
    }
    else if (g->etapa == ETAPA_CORTADA) {
        int32_t b_ap = (int32_t)attr->brillo_real - 10 + (attr->sesgo_visual / 2);
        int32_t f_ap = (int32_t)attr->fuego_real  - 10 + (attr->sesgo_visual / 2);
        attr->brillo_aparente = (uint8_t)(b_ap < 0 ? 0 : (b_ap > 255 ? 255 : b_ap));
        attr->fuego_aparente  = (uint8_t)(f_ap < 0 ? 0 : (f_ap > 255 ? 255 : f_ap));
    }
    else {
        attr->brillo_aparente = attr->brillo_real;
        attr->fuego_aparente  = attr->fuego_real;
    }

    attr->calidad_aparente = (uint8_t)(((uint32_t)attr->brillo_aparente + attr->fuego_aparente) / 2);

    if (g->flags & GEMA_FLAG_GRIETAS) {
        attr->brillo_aparente  /= 2;
        attr->fuego_aparente   /= 3;
        attr->calidad_aparente /= 2;
    }
}

/* ------------------------------------------------------------------ */
/* Motor de Economía Exponencial                                      */
/* ------------------------------------------------------------------ */

static uint32_t aplicar_curva_exponencial(uint32_t precio_por_quilate, uint16_t quilates)
{
    if (quilates == 0) return 0;

    uint64_t total            = (uint64_t)precio_por_quilate * quilates;
    uint64_t factor_exponencial = 100;
    int tramos = quilates / 30;

    for (int i = 0; i < tramos; i++) {
        factor_exponencial = (factor_exponencial * 125u) / 100u;
        if (factor_exponencial > 500000u) {
            factor_exponencial = 500000u;
            break;
        }
    }

    uint64_t precio_final = (total * factor_exponencial) / 100u;
    if (precio_final > 0xFFFFFFFFu) return 0xFFFFFFFFu;
    return (uint32_t)precio_final;
}

uint32_t gema_valor_real(const Gema *g)
{
    if (!gema_es_valida(g)) return 0;

    TipoOpalo   tipo   = gema_tipo(g);
    PatronOpalo patron = gema_patron(g);

    if (tipo   >= NUM_TIPOS_OPALO) return 0;
    if (patron >= NUM_PATRONES)    return 0;

    /*
     * Precio base por quilate indexado por tipo.
     * Al añadir tipos en opalo_data.h hay que extender esta tabla.
     * Una mejora futura puede mover estos valores a TipoOpaloDef.
     */
    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = {
        /* NEGRO   */ 45,
        /* CRISTAL */ 30,
        /* FUEGO   */ 35,
        /* BLANCO  */ 12,
        /* ROSA    */ 18,
        /* GRIS    */  5,
    };

    /*
     * Multiplicador de patrón indexado por PatronOpalo.
     * Al añadir patrones en patron_data.h hay que extender esta tabla.
     * Una mejora futura puede mover estos valores a PatronDef.
     */
    static const uint8_t MULT_PATRON[NUM_PATRONES] = {
        /* NEBULA    */ 10,
        /* VENAS     */ 12,
        /* MATRIX    */ 15,
        /* MOSAICO   */ 20,
        /* CHAOS     */ 25,
        /* HARLEQUIN */ 35,
    };

    uint32_t precio_q = ((uint32_t)PRECIO_QUILATE_BASE[tipo] * MULT_PATRON[patron]) / 10u;
    precio_q += (slot(g->seed, 12, 50)) + (slot(g->seed, 14, 50));

    uint32_t valor = aplicar_curva_exponencial(precio_q, g->quilates);

    if (g->flags & GEMA_FLAG_GRIETAS) {
        valor = (valor * 30u) / 100u;
    }

    return valor;
}

uint32_t gema_valor_estimado(const Gema *g)
{
    if (!gema_es_valida(g)) return 0;

    if (g->etapa >= ETAPA_PULIDA) {
        return gema_valor_real(g);
    }

    AtributosGema attr;
    gema_calcular_atributos(g, &attr);

    TipoOpalo tipo = gema_tipo(g);
    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = {
        45, 30, 35, 12, 18, 5
    };

    if (tipo >= NUM_TIPOS_OPALO) return 0;

    uint32_t precio_q_estimado = PRECIO_QUILATE_BASE[tipo];

    if (g->etapa == ETAPA_CORTADA) {
        precio_q_estimado = (precio_q_estimado * 75u) / 100u;
        precio_q_estimado += (attr.calidad_aparente / 4);
        return aplicar_curva_exponencial(precio_q_estimado, g->quilates);
    }

    precio_q_estimado = (precio_q_estimado * 18u) / 100u;
    precio_q_estimado += (attr.calidad_aparente / 8);

    uint32_t valor_f1 = aplicar_curva_exponencial(precio_q_estimado, g->quilates);
    return (valor_f1 < 10u) ? 10u : valor_f1;
}

/* ------------------------------------------------------------------ */
/* Construcción procedural                                            */
/* ------------------------------------------------------------------ */

/*
 * crear_gema() — reemplaza crear_gema_desde_chunk()
 *
 * Construye una Gema directamente desde ciudad_id, dia_actual y una
 * semilla de entropía externa (puede venir del RNG del juego, de un
 * timer, del índice de slot, etc.).
 *
 * Layout de seed:
 *   bits [31:24] = ciudad_id
 *   bits [23:16] = dia_actual
 *   bits [15: 0] = entropia (nunca 0 si ciudad=0 y dia=0)
 */
void crear_gema(Gema *g, uint8_t ciudad_id, uint8_t dia_actual, uint16_t entropia)
{
    uint16_t rand_part = (uint16_t)(hash32((uint32_t)entropia ^ ((uint32_t)ciudad_id * 0x9e3779b9u)) & 0xFFFFu);
    if (rand_part == 0 && ciudad_id == 0 && dia_actual == 0) rand_part = 1;

    g->seed     = ((uint32_t)ciudad_id << 24)
                | ((uint32_t)dia_actual << 16)
                | (uint32_t)rand_part;
    g->etapa    = ETAPA_BRUTA;
    g->flags    = 0;
    g->quilates = 30u + (uint16_t)(hash32(rand_part) % 30u);
}

/* ------------------------------------------------------------------ */
/* Utilidades de ciclo de vida e interfaz                             */
/* ------------------------------------------------------------------ */

void gema_pista_color_u8(const Gema *g, uint8_t *ruido_out)
{
    *ruido_out = slot(g->seed, 20, 3);
}

uint8_t gema_pista_color(const Gema *g)
{
    uint8_t ruido;
    gema_pista_color_u8(g, &ruido);
    if (ruido == 0) return (uint8_t)gema_tipo(g);
    return (uint8_t)(((uint8_t)gema_tipo(g) + ruido) % NUM_TIPOS_OPALO);
}

uint8_t gema_pista_patron(const Gema *g)     { return slot(g->seed, 21, (uint16_t)NUM_PATRONES); }
uint8_t gema_pista_intensidad(const Gema *g) { return (uint8_t)(1 + slot(g->seed, 22, 4)); }

/* ------------------------------------------------------------------ */
/* Persistencia                                                       */
/* ------------------------------------------------------------------ */

int gema_serializar(const Gema *g, uint8_t *buf)
{
    buf[0] = (uint8_t)( g->seed         & 0xFF);
    buf[1] = (uint8_t)((g->seed >>  8)  & 0xFF);
    buf[2] = (uint8_t)((g->seed >> 16)  & 0xFF);
    buf[3] = (uint8_t)((g->seed >> 24)  & 0xFF);
    buf[4] = (uint8_t)( g->quilates     & 0xFF);
    buf[5] = (uint8_t)((g->quilates >>  8) & 0xFF);
    buf[6] = g->etapa;
    buf[7] = g->flags;
    return 8;
}

int gema_deserializar(Gema *g, const uint8_t *buf)
{
    g->seed     = (uint32_t)buf[0]
                | ((uint32_t)buf[1] <<  8)
                | ((uint32_t)buf[2] << 16)
                | ((uint32_t)buf[3] << 24);
    g->quilates = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    g->etapa    = buf[6];
    g->flags    = buf[7];
    if (g->etapa > ETAPA_PULIDA) return 0;
    if (g->seed  == 0)           return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Conversión temporal con Opalo (heredado — eliminar en Fase 4)      */
/* ------------------------------------------------------------------ */

void opalo_to_gema(const Opalo *o, Gema *g)
{
    gema_init(g);
    uint16_t rand_part = (uint16_t)(o->seed & 0xFFFFu);
    if (rand_part == 0) rand_part = 1;
    g->seed     = (uint32_t)rand_part;
    g->quilates = o->quilates;
    g->etapa    = ETAPA_PULIDA;
    g->flags    = 0;
}

void gema_a_opalo_temp(Opalo *o, const Gema *g)
{
    o->tipo         = gema_tipo(g);
    o->patron       = gema_patron(g);
    o->brillo       = gema_brillo(g);
    o->pureza       = gema_pureza(g);
    o->iridiscencia = gema_iridiscencia(g);
    o->saturacion   = gema_saturacion(g);
    o->quilates     = g->quilates;
    o->seed         = g->seed;
    o->color_offset = (uint8_t)(g->seed & 0xFF);
}
