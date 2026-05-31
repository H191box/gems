#include "opalo.h"
#include "mina.h"
#include "data.h"
#include "ciudades.h"
#include "gema.h"

int COLOR_PRUEBA = 2;

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

// ----------------------------------------------------
// SISTEMA DE AFINIDADES POR BIOMA
//
// Ciclo: CRISTAL(1) → BLANCO(3) → ROSA(4) → FUEGO(2) → NEGRO(0) → CRISTAL
// Índices en el ciclo:  0=CRISTAL  1=BLANCO  2=ROSA  3=FUEGO  4=NEGRO
//
// Probabilidades base sin bioma (sobre 120 puntos):
//   Cada uno de los 5 del ciclo: 20 puntos (16.7%)
//   GRIS: 20 puntos (16.7%)
//
// Modificadores por bioma aplicados al afín y sus vecinos:
//   Afín:              +30
//   Adyacentes afín:   +10 cada uno
//   Opuesto:           -30
//   Adyacentes opuesto:-10 cada uno
//   GRIS:              siempre fijo en 20 (no se toca)
// ----------------------------------------------------

// Orden del ciclo — índice → TipoOpalo
static const TipoOpalo CICLO[5] = {
    OPALO_CRISTAL,
    OPALO_BLANCO,
    OPALO_ROSA,
    OPALO_FUEGO,
    OPALO_NEGRO
};

// Bioma → posición del afín en el ciclo
static const int8_t BIOMA_AFIN[5] = {
    0,  // Bioma 0 Glaciar → CRISTAL (pos 0)
    1,  // Bioma 1 Bosque  → BLANCO  (pos 1)
    2,  // Bioma 2 Costa   → ROSA    (pos 2)
    4,  // Bioma 3 Pantano → NEGRO   (pos 4)
    3,  // Bioma 4 Cañón   → FUEGO   (pos 3)
};

static TipoOpalo elegir_tipo_por_bioma(uint32_t h, uint8_t bioma) {
    // Pesos base: 20 para cada tipo del ciclo + 20 para GRIS = 120 total
    int pesos[6]; // índices 0-4 = ciclo, 5 = GRIS
    for (int i = 0; i < 5; i++) pesos[i] = 20;
    pesos[5] = 20; // GRIS fijo

    if (bioma < 5) {
        int afin    = BIOMA_AFIN[bioma];
        int opuesto = (afin + 2) % 5; // opuesto en ciclo de 5
        int adj_afin_a  = (afin + 1) % 5;
        int adj_afin_b  = (afin + 4) % 5;
        int adj_op_a    = (opuesto + 1) % 5;
        int adj_op_b    = (opuesto + 4) % 5;

        pesos[afin]       += 30;
        pesos[adj_afin_a] += 10;
        pesos[adj_afin_b] += 10;
        pesos[opuesto]    -= 30;
        pesos[adj_op_a]   -= 10;
        pesos[adj_op_b]   -= 10;

        // Clamp mínimo a 1 para que siempre haya alguna probabilidad
        for (int i = 0; i < 5; i++)
            if (pesos[i] < 1) pesos[i] = 1;
    }

    // Suma total y sorteo
    int total = 0;
    for (int i = 0; i < 6; i++) total += pesos[i];

    int r = (int)((h >> 4) % (uint32_t)total);
    for (int i = 0; i < 5; i++) {
        r -= pesos[i];
        if (r < 0) return CICLO[i];
    }
    return OPALO_GRIS;
}

// ----------------------------------------------------
// BONUS REGIONAL (estadísticas del ópalo según bioma)
// Se aplica DESPUÉS de generar el ópalo, ajusta pureza/brillo
// ----------------------------------------------------
void aplicar_bonus_region(Opalo* o, Chunk* c) {
    if (ciudad_actual_idx < 0 || ciudad_actual_idx >= NUM_TOTAL_CIUDADES) return;

    uint8_t bioma = ciudades[ciudad_actual_idx].bioma_id;
    if (bioma >= 5) return;

    int afin    = BIOMA_AFIN[bioma];
    TipoOpalo tipo_afin = CICLO[afin];

    // Si el ópalo es del tipo afín: bonus de calidad
    if (o->tipo == tipo_afin) {
        if (o->pureza < 95)  o->pureza  += 8;
        if (o->brillo < 28)  o->brillo  += 2;
    }

    // Si es GRIS: siempre un pequeño bonus de pureza (es el "común mejorado")
    if (o->tipo == OPALO_GRIS) {
        if (o->pureza < 90) o->pureza += 4;
    }

    if (o->pureza > 100) o->pureza = 100;
    if (o->brillo  > 31) o->brillo  = 31;

    (void)c; // c disponible para futuros usos
}

// ----------------------------------------------------
// QUILATES
// ----------------------------------------------------
static uint16_t calcular_quilates_exacto(uint32_t h, int bonus) {
    uint32_t r = h % 1000000;

    if (bonus == 0) {
        if (r < 200000) return 1  + (h % 5);
        if (r < 600000) return 5  + (h % 26);
        if (r < 900000) return 30 + (h % 16);
        return                45  + (h % 6);
    }
    if (bonus == 15) {
        if (r < 100000) return 1  + (h % 5);
        if (r < 600000) return 5  + (h % 26);
        if (r < 850000) return 30 + (h % 31);
        if (r < 970000) return 60 + (h % 21);
        return                80  + (h % 11);
    }
    if (bonus == 30) {
        if (r < 50000)  return 1   + (h % 5);
        if (r < 200000) return 5   + (h % 26);
        if (r < 550000) return 30  + (h % 41);
        if (r < 800000) return 70  + (h % 31);
        if (r < 950000) return 100 + (h % 21);
        if (r < 995000) return 120 + (h % 21);
        return                140  + (h % 11);
    }
    return 1 + (h % 10);
}


