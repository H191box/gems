// gema.c

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
    /*
     * Distribución de probabilidad por rareza (255 valores):
     *
     *   [  0, 109) → NEBULA     110/255 = 43%  rareza 0 — común
     *   [110, 174) → VENAS       65/255 = 25%  rareza 1 — poco común
     *   [174, 210) → MOSAICO     36/255 = 14%  rareza 2 — raro
     *   [210, 234) → CHAOS       24/255 =  9%  rareza 3 — épico
     *   [234, 244) → PINFIRE     10/255 =  4%  rareza 3 — épico
     *   [244, 251) → HARLEQUIN    7/255 =  3%  rareza 4 — legendario
     *   [251, 255) → MATRIX       4/255 =  2%  rareza 4 — legendario
     *
     * NOTA: Matrix pasa a ser el más raro del juego (2%), coherente
     * con ser el patrón visualmente más complejo y valioso.
     * Pinfire y Chaos comparten rareza épica pero Pinfire es algo
     * menos frecuente para reforzar su sensación de hallazgo especial.
     */
    uint8_t pr = slot(g->seed, 11, 255);
    uint8_t id;
    if      (pr < 110) id = PATRON_ID_NEBULA;
    else if (pr < 175) id = PATRON_ID_VENAS;
    else if (pr < 211) id = PATRON_ID_MOSAICO;
    else if (pr < 235) id = PATRON_ID_CHAOS;
    else if (pr < 245) id = PATRON_ID_PINFIRE;
    else if (pr < 252) id = PATRON_ID_HARLEQUIN;
    else               id = PATRON_ID_MATRIX;

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

uint8_t gema_sesgo_peso(const Gema *g)
{
    return 5 + ((g->seed >> 7) % 36);
}

uint16_t gema_quilates_aparentes(const Gema *g)
{
    uint32_t q = g->quilates;

    if (g->etapa == ETAPA_BRUTA) {
        q += (q * gema_sesgo_peso(g)) / 100;
    }

    return (uint16_t)q;
}

/* ------------------------------------------------------------------ */
/* Suciedad aparente (Fase 2)                                         */
/*                                                                    */
/* Análogo exacto a gema_sesgo_peso(): deriva un nivel consistente    */
/* por gema que en ETAPA_CORTADA distorsiona los atributos estéticos  */
/* visibles. En ETAPA_PULIDA el render ignora este valor.             */
/*                                                                    */
/* Usa slot 25 — libre, no colisiona con ningún slot existente.       */
/*                                                                    */
/* Tabla de desviación máxima por nivel:                              */
/*   0 → ±5%   (gema casi limpia, la suciedad apenas afecta)         */
/*   1 → ±15%  (suciedad moderada)                                   */
/*   2 → ±28%  (muy sucia)                                           */
/*   3 → ±40%  (lodosa, atributos muy difíciles de leer)             */
/* ------------------------------------------------------------------ */

uint8_t gema_suciedad(const Gema *g)
{
    return slot(g->seed, 25, SUCIEDAD_NIVELES);
}

/* ------------------------------------------------------------------ */
/* Simulación procedural de Apariencia                                */
/* ------------------------------------------------------------------ */

/*
 * Tabla de desviación máxima por nivel de suciedad (en 1/100).
 * Nivel 0: ±5%, nivel 1: ±15%, nivel 2: ±28%, nivel 3: ±40%.
 * El signo de la desviación se decide con un segundo slot por atributo,
 * de modo que la suciedad puede tanto inflar como desinflar cada valor
 * — igual que el sesgo_visual, nunca se sabe si la gema mejora o empeora
 * al pulirse.
 */
