#ifndef GALERIA_H
#define GALERIA_H



#include <stdint.h>


typedef enum {
    VISTA_LISTA,
    VISTA_SUBMENU,
    VISTA_FICHA,
    VISTA_IMAGEN,
    VISTA_G2,
    VISTA_G2_SUBMENU,
    VISTA_G2_OBSERVAR,
    VISTA_G3,
    VISTA_G3_SUBMENU,
    VISTA_G3_OBSERVAR
} VistaGaleria;


void galeria_init(void);
void galeria_input(uint16_t keys);  // llama cada frame con keysDown()

#endif
