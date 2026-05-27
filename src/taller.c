#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "taller.h"
#include "save.h"
#include "video.h"
#include "font.h"
#include "opalo.h"
#include "plasma.h"
#include "menu.h"

#define LIST_W      115
#define LIST_ITEM_H 14
#define LIST_ITEMS  8

#define THUMB_X     146
#define THUMB_Y      18

static const char* TEXTO_FORMA[4] = {
    "LINEAL",
    "RAMIFICADA",
    "ESPIRAL",
    "CAOTICA"
};

static const char* TEXTO_INTENSIDAD[5] = {
    "",
    "DEBIL",
    "MEDIA",
    "FUERTE",
    "EXTREMA"
};

static Chunk taller_items[MAX_TALLER];
static int   num_taller;
static int   cursor;
static int   scroll;

typedef enum {
    TALLER_LISTA,
    TALLER_SUBMENU
} VistaTaller;

static VistaTaller vista_taller;
static int         opcion_submenu;

static void init_paleta_ui(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0]  = 0x0000;
    pal[1]  = 0x294A;
    pal[2]  = 0x681F;
    pal[3]  = 0x4210;
    pal[4]  = 0x6810;
    pal[5]  = 0x1F00;
    for (int i = 6; i <= 15; i++) pal[i] = 0x0000;
    pal[255] = 0x7FFF;
}

static void render_taller(void) {
    init_paleta_ui();
    uint16_t* vram = get_vram();
    clear(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "TALLER - CORTAR", 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (num_taller == 0) {
        draw_text(vram, 10, 70, "TALLER VACIO",  255);
        draw_text(vram, 10, 85, "FARMEA CHUNKS", 255);
        flip();
        return;
    }

    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_taller) break;

        int y      = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char label[16];
        sprintf(label, "CHUNK %d", idx + 1);
        draw_text(vram, 4, y + 3, label, 255);
    }

    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13,  "^", 255);
    if (scroll + LIST_ITEMS < num_taller)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (cursor < num_taller) {
        Chunk* c = &taller_items[cursor];

        renderizar_roca_pequena(THUMB_X, THUMB_Y, c);
        init_paleta_ui();

        int x = LIST_W + 4;
        int y = 105;

        char buf[24];
        sprintf(buf, "TAM:    %d", c->tamanyo);
        draw_text(vram, x, y, buf, 255); y += 12;

        sprintf(buf, "PESO:   %d", c->quilates);
        draw_text(vram, x, y, buf, 255); y += 12;

        sprintf(buf, "GRIETAS:%d", c->grietas);
        draw_text(vram, x, y, buf, 255);
        y += 12;

        if (c->grietas) {
            sprintf(buf, "INT:%s", TEXTO_INTENSIDAD[c->intensidad_grieta]);
            draw_text(vram, x, y, buf, 255);
            y += 12;

            sprintf(buf, "FORMA:%s", TEXTO_FORMA[c->forma_grieta]);
            draw_text(vram, x, y, buf, 255);
            y += 12;

            sprintf(buf, "PUREZA:%d?", c->pureza_aprox);
            draw_text(vram, x, y, buf, 255);
            y += 14;
        }

        if (c->grietas > 0 && c->pista > 0) {
            static const char* PISTA_TEXTO[5] = {
                "", "NEGRO?", "CRISTAL?", "FUEGO?", "BLANCO?"
            };
            draw_text(vram, x, y, PISTA_TEXTO[c->pista], 255);
            y += 12;
        }

        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista_taller == TALLER_SUBMENU) {
        int sm_x = LIST_W + 10;
        int sm_y = 100;
        int sm_w = 105;
        int sm_h = 42;

        fill_rect(vram, sm_x,     sm_y,     sm_w,     sm_h,     3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        if (opcion_submenu == 0) {
            draw_text(vram, sm_x + 6, sm_y + 8,  "> CORTE NORMAL",  255);
            draw_text(vram, sm_x + 6, sm_y + 24, "  MOVER GALERIA", 255);
        } else {
            draw_text(vram, sm_x + 6, sm_y + 8,  "  CORTE NORMAL",  255);
            draw_text(vram, sm_x + 6, sm_y + 24, "> MOVER GALERIA", 255);
        }
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
    flip();
}

static void cortar_chunk_seleccionado(void) {
    Chunk c = taller_items[cursor];
    uint32_t h = c.seed;

    int perdida = 0;
    if (c.pureza_aprox < 45)
        perdida = 35;
    else if (c.pureza_aprox < 70)
        perdida = 20;
    else
        perdida = 10;

    uint32_t r = (h >> 12) & 99;

    if (r < perdida)
        c.quilates = (c.quilates * 65) / 100;
    else if (r > 92)
        c.quilates = (c.quilates * 95) / 100;

    c.cortado = 1;

    guardar_chunk(&c);
    reset_taller();
    for (int i = 0; i < num_taller; i++) {
        if (i != cursor) guardar_chunk_taller(&taller_items[i]);
    }
    taller_recargar();
}

static void mover_a_galeria(void) {
    Chunk c = taller_items[cursor];
    guardar_chunk(&c);
    reset_taller();
    for (int i = 0; i < num_taller; i++) {
        if (i != cursor) guardar_chunk_taller(&taller_items[i]);
    }
    taller_recargar();
}

void taller_init(void) {
    taller_recargar();
    cursor         = 0;
    scroll         = 0;
    vista_taller   = TALLER_LISTA;
    opcion_submenu = 0;
    render_taller();
}

void taller_input(uint16_t keys) {

    if (keys & KEY_START) {
        volver_menu();
        return;
    }

    if (vista_taller == TALLER_LISTA) {
        if ((keys & KEY_DOWN) && cursor + 1 < num_taller) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS)
                scroll = cursor - LIST_ITEMS + 1;
            render_taller();
        }
        if ((keys & KEY_UP) && cursor > 0) {
            cursor--;
            if (cursor < scroll) scroll = cursor;
            render_taller();
        }
        if ((keys & KEY_A) && num_taller > 0) {
            vista_taller   = TALLER_SUBMENU;
            opcion_submenu = 0;
            render_taller();
        }
    } else {
        if ((keys & KEY_UP) || (keys & KEY_DOWN)) {
            opcion_submenu = !opcion_submenu;
            render_taller();
        }
        if (keys & KEY_B) {
            vista_taller = TALLER_LISTA;
            render_taller();
        }
        if (keys & KEY_A) {
            if (opcion_submenu == 0)
                cortar_chunk_seleccionado();
            else
                mover_a_galeria();
            vista_taller = TALLER_LISTA;
            render_taller();
        }
    }
}

void taller_recargar(void) {
    num_taller = cargar_chunks_taller(taller_items);
    if (cursor >= num_taller)
        cursor = (num_taller > 0) ? num_taller - 1 : 0;
    if (cursor < 0) cursor = 0;
}