static const uint8_t SUCIEDAD_DESV[SUCIEDAD_NIVELES] = { 5, 15, 28, 40 };

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
        /*
         * Fase 2: los atributos estéticos reales son ya visibles en
         * principio, pero la suciedad residual los distorsiona.
         *
         * Mecánica (paralela a quilates_aparentes en Fase 1):
         *   - gema_suciedad() devuelve el nivel [0..3], que mapea a un
         *     porcentaje de desviación máxima via SUCIEDAD_DESV[].
         *   - Dos slots independientes (26 para brillo, 27 para fuego)
         *     determinan el signo y la magnitud real de cada distorsión.
         *     Esto hace que brillo y fuego puedan desviarse en sentidos
         *     opuestos: una gema puede parecer apagada pero con mucho
         *     fuego, o viceversa.
         *   - El rango de slot es 201 [0..200] → restar 100 da [-100..+100],
         *     luego se escala por desv/100 para obtener el delta final.
         *   - Al pulir (Fase 3), el else inferior devuelve los valores
         *     reales sin ningún sesgo: la "revelación" es inmediata.
         */
        uint8_t nivel_suciedad = gema_suciedad(g);
        uint8_t desv           = SUCIEDAD_DESV[nivel_suciedad];

        /* Delta brillo: slot 26, rango [-100..+100] escalado a [-desv..+desv] */
        int32_t delta_b = ((int32_t)slot(g->seed, 26, 201) - 100) * desv / 100;
        /* Delta fuego:  slot 27, independiente del anterior              */
        int32_t delta_f = ((int32_t)slot(g->seed, 27, 201) - 100) * desv / 100;

        int32_t b_ap = (int32_t)attr->brillo_real + delta_b;
        int32_t f_ap = (int32_t)attr->fuego_real  + delta_f;

        attr->brillo_aparente = (uint8_t)(b_ap < 0 ? 0 : (b_ap > 255 ? 255 : b_ap));
        attr->fuego_aparente  = (uint8_t)(f_ap < 0 ? 0 : (f_ap > 255 ? 255 : f_ap));
    }
    else {
        /* ETAPA_PULIDA: suciedad eliminada, atributos reales al descubierto */
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

    uint64_t total              = (uint64_t)precio_por_quilate * quilates;
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
     * BASE ECONÓMICA AJUSTADA
     * Objetivo: bajar inflación global (~30–45% menos que antes)
     */
    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = {
        /* NEGRO   */ 28,
        /* CRISTAL */ 22,
        /* FUEGO   */ 25,
        /* BLANCO  */ 12,
        /* ROSA    */ 16,
        /* GRIS    */  6,
    };

    /*
     * PATRONES REBALANCEADOS (menos escalones extremos)
     * Objetivo: diferencias visibles pero no explosivas
     */
    static const uint8_t MULT_PATRON[NUM_PATRONES] = {
        /* NEBULA    */ 10,  /* 1.00x */
        /* VENAS     */ 11,  /* 1.10x */
        /* MATRIX    */ 12,  /* 1.20x */
        /* MOSAICO   */ 13,  /* 1.30x */
        /* CHAOS     */ 14,  /* 1.40x */
        /* HARLEQUIN */ 16,  /* 1.60x */
    };

    uint32_t precio_q =
        ((uint32_t)PRECIO_QUILATE_BASE[tipo] * MULT_PATRON[patron]) / 10u;

    /*
     * RNG reducido (~±10% de ruido)
     */
    int32_t ruido =
        (int32_t)slot(g->seed, 12, 21) - 10 +
        (int32_t)slot(g->seed, 14, 21) - 10;

    precio_q = (uint32_t)((int32_t)precio_q + ruido);
    if ((int32_t)precio_q < 1) precio_q = 1;

    uint32_t valor = aplicar_curva_exponencial(precio_q, g->quilates);

    if (g->flags & GEMA_FLAG_GRIETAS) {
        valor = (valor * 45u) / 100u;  /* 55% pérdida */
    }

    return valor;
}

uint32_t gema_valor_estimado(const Gema *g)
{
    if (!gema_es_valida(g)) return 0;

    /* FASE 3 (Etapa Pulida): valor real definitivo */
    if (g->etapa >= ETAPA_PULIDA) {
        return gema_valor_real(g);
    }

    TipoOpalo tipo = gema_tipo(g);
    if (tipo >= NUM_TIPOS_OPALO) return 0;

    /* Mismos arrays que gema_valor_real() para evitar choque de precios */
    static const uint16_t PRECIO_QUILATE_BASE[NUM_TIPOS_OPALO] = {
        28, 22, 25, 12, 16, 6
    };
    static const uint8_t MULT_PATRON[NUM_PATRONES] = {
        10, 11, 12, 13, 14, 16
    };

    PatronOpalo patron = gema_patron(g);

    uint32_t precio_q = ((uint32_t)PRECIO_QUILATE_BASE[tipo] * MULT_PATRON[patron]) / 10u;

    int32_t ruido = (int32_t)slot(g->seed, 12, 21) - 10 + (int32_t)slot(g->seed, 14, 21) - 10;
    precio_q = (uint32_t)((int32_t)precio_q + ruido);
    if ((int32_t)precio_q < 1) precio_q = 1;

    /*
     * FASE 2 (Etapa Cortada):
     * Valor al 85% del potencial real. Al pulir subirá ~17%...
     * a menos que se agriete (cae al 45%).
     */
    if (g->etapa == ETAPA_CORTADA) {
        uint32_t valor_teorico = aplicar_curva_exponencial(precio_q, g->quilates);
        return (valor_teorico * 85u) / 100u;
    }

    /*
     * FASE 1 (Etapa Bruta):
     * Base al 50%, pero quilates aparentes (inflados con roca inútil).
     * Al cortar el peso bajará pero la calidad subirá del 50% al 85%.
     */
    uint32_t precio_q_bruto = (precio_q * 50u) / 100u;
    if (precio_q_bruto < 1) precio_q_bruto = 1;

    uint32_t valor_f1 = aplicar_curva_exponencial(precio_q_bruto, gema_quilates_aparentes(g));

    return (valor_f1 < 10u) ? 10u : valor_f1;
}

/* ------------------------------------------------------------------ */
/* Construcción procedural                                            */
/* ------------------------------------------------------------------ */

/*
 * crear_gema() — reemplaza crear_gema_desde_chunk()
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
