#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include "galeria.h"
#include "save.h"
#include "plasma.h"
#include "video.h"
#include "font.h"
#include "opalo.h"
#include "thumb_cache.h"

// -------------------------------------------------------
// CONFIG LAYOUT
// Pantalla 240x160
// Izquierda: 0-114   (lista)
// Divisor:   115-116
// Derecha:   117-239 (descripcion)
// -------------------------------------------------------
#define LIST_W      115
#define LIST_ITEM_H  14
#define LIST_ITEMS    8   // filas visibles
#define SCROLL_MAX   (MAX_CAPTURAS - LIST_ITEMS)

// Nombres de tipo
static const char* NOMBRE_TIPO[4] = {
    "OPALO NEGRO",
    "OPALO CRISTAL",
    "OPALO FUEGO",
    "OPALO BLANCO"
};

// Nombres de patrón
static const char* NOMBRE_PATRON[4] = {
    "NEBULA",
    "VENAS",
    "MOSAICO",
    "CHAOS"
};

// Nombres de tamaño
static const char* NOMBRE_TAM[5] = {
    "S", "M", "L", "XL", "XXL"
};

// -------------------------------------------------------
// STATE
// -------------------------------------------------------
typedef enum {
    VISTA_LISTA,
    VISTA_IMAGEN
} VistaGaleria;

static VistaGaleria vista;
static Chunk        items[MAX_CAPTURAS];
static int          num_items;
static int          cursor;
static int          scroll;    // primera fila visible

// -------------------------------------------------------
// PALETA UI
// -------------------------------------------------------
static void init_paleta_ui(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0]  = 0x0000;                               // negro fondo
    pal[1]  = 0x294A;                               // gris oscuro fila normal
    pal[2]  = 0x681F;                               // morado fila seleccionada
    pal[3]  = 0x4210;                               // gris divisor
    pal[4]  = 0x6810;                               // azul cabecera
    pal[255]= 0x7FFF;                               // blanco texto
}

// -------------------------------------------------------
// HELPERS DE DIBUJO
// -------------------------------------------------------
static void clear_vram(uint16_t* vram) {
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

static void put_pixel(uint16_t* vram, int x, int y, uint8_t c) {
    if (x < 0 || x >= 240 || y < 0 || y >= 160) return;
    int idx = y * 120 + x / 2;
    if (x & 1) vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)c << 8);
    else        vram[idx] = (vram[idx] & 0xFF00) | c;
}

static void fill_rect(uint16_t* vram, int x, int y,
                      int w, int h, uint8_t c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            put_pixel(vram, xx, yy, c);
}

static void vline(uint16_t* vram, int x, int y, int h, uint8_t c) {
    for (int yy = y; yy < y + h; yy++) put_pixel(vram, x, yy, c);
}

// Convierte uint8 a 2 dígitos decimales en buf[2]
static void u8_to_dec(uint8_t v, char* buf) {
    buf[0] = '0' + (v / 10);
    buf[1] = '0' + (v % 10);
}

