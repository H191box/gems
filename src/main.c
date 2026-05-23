#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>

#include "menu.h"
#include "plasma.h"
#include "save.h"
#include "video.h"
#include "galeria.h"
#include "font.h"
#include "opalo.h"

typedef enum {
    ESTADO_MENU,
    ESTADO_FARMEO,
    ESTADO_GALERIA,
    ESTADO_SELECTOR
} EstadoJuego;

static uint32_t semilla_actual;
static int opcion_menu = 0;
static int slot_cursor = 0;

static Opalo opalo_actual;

// -------------------------------------------------------
// Genera y renderiza el opalo actual
// -------------------------------------------------------
static void refrescar_opalo(void) {

    generar_opalo(&opalo_actual, semilla_actual);

    generar_paleta(&opalo_actual);

    renderizar_opalo(&opalo_actual);

    flip();
}

// -------------------------------------------------------
// Paleta fija del selector
// -------------------------------------------------------
static void aplicar_paleta_selector(void) {

    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;

    pal[0]   = 0x0000;
    pal[1]   = 0x6810;
    pal[2]   = 0x294A;
    pal[3]   = 0x681F;
    pal[4]   = 0x4210;
    pal[255] = 0x7FFF;
}

// -------------------------------------------------------
// Rectangulo simple
// -------------------------------------------------------
static void rect(
    uint16_t* vram,
    int x,
    int y,
    int w,
    int h,
    uint8_t color
) {

    uint16_t packed = (color << 8) | color;

    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;

    for (int yy = y; yy < y + h; yy++) {

        for (int xx = x0; xx < x1; xx += 2) {

            vram[yy * 120 + xx / 2] = packed;
        }
    }
}

// -------------------------------------------------------
// Selector de slot
// -------------------------------------------------------
static void dibujar_selector(int cursor) {

    aplicar_paleta_selector();

    uint16_t* vram = get_vram();

    // limpiar pantalla
    for (int i = 0; i < 19200; i++) {
        vram[i] = 0;
    }

    // titulo
    rect(vram, 0, 0, 240, 20, 1);

    draw_text(
        vram,
        26,
        6,
        "GALERIA LLENA. SOBREESCRIBIR?",
        255
    );

    // slots
    for (int i = 0; i < MAX_CAPTURAS; i++) {

        int col = i % 2;
        int row = i / 2;

        int x = 8 + col * 116;
        int y = 26 + row * 30;

        uint8_t bg = (i == cursor) ? 3 : 2;

        rect(vram, x, y, 108, 22, bg);

        char label[8] = "SLOT X";
        label[5] = '1' + i;

        draw_text(vram, x + 6, y + 8, label, 255);
    }

    draw_text(
        vram,
        20,
        150,
        "CRUZ:MOVER  A:CONFIRMAR  B:CANCELAR",
        255
    );

    flip();
}

// -------------------------------------------------------
// Main
// -------------------------------------------------------
int main() {

    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    semilla_actual = cargar_seed();

    EstadoJuego estado = ESTADO_MENU;

    dibujar_menu(opcion_menu);

    flip();

    while (1) {

        scanKeys();

        uint16_t keys = keysDown();

        switch (estado) {

        // -------------------------------------------------
        // MENU
        // -------------------------------------------------
        case ESTADO_MENU:

            if (keys & KEY_UP) {

                opcion_menu--;

                if (opcion_menu < 0) {
                    opcion_menu = 1;
                }

                dibujar_menu(opcion_menu);

                flip();
            }

            if (keys & KEY_DOWN) {

                opcion_menu++;

                if (opcion_menu > 1) {
                    opcion_menu = 0;
                }

                dibujar_menu(opcion_menu);

                flip();
            }

            if (keys & KEY_A) {

                if (opcion_menu == 0) {

                    estado = ESTADO_FARMEO;

                    refrescar_opalo();

                } else {

                    estado = ESTADO_GALERIA;

                    galeria_init();
                }
            }

            break;

        // -------------------------------------------------
        // FARMEO
        // -------------------------------------------------
        case ESTADO_FARMEO:

            // reroll
            if (keys & KEY_B) {

                semilla_actual += 0x9E3779B9;

                refrescar_opalo();
            }

            // guardar
            if (keys & KEY_A) {

                uint32_t slots[MAX_CAPTURAS];

                int n = cargar_capturas(slots);

                // espacio libre
                if (n < MAX_CAPTURAS) {

                    guardar_seed(semilla_actual);

                    guardar_captura(semilla_actual);

                    flash_guardado();

                    refrescar_opalo();

                } else {

                    // galeria llena
                    slot_cursor = 0;

                    estado = ESTADO_SELECTOR;

                    dibujar_selector(slot_cursor);
                }
            }

            // volver menu
            if (keys & KEY_START) {

                estado = ESTADO_MENU;

                dibujar_menu(opcion_menu);

                flip();
            }

            break;

        // -------------------------------------------------
        // SELECTOR
        // -------------------------------------------------
        case ESTADO_SELECTOR:

            if (keys & KEY_RIGHT) {

                if ((slot_cursor % 2) == 0) {
                    slot_cursor++;
                }

                dibujar_selector(slot_cursor);
            }

            if (keys & KEY_LEFT) {

                if ((slot_cursor % 2) == 1) {
                    slot_cursor--;
                }

                dibujar_selector(slot_cursor);
            }

            if (keys & KEY_DOWN) {

                if (slot_cursor + 2 < MAX_CAPTURAS) {
                    slot_cursor += 2;
                }

                dibujar_selector(slot_cursor);
            }

            if (keys & KEY_UP) {

                if (slot_cursor >= 2) {
                    slot_cursor -= 2;
                }

                dibujar_selector(slot_cursor);
            }

            // confirmar overwrite
            if (keys & KEY_A) {

                guardar_seed(semilla_actual);

                sobreescribir_captura(
                    slot_cursor,
                    semilla_actual
                );

                flash_guardado();

                estado = ESTADO_FARMEO;

                refrescar_opalo();
            }

            // cancelar
            if (keys & KEY_B) {

                estado = ESTADO_FARMEO;

                refrescar_opalo();
            }

            break;

        // -------------------------------------------------
        // GALERIA
        // -------------------------------------------------
        case ESTADO_GALERIA:

            if (keys & KEY_START) {

                estado = ESTADO_MENU;

                dibujar_menu(opcion_menu);

                flip();

            } else {

                galeria_input(keys);
            }

            break;
        }
    }
}
