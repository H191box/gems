#ifndef DATA_H
#define DATA_H
#include <stdint.h>

typedef struct {
    char nombre[16];
    uint16_t color_paleta;
    char descripcion[64];
    uint8_t dificultad;
    uint16_t tasa_venta;
} Ciudad;

#endif
