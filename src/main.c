#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>
#include "video.h"
#include "menu.h"
// ELIMINADOS: mina.h y taller.h ya no se incluyen
#include "galeria.h"
#include "tienda.h"
#include "viajar.h"
#include "game_state.h"
#include "save.h"
#include "data.h"
#include "ciudades.h"

// ============================================================
// DEFINICIÓN DE VARIABLES GLOBALES (Estado del Juego y Mundo)
// ============================================================
EstadoJuego estado = ESTADO_MENU;


// ----------------------------------------------------
// GENERADOR DE SEMILLA CON ENTROPÍA REAL
// ----------------------------------------------------
static uint32_t generar_semilla_inicial(void) {
    REG_TM0CNT_H = 0;          // parar y resetear
    REG_TM0CNT_L = 0;
    REG_TM0CNT_H = (1 << 7);   // arrancar

    uint16_t t1 = REG_TM0CNT_L;
    uint16_t vc = REG_VCOUNT;

    REG_TM0CNT_H = 0;

    uint32_t prev = cargar_seed();

    // Mezclar las fuentes de hardware para aleatoriedad real
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

// ----------------------------------------------------
// PUNTO DE ENTRADA PRINCIPAL
// ----------------------------------------------------
int main() {
    // Configurar el modo de vídeo (Modo 4 indexado con Doble Buffer)
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    // 1. Inicializar la SRAM y comprobar si hay partida guardada válida.
    // ¡Sincronización crítica! Al ejecutarse, rellenará automáticamente 
    // dia_actual, mes_actual, pos_x, pos_y y ciudad_actual_idx con los datos guardados.
    save_init();

    // 2. Generar semilla con entropía para el sistema global de gacha
    uint32_t semilla = generar_semilla_inicial();
    guardar_seed(semilla);
    // ELIMINADO: mina_init_semilla(semilla); ya no es necesario

    sync_save_world_state();

    // 3. Cargar entorno visual basándose en la ciudad recuperada de la SRAM
    aplicar_paleta_segun_bioma(ciudad_actual_idx);
    dibujar_menu(0);
    flip();      /* presentar el menú antes del fade */
    fade_in();
    
    

    // 4. Bucle principal del juego (Game Loop)
    while (1) {
        scanKeys();
        uint16_t keys = keysDown();

        // Enrutar los inputs según el estado actual reducido del juego
        switch (estado) {
            case ESTADO_MENU:    menu_input(keys);    break;
            // ELIMINADOS: ESTADO_MINA, ESTADO_FARMEO y ESTADO_TALLER ya no interceptan inputs
            case ESTADO_TIENDA:  tienda_input(keys);  break;
            case ESTADO_GALERIA: galeria_input(keys); break;
            case ESTADO_VIAJAR:  viajar_input(keys);  break;
            default: break;
        }

        // Sincronización de refresco Vertical (VBlank) 
        // Evita el screen tearing (pantalla partida) y ralentiza el bucle a 60 FPS estables
         while (REG_VCOUNT >= 160);
         while (REG_VCOUNT < 160);
    }

    return 0;
}
