#ifndef CIUDADES_H
#define CIUDADES_H

#include <stdint.h>

// Cantidad total de ciudades en el mapa
#define NUM_TOTAL_CIUDADES 25

typedef struct {
    char nombre[16];
    uint16_t color_paleta;
    char descripcion[64];
    uint8_t dificultad;
    uint16_t tasa_venta;
    int x;
    int y;
    uint8_t bioma_id; 
} Ciudad;

// Declaramos que el array existe en algún lugar del programa
extern const Ciudad ciudades[NUM_TOTAL_CIUDADES];

#endif // CIUDADES_H
