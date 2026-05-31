#ifndef MENU_H
#define MENU_H

#include <stdint.h>

// Añade esto a menu.h
void aplicar_paleta_segun_bioma(int idx);
void dibujar_menu(int opcion);
void menu_input(uint16_t keys);
void volver_menu(void);
void volver_menu_con_fade(void);
int  menu_obtener_opcion(void);

#endif
