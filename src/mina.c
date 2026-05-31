#include "mina.h"
#include "data.h"
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>
#include "video.h"
#include "font.h"
#include "save.h"
#include "game_state.h"
#include "menu.h"
#include "plasma.h"
#include "opalo.h"
#include "gema.h"
#include "gema_render.h"
#include "render.h"
#include "ciudades.h"

// ----------------------------------------------------
// CONFIG
// ----------------------------------------------------
#define LIST_W      115
#define LIST_ITEM_H  20

typedef struct {
    const char* nombre;
    int precio;
    int tiradas;
    int bonus;
    const char* desc1;
    const char* desc2;
} MinaInfo;

static MinaInfo MINAS[3] = {
    { "MINA T1",  100,   15,  0, "PROBABILIDAD NORMAL", "MINA BASICA"       },
    { "MINA T2",  10000, 12, 15, "MAS OPALOS RAROS",    "MAS CHUNKS GRANDES"},
    { "MINA T3",  50,    10, 30, "ALTA CALIDAD",        "MAXIMA SUERTE"     },
};

// ----------------------------------------------------
// VARIABLES
// ----------------------------------------------------
static int      cursor                = 0;
static int      tiradas_restantes     = 0;
static int      bonus_actual          = 0;
static int      procesando_transicion = 0;
static uint32_t semilla_actual;
static Chunk    chunk_actual;

/* opalo_actual eliminado — solo se usaba para aplicar_bonus_region
   y para pasar a renderizar_roca. Ahora renderizamos vía Gema temporal. */

extern EstadoJuego estado;

// ----------------------------------------------------
// HELPER: Chunk → Gema temporal solo para render de roca
//
// No se persiste. ID = 0 indica temporal.
// La etapa queda en BRUTA — es exactamente lo que
// necesita renderizar_roca para mostrar la piedra sin cortar.
// ----------------------------------------------------
static void chunk_a_gema_temp(Gema *g, const Chunk *c)
{
    uint8_t bioma = 0;
    if (c->ciudad_id >= 0 && c->ciudad_id < NUM_TOTAL_CIUDADES)
        bioma = ciudades[c->ciudad_id].bioma_id;
    crear_gema_desde_chunk(g, c, 0, bioma);
    /* etapa BRUTA por defecto — renderizar_roca muestra la piedra */
}

// ----------------------------------------------------
// GETTERS
// ----------------------------------------------------
int mina_obtener_tiradas(void) { return tiradas_restantes; }
int mina_obtener_bonus(void)   { return bonus_actual; }

void mina_gastar_tirada(void) {
    if (tiradas_restantes > 0) tiradas_restantes--;
}

void mina_init_semilla(uint32_t semilla) {
    semilla_actual = semilla;
    generar_chunk(&chunk_actual, semilla_actual);
}

// ----------------------------------------------------
// MEZCLAR SEMILLA CON ENTROPÍA DEL MOMENTO
// ----------------------------------------------------
static uint32_t mezclar_semilla_con_timer(uint32_t base) {
    uint16_t t = REG_TM0CNT_L;
    uint16_t v = REG_VCOUNT;
    uint32_t s = base ^ ((uint32_t)t << 16) ^ ((uint32_t)v << 8) ^ t;
    s ^= s >> 16;
    s *= 0x45d9f3b;
    s ^= s >> 16;
    if (s == 0) s = 0xBEEF0001;
    return s;
}

// ----------------------------------------------------
// HELPERS FARMEO
// ----------------------------------------------------
static void refrescar_chunk(void)
{
    /* 1. Generar chunk y aplicar bonus de región (flujo sin cambios) */
    generar_chunk(&chunk_actual, semilla_actual);

    {
        /* Opalo temporal solo para aplicar_bonus_region —
           modifica chunk_actual en sitio, luego se descarta */
        Opalo o_tmp;
        generar_opalo(&o_tmp, semilla_actual);
        aplicar_bonus_region(&o_tmp, &chunk_actual);
    }

    chunk_actual.dia       = dia_actual;
    chunk_actual.mes       = mes_actual;
    chunk_actual.ciudad_id = ciudad_actual_idx;

    /* 2. Render: inflar chunk a Gema temporal y pasar a renderizar_roca */
    Gema g_tmp;
    chunk_a_gema_temp(&g_tmp, &chunk_actual);

    Opalo o_pal;
    gema_a_opalo_temp(&o_pal, &g_tmp);
    generar_paleta(&o_pal);

    renderizar_roca(&g_tmp);
}

