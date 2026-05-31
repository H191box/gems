#ifndef VIAJAR_H
#define VIAJAR_H

#include <stdint.h>

// Inicializa el estado de navegación por el mapa de 5x5
void viajar_init(void);

// Procesa el movimiento direccional (N, S, E, O) y la confirmación de viaje
void viajar_input(uint16_t keys);

#endif
