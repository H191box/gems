#include <stdint.h>
#include <gba_video.h>

#include "video.h"
#include "font.h"

// -------------------------------------------------------
// Paleta fija del menu — se aplica cada vez que se dibuja
// para no depender de lo que haya dejado generar_paleta()
//
// Indices usados:
//   0   = negro (fondo)
//   1   = azul oscuro (banda titulo)
//   2   = gris oscuro (boton inactivo)
//   3   = morado (boton activo / seleccionado)
//   4   = gris medio (separador / decoracion)
//   255 = blanco (texto)
// -------------------------------------------------------

static void aplicar_paleta_menu(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[  0] = 0x0000;  // negro
    pal[  1] = 0x6810;  // azul oscuro
    pal[  2] = 0x294A;  // gris oscuro
    pal[  3] = 0x681F;  // morado/violeta
    pal[  4] = 0x4210;  // gris medio
    pal[255] = 0x7FFF;  // blanco
}

static void clear(uint16_t* vram, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    for (int i = 0; i < 19200; i++) vram[i] = packed;
}

static void rect(uint16_t* vram, int x, int y, int w, int h, uint8_t color) {
    uint16_t packed = (color << 8) | color;
    int x0 = x & ~1;
    int x1 = (x + w + 1) & ~1;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x0; xx < x1; xx += 2)
            vram[yy * 120 + xx / 2] = packed;
}

// Linea horizontal de 1px
static void hline(uint16_t* vram, int x, int y, int w, uint8_t color) {
    rect(vram, x, y, w, 1, color);
}

void dibujar_menu(int opcion) {
    aplicar_paleta_menu();

    uint16_t* vram = get_vram();
    clear(vram, 0);

    // ---- Banda de titulo ----
    rect(vram, 0, 0, 240, 36, 1);
    hline(vram, 0, 36, 240, 4);  // linea separadora
    draw_text(vram, 74, 6,  "PLASMA",   255);
    draw_text(vram, 50, 18, "GENERADOR DE FONDOS", 255);

    // ---- Opcion 0: FARMEO ----
    uint8_t col0 = (opcion == 0) ? 3 : 2;
    rect(vram, 16, 48, 208, 44, col0);
    hline(vram, 16, 92, 208, 4);  // sombra inferior
    draw_text(vram, 24, 54, "FARMEO",                        255);
    draw_text(vram, 24, 66, "GENERA Y GUARDA FONDOS",        255);
    draw_text(vram, 24, 78, "B:MUTAR  A:GUARDAR  START:MENU",255);

    // ---- Opcion 1: GALERIA ----
    uint8_t col1 = (opcion == 1) ? 3 : 2;
    rect(vram, 16, 100, 208, 44, col1);
    hline(vram, 16, 144, 208, 4);  // sombra inferior
    draw_text(vram, 24, 106, "GALERIA",                       255);
    draw_text(vram, 24, 118, "VER FONDOS GUARDADOS",          255);
    draw_text(vram, 24, 130, "CRUZ:MOVER  A:VER  B:VOLVER",   255);

    // ---- Instruccion global ----
    draw_text(vram, 56, 152, "ARRIBA/ABAJO PARA MOVER", 255);
}
