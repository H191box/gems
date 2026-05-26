#ifndef OPALO_H
#define OPALO_H

#include <stdint.h>

// Tipos de opalo (semánticos)
typedef enum {
    OPALO_NEGRO,
    OPALO_CRISTAL,
    OPALO_FUEGO,
    OPALO_BLANCO,
    OPALO_LEGENDARIO // Añadido para el "Opal Queen" y otros
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
    uint16_t quilates;     // Peso real de la gema revelada
    uint8_t brillo;        // 0-31
    uint8_t saturacion;    // 0-31
    uint8_t iridiscencia;  // 0-31 - NUEVO
    uint8_t color_offset;
} Opalo;

// -------------------------------------------------------
// Entidad chunk (roca en bruto, sin cortar)
// -------------------------------------------------------
typedef struct {
    uint32_t seed;
    uint16_t quilates; 
    uint8_t  tamanyo;
    uint8_t  grietas;
    uint8_t  pista;
    uint8_t  cortado;
    // Campos para la Ficha Técnica
    uint8_t  dia;
    uint8_t  mes;
    uint8_t  ciudad_id;
} Chunk;

// Generadores deterministas
void generar_opalo(Opalo* o, uint32_t seed);
void generar_chunk(Chunk* c, uint32_t seed);

// Función para calcular valor basada en quilates, tipo y estética
// Ahora recibe el ópalo revelado para obtener brillo, sat e iridiscencia
uint32_t calcular_valor_opalo(const Opalo* o);

#endif // OPALO_H
