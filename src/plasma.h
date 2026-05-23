#ifndef PLASMA_H
#define PLASMA_H

#include <stdint.h>
#include "opalo.h"

void    generar_paleta(Opalo* o);
void    renderizar_opalo(Opalo* o);
void    renderizar_roca(const Chunk* c);
uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o);
void    flash_guardado(void);

#endif
