#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "galeria.h"
#include "save.h"
#include "plasma.h"
#include "video.h"
#include "font.h"
#include "opalo.h"
#include "thumb_cache.h"
#include "ciudades.h"

#define LIST_W      115
#define LIST_ITEM_H  14
#define LIST_ITEMS    8
#define SCROLL_MAX   (MAX_GALERIA - LIST_ITEMS)

#define THUMB_X     146
#define THUMB_Y      18

static const char* NOMBRE_TIPO[4] = {
    "OPALO NEGRO",
    "OPALO CRISTAL",
    "OPALO FUEGO",
    "OPALO BLANCO"
};

static const char* NOMBRE_PATRON[4] = {
    "NEBULA",
    "VENAS",
    "MOSAICO",
    "CHAOS"
};

static const char* NOMBRE_TAM[5] = {
    "S", "M", "L", "XL", "XXL"
};

typedef enum {
    VISTA_LISTA,
    VISTA_SUBMENU,
    VISTA_FICHA,
    VISTA_IMAGEN
} VistaGaleria;

static VistaGaleria vista;
static Chunk        items[MAX_GALERIA];
static int          num_items;
static int          cursor;
static int          scroll;
static int          opcion_submenu;

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

static void clear_vram(uint16_t* vram) {
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

static void u8_to_dec(uint8_t v, char* buf) {
    buf[0] = '0' + (v / 10);
    buf[1] = '0' + (v % 10);
}

static uint32_t calcular_valor_opalo_wrapper(int idx) {
    Opalo o;
    generar_opalo(&o, items[idx].seed);
    return calcular_valor_opalo(&o);
}

static uint32_t calcular_valor_chunk_bruto(int idx) {
    return (uint32_t)(items[idx].tamanyo * items[idx].quilates * 2)
         + (items[idx].grietas * 5);
}

static void vender_item_seleccionado(int idx_lista) {
    Chunk todos[MAX_GALERIA];
    int n = cargar_chunks(todos);

    int slot_sram = -1;
    for (int i = 0; i < n; i++) {
        if (todos[i].seed == items[idx_lista].seed) {
            slot_sram = i;
            break;
        }
    }
    if (slot_sram == -1) return;

    uint32_t valor = items[idx_lista].cortado
                   ? calcular_valor_opalo_wrapper(idx_lista)
                   : calcular_valor_chunk_bruto(idx_lista);

    modificar_dinero((int32_t)valor);

    for (int i = slot_sram; i < n - 1; i++) {
        sobreescribir_chunk(i, &todos[i + 1]);
    }
    Chunk vacio = {0};
    sobreescribir_chunk(n - 1, &vacio);
    decrementar_num_chunks();
}

/* ---- render_thumb: ahora pasa tamanyo a renderizar_opalo_pequeno ---- */
static void render_thumb(int idx) {
    if (items[idx].cortado) {
        Opalo o;
        generar_opalo(&o, items[idx].seed);
        generar_paleta(&o);
        renderizar_opalo_pequeno(THUMB_X, THUMB_Y, &o, items[idx].tamanyo);
    } else {
        renderizar_roca_pequena(THUMB_X, THUMB_Y, &items[idx]);
    }
    init_paleta_ui();
}

extern const Ciudad ciudades[3];

static void render_ficha(int idx) {
    uint16_t* vram = get_vram();
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < LIST_W; x += 2) {
            vram[(y * 120) + (x / 2)] = 0;
        }
    }

    int y = 20;
    draw_text(vram, 5, y, "FICHA TECNICA", 255); y += 20;

    char buf[40];
    sprintf(buf, "FECHA: %d/%d", items[idx].dia, items[idx].mes);
    draw_text(vram, 5, y, buf, 255); y += 14;

    draw_text(vram, 5, y, "ORIGEN:", 255); y += 12;
    draw_text(vram, 5, y, (char*)ciudades[items[idx].ciudad_id].nombre, 255); y += 14;

    sprintf(buf, "PESO: %d K", items[idx].quilates);
    draw_text(vram, 5, y, buf, 255);

    draw_text(vram, 5, 140, "B: VOLVER", 255);
}

