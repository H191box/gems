#include <stdint.h>
#include <stdio.h>
#include <gba_input.h>
#include "menu.h"
#include "video.h"
#include "font.h"
#include "save.h"
#include "data.h"
#include "game_state.h"
#include "mina.h"
#include "taller.h"
#include "galeria.h"
#include "tienda.h"
#include "viajar.h"

extern EstadoJuego estado;

static int opcion_menu = 0;

int menu_obtener_opcion(void) {
    return opcion_menu;
}

void aplicar_paleta_segun_ciudad(int idx) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    uint16_t color = ciudades[idx].color_paleta;
    pal[0]   = 0x0000;
    pal[1]   = color;
    pal[2]   = (color >> 1) & 0x3DEF;
    pal[3]   = color + 0x0318;
    pal[4]   = 0x4210;
    pal[255] = 0x7FFF;
}

void dibujar_menu(int opcion) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 1);
    draw_text(vram, 4, 2, "MENU", 255);

    vline(vram, 115, 12, 160, 4);

    const char* opciones_txt[] = {"FARMEO", "TALLER", "GALERIA", "TIENDA", "VIAJAR"};
    for (int i = 0; i < 5; i++) {
        int y = 20 + (i * 20);
        uint8_t col = (opcion == i) ? 3 : 2;
        fill_rect(vram, 0, y, 114, 18, col);
        draw_text(vram, 10, y + 5, (char*)opciones_txt[i], 255);
    }

    int x = 125;
    char txt_oro[16];
    sprintf(txt_oro, "ORO: %d", obtener_dinero());
    draw_text(vram, x, 20, txt_oro, 255);
    draw_text(vram, x, 35, (char*)ciudades[ciudad_actual_idx].nombre, 255);

    const char* desc_txt[] = {
        "BUSCA ROCAS",
        "CORTA CHUNKS",
        "TUS OPALOS",
        "COMPRAR SACOS",
        "CAMBIAR ZONA"
    };
    draw_text(vram, x, 60, "ACCION:", 255);
    draw_text(vram, x, 75, (char*)desc_txt[opcion], 255);
    draw_text(vram, x, 140, "A: CONFIRMAR", 255);
}

void menu_input(uint16_t keys) {
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
        switch (opcion_menu) {
            case 0: estado = ESTADO_MINA;    mina_init();    break;
            case 1: estado = ESTADO_TALLER;  taller_init();  break;
            case 2: estado = ESTADO_GALERIA; galeria_init(); break;
            case 3: estado = ESTADO_TIENDA;  tienda_init();  break;
            case 4: estado = ESTADO_VIAJAR;  viajar_init();  break;
        }
    }
}

// Llamar desde cualquier módulo para volver al menú
void volver_menu(void) {
    aplicar_paleta_segun_ciudad(ciudad_actual_idx);
    dibujar_menu(opcion_menu);
    flip();
    estado = ESTADO_MENU;
}

// Versión con fade para transiciones importantes (ej: viajar)
void volver_menu_con_fade(void) {
    fade_out();
    aplicar_paleta_segun_ciudad(ciudad_actual_idx);
    dibujar_menu(opcion_menu);
    fade_in();
    estado = ESTADO_MENU;
}
