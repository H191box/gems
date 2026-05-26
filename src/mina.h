// mina.h

#ifndef MINA_H
#define MINA_H

#include <stdint.h>

void mina_init(void);
void mina_input(uint16_t keys);

int  mina_obtener_tiradas(void);
int  mina_obtener_bonus(void);
void mina_gastar_tirada(void);

#endif
