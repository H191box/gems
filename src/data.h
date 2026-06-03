#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include "ciudades.h" // Incluimos para tener acceso al struct Ciudad completo


extern uint8_t ciudad_actual_idx;

// Ahora usamos 25 ciudades
extern const Ciudad ciudades[25];

// Variables globales para gestionar la fecha y posición (coordenadas)
extern uint8_t dia_actual;
extern uint8_t mes_actual;
extern int pos_x; // Nueva variable de posición X
extern int pos_y; // Nueva variable de posición Y

// Función para avanzar el calendario
void avanzar_tiempo(void);

#endif // DATA_H
