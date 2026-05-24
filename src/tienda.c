#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include "tienda.h"
#include "save.h"
#include "plasma.h"
#include "video.h"
#include "font.h"
#include "opalo.h"
#include "thumb_cache.h"

// -------------------------------------------------------
// CONFIG
// -------------------------------------------------------
#define COLS     4
#define ROWS     2
#define GAP_X    4
#define GAP_Y    4
#define MARGIN_X 10
#define MARGIN_Y 16

#define PAL_UI_WHITE 255

typedef enum {
    VISTA_GRID,
    VISTA_DETALLE
} VistaTienda;

// -------------------------------------------------------
// PRECIO: derivado de la seed del opalo
// -------------------------------------------------------
static uint32_t calcular_precio(uint32_t seed) {
    uint32_t q = seed ^ (seed >> 16);
    q ^= (q >> 8);
    q &= 0xFF;
    return 10 + q;
}

// -------------------------------------------------------
// STATE
// -------------------------------------------------------
static VistaTienda vista;
static Chunk capturas[MAX_CAPTURAS];
static ThumbCache thumbs[MAX_CAPTURAS];
static int num_capturas;
static int cursor;
static int detalle_idx;
static uint32_t monedas;

// -------------------------------------------------------
// UTILIDADES DE DIBUJO
// -------------------------------------------------------
static void put_pixel(uint16_t* vram, int x, int y, uint8_t color) {
    if (x < 0 || x >= 240 || y < 0 || y >= 160) return;
    int idx = y * 120 + x / 2;
    if (x & 1)
        vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)color << 8);
    else
        vram[idx] = (vram[idx] & 0xFF00) | color;
}

static void clear_back(uint16_t* vram) {
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

static void init_paleta_ui(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0] = 0x0000;
    pal[1] = RGB5(6, 6, 6);
    pal[255] = 0x7FFF;
}

static void dibujar_borde(uint16_t* vram, int ox, int oy) {
    for (int x = ox - 1; x < ox + THUMB_W + 1; x++) {
        put_pixel(vram, x, oy - 1,       PAL_UI_WHITE);
        put_pixel(vram, x, oy + THUMB_H,  PAL_UI_WHITE);
    }
    for (int y = oy - 1; y < oy + THUMB_H + 1; y++) {
        put_pixel(vram, ox - 1,       y, PAL_UI_WHITE);
        put_pixel(vram, ox + THUMB_W, y, PAL_UI_WHITE);
    }
}

// -------------------------------------------------------
// Dibuja un uint32 sin sprintf
// -------------------------------------------------------
static void draw_uint(uint16_t* vram, int x, int y, uint32_t val, uint8_t color) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0 && i > 0) {
            buf[--i] = '0' + (val % 10);
            val /= 10;
        }
    }
    draw_text(vram, x, y, &buf[i], color);
}

// -------------------------------------------------------
// GRID
// -------------------------------------------------------
static void render_grid(void) {
    uint16_t* vram = get_vram();
    clear_back(vram);

    draw_text(vram, 74, 3, "TIENDA OPALO", PAL_UI_WHITE);
    draw_text(vram, 160, 3, "$", PAL_UI_WHITE);
    draw_uint(vram, 168, 3, monedas, PAL_UI_WHITE);

    for (int i = 0; i < MAX_CAPTURAS; i++) {
        int col = i % COLS;
        int row = i / COLS;
        int ox  = MARGIN_X + col * (THUMB_W + GAP_X);
        int oy  = MARGIN_Y + row * (THUMB_H + GAP_Y);

        if (i < num_capturas) {
            thumb_dibujar(&thumbs[i], vram, ox, oy);
            if (i == cursor) dibujar_borde(vram, ox, oy);
        } else {
            for (int py = oy; py < oy + THUMB_H; py++)
                for (int px = ox; px < ox + THUMB_W; px++)
                    put_pixel(vram, px, py, 1);
        }
    }

    draw_text(vram, 10, 152, "CRUZ:MOVER A:VENDER START:MENU", PAL_UI_WHITE);
    flip();
}

