#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "taller.h"
#include "save.h"
#include "video.h"
#include "font.h"
#include "opalo.h"

#define LIST_W      115
#define LIST_ITEM_H 14
#define LIST_ITEMS  8

static Chunk taller_items[MAX_TALLER];
static int   num_taller;
static int   cursor;
static int   scroll;

// Función para renderizar la pantalla
static void render_taller(void) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    // Cabecera
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "TALLER - CORTAR", 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (num_taller == 0) {
        draw_text(vram, 10, 70, "TALLER VACIO", 255);
        flip();
        return;
    }

    // Lista de Chunks
    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_taller) break;

        int y = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char label[16];
        sprintf(label, "CHUNK %d", idx + 1);
        draw_text(vram, 4, y + 3, label, 255);
    }

    // Panel Derecho
    if (cursor < num_taller) {
        int x = LIST_W + 4;
        Chunk* c = &taller_items[cursor];
        
        char buf[32];
        sprintf(buf, "TAM: %d", c->tamanyo);
        draw_text(vram, x, 20, buf, 255);
        sprintf(buf, "GRIETAS: %d", c->grietas);
        draw_text(vram, x, 32, buf, 255);
        sprintf(buf, "PESO: %d", c->peso);
        draw_text(vram, x, 44, buf, 255);
        
        draw_text(vram, x, 80, "A: CORTAR", 255);
    }
    flip();
}

void taller_init(void) {
    taller_recargar();
    cursor = 0;
    scroll = 0;
    render_taller();
}

void taller_input(uint16_t keys) {
    if ((keys & KEY_DOWN) && cursor + 1 < num_taller) {
        cursor++;
        if (cursor >= scroll + LIST_ITEMS) scroll = cursor - LIST_ITEMS + 1;
        render_taller();
    }
    if ((keys & KEY_UP) && cursor > 0) {
        cursor--;
        if (cursor < scroll) scroll = cursor;
        render_taller();
    }

    // Lógica al pulsar A (Corte)
    if ((keys & KEY_A) && num_taller > 0) {
        // 1. Procesar el corte
        Chunk c = taller_items[cursor];
        c.cortado = 1;
        guardar_chunk(&c); // Guarda en la galería

        // 2. Reorganizar taller (eliminar el cortado)
        reset_taller();
        for(int i = 0; i < num_taller; i++) {
            if(i != cursor) guardar_chunk_taller(&taller_items[i]);
        }

        // 3. Recargar estado y refrescar
        taller_recargar();
        render_taller();
    }
}

void taller_recargar(void) {
    num_taller = cargar_chunks_taller(taller_items);
    if (cursor >= num_taller) {
        cursor = (num_taller > 0) ? num_taller - 1 : 0;
    }
    if (cursor < 0) cursor = 0;
}
