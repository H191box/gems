/*
 * gema.c
 * Implementación de la entidad Gema — Gacha de Ópalos GBA
 */

#include "gema.h"
#include "opalo.h"
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

/* * SOLUCIÓN AL WARNING: 'rango' ahora es uint16_t para soportar 256 de forma segura
 * y evitar una catástrofe de división por cero en el procesador ARM7TDMI.
 */
static uint8_t slot(uint32_t seed, uint8_t s, uint16_t rango)
{
    uint32_t entropy = seed & 0xFFFFu;  
    return (uint8_t)(hash32(entropy ^ ((uint32_t)s * 0x9e3779b9u)) % rango);
}

/* ------------------------------------------------------------------ */
/* Accesores de campos empaquetados en seed                           */
/* ------------------------------------------------------------------ */

uint8_t gema_ciudad_id(const Gema *g)  { return (uint8_t)((g->seed >> 24) & 0xFF); }
uint8_t gema_dia(const Gema *g)        { return (uint8_t)((g->seed >> 16) & 0xFF); }
uint16_t gema_rand(const Gema *g)      { return (uint16_t)(g->seed & 0xFFFF); }

/* ------------------------------------------------------------------ */
/* Distribución de tipo por bioma (6 Elementos Reales)                 */
/* ------------------------------------------------------------------ */

static const TipoOpalo CICLO[6] = {
    OPALO_CRISTAL, 
    OPALO_BLANCO, 
    OPALO_ROSA, 
    OPALO_FUEGO, 
    OPALO_NEGRO,
    OPALO_GRIS     
};

static const int8_t BIOMA_AFIN[5] = { 0, 1, 2, 4, 3 };

static TipoOpalo derivar_tipo(uint32_t seed, uint8_t bioma)
{
    uint32_t h = hash32(seed & 0xFFFFu);
    int pesos[6];
    
    for (int i = 0; i < 6; i++) pesos[i] = 20;

    if (bioma < 5) {
        int afin       = BIOMA_AFIN[bioma];
        int opuesto    = (afin + 2) % 5;
        int adj_afin_a = (afin + 1) % 5;
        int adj_afin_b = (afin + 4) % 5;
        int adj_op_a   = (opuesto + 1) % 5;
        int adj_op_b   = (opuesto + 4) % 5;

        pesos[afin]       += 30;
        pesos[adj_afin_a] += 10;
        pesos[adj_afin_b] += 10;
        pesos[opuesto]    -= 30;
        pesos[adj_op_a]   -= 10;
        pesos[adj_op_b]   -= 10;

        for (int i = 0; i < 5; i++) {
            if (pesos[i] < 1) pesos[i] = 1;
        }
    }

    int total = 0;
    for (int i = 0; i < 6; i++) total += pesos[i];

    int r = (int)((h >> 4) % (uint32_t)total);
    for (int i = 0; i < 6; i++) {
        r -= pesos[i];
        if (r < 0) return CICLO[i];
    }
    
    return OPALO_GRIS; 
}

/* ------------------------------------------------------------------ */
/* Visibilidad por etapa                                              */
/* ------------------------------------------------------------------ */

static const uint8_t VISIBILIDAD[3] = {
    0x00,
    CAMPO_BRILLO | CAMPO_PATRON,
    CAMPO_TIPO | CAMPO_PATRON | CAMPO_BRILLO | CAMPO_PUREZA | CAMPO_VALOR,
};

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