// -------------------------------------------------------
// RENDER LISTA
// -------------------------------------------------------
static void render_lista(void) {
    init_paleta_ui();
    uint16_t* vram = get_vram();
    clear_vram(vram);

    // Cabecera
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4,   2, "GALERIA",        255);
    draw_text(vram, 130, 2, "DESCRIPCION",    255);

    // Divisor vertical
    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10, 70, "GALERIA VACIA", 255);
        draw_text(vram, 10, 85, "CORTA CHUNKS", 255);
        draw_text(vram, 10, 100,"EN EL TALLER",  255);
        flip();
        return;
    }

    // Lista: filas visibles según scroll
    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_items) break;

        int y   = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        // Número + nombre tipo
        char num[4] = "XX.";
        u8_to_dec((uint8_t)(idx + 1), num);
        draw_text(vram, 2, y + 3, num, 255);

        Opalo o;
        generar_opalo(&o, items[idx].seed);
        draw_text(vram, 20, y + 3, NOMBRE_TIPO[o.tipo], 255);
    }

    // Indicadores de scroll
    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13,      "^", 255);
    if (scroll + LIST_ITEMS < num_items)
        draw_text(vram, LIST_W / 2 - 4, 150,     "v", 255);

    // Panel derecho: descripción del item seleccionado
    if (cursor < num_items) {
        Opalo o;
        generar_opalo(&o, items[cursor].seed);

        int x = LIST_W + 4;
        int y = 14;

        draw_text(vram, x, y,      NOMBRE_TIPO[o.tipo],       255); y += 12;
        draw_text(vram, x, y,      NOMBRE_PATRON[o.patron],   255); y += 14;

        // Tamaño
        char tam[8] = "TAM: X";
        tam[5] = NOMBRE_TAM[items[cursor].tamanyo - 1][0];
        draw_text(vram, x, y, tam, 255); y += 11;

        // Peso
        char peso[10] = "PESO: XX";
        u8_to_dec(items[cursor].peso, &peso[6]);
        draw_text(vram, x, y, peso, 255); y += 11;

        // Brillo
        char bri[12] = "BRILL: XX";
        u8_to_dec(o.brillo, &bri[7]);
        draw_text(vram, x, y, bri, 255); y += 11;

        // Saturación
        char sat[12] = "SAT:   XX";
        u8_to_dec(o.saturacion, &sat[7]);
        draw_text(vram, x, y, sat, 255); y += 14;

        // Valor estimado (peso * tamaño * brillo / 10)
        uint16_t valor = (uint16_t)items[cursor].peso
                       * items[cursor].tamanyo
                       * o.brillo / 10;
        char val[12] = "VALOR: XXX";
        val[7] = '0' + (valor / 100) % 10;
        val[8] = '0' + (valor /  10) % 10;
        val[9] = '0' + (valor      ) % 10;
        draw_text(vram, x, y, val, 255); y += 14;

        draw_text(vram, x, y, "A:VER IMAGEN", 255);
    }

    // Instrucciones pie
    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
    flip();
}

// -------------------------------------------------------
// RENDER IMAGEN COMPLETA
// -------------------------------------------------------
static void render_imagen(int idx) {
    Opalo o;
    generar_opalo(&o, items[idx].seed);
    generar_paleta(&o);
    renderizar_opalo(&o);

    // Aseguramos que el blanco esté disponible para el texto
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[255] = 0x7FFF;

    uint16_t* vram = get_vram();
    draw_text(vram, 2, 2, NOMBRE_TIPO[o.tipo], 255);
    draw_text(vram, 2, 150, "B:VOLVER  IZQ/DER:CAMBIAR", 255);
    flip();
}

// -------------------------------------------------------
// API PUBLICA
// -------------------------------------------------------
void galeria_init(void) {
    // Solo mostramos los chunks ya cortados
    Chunk todos[MAX_CAPTURAS];
    int n = cargar_chunks(todos);
    num_items = 0;
    for (int i = 0; i < n; i++) {
        if (todos[i].cortado) {
            items[num_items++] = todos[i];
        }
    }

    cursor = 0;
    scroll = 0;
    vista  = VISTA_LISTA;
    render_lista();
}

void galeria_input(uint16_t keys) {

    if (vista == VISTA_LISTA) {

        if ((keys & KEY_DOWN) && cursor + 1 < num_items) {
            cursor++;
            // Scroll si el cursor sale de la ventana visible
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
            vista = VISTA_IMAGEN;
            render_imagen(cursor);
        }

    } else {  // VISTA_IMAGEN

        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
        }
        if ((keys & KEY_LEFT) && cursor > 0) {
            cursor--;
            if (cursor < scroll) scroll = cursor;
            render_imagen(cursor);
        }
        if ((keys & KEY_RIGHT) && cursor + 1 < num_items) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS)
                scroll = cursor - LIST_ITEMS + 1;
            render_imagen(cursor);
        }
    }
}
