#include <stdint.h>
#include <gba_video.h>
#include "video.h"
#include "font.h"

// -------------------------------------------------------
// Paleta fija del menu
//
// 0   = negro (fondo)
// 1   = azul oscuro (banda titulo)
// 2   = gris oscuro (boton inactivo)
// 3   = morado (boton activo)
// 4   = gris medio (separador)
// 255 = blanco (texto)
// -------------------------------------------------------
static void aplicar_paleta_menu(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[  0] = 0x0000; // negro
    pal[  1] = 0x6810; // azul oscuro
    pal[  2] = 0x294A; // gris oscuro
    pal[  3] = 0x681F; // morado/violeta
    pal[  4] = 0x4210; // gris medio
    pal[255] = 0x7FFF; // blanco
}

static void clear(uint16_t* vram, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    for (int i = 0; i < 19200; i++) vram[i] = packed;
}

static void rect(uint16_t* vram,
                 int x, int y, int w, int h,
                 uint8_t color) {
    uint16_t packed = (color << 8) | color;
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x0; xx < x1; xx += 2)
            vram[yy * 120 + xx / 2] = packed;
}

static void hline(uint16_t* vram, int x, int y, int w, uint8_t color) {
    rect(vram, x, y, w, 1, color);
}

// -------------------------------------------------------
// Layout con 3 opciones en 160px:
//
//  0- 28  banda titulo (28px)
// 29- 29  separador
// 30- 67  opcion 0: FARMEO    (38px)
// 68- 68  separador
// 69-106  opcion 1: TALLER    (38px)
//107-107  separador
//108-145  opcion 2: GALERIA   (38px)
//146-159  instruccion global  (14px)
// -------------------------------------------------------

void dibujar_menu(int opcion) {
    aplicar_paleta_menu();
    uint16_t* vram = get_vram();
    clear(vram, 0);

    // ---- Banda de titulo ----
    rect(vram,  0,  0, 240, 28, 1);
    hline(vram, 0, 28, 240,  4);
    draw_text(vram, 84,  6, "GEMS",          255);
    draw_text(vram, 44, 17, "GACHA DE GEMAS", 255);

    // ---- Opcion 0: FARMEO ----
    uint8_t col0 = (opcion == 0) ? 3 : 2;
    rect(vram, 8, 30, 224, 37, col0);
    hline(vram, 8, 67, 224, 4);
    draw_text(vram, 16, 34, "FARMEO",                    255);
    draw_text(vram, 16, 44, "BUSCA ROCAS CON GRIETAS",   255);
    draw_text(vram, 16, 54, "B:NUEVA  A:GUARDAR",        255);

    // ---- Opcion 1: TALLER ----
    uint8_t col1 = (opcion == 1) ? 3 : 2;
    rect(vram, 8, 69, 224, 37, col1);
    hline(vram, 8, 106, 224, 4);
    draw_text(vram, 16,  73, "TALLER",                   255);
    draw_text(vram, 16,  83, "CORTA TUS CHUNKS",         255);
    draw_text(vram, 16,  93, "CRUZ:ELEGIR  A:CORTAR",    255);

    // ---- Opcion 2: GALERIA ----
    uint8_t col2 = (opcion == 2) ? 3 : 2;
    rect(vram, 8, 108, 224, 37, col2);
    hline(vram, 8, 145, 224, 4);
    draw_text(vram, 16, 112, "GALERIA",                  255);
    draw_text(vram, 16, 122, "VER OPALES REVELADOS",     255);
    draw_text(vram, 16, 132, "CRUZ:MOVER  A:VER",        255);

    // ---- Instruccion global ----
    draw_text(vram, 36, 149, "ARRIBA/ABAJO PARA MOVER",  255);
}
