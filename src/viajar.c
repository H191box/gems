#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "data.h"       
#include "viajar.h"
#include "video.h"     
#include "font.h"
#include "save.h"      
#include "game_state.h"
#include "menu.h"

#define LIST_W 115

// Declaraciones externas
extern int opcion_menu;
extern EstadoJuego estado;

typedef enum { VISTA_LISTA, VISTA_CONFIRMAR } VistaViaje;
static VistaViaje vista;
static int cursor = 0;

void render_viaje(int cursor) {
    uint16_t* vram = get_vram();
    for (int i = 0; i < 19200; i++) vram[i] = 0;

    char txt_oro[16];
    sprintf(txt_oro, "ORO: %ld", obtener_dinero());
    draw_text(vram, 4, 2, "VIAJAR", 255);
    draw_text(vram, 160, 2, txt_oro, 255);

    rect(vram, LIST_W, 12, 2, 148, 3);

    for (int i = 0; i < 3; i++) {
        if (i == ciudad_actual_idx) continue; // saltar ciudad actual
        uint8_t bg = (i == cursor) ? 2 : 1;
        rect(vram, 5, 20 + (i * 40), LIST_W - 10, 30, bg);
        draw_text(vram, 10, 27 + (i * 40), (char*)ciudades[i].nombre, 255);
    }

    int x_desc = LIST_W + 10;
    draw_text(vram, x_desc, 20, (char*)ciudades[cursor].nombre, 255);
    draw_text(vram, x_desc, 40, (char*)ciudades[cursor].descripcion, 255);
    
    char precio[20];
    sprintf(precio, "COSTE: %d", ciudades[cursor].tasa_venta);
    draw_text(vram, x_desc, 80, precio, 255);

    if (vista == VISTA_CONFIRMAR) {
        rect(vram, 120, 100, 110, 40, 5);
        draw_text(vram, 130, 110, "VIAJAR?", 255);
        draw_text(vram, 130, 125, "A: SI  B: NO", 255);
    }
    flip();
}

void viajar_init(void) {
    cursor = (ciudad_actual_idx == 0) ? 1 : 0;
    vista = VISTA_LISTA;
    render_viaje(cursor);
}

int viajar_input(uint16_t keys) {
    if (vista == VISTA_LISTA) {
        if (keys & KEY_UP) {
            do { cursor--; if (cursor < 0) cursor = 2; } while (cursor == ciudad_actual_idx);
        }
        if (keys & KEY_DOWN) {
            do { cursor++; if (cursor > 2) cursor = 0; } while (cursor == ciudad_actual_idx);
        }
        if (keys & KEY_A) vista = VISTA_CONFIRMAR;
        render_viaje(cursor);
    } else {
        if (keys & KEY_A) {
            if (obtener_dinero() >= ciudades[cursor].tasa_venta) {
                // --- INTEGRACIÓN: Tiempo y Viaje ---
                modificar_dinero(-(int32_t)ciudades[cursor].tasa_venta);
                ciudad_actual_idx = cursor; // Actualizamos la ciudad global
                avanzar_tiempo();           // El tiempo avanza al viajar
                
                sync_save_world_state();
                
                fundido_a_negro();
                REG_BLDCNT = 0;
                REG_BLDY = 0;
                
                estado = ESTADO_MENU;
                dibujar_menu(opcion_menu);
                flip();
                return 1;
            }
            vista = VISTA_LISTA;
        } else if (keys & KEY_B) {
            vista = VISTA_LISTA;
        }
        render_viaje(cursor);
    }
    return 0;
}
