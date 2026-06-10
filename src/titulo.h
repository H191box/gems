#ifndef TITULO_H
#define TITULO_H

#include <stdint.h>

/*
 * titulo.h — Pantalla de inicio "PIEDRAS"
 *
 *   titulo_init()    — llamar una vez al arrancar el juego
 *   titulo_update()  — llamar cada iteración del bucle; devuelve 1 al pulsar START
 *   titulo_free()    — limpieza antes de pasar al menú
 */

void    titulo_init(void);
uint8_t titulo_update(void);
void    titulo_free(void);

#endif /* TITULO_H */
