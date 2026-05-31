#ifndef PLASMA_H
#define PLASMA_H

#include <stdint.h>
#include "gema.h" // Necesario para que plasma conozca la estructura Gema

/* * El plasma ahora utiliza los patrones y propiedades definidas en la estructura Gema
 * para renderizar píxeles específicos basados en el tipo de ópalo.
 */

// Ahora estas funciones reciben el puntero a Gema para acceder a los patrones
uint8_t plasma_pixel(int x, int y, uint8_t off, const Gema* g);
uint8_t plasma_pixel_smooth(int x, int y, uint8_t off, const Gema* g);

// Declaraciones para que render.c pueda verlas
int16_t lut_s(int i);
int16_t lut_c(int i);

#endif
