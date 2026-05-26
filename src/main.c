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
        case ESTADO_MENU:
            if (keys & KEY_UP)   { opcion_menu = (opcion_menu <= 0) ? 4 : opcion_menu - 1; dibujar_menu(opcion_menu); flip(); }
            if (keys & KEY_DOWN) { opcion_menu = (opcion_menu >= 4) ? 0 : opcion_menu + 1; dibujar_menu(opcion_menu); flip(); }
            if (keys & KEY_A) {
                if (opcion_menu == 0)      { estado = ESTADO_FARMEO; refrescar_chunk(); }
                else if (opcion_menu == 1) { estado = ESTADO_TALLER; taller_init(); }
                else if (opcion_menu == 2) { estado = ESTADO_GALERIA; galeria_init(); }
                else if (opcion_menu == 3) { estado = ESTADO_TIENDA; tienda_init(); }
                else                       { estado = ESTADO_VIAJAR; viajar_init(); }
            }
            break;

        case ESTADO_FARMEO:
            if (keys & KEY_B) { 
                semilla_actual += 0x9E3779B9; 
                refrescar_chunk(); 
            }
            if (keys & KEY_A) {
                if (chunk_actual.grietas > 0) {
                    guardar_seed(semilla_actual);
                    guardar_chunk_taller(&chunk_actual);
                    flash_guardado();
                    semilla_actual += 0x9E3779B9;
                    refrescar_chunk();
                }
            }
            if (keys & KEY_START) { estado = ESTADO_MENU; dibujar_menu(opcion_menu); flip(); }
            break;

        case ESTADO_TALLER:
            if (keys & KEY_START) { estado = ESTADO_MENU; dibujar_menu(opcion_menu); flip(); }
            else { taller_input(keys); }
            break;

        case ESTADO_TIENDA:
            if (keys & KEY_START) { estado = ESTADO_MENU; dibujar_menu(opcion_menu); flip(); }
            else { tienda_input(keys); }
            break;

        case ESTADO_GALERIA:
            if (keys & KEY_START) { estado = ESTADO_MENU; dibujar_menu(opcion_menu); flip(); }
            else { galeria_input(keys); }
            break;
            
        case ESTADO_VIAJAR:
            if (keys & KEY_START) { estado = ESTADO_MENU; dibujar_menu(opcion_menu); flip(); }
            else { viajar_input(keys); }
            break;
        }
    }
    return 0;
}
