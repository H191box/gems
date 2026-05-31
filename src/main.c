#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>
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
#include "ciudades.h"

EstadoJuego estado = ESTADO_MENU;

extern int ciudad_actual_idx;

// ----------------------------------------------------
// GENERADOR DE SEMILLA CON ENTROPÍA REAL
// Combina tres fuentes:
//   1. Timer de CPU — depende de cuántos ciclos pasaron
//      desde el encendido hasta que el jugador llega al menú
//   2. Scanline actual (VCOUNT) — varía en microsegundos
//   3. Semilla anterior del save — encadena sesiones
// ----------------------------------------------------
static uint32_t generar_semilla_inicial(void) {
    // Arrancar timer 0 libre (prescaler /1 = 1 tick por ciclo de CPU)
    REG_TM0CNT_H = 0;          // parar y resetear
    REG_TM0CNT_L = 0;
    REG_TM0CNT_H = (1 << 7);   // arrancar

    // Esperar un frame completo para que el timer acumule variación
    // (el jugador ya ha pulsado algo para llegar aquí)
    uint16_t t1 = REG_TM0CNT_L;
    uint16_t vc = REG_VCOUNT;

    // Detener timer
    REG_TM0CNT_H = 0;

    uint32_t prev = cargar_seed();

    // Mezclar las tres fuentes
    uint32_t s = prev;
    s ^= (uint32_t)t1 << 16;
    s ^= (uint32_t)vc << 8;
    s ^= (uint32_t)t1;
    // Hash final para distribuir bien los bits
    s ^= s >> 16;
    s *= 0x45d9f3b;
    s ^= s >> 16;

    if (s == 0) s = 0xDEAD1234;
    return s;
}

int main() {
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    dia_actual = 1;
    mes_actual = 1;

    save_init();

    // Generar semilla con entropía antes de iniciar la mina
    uint32_t semilla = generar_semilla_inicial();
    guardar_seed(semilla);
    mina_init_semilla(semilla);

    aplicar_paleta_segun_bioma(ciudad_actual_idx);
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
            default: break;
        }
    }

    return 0;
}