static void mostrar_agotado(void) {
    uint16_t* vram = get_vram();
    clear(vram, 0);
    draw_text(vram, 82, 70, "AGOTADO", 255);
    flip();
    for (volatile int i = 0; i < 300000; i++);
}

// ----------------------------------------------------
// UI SELECCIÓN DE MINA
// ----------------------------------------------------
static void dibujar_mina(void) {
    uint16_t* vram = get_vram();
    clear(vram, 0);

    rect(vram, 0, 0, 240, 16, 4);
    draw_text(vram, 5, 4, "MINAS", 255);

    char oro[32];
    sprintf(oro, "ORO:%d", obtener_dinero());
    draw_text(vram, 160, 4, oro, 255);

    vline(vram, LIST_W, 16, 144, 3);

    for (int i = 0; i < 3; i++) {
        int y = 22 + (i * LIST_ITEM_H);
        if (i == cursor)
            fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, 2);
        draw_text(vram, 5, y + 4, (char*)MINAS[i].nombre, 255);
    }

    int x = LIST_W + 8;
    char buf[64];
    sprintf(buf, "PRECIO: %d",  MINAS[cursor].precio);  draw_text(vram, x,  28, buf, 255);
    sprintf(buf, "TIRADAS: %d", MINAS[cursor].tiradas); draw_text(vram, x,  44, buf, 255);
    sprintf(buf, "BONUS: %d",   MINAS[cursor].bonus);   draw_text(vram, x,  60, buf, 255);
    draw_text(vram, x,  90, (char*)MINAS[cursor].desc1, 255);
    draw_text(vram, x, 104, (char*)MINAS[cursor].desc2, 255);
    draw_text(vram, x, 132, "A: ENTRAR",   255);
    draw_text(vram, x, 146, "START: MENU", 255);
    flip();
}

// ----------------------------------------------------
// ENTRAR A MINA
// ----------------------------------------------------
static void entrar_mina(int idx) {
    if (obtener_dinero() < MINAS[idx].precio) return;

    modificar_dinero(-MINAS[idx].precio);
    tiradas_restantes = MINAS[idx].tiradas;
    bonus_actual      = MINAS[idx].bonus;
    estado            = ESTADO_FARMEO;

    semilla_actual = mezclar_semilla_con_timer(semilla_actual);
    guardar_seed(semilla_actual);

    fade_out();
    refrescar_chunk();
    fade_in();
}

// ----------------------------------------------------
// INPUT SELECCIÓN DE MINA
// ----------------------------------------------------
void mina_init(void) {
    cursor = 0;
    procesando_transicion = 0;
    dibujar_mina();
}

void mina_input(uint16_t keys) {
    if (keys & KEY_START) { volver_menu(); return; }
    if ((keys & KEY_DOWN) && cursor < 2) { cursor++; dibujar_mina(); }
    if ((keys & KEY_UP)   && cursor > 0) { cursor--; dibujar_mina(); }
    if (keys & KEY_A) {
        entrar_mina(cursor);
        if (estado != ESTADO_FARMEO) dibujar_mina();
    }
}

// ----------------------------------------------------
// INPUT FARMEO
// ----------------------------------------------------
void farmeo_input(uint16_t keys) {
    if (procesando_transicion) return;

    if (tiradas_restantes <= 0) {
        procesando_transicion = 1;
        volver_menu_con_fade();
        return;
    }

    int accion = 0; /* 0: nada, 1: B (pasar), 2: A (guardar y pasar) */

    if (keys & KEY_B) {
        accion = 1;
    } else if (keys & KEY_A) {
        if (chunk_actual.grietas > 0) accion = 2;
    } else if (keys & KEY_START) {
        procesando_transicion = 1;
        fade_out();
        volver_menu();
        return;
    }

    if (accion > 0) {
        procesando_transicion = 1;
        fade_out();

        if (accion == 1) {
            semilla_actual += 0x9E3779B9;
            mina_gastar_tirada();
        } else {
            guardar_seed(semilla_actual);
            guardar_chunk_taller(&chunk_actual);
            mina_gastar_tirada();
            semilla_actual += 0x9E3779B9;
        }

        if (tiradas_restantes <= 0) {
            mostrar_agotado();
            procesando_transicion = 0;
            volver_menu_con_fade();
        } else {
            refrescar_chunk();
            fade_in();
            procesando_transicion = 0;
        }
    }
}
