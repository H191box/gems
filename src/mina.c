#include "mina.h"
#include "data.h"
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "video.h"
#include "font.h"
#include "save.h"
#include "game_state.h"
#include "menu.h"
#include "plasma.h"
#include "opalo.h"

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
    { "MINA T1", 100,  15,  0, "PROBABILIDAD NORMAL", "MINA BASICA"      },
    { "MINA T2", 10000,  12, 15, "MAS OPALOS RAROS",    "MAS CHUNKS GRANDES"},
    { "MINA T3", 50000, 10, 30, "ALTA CALIDAD",        "MAXIMA SUERTE"    },
};

// ----------------------------------------------------
// VARIABLES
// ----------------------------------------------------
static int      cursor           = 0;
static int      tiradas_restantes = 0;
static int      bonus_actual      = 0;
static int procesando_transicion = 0; // Bloqueo de seguridad
static uint32_t semilla_actual;
static Chunk    chunk_actual;
static Opalo    opalo_actual;

extern EstadoJuego estado;

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
// HELPERS FARMEO
// ----------------------------------------------------
static void refrescar_chunk(void) {
    generar_chunk(&chunk_actual, semilla_actual);
    generar_opalo(&opalo_actual, semilla_actual);
    aplicar_bonus_region(&opalo_actual, &chunk_actual);

    chunk_actual.dia       = dia_actual;
    chunk_actual.mes       = mes_actual;
    chunk_actual.ciudad_id = ciudad_actual_idx;

    renderizar_roca(&chunk_actual);

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
    sprintf(buf, "PRECIO: %d",  MINAS[cursor].precio);  draw_text(vram, x, 28,  buf, 255);
    sprintf(buf, "TIRADAS: %d", MINAS[cursor].tiradas); draw_text(vram, x, 44,  buf, 255);
    sprintf(buf, "BONUS: %d",   MINAS[cursor].bonus);   draw_text(vram, x, 60,  buf, 255);
    draw_text(vram, x, 90,  (char*)MINAS[cursor].desc1, 255);
    draw_text(vram, x, 104, (char*)MINAS[cursor].desc2, 255);
    draw_text(vram, x, 132, "A: ENTRAR",    255);
    draw_text(vram, x, 146, "START: MENU",  255);
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


	fade_out();
    // Dibujar primer chunk inmediatamente — sin bug de frame viejo
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
    if (keys & KEY_START) {
        volver_menu();
        return;
    }
    if ((keys & KEY_DOWN) && cursor < 2) { cursor++; dibujar_mina(); }
    if ((keys & KEY_UP)   && cursor > 0) { cursor--; dibujar_mina(); }
    if (keys & KEY_A) {
        entrar_mina(cursor);
        if (estado != ESTADO_FARMEO) dibujar_mina(); // sin dinero: redibujar
    }
}

// ----------------------------------------------------
// INPUT FARMEO
// ----------------------------------------------------



void farmeo_input(uint16_t keys) {
    // Si estamos en medio de una transición, ignoramos cualquier pulsación
    if (procesando_transicion) return;

    if (tiradas_restantes <= 0) {
        volver_menu();
        return;
    }

    int accion = 0; // 0: nada, 1: B (pasar), 2: A (guardar y pasar)

    // Detectar qué botón se ha pulsado
    if (keys & KEY_B) {
        accion = 1;
    } else if (keys & KEY_A) {
        if (chunk_actual.grietas > 0) {
            accion = 2;
        }
    } else if (keys & KEY_START) {
        procesando_transicion = 1; // Bloqueamos input
        fade_out();
        volver_menu();
        // No hace falta desbloquear porque volvemos al menú
        return;
    }

    // Ejecutar el cambio con FADE si hubo acción
    if (accion > 0) {
        procesando_transicion = 1; // 1. BLOQUEAR INPUT
        
        fade_out(); // 2. Oscurece y limpia buffers

        // 3. Realizar lógica
        if (accion == 1) {
            semilla_actual += 0x9E3779B9;
            mina_gastar_tirada();
        } else {
            guardar_seed(semilla_actual);
            guardar_chunk_taller(&chunk_actual);
            mina_gastar_tirada();
            semilla_actual += 0x9E3779B9;
        }

        // 4. Comprobar si se acabó el juego
        if (tiradas_restantes <= 0) {
            // Ya estamos en fade_out, mostramos agotado
            mostrar_agotado();
            procesando_transicion = 0;
            volver_menu_con_fade();
            // El desbloqueo ocurrirá en el siguiente estado
        } else {
            // 5. Redibujar con la nueva paleta y chunk
            refrescar_chunk(); 
            // 6. Entrar desde negro
            fade_in();
            
            procesando_transicion = 0; // 7. DESBLOQUEAR INPUT
        }
    }
}
