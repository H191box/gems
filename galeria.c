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
// CONFIG
// -------------------------------------------------------

#define COLS        4
#define ROWS        2

#define GAP_X       4
#define GAP_Y       4

#define MARGIN_X    10
#define MARGIN_Y    16

#define PAL_UI_WHITE 255

typedef enum {
    VISTA_GRID,
    VISTA_DETALLE
} VistaGaleria;

// -------------------------------------------------------
// STATE
// -------------------------------------------------------

static VistaGaleria vista;

static uint32_t capturas[MAX_CAPTURAS];

static ThumbCache thumbs[MAX_CAPTURAS];

static int num_capturas;
static int cursor;
static int detalle_idx;

// -------------------------------------------------------
// VRAM
// -------------------------------------------------------

static void put_pixel(
    uint16_t* vram,
    int x,
    int y,
    uint8_t color
) {

    if (x < 0 || x >= 240 || y < 0 || y >= 160)
        return;

    int idx = y * 120 + x / 2;

    if (x & 1) {

        vram[idx] =
            (vram[idx] & 0x00FF) |
            ((uint16_t)color << 8);

    } else {

        vram[idx] =
            (vram[idx] & 0xFF00) |
            color;
    }
}

static void clear_back(uint16_t* vram) {

    for (int i = 0; i < 19200; i++) {
        vram[i] = 0;
    }
}

// -------------------------------------------------------
// UI PALETTE
// -------------------------------------------------------

static void init_paleta_ui(void) {

    volatile uint16_t* pal =
        (volatile uint16_t*)0x05000000;

    pal[0] = 0x0000;

    pal[1] = RGB5(6,6,6);

    pal[255] = 0x7FFF;
}

// -------------------------------------------------------
// CURSOR BORDER
// -------------------------------------------------------

static void dibujar_borde(
    uint16_t* vram,
    int ox,
    int oy
) {

    for (int x = ox - 1;
         x < ox + THUMB_W + 1;
         x++) {

        put_pixel(
            vram,
            x,
            oy - 1,
            PAL_UI_WHITE
        );

        put_pixel(
            vram,
            x,
            oy + THUMB_H,
            PAL_UI_WHITE
        );
    }

    for (int y = oy - 1;
         y < oy + THUMB_H + 1;
         y++) {

        put_pixel(
            vram,
            ox - 1,
            y,
            PAL_UI_WHITE
        );

        put_pixel(
            vram,
            ox + THUMB_W,
            y,
            PAL_UI_WHITE
        );
    }
}

// -------------------------------------------------------
// GRID
// -------------------------------------------------------

static void render_grid(void) {

    uint16_t* vram =
        get_vram();

    clear_back(vram);

    draw_text(
        vram,
        74,
        3,
        "GALERIA OPALO",
        PAL_UI_WHITE
    );

    for (int i = 0;
         i < MAX_CAPTURAS;
         i++) {

        int col = i % COLS;
        int row = i / COLS;

        int ox =
            MARGIN_X +
            col * (THUMB_W + GAP_X);

        int oy =
            MARGIN_Y +
            row * (THUMB_H + GAP_Y);

        // ------------------------------------------------
        // SLOT OCUPADO
        // ------------------------------------------------

        if (i < num_capturas) {

            thumb_dibujar(
                &thumbs[i],
                vram,
                ox,
                oy
            );

            if (i == cursor) {

                dibujar_borde(
                    vram,
                    ox,
                    oy
                );
            }

        // ------------------------------------------------
        // SLOT VACIO
        // ------------------------------------------------

        } else {

            for (int y = oy;
                 y < oy + THUMB_H;
                 y++) {

                for (int x = ox;
                     x < ox + THUMB_W;
                     x++) {

                    put_pixel(
                        vram,
                        x,
                        y,
                        1
                    );
                }
            }

            draw_text(
                vram,
                ox + 24,
                oy + 28,
                "+",
                PAL_UI_WHITE
            );
        }
    }

    draw_text(
        vram,
        10,
        152,
        "CRUZ:MOVER  A:VER  START:MENU",
        PAL_UI_WHITE
    );

    flip();
}

// -------------------------------------------------------
// DETAIL VIEW
// -------------------------------------------------------

static void render_detalle(void) {

    Opalo o;

    generar_opalo(
        &o,
        capturas[detalle_idx]
    );

    generar_paleta(&o);

    renderizar_opalo(&o);

    volatile uint16_t* pal =
        (volatile uint16_t*)0x05000000;

    pal[255] = 0x7FFF;

    uint16_t* vram =
        get_vram();

    draw_text(
        vram,
        2,
        2,
        "IZQ/DER:CAMBIAR  B:VOLVER",
        255
    );

    flip();
}

// -------------------------------------------------------
// API
// -------------------------------------------------------

void galeria_init(void) {

    num_capturas =
        cargar_capturas(capturas);

    cursor = 0;

    detalle_idx = 0;

    vista = VISTA_GRID;

    // ---------------------------------------------------
    // PRECOMPUTE THUMBNAILS
    // ---------------------------------------------------

    for (int i = 0;
         i < num_capturas;
         i++) {

        thumb_generar(
            &thumbs[i],
            capturas[i]
        );
    }

    init_paleta_ui();

    render_grid();
}

void galeria_input(uint16_t keys) {

    // ---------------------------------------------------
    // GRID
    // ---------------------------------------------------

    if (vista == VISTA_GRID) {

        int moved = 0;

        if ((keys & KEY_RIGHT) &&
            cursor % COLS < COLS - 1 &&
            cursor + 1 < num_capturas) {

            cursor++;
            moved = 1;
        }

        if ((keys & KEY_LEFT) &&
            cursor % COLS > 0) {

            cursor--;
            moved = 1;
        }

        if ((keys & KEY_DOWN) &&
            cursor + COLS < num_capturas) {

            cursor += COLS;
            moved = 1;
        }

        if ((keys & KEY_UP) &&
            cursor >= COLS) {

            cursor -= COLS;
            moved = 1;
        }

        if (moved) {
            render_grid();
        }

        if ((keys & KEY_A) &&
            num_capturas > 0) {

            detalle_idx = cursor;

            vista = VISTA_DETALLE;

            render_detalle();
        }

    // ---------------------------------------------------
    // DETAIL
    // ---------------------------------------------------

    } else {

        if (keys & KEY_B) {

            vista = VISTA_GRID;

            init_paleta_ui();

            render_grid();
        }

        if ((keys & KEY_LEFT) &&
            detalle_idx > 0) {

            detalle_idx--;

            render_detalle();
        }

        if ((keys & KEY_RIGHT) &&
            detalle_idx + 1 < num_capturas) {

            detalle_idx++;

            render_detalle();
        }
    }
}
