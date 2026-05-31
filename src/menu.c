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
#include "ciudades.h"

extern EstadoJuego estado;
extern int ciudad_actual_idx;

static int opcion_menu = 0;

int menu_obtener_opcion(void) { return opcion_menu; }

void aplicar_paleta_segun_bioma(int idx) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    uint16_t color = ciudades[idx].color_paleta;
    uint8_t  bioma = ciudades[idx].bioma_id;

    pal[0]   = 0x0000;
    pal[4]   = 0x4210;
    pal[255] = 0x7FFF;

    switch(bioma) {
        case 0: pal[1] = color | 0x7C00; break; // Glaciar: tinte azul
        case 1: pal[1] = color | 0x03E0; break; // Bosque: tinte verde
        case 4: pal[1] = color | 0x001F; break; // Cañón: tinte rojo
        default: pal[1] = color; break;
    }
    pal[2] = (pal[1] >> 1) & 0x3DEF;
    pal[3] = pal[1] + 0x0318;
}

void dibujar_menu(int opcion) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 1);
    draw_text(vram, 4, 2, "MENU", 255);

    vline(vram, 115, 12, 160, 4);

    // 6 opciones ahora
    const char* opciones_txt[] = {"FARMEO", "TALLER", "GALERIA", "VITRINA", "TIENDA", "VIAJAR"};
    for (int i = 0; i < 6; i++) {
        int y = 16 + (i * 16);  // ligeramente más compacto para caber 6
        uint8_t col = (opcion == i) ? 3 : 2;
        fill_rect(vram, 0, y, 114, 14, col);
        draw_text(vram, 10, y + 3, (char*)opciones_txt[i], 255);
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
        "OPALOS ESPECIALES",
        "COMPRAR SACOS",
        "CAMBIAR ZONA"
    };
    draw_text(vram, x, 60, "ACCION:", 255);
    draw_text(vram, x, 75, (char*)desc_txt[opcion], 255);
    draw_text(vram, x, 140, "A: CONFIRMAR", 255);
}

void menu_input(uint16_t keys) {
    if (keys & KEY_UP) {
        opcion_menu = (opcion_menu <= 0) ? 5 : opcion_menu - 1;
        dibujar_menu(opcion_menu);
        flip();
    }
    if (keys & KEY_DOWN) {
        opcion_menu = (opcion_menu >= 5) ? 0 : opcion_menu + 1;
        dibujar_menu(opcion_menu);
        flip();
    }
    if (keys & KEY_A) {
        switch (opcion_menu) {
            case 0: estado = ESTADO_MINA;    mina_init();    break;
            case 1: estado = ESTADO_TALLER;  taller_init();  break;
            case 2: estado = ESTADO_GALERIA; galeria_init(); break;
            case 3: estado = ESTADO_GALERIA; galeria_init(); break; // entra en galería y lleva a vitrina
            case 4: estado = ESTADO_TIENDA;  tienda_init();  break;
            case 5: estado = ESTADO_VIAJAR;  viajar_init();  break;
        }
    }
}

void volver_menu(void) {
    aplicar_paleta_segun_bioma(ciudad_actual_idx);
    dibujar_menu(opcion_menu);
    flip();
    estado = ESTADO_MENU;
}

void volver_menu_con_fade(void) {
    fade_out();
    aplicar_paleta_segun_bioma(ciudad_actual_idx);
    dibujar_menu(opcion_menu);
    fade_in();
    estado = ESTADO_MENU;
}
