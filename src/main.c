#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include "menu.h"
#include "plasma.h"
#include "save.h"
#include "video.h"
#include "viajar.h"
#include "galeria.h"
#include "tienda.h"
#include "taller.h"
#include "font.h"
#include "opalo.h"
#include "data.h"
#include "sacos.h"
#include "game_state.h"
#include "mina.h"


// --- VARIABLES GLOBALES ---
int opcion_menu    = 0;
EstadoJuego estado = ESTADO_MENU;

// --- VARIABLES LOCALES PARA FARMEO ---
static uint32_t semilla_actual;
static Chunk    chunk_actual;

// -------------------------------------------------------
// HELPERS FARMEO
// -------------------------------------------------------
static void refrescar_chunk(void) {
    generar_chunk(&chunk_actual, semilla_actual);
    
    // --- INTEGRACIÓN: Etiquetar el chunk con datos de la ciudad ---
    chunk_actual.dia = dia_actual;
    chunk_actual.mes = mes_actual;
    chunk_actual.ciudad_id = ciudad_actual_idx;
    // -------------------------------------------------------------
    
    renderizar_roca(&chunk_actual);
    flip();
}

// -------------------------------------------------------
// MAIN
// -------------------------------------------------------
int main() {
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    // Inicialización del sistema
    semilla_actual = cargar_seed();
    
    // Inicialización de tiempo (ej: día 1 mes 1)
    dia_actual = 1;
    mes_actual = 1;
    ciudad_actual_idx = 0;

    generar_chunk(&chunk_actual, semilla_actual);
    dibujar_menu(opcion_menu);
    flip();
while (1) {

    scanKeys();

    uint16_t keys = keysDown();

    switch (estado) {

    // -------------------------------------------------
    // MENU
    // -------------------------------------------------

    case ESTADO_MENU:

        if (keys & KEY_UP) {
            opcion_menu = (opcion_menu <= 0) ? 4 : opcion_menu - 1;
            dibujar_menu(opcion_menu);
            flip();
        }

        if (keys & KEY_DOWN) {
            opcion_menu = (opcion_menu >= 4) ? 0 : opcion_menu + 1;
            dibujar_menu(opcion_menu);
            flip();
        }

        if (keys & KEY_A) {

            if (opcion_menu == 0) {

                estado = ESTADO_MINA;
                mina_init();
            }

            else if (opcion_menu == 1) {

                estado = ESTADO_TALLER;
                taller_init();
            }

            else if (opcion_menu == 2) {

                estado = ESTADO_GALERIA;
                galeria_init();
            }

            else if (opcion_menu == 3) {

                estado = ESTADO_TIENDA;
                tienda_init();
            }

            else {

                estado = ESTADO_VIAJAR;
                viajar_init();
            }
        }

        break;

    // -------------------------------------------------
    // MINA
    // -------------------------------------------------

    case ESTADO_MINA:

        if (keys & KEY_START) {

            estado = ESTADO_MENU;

            dibujar_menu(opcion_menu);

            flip();
        }
        else {

            mina_input(keys);
        }

        break;

// -------------------------------------------------
    // FARMEO
    // -------------------------------------------------

    case ESTADO_FARMEO:

        // 1. Bloqueo de seguridad inicial
        if (mina_obtener_tiradas() <= 0) {
            estado = ESTADO_MENU;
            dibujar_menu(opcion_menu);
            flip();
            break; 
        }

        // 2. Refrescar roca: AHORA GASTA TIRADA
        if (keys & KEY_B) {
            semilla_actual += 0x9E3779B9;
            mina_gastar_tirada(); // <--- GASTA TIRADA AQUÍ TAMBIÉN

            // Comprobamos si al refrescar nos quedamos sin tiradas
            if (mina_obtener_tiradas() <= 0) {
                // Mensaje de agotado al refrescar
                uint16_t* vram = get_vram();
                clear(vram, 0);
                draw_text(vram, 82, 70, "AGOTADO", 255);
                flip();
                for (volatile int i = 0; i < 300000; i++);
                
                estado = ESTADO_MENU;
                dibujar_menu(opcion_menu);
                flip();
            } else {
                refrescar_chunk();
            }
        }

        // 3. Picar piedra (Minar): Gasta tirada
        if (keys & KEY_A) {
            if (chunk_actual.grietas > 0) {
                guardar_seed(semilla_actual);
                guardar_chunk_taller(&chunk_actual);
                flash_guardado();
                
                mina_gastar_tirada(); // Gasta tirada al picar

                if (mina_obtener_tiradas() <= 0) {
                    uint16_t* vram = get_vram();
                    clear(vram, 0);
                    draw_text(vram, 82, 70, "AGOTADO", 255);
                    flip();
                    for (volatile int i = 0; i < 300000; i++);

                    estado = ESTADO_MENU;
                    dibujar_menu(opcion_menu);
                    flip();
                } else {
                    semilla_actual += 0x9E3779B9;
                    refrescar_chunk();
                }
            }
        }

        if (keys & KEY_START) {
            estado = ESTADO_MENU;
            dibujar_menu(opcion_menu);
            flip();
        }

        break;
    // -------------------------------------------------
    // TALLER
    // -------------------------------------------------

    case ESTADO_TALLER:

        if (keys & KEY_START) {

            estado = ESTADO_MENU;

            dibujar_menu(opcion_menu);

            flip();
        }
        else {

            taller_input(keys);
        }

        break;

    // -------------------------------------------------
    // TIENDA
    // -------------------------------------------------

    case ESTADO_TIENDA:

        if (keys & KEY_START) {

            estado = ESTADO_MENU;

            dibujar_menu(opcion_menu);

            flip();
        }
        else {

            tienda_input(keys);
        }

        break;

    // -------------------------------------------------
    // GALERIA
    // -------------------------------------------------

    case ESTADO_GALERIA:

        if (keys & KEY_START) {

            estado = ESTADO_MENU;

            dibujar_menu(opcion_menu);

            flip();
        }
        else {

            galeria_input(keys);
        }

        break;

    // -------------------------------------------------
    // VIAJAR
    // -------------------------------------------------

    case ESTADO_VIAJAR:

        if (keys & KEY_START) {

            estado = ESTADO_MENU;

            dibujar_menu(opcion_menu);

            flip();
        }
        else {

            viajar_input(keys);
        }

        break;
    }
}

return 0;
}