PatronOpalo gema_patron(const Gema *g)
{
    uint8_t pr = slot(g->seed, 11, 255);
    if      (pr < 128) return PATRON_NEBULA;
    else if (pr < 192) return PATRON_VENAS;
    else if (pr < 224) return PATRON_MATRIX;
    else if (pr < 240) return PATRON_MOSAICO;
    else if (pr < 252) return PATRON_CHAOS;
    else               return PATRON_HARLEQUIN;
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

    /* Aquí ya no saltará warning gracias al rango uint16_t de slot() */
    attr->brillo_real     = slot(g->seed, 12, 256);
    attr->fuego_real      = slot(g->seed, 14, 256); 
    attr->saturacion_real = slot(g->seed, 13, 256);
    attr->pureza_real     = slot(g->seed, 15, 256);
    attr->quilates        = g->quilates;

    int32_t raw_sesgo = slot(g->seed, 30, 81); 
    attr->sesgo_visual = (int8_t)(raw_sesgo - 40);

    if (g->etapa == ETAPA_BRUTA) { 
        float factor_calidad = attr->brillo_real / 255.0f;
        int32_t sesgo_aplicado = (int32_t)(attr->sesgo_visual * (0.5f + factor_calidad));

        int32_t b_ap = (int32_t)attr->brillo_real - 40 + sesgo_aplicado;
        int32_t f_ap = (int32_t)attr->fuego_real - 50 + sesgo_aplicado;

        attr->brillo_aparente = (uint8_t)(b_ap < 0 ? 0 : (b_ap > 255 ? 255 : b_ap));
        attr->fuego_aparente  = (uint8_t)(f_ap < 0 ? 0 : (f_ap > 255 ? 255 : f_ap));
    }
    else if (g->etapa == ETAPA_CORTADA) { 
        int32_t b_ap = (int32_t)attr->brillo_real - 10 + (attr->sesgo_visual / 2);
        int32_t f_ap = (int32_t)attr->fuego_real - 10 + (attr->sesgo_visual / 2);

        attr->brillo_aparente = (uint8_t)(b_ap < 0 ? 0 : (b_ap > 255 ? 255 : b_ap));
        attr->fuego_aparente  = (uint8_t)(f_ap < 0 ? 0 : (f_ap > 255 ? 255 : f_ap));
    }
    else { 
        attr->brillo_aparente = attr->brillo_real;
        attr->fuego_aparente  = attr->fuego_real;
    }

    attr->calidad_aparente = (uint8_t)(((uint32_t)attr->brillo_aparente + attr->fuego_aparente) / 2);

    if (g->flags & GEMA_FLAG_GRIETAS) {
        attr->brillo_aparente /= 2;
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

    uint64_t total = (uint64_t)precio_por_quilate * quilates;
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
    if ((uint8_t)tipo >= NUM_TIPOS_OPALO) return 0;

    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = {
        /* NEGRO   */ 45, /* CRISTAL */ 30, /* FUEGO   */ 35, 
        /* BLANCO  */ 12, /* ROSA    */ 18, /* GRIS    */  5,
    };

    static const uint8_t MULT_PATRON[6] = {
        /* NEBULA */ 10, /* VENAS */ 12, /* MATRIX */ 15, 
        /* MOSAICO */ 20, /* CHAOS */ 25, /* HARLEQUIN */ 35
    };

    uint32_t precio_q = (PRECIO_QUILATE_BASE[(uint8_t)tipo] * MULT_PATRON[(uint8_t)patron]) / 10u;
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
    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = { 45, 30, 35, 12, 18, 5 };
    uint32_t precio_q_estimado = PRECIO_QUILATE_BASE[(uint8_t)tipo];

    /* SOLUCIÓN AL ERROR: Eliminado 'precio_q_estimated' y líneas rotas */
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
/* Resto de utilidades de ciclo de vida e interfaz                    */
/* ------------------------------------------------------------------ */

void gema_pista_color_u8(const Gema *g, uint8_t *ruido_out) { *ruido_out = slot(g->seed, 20, 3); }

uint8_t gema_pista_color(const Gema *g)
{
    uint8_t ruido;
    gema_pista_color_u8(g, &ruido);
    if (ruido == 0) return (uint8_t)gema_tipo(g);
    return (uint8_t)(((uint8_t)gema_tipo(g) + ruido) % NUM_TIPOS_OPALO);
}

uint8_t gema_pista_patron(const Gema *g)     { return slot(g->seed, 21, 6); }
uint8_t gema_pista_intensidad(const Gema *g) { return (uint8_t)(1 + slot(g->seed, 22, 4)); }

void crear_gema_desde_chunk(Gema *g, const Chunk *chunk, uint8_t ciudad_id, uint8_t dia_actual)
{
    uint16_t rand_part = (uint16_t)(hash32(chunk->seed ^ ((uint32_t)ciudad_id * 0x9e3779b9u)) & 0xFFFFu);
    if (rand_part == 0 && ciudad_id == 0 && dia_actual == 0) rand_part = 1;

    g->seed  = ((uint32_t)ciudad_id  << 24) | ((uint32_t)dia_actual << 16) | (uint32_t)rand_part;
    g->etapa    = ETAPA_BRUTA;
    g->flags    = 0;
    g->quilates = chunk->quilates + (uint16_t)(hash32(rand_part) % 30u);
}

int gema_serializar(const Gema *g, uint8_t *buf)
{
    buf[0] = (uint8_t)( g->seed        & 0xFF);
    buf[1] = (uint8_t)((g->seed >>  8) & 0xFF);
    buf[2] = (uint8_t)((g->seed >> 16) & 0xFF);
    buf[3] = (uint8_t)((g->seed >> 24) & 0xFF);
    buf[4] = (uint8_t)( g->quilates    & 0xFF);
    buf[5] = (uint8_t)((g->quilates >>  8) & 0xFF);
    buf[6] = g->etapa;
    buf[7] = g->flags;
    return 8;
}

int gema_deserializar(Gema *g, const uint8_t *buf)
{
    g->seed = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    g->quilates = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    g->etapa = buf[6];
    g->flags = buf[7];
    if (g->etapa > ETAPA_PULIDA) return 0;
    if (g->seed  == 0)           return 0;
    return 1;
}

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
