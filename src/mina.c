// mina.c

#include "mina.h"

#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>

#include "video.h"
#include "font.h"
#include "save.h"
#include "game_state.h"

// ----------------------------------------------------
// CONFIG
// ----------------------------------------------------

#define LIST_W       115
#define LIST_ITEM_H   20

typedef struct {
    const char* nombre;
    int precio;
    int tiradas;
    int bonus;
    const char* desc1;
    const char* desc2;
} MinaInfo;

// bonus:
// 0  = normal
// 15 = mejor
// 30 = muy buena

static MinaInfo MINAS[3] = {
    {
        "MINA T1",
        100,
        15,
        0,
        "PROBABILIDAD NORMAL",
        "MINA BASICA"
    },

    {
        "MINA T2",
        350,
        12,
        15,
        "MAS OPALOS RAROS",
        "MAS CHUNKS GRANDES"
    },

    {
        "MINA T3",
        1000,
        10,
        30,
        "ALTA CALIDAD",
        "MAXIMA SUERTE"
    }
};

// ----------------------------------------------------
// VARIABLES
// ----------------------------------------------------

static int cursor = 0;

static int tiradas_restantes = 0;
static int bonus_actual      = 0;

// vienen de main.c
extern EstadoJuego estado;
extern int opcion_menu;

// ----------------------------------------------------
// GETTERS
// ----------------------------------------------------

int mina_obtener_tiradas(void) {
    return tiradas_restantes;
}

int mina_obtener_bonus(void) {
    return bonus_actual;
}


void mina_gastar_tirada(void) {

    if (tiradas_restantes > 0) {
        tiradas_restantes--;
    }
}



// ----------------------------------------------------
// UI
// ----------------------------------------------------

static void dibujar_mina(void) {

    uint16_t* vram = get_vram();

    clear(vram, 0);

    // cabecera
    rect(vram, 0, 0, 240, 16, 4);

    draw_text(vram, 5, 4, "MINAS", 255);

    char oro[32];
    sprintf(oro, "ORO:%d", obtener_dinero());
    draw_text(vram, 160, 4, oro, 255);

    // divisor
    vline(vram, LIST_W, 16, 144, 3);

    // lista izquierda
    for (int i = 0; i < 3; i++) {

        int y = 22 + (i * LIST_ITEM_H);

        if (i == cursor) {
            fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, 2);
        }

        draw_text(vram, 5, y + 4, (char*)MINAS[i].nombre, 255);
    }

    // descripción derecha
    int x = LIST_W + 8;

    char buf[64];

    sprintf(buf, "PRECIO: %d", MINAS[cursor].precio);
    draw_text(vram, x, 28, buf, 255);

    sprintf(buf, "TIRADAS: %d", MINAS[cursor].tiradas);
    draw_text(vram, x, 44, buf, 255);

    sprintf(buf, "BONUS: %d", MINAS[cursor].bonus);
    draw_text(vram, x, 60, buf, 255);

    draw_text(vram, x, 90,  (char*)MINAS[cursor].desc1, 255);
    draw_text(vram, x, 104, (char*)MINAS[cursor].desc2, 255);

    draw_text(vram, x, 132, "A: ENTRAR", 255);
    draw_text(vram, x, 146, "START: MENU", 255);

    flip();
}

// ----------------------------------------------------
// ENTRAR A MINA
// ----------------------------------------------------

static void entrar_mina(int idx) {

    MinaInfo* m = &MINAS[idx];

    if (obtener_dinero() < m->precio) {
        return;
    }

    modificar_dinero(-m->precio);

    tiradas_restantes = m->tiradas;
    bonus_actual      = m->bonus;

    estado = ESTADO_FARMEO;
}

// ----------------------------------------------------
// API
// ----------------------------------------------------

void mina_init(void) {

    cursor = 0;

    dibujar_mina();
}

void mina_input(uint16_t keys) {

    if ((keys & KEY_DOWN) && cursor < 2) {
        cursor++;
        dibujar_mina();
    }

    if ((keys & KEY_UP) && cursor > 0) {
        cursor--;
        dibujar_mina();
    }

    if (keys & KEY_A) {

        entrar_mina(cursor);

        if (estado != ESTADO_FARMEO) {
            dibujar_mina();
        }
    }
}
