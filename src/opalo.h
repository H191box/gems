#ifndef OPALO_H
#define OPALO_H
#include <stdint.h>

extern int COLOR_PRUEBA;


typedef enum {
    OPALO_NEGRO,
    OPALO_CRISTAL,
    OPALO_FUEGO,
    OPALO_BLANCO,
    OPALO_LEGENDARIO
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

void generar_opalo(Opalo* o, uint32_t seed);
void generar_chunk(Chunk* c, uint32_t seed);
void aplicar_bonus_region(Opalo* o, Chunk* c);
uint32_t calcular_valor_opalo(const Opalo* o);

#endif
