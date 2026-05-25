#ifndef OPALO_H
#define OPALO_H

#include <stdint.h>

// Tipos de opalo (semánticos)
typedef enum {
    OPALO_NEGRO,
    OPALO_CRISTAL,
    OPALO_FUEGO,
    OPALO_BLANCO
} TipoOpalo;

// Patrones visuales
typedef enum {
    PATRON_NEBULA,
    PATRON_VENAS,
    PATRON_MOSAICO,
    PATRON_CHAOS
} PatronOpalo;

// -------------------------------------------------------
// Entidad ópalo (gema ya revelada)
// -------------------------------------------------------
typedef struct {
    uint32_t seed;
    TipoOpalo tipo;
    PatronOpalo patron;
    uint8_t brillo;
    uint8_t saturacion;
    uint8_t color_offset;
} Opalo;

// -------------------------------------------------------
// Entidad chunk (roca en bruto, sin cortar)
//
// peso    : 1-10 — afecta valor económico final
// tamanyo : 1-5  — S/M/L/XL/XXL
// grietas : 0-3  — cuántas grietas visibles en la roca
// pista   : 0-4  — índice de color que se asoma por las
//                  grietas (0 = ninguna pista visible)
// cortado : 0=en bruto, 1=procesado en el taller
// -------------------------------------------------------
typedef struct {
    uint32_t seed;
    uint8_t  peso;
    uint8_t  tamanyo;
    uint8_t  grietas;
    uint8_t  pista;
    uint8_t  cortado;
} Chunk;

// Generadores deterministas
void generar_opalo(Opalo* o, uint32_t seed);
void generar_chunk(Chunk* c, uint32_t seed);

#endif

uint16_t calcular_valor_chunk(const Chunk* c);
