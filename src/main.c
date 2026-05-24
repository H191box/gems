// -------------------------------------------------------
//2.0 2.0 2.0 2.0
//


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
    ESTADO_TALLER,
    ESTADO_GALERIA,
    ESTADO_SELECTOR
} EstadoJuego;

static uint32_t semilla_actual;
static int      opcion_menu   = 0;
static int      slot_cursor   = 0;
static Chunk    chunk_actual;
static Opalo    opalo_actual;

// -------------------------------------------------------
// Taller: solo chunks sin cortar
// -------------------------------------------------------
static Chunk taller_slots[MAX_CAPTURAS];
static int   taller_n      = 0;
static int   taller_cursor = 0;

static void taller_recargar(void) {
    Chunk todos[MAX_CAPTURAS];
    int n = cargar_chunks(todos);
    taller_n = 0;
    for (int i = 0; i < n; i++) {
        if (!todos[i].cortado) {
            taller_slots[taller_n++] = todos[i];
        }
    }
}

// -------------------------------------------------------
// Genera un nuevo chunk y lo renderiza como roca
// -------------------------------------------------------
static void refrescar_chunk(void) {
    generar_chunk(&chunk_actual, semilla_actual);
    renderizar_roca(&chunk_actual);
    flip();
}

// -------------------------------------------------------
// Muestra el ópalo del chunk (usado en taller)
// -------------------------------------------------------
static void mostrar_opalo_chunk(const Chunk* c) {
    generar_opalo(&opalo_actual, c->seed);
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
static void rect(uint16_t* vram, int x, int y,
                 int w, int h, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x0; xx < x1; xx += 2)
            vram[yy * 120 + xx / 2] = packed;
}

// -------------------------------------------------------
// Selector de slot (galería llena)
// -------------------------------------------------------
static void dibujar_selector(int cursor) {
    aplicar_paleta_selector();
    uint16_t* vram = get_vram();
    for (int i = 0; i < 19200; i++) vram[i] = 0;

    rect(vram, 0, 0, 240, 20, 1);
    draw_text(vram, 26, 6, "GALERIA LLENA. SOBREESCRIBIR?", 255);

    for (int i = 0; i < MAX_CAPTURAS; i++) {
        int col = i % 2;
        int row = i / 2;
        int x   = 8 + col * 116;
        int y   = 26 + row * 30;
        uint8_t bg = (i == cursor) ? 3 : 2;
        rect(vram, x, y, 108, 22, bg);
        char label[8] = "SLOT X";
        label[5] = '1' + i;
        draw_text(vram, x + 6, y + 8, label, 255);
    }
    draw_text(vram, 20, 150, "CRUZ:MOVER A:CONFIRMAR B:CANCELAR", 255);
    flip();
}

// -------------------------------------------------------
// Taller: lista de chunks sin cortar
// -------------------------------------------------------
static void dibujar_taller(int cursor) {
    aplicar_paleta_selector();
    uint16_t* vram = get_vram();
    for (int i = 0; i < 19200; i++) vram[i] = 0;

    rect(vram, 0, 0, 240, 20, 1);
    draw_text(vram, 60, 6, "TALLER - ELIGE CHUNK", 255);

    if (taller_n == 0) {
        draw_text(vram, 40,  80, "NO HAY CHUNKS GUARDADOS", 255);
        draw_text(vram, 48, 100, "FARMEA PRIMERO (START)",  255);
    } else {
        for (int i = 0; i < taller_n; i++) {
            int col = i % 2;
            int row = i / 2;
            int x   = 8 + col * 116;
            int y   = 26 + row * 30;
            uint8_t bg = (i == cursor) ? 3 : 2;
            rect(vram, x, y, 108, 22, bg);

            char label[12] = "TAM:X GR:X";
            label[4] = '0' + taller_slots[i].tamanyo;
            label[9] = '0' + taller_slots[i].grietas;
            draw_text(vram, x + 4, y + 4,  label, 255);

            char peso[8] = "PESO:XX";
            uint8_t p = taller_slots[i].peso;
            peso[5] = '0' + (p / 10);
            peso[6] = '0' + (p % 10);
            draw_text(vram, x + 4, y + 13, peso, 255);
        }
    }
    draw_text(vram, 8, 150, "CRUZ:ELEGIR  A:CORTAR START:MENU", 255);
    flip();
}

