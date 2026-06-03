/*
 * gema.c
 * Implementación de la entidad Gema — Gacha de Ópalos GBA
 *
 * REGLAS FUNDAMENTALES:
 * 1. La seed empaqueta ciudad_id (bits 31-24), dia (bits 23-16)
 *    y 16 bits de entropía visual (bits 15-0).
 * 2. ciudad_id permite reconstruir el bioma → gema_tipo() es fiel
 *    a la distribución original sin almacenar nada extra.
 * 3. dia permite mostrar la antigüedad del ópalo en la ficha técnica.
 * 4. Todo atributo visual o comercial se deriva desde seed.
 *    Nunca se almacena.
 * 5. seed == 0 es el centinela de ranura vacía.
 */

#include "gema.h"
#include "opalo.h"
#include "ciudades.h"   /* ciudades[], NUM_TOTAL_CIUDADES */

/* ------------------------------------------------------------------ */
/* Hash interno                                                        */
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
 * Extrae un valor en [0, rango) para un slot independiente.
 * Opera sobre los 16 bits de entropía para que ciudad_id y dia
 * no interfieran con los atributos visuales.
 */
static uint8_t slot(uint32_t seed, uint8_t s, uint8_t rango)
{
    uint32_t entropy = seed & 0xFFFFu;  /* solo los 16 bits de rand */
    return (uint8_t)(hash32(entropy ^ ((uint32_t)s * 0x9e3779b9u)) % rango);
}

/* ------------------------------------------------------------------ */
/* Accesores de campos empaquetados en seed                           */
/* ------------------------------------------------------------------ */

uint8_t gema_ciudad_id(const Gema *g)
{
    return (uint8_t)((g->seed >> 24) & 0xFF);
}

uint8_t gema_dia(const Gema *g)
{
    return (uint8_t)((g->seed >> 16) & 0xFF);
}

uint16_t gema_rand(const Gema *g)
{
    return (uint16_t)(g->seed & 0xFFFF);
}

/* ------------------------------------------------------------------ */
/* Distribución de tipo por bioma                                     */
/*                                                                    */
/* Duplicada desde opalo.c — si cambias allí, cambia aquí también.   */
/* ------------------------------------------------------------------ */

static const TipoOpalo CICLO[5] = {
    OPALO_CRISTAL,
    OPALO_BLANCO,
    OPALO_ROSA,
    OPALO_FUEGO,
    OPALO_NEGRO
};

static const int8_t BIOMA_AFIN[5] = {
    0,  /* Bioma 0 Glaciar → CRISTAL */
    1,  /* Bioma 1 Bosque  → BLANCO  */
    2,  /* Bioma 2 Costa   → ROSA    */
    4,  /* Bioma 3 Pantano → NEGRO   */
    3,  /* Bioma 4 Cañón   → FUEGO   */
};

static TipoOpalo derivar_tipo(uint32_t seed, uint8_t bioma)
{
    uint32_t h = hash32(seed & 0xFFFFu);  /* solo entropía */

    int pesos[6];
    for (int i = 0; i < 5; i++) pesos[i] = 20;
    pesos[5] = 20;

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

        for (int i = 0; i < 5; i++)
            if (pesos[i] < 1) pesos[i] = 1;
    }

    int total = 0;
    for (int i = 0; i < 6; i++) total += pesos[i];

    int r = (int)((h >> 4) % (uint32_t)total);
    for (int i = 0; i < 5; i++) {
        r -= pesos[i];
        if (r < 0) return CICLO[i];
    }
    return OPALO_GRIS;
}

/* ------------------------------------------------------------------ */
/* Tabla de visibilidad por etapa                                     */
/* ------------------------------------------------------------------ */

