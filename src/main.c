#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include "video.h"
#include "menu.h"
#include "mina.h"
#include "taller.h"
#include "galeria.h"
#include "tienda.h"
#include "viajar.h"
#include "game_state.h"
#include "save.h"
#include "data.h"

EstadoJuego estado = ESTADO_MENU;

int main() {
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    // Inicialización del sistema
    dia_actual        = 1;
    mes_actual        = 1;
    ciudad_actual_idx = 0;
    mina_init_semilla(cargar_seed());

    // Primera pantalla: menú con fade in
    aplicar_paleta_segun_ciudad(ciudad_actual_idx);
    dibujar_menu(0);
    fade_in();

    while (1) {
        scanKeys();
        uint16_t keys = keysDown();

        switch (estado) {
            case ESTADO_MENU:    menu_input(keys);    break;
            case ESTADO_MINA:    mina_input(keys);    break;
            case ESTADO_FARMEO:  farmeo_input(keys);  break;
            case ESTADO_TALLER:  taller_input(keys);  break;
            case ESTADO_TIENDA:  tienda_input(keys);  break;
            case ESTADO_GALERIA: galeria_input(keys); break;
            case ESTADO_VIAJAR:  viajar_input(keys);  break;
        }
    }

    return 0;
}