// -------------------------------------------------------
// Main
// -------------------------------------------------------
int main() {
    REG_DISPCNT = MODE_4 | BG2_ENABLE;

    semilla_actual = cargar_seed();
    generar_chunk(&chunk_actual, semilla_actual);

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
                if (opcion_menu < 0) opcion_menu = 2;
                dibujar_menu(opcion_menu);
                flip();
            }
            if (keys & KEY_DOWN) {
                opcion_menu++;
                if (opcion_menu > 2) opcion_menu = 0;
                dibujar_menu(opcion_menu);
                flip();
            }
            if (keys & KEY_A) {
                if (opcion_menu == 0) {
                    estado = ESTADO_FARMEO;
                    refrescar_chunk();
                } else if (opcion_menu == 1) {
                    taller_recargar();
                    taller_cursor = 0;
                    estado = ESTADO_TALLER;
                    dibujar_taller(taller_cursor);
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
    if (keys & KEY_B) {
        semilla_actual += 0x9E3779B9;
        refrescar_chunk();
    }
    if (keys & KEY_A) {
        if (chunk_actual.grietas == 0) break;

        // Contar solo chunks sin cortar (los del taller)
        Chunk slots[MAX_CAPTURAS];
        int n = cargar_chunks(slots);
        int sin_cortar = 0;
        for (int i = 0; i < n; i++)
            if (!slots[i].cortado) sin_cortar++;

        if (sin_cortar < MAX_CAPTURAS) {
            guardar_seed(semilla_actual);
            guardar_chunk(&chunk_actual);
            flash_guardado();
            semilla_actual += 0x9E3779B9;
            refrescar_chunk();
        } else {
            slot_cursor = 0;
            estado = ESTADO_SELECTOR;
            dibujar_selector(slot_cursor);
        }
    }
    if (keys & KEY_START) {
        estado = ESTADO_MENU;
        dibujar_menu(opcion_menu);
        flip();
    }
    break;

// -------------------------------------------------
// SELECTOR (galería llena)
// -------------------------------------------------
case ESTADO_SELECTOR:
    if (keys & KEY_RIGHT) {
        if ((slot_cursor % 2) == 0) slot_cursor++;
        dibujar_selector(slot_cursor);
    }
    if (keys & KEY_LEFT) {
        if ((slot_cursor % 2) == 1) slot_cursor--;
        dibujar_selector(slot_cursor);
    }
    if (keys & KEY_DOWN) {
        if (slot_cursor + 2 < MAX_CAPTURAS) slot_cursor += 2;
        dibujar_selector(slot_cursor);
    }
    if (keys & KEY_UP) {
        if (slot_cursor >= 2) slot_cursor -= 2;
        dibujar_selector(slot_cursor);
    }
    if (keys & KEY_A) {
        guardar_seed(semilla_actual);
        sobreescribir_chunk(slot_cursor, &chunk_actual);
        flash_guardado();
        // Avanzar igual que al guardar normal
        semilla_actual += 0x9E3779B9;
        estado = ESTADO_FARMEO;
        refrescar_chunk();
    }
    if (keys & KEY_B) {
        estado = ESTADO_FARMEO;
        refrescar_chunk();
    }
    break;

        // -------------------------------------------------
        // TALLER
        // -------------------------------------------------
        case ESTADO_TALLER:
            if (keys & KEY_RIGHT) {
                if ((taller_cursor % 2) == 0 &&
                     taller_cursor + 1 < taller_n)
                    taller_cursor++;
                dibujar_taller(taller_cursor);
            }
            if (keys & KEY_LEFT) {
                if ((taller_cursor % 2) == 1) taller_cursor--;
                dibujar_taller(taller_cursor);
            }
            if (keys & KEY_DOWN) {
                if (taller_cursor + 2 < taller_n) taller_cursor += 2;
                dibujar_taller(taller_cursor);
            }
            if (keys & KEY_UP) {
                if (taller_cursor >= 2) taller_cursor -= 2;
                dibujar_taller(taller_cursor);
            }
            if ((keys & KEY_A) && taller_n > 0) {
                // Mostrar el ópalo
                mostrar_opalo_chunk(&taller_slots[taller_cursor]);

                // Buscar en SRAM por seed y marcar cortado
                Chunk todos[MAX_CAPTURAS];
                int n = cargar_chunks(todos);
                for (int i = 0; i < n; i++) {
                    if (todos[i].seed == taller_slots[taller_cursor].seed) {
                        todos[i].cortado = 1;
                        sobreescribir_chunk(i, &todos[i]);
                        break;
                    }
                }

                // Esperar START para volver
                while (1) {
                    scanKeys();
                    if (keysDown() & KEY_START) break;
                }

                // Recargar taller sin el chunk recién cortado
                taller_recargar();
                if (taller_cursor >= taller_n)
                    taller_cursor = taller_n > 0 ? taller_n - 1 : 0;

                estado = ESTADO_TALLER;
                dibujar_taller(taller_cursor);
            }
            if (keys & KEY_START) {
                estado = ESTADO_MENU;
                dibujar_menu(opcion_menu);
                flip();
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
