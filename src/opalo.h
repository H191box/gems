#ifndef OPALO_H
#define OPALO_H

#include <stdint.h>

/* Declaración adelantada para evitar errores de referencia circular */
typedef struct Gema Gema;

extern int COLOR_PRUEBA;

typedef enum {
    OPALO_NEGRO    = 0,
    OPALO_CRISTAL  = 1,
    OPALO_FUEGO    = 2,
    OPALO_BLANCO   = 3,
    OPALO_ROSA     = 4,
    OPALO_GRIS     = 5,
    NUM_TIPOS_OPALO
} TipoOpalo;

typedef enum {
    PATRON_NEBULA,
    PATRON_VENAS,
    PATRON_MOSAICO,
    PATRON_CHAOS,
    PATRON_MATRIX,
    PATRON_HARLEQUIN
} PatronOpalo;

typedef struct {
    uint32_t seed;
    TipoOpalo tipo;
    PatronOpalo patron;
    uint16_t quilates;
    uint8_t brillo;
    uint8_t saturacion;
    uint8_t iridiscencia;
    uint8_t pureza;
    uint8_t color_offset;
} Opalo;

typedef struct {
    uint32_t seed;
    uint16_t quilates;
    uint8_t tamanyo;
    uint8_t grietas;
    uint8_t pista;
    uint8_t intensidad_grieta;
    uint8_t forma_grieta;
    uint8_t pureza_aprox;
    uint8_t cortado;
    uint8_t dia;
    uint8_t mes;
    uint8_t ciudad_id;
} Chunk;

/* --- Prototipos de Funciones --- */

// Generación de ópalos y atributos
void generar_opalo(Opalo* o, uint32_t seed);
void calcular_atributos_gema(Gema* g, uint32_t seed, uint8_t bioma);

// Gestión de chunks
void generar_chunk(Chunk* c, uint32_t seed);
void aplicar_bonus_region(Opalo* o, Chunk* c);
uint32_t calcular_valor_opalo(const Opalo* o);

#endif // OPALO_H
