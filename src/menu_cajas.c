/* menu_cajas.c
   Gestor de cajas — editor de filtros 6×6 con texto.
   Navegación: DPAD mover cursor, A toggle, R toggle columna,
               SELECT toggle fila, L limpiar todo, START guardar, B cancelar.
*/
#include <stdint.h>
#include <stdio.h>
#include <gba_input.h>
#include "menu_cajas.h"
#include "caja_filtro.h"
#include "save.h"
#include "galeria.h"
#include "font.h"
#include "video.h"
#include "render.h"
#include "menu.h"
/* Nombres cortos para la matriz — 4 chars máximo para que quepan */
static const char *NOMBRE_TIPO_CORTO[CF_NUM_TIPOS] = {
    "NGR", "CRI", "FUE", "BLA", "ROS", "GRS"
};

static const char *NOMBRE_PATRON_CORTO[CF_NUM_PATRONES] = {
    "NEB", "VEN", "MAT", "MOS", "CHA", "HAR"
};

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    MC_LISTA,    /* lista de 8 cajas con conteo */
    MC_EDITOR    /* editor de matriz 6×6        */
} EstadoMenuCajas;

static EstadoMenuCajas estado;
static CajaFiltro cajas[NUM_CAJAS];
static CajaFiltro caja_editando;   /* copia de trabajo */
static int caja_sel;               /* caja seleccionada en lista */
static int cur_tipo;               /* cursor editor fila   */
static int cur_patron;             /* cursor editor columna */
static uint16_t keys_old;

/* ------------------------------------------------------------------ */
/* Render lista                                                        */
/* ------------------------------------------------------------------ */

static void render_lista_cajas(void)
{
    uint16_t *vram = get_vram();
    clear(vram, 0);

    draw_text(vram, 4, 2,  "GESTOR DE CAJAS",   255);
    draw_text(vram, 4, 12, "A:editar  B:volver", 6);

    for (int c = 0; c < NUM_CAJAS; c++) {
        char buf[32];
        const char *estado_filtro = caja_filtro_vacia(&cajas[c])
                                    ? "SIN FILTRO" : "ACTIVO";
        snprintf(buf, sizeof(buf), "CAJA %d  [%s]", c + 1, estado_filtro);

        uint8_t color = (c == caja_sel) ? 255 : 6;
        draw_text(vram, 6, 20 + c * 14, buf, color);
    }

    vsync_only();
    flip_no_vsync();
}

/* ------------------------------------------------------------------ */
/* Render editor                                                       */
/* ------------------------------------------------------------------ */

static void render_editor(void)
{
    uint16_t *vram = get_vram();
    clear(vram, 0);

    char buf[48];
    snprintf(buf, sizeof(buf), "CAJA %d", caja_sel + 1);
    draw_text(vram, 4,  2, buf, 255);
    draw_text(vram, 4, 12, "A:tog R:col SEL:fila L:reset", 6);
    draw_text(vram, 4, 20, "START:guardar  B:cancelar",    6);

    /* Cabeceras de columna (patrones) — cada celda ocupa 18px */
    for (int p = 0; p < CF_NUM_PATRONES; p++) {
        uint8_t color = (cur_patron == p) ? 255 : 6;
        draw_text(vram, 46 + p * 18, 32, (char*)NOMBRE_PATRON_CORTO[p], color);
    }

    /* Filas (tipos) + celdas */
    for (int t = 0; t < CF_NUM_TIPOS; t++) {
        uint8_t color_fila = (cur_tipo == t) ? 255 : 6;
        draw_text(vram, 2, 44 + t * 16, (char*)NOMBRE_TIPO_CORTO[t], color_fila);

        for (int p = 0; p < CF_NUM_PATRONES; p++) {
            int activo = caja_filtro_activo(&caja_editando, t, p);
            int es_cur = (t == cur_tipo && p == cur_patron);
            const char *celda = activo ? "[X]" : "[ ]";
            uint8_t color = es_cur ? 255 : (activo ? 12 : 3);
            draw_text(vram, 46 + p * 18, 44 + t * 16, (char*)celda, color);
        }
    }

    vsync_only();
    flip_no_vsync();
}
/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

void menu_cajas_init(void)
{
    cargar_cajas(cajas);
    caja_sel   = 0;
    estado     = MC_LISTA;
    keys_old   = 0;
    render_lista_cajas();
}

void menu_cajas_input(uint16_t keys)
{
    uint16_t pressed = keys & ~keys_old;
    keys_old = keys;

    if (estado == MC_LISTA) {
        if (pressed & KEY_UP)   { if (caja_sel > 0) caja_sel--; render_lista_cajas(); }
        if (pressed & KEY_DOWN) { if (caja_sel < NUM_CAJAS - 1) caja_sel++; render_lista_cajas(); }
        if (pressed & KEY_A) {
            caja_editando = cajas[caja_sel];
            cur_tipo   = 0;
            cur_patron = 0;
            estado = MC_EDITOR;
            render_editor();
        }
        if (pressed & KEY_B) {
            /* Volver al menú principal — la galería se recarga por si acaso */
            galeria_recargar_caja_activa();
            volver_menu_con_fade();   /* o la función equivalente en menu.c */
        }

    } else { /* MC_EDITOR */
        if (pressed & KEY_UP)    { if (cur_tipo   > 0) cur_tipo--;   else cur_tipo   = CF_NUM_TIPOS    - 1; render_editor(); }
        if (pressed & KEY_DOWN)  { if (cur_tipo   < CF_NUM_TIPOS    - 1) cur_tipo++;   else cur_tipo   = 0; render_editor(); }
        if (pressed & KEY_LEFT)  { if (cur_patron > 0) cur_patron--; else cur_patron = CF_NUM_PATRONES - 1; render_editor(); }
        if (pressed & KEY_RIGHT) { if (cur_patron < CF_NUM_PATRONES - 1) cur_patron++; else cur_patron = 0; render_editor(); }

        if (pressed & KEY_A) {
            /* Toggle celda individual */
            if (caja_filtro_activo(&caja_editando, cur_tipo, cur_patron))
                caja_filtro_desactivar(&caja_editando, cur_tipo, cur_patron);
            else
                caja_filtro_activar(&caja_editando, cur_tipo, cur_patron);
            render_editor();
        }
        if (pressed & KEY_R) {
            caja_filtro_toggle_columna(&caja_editando, cur_patron);
            render_editor();
        }
        if (pressed & KEY_SELECT) {
            caja_filtro_toggle_fila(&caja_editando, cur_tipo);
            render_editor();
        }
        if (pressed & KEY_L) {
            caja_filtro_reset(&caja_editando);
            render_editor();
        }
        if (pressed & KEY_START) {
            /* Guardar — persistir en SRAM y actualizar array local */
            cajas[caja_sel] = caja_editando;
            guardar_cajas(cajas);
            galeria_recargar_caja_activa();
            estado = MC_LISTA;
            render_lista_cajas();
        }
        if (pressed & KEY_B) {
            /* Cancelar sin guardar */
            estado = MC_LISTA;
            render_lista_cajas();
        }
    }
}
