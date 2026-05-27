#include "tienda.h"
#include "taller.h"
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "video.h"
#include "font.h"
#include "save.h"
#include "sacos.h"
#include "util.h"
#include "menu.h"

#define LIST_W      115
#define LIST_ITEM_H  20

static int cursor = 0;

static void dibujar_tienda(void) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    rect(vram, 0, 0, 240, 16, 4);
    draw_text(vram, 5, 4, "TIENDA", 255);

    char buf[32];
    sprintf(buf, "ORO: %d", obtener_dinero());
    draw_text(vram, 150, 4, buf, 255);

    vline(vram, LIST_W, 16, 144, 3);
    for (int i = 0; i < 4; i++) {
        int y = 20 + (i * LIST_ITEM_H);
        if (i == cursor) fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, 2);
        draw_text(vram, 5, y + 4, (char*)SACOS[i].nombre, 255);
    }

    int x_desc = LIST_W + 10;
    sprintf(buf, "CANT: %d", SACOS[cursor].cantidad_chunks);
    draw_text(vram, x_desc, 30, buf, 255);
    sprintf(buf, "PRECIO: %d", SACOS[cursor].precio);
    draw_text(vram, x_desc, 50, buf, 255);
    draw_text(vram, x_desc, 100, "A: COMPRAR", 255);
    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);

    flip();
}

void tienda_init(void) {
    cursor = 0;
    dibujar_tienda();
}

void tienda_input(uint16_t keys) {
    if (keys & KEY_START) {
        volver_menu();
        return;
    }
    if ((keys & KEY_DOWN) && cursor < 3) {
        cursor++;
        dibujar_tienda();
    }
    if ((keys & KEY_UP) && cursor > 0) {
        cursor--;
        dibujar_tienda();
    }
    if (keys & KEY_A) {
        if (obtener_dinero() >= SACOS[cursor].precio) {
            comprar_saco(cursor);
            taller_recargar();
            dibujar_tienda();
        }
    }
}