static const uint8_t VISIBILIDAD[3] = {
    /* ETAPA_BRUTA   */ 0x00,
    /* ETAPA_CORTADA */ CAMPO_BRILLO | CAMPO_PATRON,
    /* ETAPA_PULIDA  */ CAMPO_TIPO | CAMPO_PATRON | CAMPO_BRILLO
                      | CAMPO_PUREZA | CAMPO_VALOR,
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

int gema_es_valida(const Gema *g)
{
    return (g->seed != 0);
}

int gema_campo_visible(const Gema *g, uint8_t campo)
{
    if (g->etapa > ETAPA_PULIDA) return 0;
    return (VISIBILIDAD[g->etapa] & campo) != 0;
}

int gema_evolucionar(Gema *g)
{
    if (g->etapa >= ETAPA_PULIDA) return 0;
    g->etapa++;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Atributos derivados                                                */
/*                                                                    */
/* gema_tipo() recupera el bioma desde ciudad_id — fiel a la         */
/* distribución original, sin aproximaciones.                        */
/* ------------------------------------------------------------------ */

TipoOpalo gema_tipo(const Gema *g)
{
    uint8_t ciudad = gema_ciudad_id(g);
    uint8_t bioma  = 0;
    if (ciudad < NUM_TOTAL_CIUDADES)
        bioma = ciudades[ciudad].bioma_id;
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

uint8_t gema_brillo(const Gema *g)
{
    uint8_t v = slot(g->seed, 12, 16);
    return (uint8_t)(16 + (v * v / 15));
}

uint8_t gema_saturacion(const Gema *g)
{
    uint8_t v = slot(g->seed, 13, 16);
    return (uint8_t)(16 + (v * v / 15));
}

uint8_t gema_iridiscencia(const Gema *g)
{
    uint8_t v = slot(g->seed, 14, 16);
    return (uint8_t)(16 + (v * v / 15));
}

uint8_t gema_pureza(const Gema *g)
{
    uint32_t p = hash32((g->seed & 0xFFFFu) ^ ((uint32_t)15 * 0x9e3779b9u)) & 1023u;
    if      (p < 500) return (uint8_t)(40 + (p % 20));
    else if (p < 850) return (uint8_t)(60 + (p % 25));
    else if (p < 980) return (uint8_t)(85 + (p % 10));
    else              return (uint8_t)(95 + (p % 6));
}

/* ------------------------------------------------------------------ */
/* Pistas — ruidosas por diseño                                       */
/* ------------------------------------------------------------------ */

uint8_t gema_pista_color(const Gema *g)
{
    uint8_t ruido = slot(g->seed, 20, 3);
    if (ruido == 0)
        return (uint8_t)gema_tipo(g);
    return (uint8_t)(((uint8_t)gema_tipo(g) + ruido) % NUM_TIPOS_OPALO);
}

uint8_t gema_pista_patron(const Gema *g)
{
    return slot(g->seed, 21, 6);
}

uint8_t gema_pista_intensidad(const Gema *g)
{
    return (uint8_t)(1 + slot(g->seed, 22, 4));
}

/* ------------------------------------------------------------------ */
/* Economía                                                           */
/* ------------------------------------------------------------------ */

uint32_t gema_valor_real(const Gema *g)
{
    if (!gema_es_valida(g))      return 0;
    if (g->etapa < ETAPA_PULIDA) return 0;

    TipoOpalo   tipo   = gema_tipo(g);
    PatronOpalo patron = gema_patron(g);
    if ((uint8_t)tipo >= NUM_TIPOS_OPALO) return 0;

    /* Precio base por quilate segun tipo */
    static const uint16_t PRECIO_QUILATE[NUM_TIPOS_OPALO] = {
        /* NEGRO   */ 48,
        /* CRISTAL */ 32,
        /* FUEGO   */ 38,
        /* BLANCO  */ 14,
        /* ROSA    */ 20,
        /* GRIS    */  6,
    };

    /* Multiplicador de patron x10 (NEBULA=x1.0, HARLEQUIN=x3.0) */
    static const uint8_t MULT_PATRON[6] = {
        /* NEBULA    */ 10,
        /* VENAS     */ 12,
        /* MATRIX    */ 14,
        /* MOSAICO   */ 18,
        /* CHAOS     */ 22,
        /* HARLEQUIN */ 30,
    };

    uint32_t precio_q = PRECIO_QUILATE[(uint8_t)tipo];
    uint32_t mult     = MULT_PATRON[(uint8_t)patron];

    /* FIX: cap de quilates para evitar desbordamiento uint32_t.
     * Sin cap: 48 * 30 * 20020^2 / 100 desborda ampliamente.
     * Con cap 1000: valor máximo = 48 * 30 * 1000 * 1000 / 100
     *             = 14.400.000, bien dentro de uint32_t. */
    uint32_t q = g->quilates;
    if (q > 1000u) q = 1000u;

    /* Dividir antes de la segunda multiplicación para evitar overflow:
     * precio_q * mult <= 48 * 30 = 1440
     * 1440 * 1000 = 1.440.000  (seguro)
     * 1.440.000 * 1000 / 100 = 14.400.000 (seguro) */
    uint32_t valor = precio_q * mult * q / 10u * q / 10u;

    /* Bonus de atributos -- complemento fino, no protagonista */
    uint32_t bonus = 0;
    bonus += (uint32_t)gema_brillo(g)      * 2u;
    bonus += (uint32_t)gema_pureza(g)      * 3u;
    bonus += (uint32_t)gema_iridiscencia(g);
    bonus += (uint32_t)gema_saturacion(g);

    return valor + bonus;
}

uint32_t gema_valor_estimado(const Gema *g)
{
    if (!gema_es_valida(g)) return 0;

    if (g->etapa >= ETAPA_PULIDA)
        return gema_valor_real(g);

    if (g->etapa == ETAPA_CORTADA) {
        uint32_t est = 100u;
        est += (uint32_t)gema_brillo(g) * 2u;
        PatronOpalo patron = gema_patron(g);
        if      (patron == PATRON_HARLEQUIN) est += 150;
        else if (patron == PATRON_MOSAICO)   est += 50;
        est += (uint32_t)g->quilates / 12u;
        return est;
    }

    /* ETAPA_BRUTA: estimacion muy vaga basada en quilates y pista.
     * FIX: cap para evitar desbordamiento con quilates de jackpot.
     * Sin cap: 20020^2 / 50 = ~8.016.008 (pasa, pero rozando)
     * Con cap garantizamos que nunca desborda. */
    uint32_t q = g->quilates;
    if (q > 1000u) q = 1000u;

    uint32_t est = 40u;
    est += (uint32_t)gema_pista_intensidad(g) * 10u;
    est += q * q / 50u;
    return est;
}

/* ------------------------------------------------------------------ */
/* crear_gema_desde_chunk                                             */
/*                                                                    */
/* Construye la seed empaquetada:                                     */
/*   bits 31-24 → ciudad_id  (del chunk)                             */
/*   bits 23-16 → dia_actual (del estado de juego)                   */
/*   bits 15- 0 → hash de chunk->seed, truncado a 16 bits            */
/*                                                                    */
/* La parte rand mezcla la seed del chunk con ciudad_id para que     */
/* dos chunks idénticos en ciudades distintas den ópalos distintos.  */
/* ------------------------------------------------------------------ */

void crear_gema_desde_chunk(Gema *g, const Chunk *chunk,
                             uint8_t ciudad_id, uint8_t dia_actual)
{
    uint16_t rand_part = (uint16_t)(
        hash32(chunk->seed ^ ((uint32_t)ciudad_id * 0x9e3779b9u)) & 0xFFFFu
    );

    /* seed 0 no es válida — forzar al menos rand_part = 1 */
    if (rand_part == 0 && ciudad_id == 0 && dia_actual == 0)
        rand_part = 1;

    g->seed  = ((uint32_t)ciudad_id  << 24)
             | ((uint32_t)dia_actual << 16)
             | (uint32_t)rand_part;

    g->etapa    = ETAPA_BRUTA;
    g->flags    = 0;

    /* Quilates: base del chunk + pequeña variación */
    g->quilates = chunk->quilates
                + (uint16_t)(hash32(rand_part) % 30u);
}

/* ------------------------------------------------------------------ */
/* Serialización little-endian (compatible GBA SAV)                  */
/*                                                                    */
/* Layout (8 bytes):                                                  */
/*   [0-3]  seed      (little-endian)                                */
/*   [4-5]  quilates  (little-endian)                                */
/*   [6]    etapa                                                     */
/*   [7]    flags                                                     */
/* ------------------------------------------------------------------ */

int gema_serializar(const Gema *g, uint8_t *buf)
{
    buf[0] = (uint8_t)( g->seed        & 0xFF);
    buf[1] = (uint8_t)((g->seed >>  8) & 0xFF);
    buf[2] = (uint8_t)((g->seed >> 16) & 0xFF);
    buf[3] = (uint8_t)((g->seed >> 24) & 0xFF);
    buf[4] = (uint8_t)( g->quilates        & 0xFF);
    buf[5] = (uint8_t)((g->quilates >>  8) & 0xFF);
    buf[6] = g->etapa;
    buf[7] = g->flags;
    return 8;
}

int gema_deserializar(Gema *g, const uint8_t *buf)
{
    g->seed = (uint32_t)buf[0]
            | ((uint32_t)buf[1] <<  8)
            | ((uint32_t)buf[2] << 16)
            | ((uint32_t)buf[3] << 24);

    g->quilates = (uint16_t)buf[4]
                | ((uint16_t)buf[5] << 8);

    g->etapa = buf[6];
    g->flags = buf[7];

    if (g->etapa > ETAPA_PULIDA) return 0;
    if (g->seed  == 0)           return 0;

    return 1;
}

/* ------------------------------------------------------------------ */
/* Conversión temporal Opalo <-> Gema                                */
/* ------------------------------------------------------------------ */

void opalo_to_gema(const Opalo *o, Gema *g)
{
    gema_init(g);
    /*
     * Un Opalo no tiene ciudad_id ni dia — se pierden al convertir.
     * Se rellena con ciudad 0, dia 0, y la seed del opalo como rand.
     * Suficiente para renderizado; no apto para ficha técnica real.
     */
    uint16_t rand_part = (uint16_t)(o->seed & 0xFFFFu);
    if (rand_part == 0) rand_part = 1;
    g->seed     = (uint32_t)rand_part;   /* ciudad=0, dia=0, rand=seed */
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
