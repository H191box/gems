/* galeria.h */
#ifndef GALERIA_H
#define GALERIA_H

#include <stdint.h>
#include "caja_filtro.h"

/* Vistas internas — necesarias para menu_cajas si quiere forzar refresh */
typedef enum {
    VISTA_LISTA,
    VISTA_SUBMENU,
    VISTA_FICHA,
    VISTA_G2,           /* reservado — heredado, en desuso */
    VISTA_G2_SUBMENU,
    VISTA_G2_OBSERVAR,
    VISTA_G3,
    VISTA_G3_SUBMENU,
    VISTA_G3_OBSERVAR
} VistaGaleria;

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

void galeria_init (void);
void galeria_input(uint16_t keys);

/* Llamado desde menu_cajas cuando se guarda un filtro,
   para que galeria recargue la vista de la caja activa */
void galeria_recargar_caja_activa(void);

#endif /* GALERIA_H */
