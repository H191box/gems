/*
 * gema.c
 * Implementación de la entidad Gema — Gacha de Ópalos GBA
 *
 * REGLAS FUNDAMENTALES:
 * 1. La seed se usa UNA SOLA VEZ en crear_gema_desde_chunk().
 * 2. Chunk no se define aquí — viene de opalo.h.
 * 3. El ID se recibe como parámetro — save.proximo_id lo gestiona.
 * 4. El valor se calcula desde atributos reales, sin rareza_real explícita.
 * 5. gema_valor_estimado() solo usa campos visibles en la etapa actual.
 */

#include "gema.h"
#include "opalo.h"   /* TipoOpalo, PatronOpalo, Chunk, generar_opalo_con_bioma */

/* ------------------------------------------------------------------ */
/* Hash interno (no exponer fuera de este módulo)                       */
/* ------------------------------------------------------------------ */

static uint32_t hash32(uint32_t x)
{
    /* Mismo algoritmo que opalo.c para coherencia */
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/* Extrae un valor en [0, rango) para un slot independiente */
static uint8_t rango_seed(uint32_t seed, uint8_t slot, uint8_t rango)
{
    return (uint8_t)(hash32(seed ^ ((uint32_t)slot * 0x9e3779b9u)) % rango);
}

/* ------------------------------------------------------------------ */
/* Tabla de visibilidad                                                 */
/* bits: CAMPO_TIPO | CAMPO_PATRON | CAMPO_BRILLO | CAMPO_PUREZA | CAMPO_VALOR */
/* ------------------------------------------------------------------ */

static const uint8_t VISIBILIDAD[3] = {
    /* ETAPA_BRUTA   */ 0x00,
    /* ETAPA_CORTADA */ CAMPO_BRILLO | CAMPO_PATRON,
    /* ETAPA_PULIDA  */ CAMPO_TIPO | CAMPO_PATRON | CAMPO_BRILLO
                      | CAMPO_PUREZA | CAMPO_VALOR,
};

/* ------------------------------------------------------------------ */
/* Tablas para el cálculo de valor                                       */
/* ------------------------------------------------------------------ */

/* Base por tipo (monedas) — sin etiqueta "Legendario", el valor emerge */
static const uint16_t BASE_TIPO[NUM_TIPOS_OPALO] = {
    /* NEGRO   */ 480,
    /* CRISTAL */ 320,
    /* FUEGO   */ 380,
    /* BLANCO  */ 140,
    /* ROSA    */ 200,
    /* GRIS    */  60,
};

/* ------------------------------------------------------------------ */
/* API pública                                                            */
/* ------------------------------------------------------------------ */

void gema_init(Gema *g)
{
    g->id               = GEMA_ID_NULO;
    g->etapa            = ETAPA_BRUTA;
    g->tipo_real        = 0;
    g->patron_real      = 0;
    g->brillo_real      = 0;
    g->pureza_real      = 0;
    g->iridiscencia     = 0;
    g->saturacion       = 0;
    g->pista_color      = 0;
    g->pista_intensidad = 0;
    g->pista_patron     = 0;
    g->quilates         = 0;
    g->seed_visual      = 0;
    g->_pad[0]          = 0;
    g->_pad[1]          = 0;
}

int gema_es_valida(const Gema *g)
{
    return (g->id != GEMA_ID_NULO);
}

int gema_campo_visible(const Gema *g, uint8_t campo)
{
    uint8_t etapa = g->etapa;
    if (etapa > ETAPA_PULIDA) return 0;
    return (VISIBILIDAD[etapa] & campo) != 0;
}

int gema_evolucionar(Gema *g)
{
    if (g->etapa >= ETAPA_PULIDA) return 0;
    g->etapa++;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Valor real — usa todos los atributos internos                         */
/* */
/* Fórmula intencionalmente continua: no hay categorías "Legendaria".   */
/* Dos ópalos del mismo tipo pueden valer 200 o 2000 según atributos.   */
/* ------------------------------------------------------------------ */

uint32_t gema_valor_real(const Gema *g)
{
    if (g->etapa < ETAPA_PULIDA) return 0;
    if (g->tipo_real >= NUM_TIPOS_OPALO) return 0;

    uint32_t base  = BASE_TIPO[g->tipo_real];

    /* Bonus multiplicativos desde atributos */
    uint32_t brillo_bonus      = (uint32_t)g->brillo_real * 3u;
    uint32_t pureza_bonus      = (uint32_t)g->pureza_real * 4u;
    uint32_t irid_bonus        = (uint32_t)g->iridiscencia * 2u;
    uint32_t sat_bonus         = (uint32_t)g->saturacion;
    uint32_t quilates_bonus    = (uint32_t)g->quilates / 8u;

    /* Bonus por patrón raro */
    uint32_t patron_bonus = 0;
    if (g->patron_real == PATRON_HARLEQUIN) patron_bonus = 300;
    else if (g->patron_real == PATRON_MOSAICO) patron_bonus = 100;

    return base
         + brillo_bonus
         + pureza_bonus
         + irid_bonus
         + sat_bonus
         + quilates_bonus
         + patron_bonus;
}

/* ------------------------------------------------------------------ */
/* Valor estimado — solo usa lo visible en la etapa actual               */
/* */
/* Esta es la clave del mercado: el comerciante no conoce el real.       */
/* En BRUTA: la estimación es solo una señal ruidosa de las grietas.     */
/* En CORTADA: mejora, pero sigue siendo parcial.                       */
/* En PULIDA: coincide con gema_valor_real().                           */
/* ------------------------------------------------------------------ */

uint32_t gema_valor_estimado(const Gema *g)
{
    if (!gema_es_valida(g)) return 0;

    if (g->etapa >= ETAPA_PULIDA) {
        return gema_valor_real(g);
    }

    if (g->etapa == ETAPA_CORTADA) {
        /* Se conoce brillo y patrón aproximado */
        uint32_t est = 100u;
        est += (uint32_t)g->brillo_real * 2u;   /* brillo visible */
        uint32_t patron_bonus = 0;
        if (g->patron_real == PATRON_HARLEQUIN) patron_bonus = 150;
        else if (g->patron_real == PATRON_MOSAICO) patron_bonus = 50;
        est += patron_bonus;
        est += (uint32_t)g->quilates / 12u;
        return est;
    }

    /* ETAPA_BRUTA: solo pistas de grietas */
    /* pista_intensidad es la única señal cuantitativa */
    uint32_t est = 40u + (uint32_t)g->pista_intensidad / 4u;

    /* El color de la grieta da una pista muy vaga del tipo */
    if (g->pista_color < NUM_TIPOS_OPALO) {
        est += BASE_TIPO[g->pista_color] / 8u;
    }

    return est;
}

/* ------------------------------------------------------------------ */
/* crear_gema_desde_chunk                                               */
/* */
/* ÚNICA función que lee la seed del chunk.                             */
/* El ID viene de save.proximo_id — el llamador lo incrementa allí.     */
/* El bioma viene de la ciudad/región actual.                           */
/* ------------------------------------------------------------------ */

void crear_gema_desde_chunk(Gema *g, const Chunk *chunk,
                            uint32_t id, uint8_t bioma)
{
    uint32_t seed = chunk->seed;

    /* --- Identidad ------------------------------------------------ */
    g->id    = id;
    g->etapa = ETAPA_BRUTA;

    /* seed_visual: independiente de los atributos económicos */
    g->seed_visual = hash32(seed ^ 0xDEADBEEFu);

    /* --- Quilates: heredados del chunk con pequeña variación ------- */
    {
        uint16_t var = rango_seed(seed, 0, 30);
        g->quilates  = chunk->quilates + var;
    }

    /* --- Atributos reales: reutilizar la lógica de generar_opalo -- */
    /* En lugar de duplicar la distribución por bioma, generamos un   */
    /* Opalo temporal solo para extraer sus atributos y descartarlo.   */
    {
        calcular_atributos_gema(g, seed, bioma);
    }

    /* --- Pistas visibles en etapa bruta --------------------------- */
    /* Derivan de la seed pero son intencionalmente ruidosas:          */
    /* no revelan el tipo real, solo dan una señal especulativa.       */
    g->pista_intensidad = chunk->intensidad_grieta;
    g->pista_patron     = rango_seed(seed, 5, 6);   /* patrón aproximado */

    /* El color de la grieta puede ser el tipo real, o uno adyacente  */
    {
        uint8_t ruido = rango_seed(seed, 6, 3);  /* 0, 1 o 2 */
        if (ruido == 0) {
            g->pista_color = g->tipo_real;
        } else {
            /* color desplazado — el jugador no puede estar seguro */
            g->pista_color = (g->tipo_real + ruido) % NUM_TIPOS_OPALO;
        }
    }

    /* --- Padding a cero ------------------------------------------- */
    g->_pad[0] = 0;
    g->_pad[1] = 0;
}

/* ------------------------------------------------------------------ */
/* Serialización little-endian (compatible GBA SAV)                     */
/* */
/* Layout (20 bytes):                                                   */
/* [0-3]   id                                                       */
/* [4]     etapa                                                    */
/* [5]     tipo_real                                                */
/* [6]     patron_real                                              */
/* [7]     brillo_real                                              */
/* [8]     pureza_real                                              */
/* [9]     iridiscencia                                             */
/* [10]    saturacion                                               */
/* [11]    pista_color                                              */
/* [12]    pista_intensidad                                         */
/* [13]    pista_patron                                             */
/* [14-15] quilates                                                 */
/* [16-19] seed_visual                                              */
/* ------------------------------------------------------------------ */

int gema_serializar(const Gema *g, uint8_t *buf)
{
    buf[0]  = (uint8_t)(g->id & 0xFF);
    buf[1]  = (uint8_t)((g->id >>  8) & 0xFF);
    buf[2]  = (uint8_t)((g->id >> 16) & 0xFF);
    buf[3]  = (uint8_t)((g->id >> 24) & 0xFF);
    buf[4]  = g->etapa;
    buf[5]  = g->tipo_real;
    buf[6]  = g->patron_real;
    buf[7]  = g->brillo_real;
    buf[8]  = g->pureza_real;
    buf[9]  = g->iridiscencia;
    buf[10] = g->saturacion;
    buf[11] = g->pista_color;
    buf[12] = g->pista_intensidad;
    buf[13] = g->pista_patron;
    buf[14] = (uint8_t)(g->quilates & 0xFF);
    buf[15] = (uint8_t)((g->quilates >> 8) & 0xFF);
    buf[16] = (uint8_t)(g->seed_visual & 0xFF);
    buf[17] = (uint8_t)((g->seed_visual >>  8) & 0xFF);
    buf[18] = (uint8_t)((g->seed_visual >> 16) & 0xFF);
    buf[19] = (uint8_t)((g->seed_visual >> 24) & 0xFF);
    return 20;
}

int gema_deserializar(Gema *g, const uint8_t *buf)
{
    g->id = (uint32_t)buf[0]
          | ((uint32_t)buf[1] <<  8)
          | ((uint32_t)buf[2] << 16)
          | ((uint32_t)buf[3] << 24);

    g->etapa = buf[4];
    if (g->etapa > ETAPA_PULIDA) return 0;

    g->tipo_real        = buf[5];
    g->patron_real      = buf[6];
    g->brillo_real      = buf[7];
    g->pureza_real      = buf[8];
    g->iridiscencia     = buf[9];
    g->saturacion       = buf[10];
    g->pista_color      = buf[11];
    g->pista_intensidad = buf[12];
    g->pista_patron     = buf[13];
    g->quilates  = (uint16_t)buf[14] | ((uint16_t)buf[15] << 8);
    g->seed_visual = (uint32_t)buf[16]
                   | ((uint32_t)buf[17] <<  8)
                   | ((uint32_t)buf[18] << 16)
                   | ((uint32_t)buf[19] << 24);

    g->_pad[0] = 0;
    g->_pad[1] = 0;

    if (g->tipo_real   >= NUM_TIPOS_OPALO) return 0;
    if (g->etapa       >  ETAPA_PULIDA)    return 0;

    return 1;
}

/* ------------------------------------------------------------------ */
/* Funciones Puente: Opalo <-> Gema                                   */
/* ------------------------------------------------------------------ */

void opalo_to_gema(const Opalo *o, Gema *g)
{
    gema_init(g);

    g->id           = GEMA_ID_NULO; /* Se inicializa huérfano de ID en el motor visual */
    g->etapa        = ETAPA_PULIDA; /* Un ópalo consolidado asume etapa completa descubierta */
    g->tipo_real    = (uint8_t)o->tipo;
    g->patron_real  = (uint8_t)o->patron;
    g->brillo_real  = o->brillo;
    g->pureza_real  = o->pureza;
    g->iridiscencia = o->iridiscencia;
    g->saturacion   = o->saturacion;
    g->quilates     = o->quilates;
    g->seed_visual  = o->seed;
    
    /* El color_offset del ópalo se mapea a pista_color para el motor de plasma */
    g->pista_color  = o->color_offset;
}

void gema_a_opalo_temp(Opalo *o, const Gema *g)
{
    o->tipo         = (TipoOpalo)g->tipo_real;
    o->patron       = (PatronOpalo)g->patron_real;
    o->brillo       = g->brillo_real;
    o->pureza       = g->pureza_real;
    o->iridiscencia = g->iridiscencia;
    o->saturacion   = g->saturacion;
    o->quilates     = g->quilates;
    o->seed         = g->seed_visual;
    /* color_offset: byte bajo de seed_visual, igual que en generar_opalo */
    o->color_offset = (uint8_t)(g->seed_visual & 0xFF);
}
