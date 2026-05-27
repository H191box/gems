#ifndef DATA_H
#define DATA_H

#include <stdint.h>

typedef struct {
    char nombre[16];
    uint16_t color_paleta;
    char descripcion[64];
    uint8_t dificultad;    // Probabilidad de mejores ópalos
    uint16_t tasa_venta;   // Multiplicador de precio
} Ciudad;



// Definición de las 3 ciudades (accesibles desde otros archivos)
extern const Ciudad ciudades[3];

// Variables globales para gestionar la fecha y ubicación actual
extern uint8_t dia_actual;
extern uint8_t mes_actual;
extern uint8_t ciudad_actual_idx;

// Función para avanzar el calendario (ej: al viajar o comprar)
void avanzar_tiempo(void);

#endif // DATA_H
