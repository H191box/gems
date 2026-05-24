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
// Entidad principal
// -------------------------------------------------------
typedef struct {
    uint32_t seed;

    TipoOpalo tipo;
    PatronOpalo patron;

    uint8_t brillo;
    uint8_t saturacion;
    uint8_t color_offset;

} Opalo;

// Generador determinista
void generar_opalo(Opalo* o, uint32_t seed);

#endif