static void render_lista(void) {
    init_paleta_ui();
    uint16_t* vram = get_vram();
    clear_vram(vram);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA", 255);

    char txt_dinero[16];
    sprintf(txt_dinero, "ORO: %d", obtener_dinero());
    draw_text(vram, 175, 2, txt_dinero, 255);

    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10,  70, "GALERIA VACIA", 255);
        draw_text(vram, 10,  85, "CORTA O MUEVE", 255);
        draw_text(vram, 10, 100, "EN EL TALLER",  255);
        flip();
        return;
    }

    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_items) break;

        int y      = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char num[4] = "XX.";
        num[0] = '0' + ((idx + 1) / 10);
        num[1] = '0' + ((idx + 1) % 10);
        draw_text(vram, 2, y + 3, num, 255);

        if (items[idx].cortado) {
            Opalo o;
            generar_opalo(&o, items[idx].seed);
            int t = (o.tipo >= 0 && o.tipo < 4) ? o.tipo : 0;
            draw_text(vram, 20, y + 3, NOMBRE_TIPO[t], 255);
        } else {
            draw_text(vram, 20, y + 3, "CHUNK BRUTO", 255);
        }
    }

    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13,  "^", 255);
    if (scroll + LIST_ITEMS < num_items)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (cursor < num_items) {
        render_thumb(cursor);

        int x = LIST_W + 4;
        int y = 105;
        char val_buf[20];

        if (items[cursor].cortado) {
            Opalo o;
            generar_opalo(&o, items[cursor].seed);
            int t = (o.tipo >= 0 && o.tipo < 4) ? o.tipo : 0;

            draw_text(vram, x, y, NOMBRE_TIPO[t], 255); y += 12;
            draw_text(vram, x, y, NOMBRE_PATRON[o.patron], 255); y += 14;

            uint32_t valor = calcular_valor_opalo_wrapper(cursor);
            sprintf(val_buf, "VALOR: %d", valor);
            draw_text(vram, x, y, val_buf, 255); y += 14;

        } else {
            char buf[24];
            draw_text(vram, x, y, "CHUNK SIN CORTAR", 255); y += 14;

            sprintf(buf, "PESO:   %d", items[cursor].quilates);
            draw_text(vram, x, y, buf, 255); y += 12;

            uint32_t valor = calcular_valor_chunk_bruto(cursor);
            sprintf(val_buf, "VALOR: %d", valor);
            draw_text(vram, x, y, val_buf, 255); y += 14;
        }

        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista == VISTA_SUBMENU) {
        int sm_x = LIST_W + 10;
        int sm_y = 100;
        int sm_w = 105;
        int sm_h = 42;

        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        int es_opalo = (cursor < num_items) && items[cursor].cortado;

        if (opcion_submenu == 0) {
            draw_text(vram, sm_x + 6, sm_y + 8,  es_opalo ? "> VER IMAGEN" : "> ---", 255);
            draw_text(vram, sm_x + 6, sm_y + 24, "  VENDER", 255);
        } else {
            draw_text(vram, sm_x + 6, sm_y + 8,  es_opalo ? "  VER IMAGEN" : "  ---", 255);
            draw_text(vram, sm_x + 6, sm_y + 24, "> VENDER", 255);
        }
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
    flip();
}

static void render_imagen(int idx) {
    Opalo o;
    generar_opalo(&o, items[idx].seed);
    generar_paleta(&o);
    renderizar_opalo(&o);

    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[255] = 0x7FFF;

    uint16_t* vram = get_vram();
    draw_text(vram, 2, 2,   NOMBRE_TIPO[o.tipo], 255);
    draw_text(vram, 2, 150, "B:VOLVER  IZQ/DER:CAMBIAR", 255);
    flip();
}

void galeria_init(void) {
    Chunk todos[MAX_GALERIA];
    int n = cargar_chunks(todos);
    num_items = 0;
    for (int i = 0; i < n; i++) {
        items[num_items++] = todos[i];
    }

    cursor = 0;
    scroll = 0;
    vista  = VISTA_LISTA;
    opcion_submenu = 0;
    render_lista();
}

void galeria_input(uint16_t keys) {

    if (vista == VISTA_LISTA) {

        if ((keys & KEY_DOWN) && cursor + 1 < num_items) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS)
                scroll = cursor - LIST_ITEMS + 1;
            render_lista();
        }
        if ((keys & KEY_UP) && cursor > 0) {
            cursor--;
            if (cursor < scroll) scroll = cursor;
            render_lista();
        }
        if ((keys & KEY_A) && num_items > 0) {
            vista = VISTA_SUBMENU;
            opcion_submenu = 0;
            render_lista();
        }

    } else if (vista == VISTA_SUBMENU) {

        if ((keys & KEY_UP) || (keys & KEY_DOWN)) {
            opcion_submenu = !opcion_submenu;
            render_lista();
        }
        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
        }
        if (keys & KEY_A) {
            if (opcion_submenu == 0) {
                vista = VISTA_FICHA;
                render_lista();
                render_ficha(cursor);
                flip();
            } else {
                vender_item_seleccionado(cursor);
                galeria_init();
            }
        }

    } else if (vista == VISTA_FICHA) {

        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
        }

    } else { // VISTA_IMAGEN

        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
        }
        if ((keys & KEY_LEFT) && cursor > 0) {
            int prev = cursor - 1;
            while (prev >= 0 && !items[prev].cortado) prev--;
            if (prev >= 0) {
                cursor = prev;
                if (cursor < scroll) scroll = cursor;
                render_imagen(cursor);
            }
        }
        if ((keys & KEY_RIGHT) && cursor + 1 < num_items) {
            int next = cursor + 1;
            while (next < num_items && !items[next].cortado) next++;
            if (next < num_items) {
                cursor = next;
                if (cursor >= scroll + LIST_ITEMS)
                    scroll = cursor - LIST_ITEMS + 1;
                render_imagen(cursor);
            }
        }
    }
}
