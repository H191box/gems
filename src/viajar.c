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
#include "ciudades.h"

extern EstadoJuego estado;
extern int pos_x, pos_y;
extern int ciudad_actual_idx;

typedef enum { VISTA_MAPA, VISTA_CONFIRMAR } VistaViaje;
static VistaViaje vista;
static int cursor = 0; // 0:N, 1:S, 2:E, 3:W

// Obtiene el índice de la ciudad en una coordenada específica
static int get_ciudad_en_pos(int x, int y) {
    if (x < 0 || x > 4 || y < 0 || y > 4) return -1;
    for (int i = 0; i < 25; i++) {
        if (ciudades[i].x == x && ciudades[i].y == y) return i;
    }
    return -1;
}

static void render_viaje(void) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    char txt_oro[16];
    sprintf(txt_oro, "ORO: %ld", obtener_dinero());
    draw_text(vram, 4, 2, "VIAJAR", 255);
    draw_text(vram, 160, 2, txt_oro, 255);

    // Obtener ciudades adyacentes
    int adj[4] = {
        get_ciudad_en_pos(pos_x, pos_y - 1), // N
        get_ciudad_en_pos(pos_x, pos_y + 1), // S
        get_ciudad_en_pos(pos_x + 1, pos_y), // E
        get_ciudad_en_pos(pos_x - 1, pos_y)  // W
    };
    const char* dir_nombres[] = {"N", "S", "E", "W"};

    // IZQUIERDA: Listado de 4 direcciones fijas
    for (int i = 0; i < 4; i++) {
        uint8_t bg = (i == cursor) ? 2 : 1;
        rect(vram, 5, 20 + (i * 30), 100, 25, bg);
        
        char linea[32];
        sprintf(linea, "%s: %s", dir_nombres[i], (adj[i] != -1) ? ciudades[adj[i]].nombre : "---");
        draw_text(vram, 10, 27 + (i * 30), linea, 255);
    }

    // DERECHA: Descripción de la ciudad seleccionada (si existe)
    int idx_sel = adj[cursor];
    if (idx_sel != -1) {
        draw_text(vram, 120, 20, (char*)ciudades[idx_sel].nombre, 255);
        draw_text(vram, 120, 40, (char*)ciudades[idx_sel].descripcion, 255);
        char precio[20];
        sprintf(precio, "COSTE: %d", ciudades[idx_sel].tasa_venta);
        draw_text(vram, 120, 80, precio, 255);
    } else {
        draw_text(vram, 120, 20, "CAMINO BLOQUEADO", 255);
    }

    if (vista == VISTA_CONFIRMAR) {
        rect(vram, 120, 100, 110, 40, 5);
        draw_text(vram, 130, 110, "VIAJAR?", 255);
        draw_text(vram, 130, 125, "A: SI  B: NO", 255);
    }
    flip();
}

void viajar_init(void) {
    cursor = 0;
    vista = VISTA_MAPA;
    render_viaje();
}

void viajar_input(uint16_t keys) {
    if (vista == VISTA_MAPA) {
        if (keys & KEY_UP)    cursor = (cursor == 0) ? 3 : cursor - 1;
        if (keys & KEY_DOWN)  cursor = (cursor == 3) ? 0 : cursor + 1;
        
        // --- NUEVO: Salir al menú principal con B ---
        if (keys & KEY_B) {
            volver_menu_con_fade(); 
            return;
        }
        
        if (keys & KEY_A) {
            int adj[4] = {
                get_ciudad_en_pos(pos_x, pos_y - 1),
                get_ciudad_en_pos(pos_x, pos_y + 1),
                get_ciudad_en_pos(pos_x + 1, pos_y),
                get_ciudad_en_pos(pos_x - 1, pos_y)
            };
            // Solo permitir confirmar si hay ciudad
            if (adj[cursor] != -1) vista = VISTA_CONFIRMAR;
        }
        render_viaje();
    } else {
        // En VISTA_CONFIRMAR: A confirma viaje, B vuelve a la vista de mapa
        if (keys & KEY_A) {
            int adj[4] = {
                get_ciudad_en_pos(pos_x, pos_y - 1),
                get_ciudad_en_pos(pos_x, pos_y + 1),
                get_ciudad_en_pos(pos_x + 1, pos_y),
                get_ciudad_en_pos(pos_x - 1, pos_y)
            };
            int idx = adj[cursor];
            
            if (obtener_dinero() >= ciudades[idx].tasa_venta) {
                modificar_dinero(-(int32_t)ciudades[idx].tasa_venta);
                
                pos_x = ciudades[idx].x;
                pos_y = ciudades[idx].y;
                ciudad_actual_idx = idx;
                
                avanzar_tiempo();
                sync_save_world_state();
                volver_menu_con_fade(); 
                return;
            }
            vista = VISTA_MAPA;
        } else if (keys & KEY_B) {
            vista = VISTA_MAPA;
        }
        render_viaje();
    }
}