// ----------------------------------------------------
// CALCULAR ATRIBUTOS GEMA
// Ahora opera directamente sobre la entidad persistente Gema
// ----------------------------------------------------
void calcular_atributos_gema(Gema* g, uint32_t seed, uint8_t bioma) {
    uint32_t h = hash32(seed);

    // Almacenamos la semilla en el campo correspondiente de la Gema
    g->seed_visual = seed; 
    
    // Tipo determinado por bioma
    g->tipo_real = (uint8_t)elegir_tipo_por_bioma(h, bioma);

    // Patrón
    {
        uint8_t pr = (h >> 2) & 255;
        if      (pr < 128) g->patron_real = PATRON_NEBULA;
        else if (pr < 192) g->patron_real = PATRON_VENAS;
        else if (pr < 224) g->patron_real = PATRON_MATRIX;
        else if (pr < 240) g->patron_real = PATRON_MOSAICO;
        else if (pr < 252) g->patron_real = PATRON_CHAOS;
        else               g->patron_real = PATRON_HARLEQUIN;
    }

    // Atributos físicos (escritos directamente en la Gema)
    g->brillo_real   = 16 + (((h >> 4)  & 15) * ((h >> 4)  & 15) / 15);
    g->saturacion    = 16 + (((h >> 8)  & 15) * ((h >> 8)  & 15) / 15);
    g->iridiscencia  = 16 + (((h >> 13) & 15) * ((h >> 13) & 15) / 15);

    // Pureza
    {
        uint32_t p = (h >> 20) & 1023;
        if      (p < 500) g->pureza_real = 40 + (p % 20);
        else if (p < 850) g->pureza_real = 60 + (p % 25);
        else if (p < 980) g->pureza_real = 85 + (p % 10);
        else              g->pureza_real = 95 + (p % 6);
    }

    g->pista_color  = (h >> 17) & 255; // Asignado a pista_color o color_offset según tu gema.h
    // g->quilates no se inicializa aquí, ya que viene del Chunk en gema.c
}



// ----------------------------------------------------
// GENERAR ÓPALO
// ----------------------------------------------------
void generar_opalo(Opalo* o, uint32_t seed) {
    uint32_t h = hash32(seed);

    o->seed = seed;

    // Tipo determinado por bioma actual
    uint8_t bioma = 0;
    if (ciudad_actual_idx >= 0 && ciudad_actual_idx < NUM_TOTAL_CIUDADES)
        bioma = ciudades[ciudad_actual_idx].bioma_id;
    o->tipo = elegir_tipo_por_bioma(h, bioma);

    // Patrón
    {
        uint8_t pr = (h >> 2) & 255;
        if      (pr < 128) o->patron = PATRON_NEBULA;
        else if (pr < 192) o->patron = PATRON_VENAS;
        else if (pr < 224) o->patron = PATRON_MATRIX;
        else if (pr < 240) o->patron = PATRON_MOSAICO;
        else if (pr < 252) o->patron = PATRON_CHAOS;
        else               o->patron = PATRON_HARLEQUIN;
    }

    o->brillo       = 16 + (((h >> 4)  & 15) * ((h >> 4)  & 15) / 15);
    o->saturacion   = 16 + (((h >> 8)  & 15) * ((h >> 8)  & 15) / 15);
    o->iridiscencia = 16 + (((h >> 13) & 15) * ((h >> 13) & 15) / 15);

    {
        uint32_t p = (h >> 20) & 1023;
        if      (p < 500) o->pureza = 40 + (p % 20);
        else if (p < 850) o->pureza = 60 + (p % 25);
        else if (p < 980) o->pureza = 85 + (p % 10);
        else              o->pureza = 95 + (p % 6);
    }

    o->color_offset = (h >> 17) & 255;
    o->quilates     = 0;
}

// ----------------------------------------------------
// GENERAR CHUNK
// ----------------------------------------------------
void generar_chunk(Chunk* c, uint32_t seed) {
    uint32_t h = hash32(seed ^ 0xDEADBEEF);
    int bonus = mina_obtener_bonus();

    c->seed    = seed;
    c->cortado = 0;

    {
        uint32_t engano = (h >> 5) & 15;
        c->tamanyo = 1 + ((h >> 2) & 3);
        if (engano < 4) {
            if (engano & 1) { if (c->tamanyo > 1) c->tamanyo--; }
            else            { if (c->tamanyo < 4) c->tamanyo++; }
        }
    }

    c->quilates          = calcular_quilates_exacto(h, bonus);
    c->grietas           = ((h >> 8) & 255) > 120 ? 1 : 0;
    c->pista             = 1 + ((h >> 16) & 3);
    c->intensidad_grieta = 1 + ((h >> 22) & 3);
    c->forma_grieta      = (h >> 24) & 3;
    c->pureza_aprox      = 30 + ((h >> 27) & 63);
}

