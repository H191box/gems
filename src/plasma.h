#ifndef PLASMA_H
#define PLASMA_H

#include <stdint.h>
#include "opalo.h"

// Genera la paleta del ópalo en índices 16..254.
// Los índices 0..15 y 255 quedan intactos (territorio UI).
void generar_paleta(Opalo* o);

// Renderiza el ópalo a pantalla completa (240x160) en el back buffer.
void renderizar_opalo(Opalo* o);

// Renderiza el ópalo como miniatura 80x80 en cabujón elíptico con volumen 3D.
// tamanyo: 1=S..5=XXL, controla el tamaño de la elipse (30px..76px de radio mayor).
// x_pos debe ser par. Llama a init_paleta_ui() después para restaurar la UI.
void renderizar_opalo_pequeno(int x_pos, int y_pos, Opalo* o, uint8_t tamanyo);

// Renderiza la roca a pantalla completa (240x160) en el back buffer.
void renderizar_roca(const Chunk* c);

// Renderiza la roca como miniatura 80x80 en (x_pos, y_pos).
// x_pos debe ser par. Llama a init_paleta_ui() después para restaurar la UI.
void renderizar_roca_pequena(int x_pos, int y_pos, const Chunk* c);

// Devuelve el índice de paleta correspondiente a un píxel del plasma.
uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o);

// Flash blanco de confirmación (guardado).
void flash_guardado(void);

#endif // PLASMA_H