// -------------------------------------------------------
// DETALLE / CONFIRMACION DE VENTA
// -------------------------------------------------------
static void render_detalle(void) {
    Opalo o;
    generar_opalo(&o, capturas[detalle_idx].seed);
    generar_paleta(&o);
    renderizar_opalo(&o);

    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[255] = 0x7FFF;

    uint16_t* vram = get_vram();
    uint32_t precio = calcular_precio(capturas[detalle_idx].seed);

    draw_text(vram, 2, 2,  "VENDER? A:SI  B:VOLVER", 255);
    draw_text(vram, 2, 12, "PRECIO: $",               255);
    draw_uint(vram, 66, 12, precio,                    255);
    flip();
}

// -------------------------------------------------------
// PANTALLA DE VENTA COMPLETADA
// -------------------------------------------------------
static void render_vendido(uint32_t precio) {
    uint16_t* vram = get_vram();
    clear_back(vram);
    init_paleta_ui();

    draw_text(vram, 60, 60, "OPALO VENDIDO!", PAL_UI_WHITE);
    draw_text(vram, 60, 75, "+$",             PAL_UI_WHITE);
    draw_uint(vram, 78, 75, precio,            PAL_UI_WHITE);
    draw_text(vram, 45, 95, "TOTAL: $",        PAL_UI_WHITE);
    draw_uint(vram, 96, 95, monedas,           PAL_UI_WHITE);

    flip();
    for (volatile int t = 0; t < 200000; t++);
}

// -------------------------------------------------------
// API
// -------------------------------------------------------
void tienda_init(void) {
    num_capturas = cargar_chunks(capturas);
    monedas      = cargar_monedas();
    cursor       = 0;
    detalle_idx  = 0;
    vista        = VISTA_GRID;

    for (int i = 0; i < num_capturas; i++)
        thumb_generar(&thumbs[i], capturas[i].seed);

    init_paleta_ui();
    render_grid();
}

void tienda_input(uint16_t keys) {

    if (vista == VISTA_GRID) {

        int moved = 0;

        if ((keys & KEY_RIGHT) && cursor % COLS < COLS - 1 && cursor + 1 < num_capturas) {
            cursor++; moved = 1;
        }
        if ((keys & KEY_LEFT) && cursor % COLS > 0) {
            cursor--; moved = 1;
        }
        if ((keys & KEY_DOWN) && cursor + COLS < num_capturas) {
            cursor += COLS; moved = 1;
        }
        if ((keys & KEY_UP) && cursor >= COLS) {
            cursor -= COLS; moved = 1;
        }

        if (moved) render_grid();

        if ((keys & KEY_A) && num_capturas > 0) {
            detalle_idx = cursor;
            vista = VISTA_DETALLE;
            render_detalle();
        }

    } else {

        if (keys & KEY_B) {
            vista = VISTA_GRID;
            init_paleta_ui();
            render_grid();
        }

        if (keys & KEY_A) {
            uint32_t precio = calcular_precio(capturas[detalle_idx].seed);
            monedas += precio;
            guardar_monedas(monedas);

            // Compactar: mover el último al hueco
            if (detalle_idx < num_capturas - 1) {
                sobreescribir_chunk(detalle_idx, &capturas[num_capturas - 1]);
            }
            // Decrementar contador en SRAM
            num_capturas--;
            {
                volatile uint8_t* sram = (volatile uint8_t*)0x0E000000;
                uint32_t n = (uint32_t)num_capturas;
                sram[0x10] = n & 0xFF;
                sram[0x11] = (n >> 8) & 0xFF;
                sram[0x12] = (n >> 16) & 0xFF;
                sram[0x13] = (n >> 24) & 0xFF;
            }

            // Recargar estado local
            num_capturas = cargar_chunks(capturas);
            if (cursor >= num_capturas && cursor > 0) cursor = num_capturas - 1;

            for (int i = 0; i < num_capturas; i++)
                thumb_generar(&thumbs[i], capturas[i].seed);

            render_vendido(precio);

            vista = VISTA_GRID;
            init_paleta_ui();
            render_grid();
        }

        if ((keys & KEY_LEFT) && detalle_idx > 0) {
            detalle_idx--;
            render_detalle();
        }
        if ((keys & KEY_RIGHT) && detalle_idx + 1 < num_capturas) {
            detalle_idx++;
            render_detalle();
        }
    }
}

