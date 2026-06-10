#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>
#include "video.h"
#include "menu.h"
#include "titulo.h"      /* <-- NUEVO */
#include "galeria.h"
#include "menu_cajas.h"
#include "tienda.h"
#include "viajar.h"
#include "game_state.h"
#include "save.h"
#include "data.h"
#include "ciudades.h"

// ============================================================
// ESTADO GLOBAL
// ============================================================
EstadoJuego estado = ESTADO_TITULO;   /* arranca en portada */

// ============================================================
// GENERADOR DE SEMILLA
// ============================================================
static uint32_t generar_semilla_inicial(void) {
    REG_TM0CNT_H = 0;
    REG_TM0CNT_L = 0;
    REG_TM0CNT_H = (1 << 7);

    uint16_t t1 = REG_TM0CNT_L;
    uint16_t vc = REG_VCOUNT;

    REG_TM0CNT_H = 0;

    uint32_t prev = cargar_seed();

    uint32_t s = prev;
    s ^= (uint32_t)t1 << 16;
    s ^= (uint32_t)vc << 8;
    s ^= (uint32_t)t1;
    s ^= s >> 16;
    s *= 0x45d9f3b;
    s ^= s >> 16;

    if (s == 0) s = 0xDEAD1234;
    return s;
}

// ============================================================
// PUNTO DE ENTRADA
// ============================================================
int main(void) {
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    /* 1. SRAM / save */
    save_init();

    /* 2. Semilla */
    uint32_t semilla = generar_semilla_inicial();
    guardar_seed(semilla);
    sync_save_world_state();

    /* 3. Portada — bloquea aquí hasta que el jugador pulse START */
    titulo_init();
    while (titulo_update() == 0) {
        /* vsync: el bucle de la portada no necesita hacer nada más;
           titulo_update() ya llama a flip() en los frames de parpadeo.
           En frames intermedios solo esperamos el barrido. */
        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
    }
    titulo_free();

    /* 4. Transición a menú principal */
    fade_out();
    aplicar_paleta_segun_bioma(ciudad_actual_idx);
    dibujar_menu(0);
    flip();
    fade_in();
    estado = ESTADO_MENU;

    /* 5. Bucle principal */
    while (1) {
        scanKeys();
        uint16_t keys = keysDown();

          switch (estado) {
            case ESTADO_MENU:    menu_input(keys);       break;
            case ESTADO_TIENDA:  tienda_input(keys);     break;
            case ESTADO_GALERIA: galeria_input(keys);    break;
            case ESTADO_VIAJAR:  viajar_input(keys);     break;
            case ESTADO_CAJAS:   menu_cajas_input(keys); break;
            default: break;
        }

        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
    }

    return 0;
}
