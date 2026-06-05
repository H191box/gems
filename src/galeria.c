#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <gba_video.h>
#include <gba_input.h>

#include "galeria.h"
#include "save.h"
#include "plasma.h"
#include "render.h"
#include "video.h"
#include "font.h"

#include "gema.h"
#include "gema_render.h"
#include "thumb_cache.h"
#include "ciudades.h"
#include "menu.h"

// ============================================================
// CONSTANTES UI
// ============================================================

#define LIST_W       115
#define LIST_ITEM_H   14
#define LIST_ITEMS     8

#define THUMB_X      125
#define THUMB_Y       18

// ============================================================
// TIPOS
// ============================================================

typedef enum {
    GAL_G1 = 0,
    GAL_G2 = 1,
    GAL_G3 = 2
} GaleriaID;

// ============================================================
// VARIABLES GLOBALES ESTÁTICAS
// ============================================================

static GaleriaID    galeria_activa  = GAL_G1;
static VistaGaleria vista           = VISTA_LISTA;

// G1
static Gema  items[MAX_GALERIA];
static int   num_items;
static int   cursor;
static int   scroll;
static int   opcion_submenu;

// G2
static Gema  g2_items[MAX_GALERIA2];
static int   g2_num;
static int   g2_cursor;

// G3
static Gema  g3_items[MAX_GALERIA3];
static int   g3_num;
static int   g3_cursor;

extern uint8_t grieta_buf[160][240];

static void limpiar_buffer_render(void) {
    memset(grieta_buf, 0, sizeof(grieta_buf));
}

// ============================================================
// HELPERS PALETA
// ============================================================

// Escribe la paleta UI base.
// REGLA: llamar SIEMPRE justo antes de flip_no_vsync(), nunca
// en medio del render ni desde subfunciones. Un único punto de
// escritura por frame elimina los flashes de paleta.
static void init_paleta_ui(void)
{
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;

    // Índice 0: backdrop de la GBA. Lo igualamos al beige oscuro del fondo
    // para que el hardware no muestre negro en los bordes del modo 4.
    pal[0]   = 27 | (25 << 5) | (21 << 10); // beige oscuro (mismo que IDX_FONDO_OSCURO)
    pal[1]   = 0x294A; // Fondo de items (Gris azulado oscuro)
    pal[2]   = 0x681F; // Color de selección (Verde/Azul brillante)
    pal[3]   = 0x4210; // Líneas de división (Gris oscuro)
    pal[4]   = 0x6810; // Barra superior

    // Índices 5 y 6: dithering del fondo (render.c los usa como IDX_FONDO_OSCURO/CLARO)
    pal[5]   = 27 | (25 << 5) | (21 << 10); // IDX_FONDO_OSCURO — beige oscuro
    pal[6]   = 30 | (29 << 5) | (25 << 10); // IDX_FONDO_CLARO  — beige claro

    // 7-14: libres, a cero
    for (int i = 7; i <= 14; i++) {
        pal[i] = 0x0000;
    }

    // 15: borde de gema (lo deja generar_paleta_gema; aquí ponemos un valor neutro
    // que no cause flash si se muestra antes de que generar_paleta_gema lo sobrescriba)
    pal[15]  = 0x7FFF;

    // 254-255: brillo/texto — coherentes con generar_paleta_gema
    pal[254] = 0x7FFF; // Blanco (destello 3D de la preview)
    pal[255] = 0x7FFF; // Blanco para fuentes/texto
}

static void clear_vram(uint16_t* vram)
{
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

// ============================================================
// NOMBRES
// ============================================================

static const char* NOMBRE_TIPO[6] = {
    "OPALO NEGRO",
    "OPALO CRISTAL",
    "OPALO FUEGO",
    "OPALO BLANCO",
    "OPALO ROSA",
    "OPALO GRIS"
};

// ============================================================
// NUEVAS ACCIONES DE GESTIÓN DE GEMAS (G1)
// ============================================================

static void mover_a_vitrina(int idx)
{
    if (num_items <= 0 || idx < 0 || idx >= num_items) return;
    if (g2_num >= MAX_GALERIA2) return;

    guardar_gema_galeria2(&items[idx]);

    g2_items[g2_num] = items[idx];
    g2_num++;

    for (int i = idx; i < num_items - 1; i++) {
        actualizar_gema_en_sram(i, &items[i + 1]);
        items[i] = items[i + 1];
    }

    decrementar_num_gemas();
    num_items--;

    if (cursor >= num_items && cursor > 0)
        cursor = num_items - 1;
    if (scroll > 0 && scroll + LIST_ITEMS > num_items)
        scroll--;
    if (scroll < 0) scroll = 0;
}

static void evolucionar_gema(int idx)
{
    if (num_items <= 0 || idx < 0 || idx >= num_items) return;

    if (items[idx].etapa < ETAPA_PULIDA) {
        items[idx].etapa++;
        actualizar_gema_en_sram(idx, &items[idx]);
    }
}

// ============================================================
// HELPERS VALOR
// ============================================================

static uint32_t valor_gema(int idx_g1)
{
    return gema_valor_estimado(&items[idx_g1]);
}

static uint32_t valor_g3(int idx)
{
    return gema_valor_estimado(&g3_items[idx]);
}

// ============================================================
// RENDER THUMB (G1)
// Renderiza la preview de la gema al buffer software y la vuelca
// sobre el back buffer. No toca la paleta — eso lo hace render_lista()
// justo antes del flip.
// ============================================================
static void render_thumb(int idx)
{
    limpiar_buffer_render();

    if (items[idx].etapa > ETAPA_PULIDA)
        return;

    // MATEMÁTICAS: Centro exacto del cuadrante superior derecho
    // Cuadrante: X[120-240], Y[0-80] -> Centro: X=180, Y=40
    int x_centrado = 180;
    int y_centrado = 40;

    // renderizar_gema_preview() renderiza al anim_buf_a y 
    // llama a volcar_buf_solo_opalo() internamente.
    renderizar_gema_preview(x_centrado, y_centrado, &items[idx]);
}
// ============================================================
// RENDER LISTA G1
// Orden estable:
//   1. Todo el contenido al back buffer (VRAM)
//   2. vsync_only()        — esperamos VBlank
//   3. init_paleta_ui()    — paleta en VBlank, cero flash
//   4. flip_no_vsync()     — page swap atómico con la paleta
// ============================================================
static void render_lista(void)
{
    uint16_t* vram = get_vram();
    limpiar_buffer_render();

    // --- 1. CONTENIDO AL BACK BUFFER ---

    dibujar_fondo_texturizado_optimizado(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA 1  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10, 70, "GALERIA VACIA", 255);
        draw_text(vram, 10, 85, "USA EL TALLER", 255);
        draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
        
        // --- 2-4. PALETA + FLIP (Caso vacío) ---
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_items) break;

        int y      = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char num[4] = "XX.";
        num[0] = '0' + ((idx + 1) / 10);
        num[1] = '0' + ((idx + 1) % 10);
        draw_text(vram, 2, y + 3, num, 255);

        int t = (int)gema_tipo(&items[idx]);
        if (t < 0 || t >= 6) t = 0;

        char etiqueta[24];
        if (items[idx].etapa == ETAPA_PULIDA)
            sprintf(etiqueta, "%s", NOMBRE_TIPO[t]);
        else if (items[idx].etapa == ETAPA_CORTADA)
            sprintf(etiqueta, "CABUJON %s", NOMBRE_TIPO[t]);
        else
            sprintf(etiqueta, "RAW %s", NOMBRE_TIPO[t]);

        draw_text(vram, 20, y + 3, etiqueta, 255);
    }

    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13, "^", 255);
    if (scroll + LIST_ITEMS < num_items)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (cursor < num_items) {
        // render_thumb vuelca al back buffer usando volcar_buf_solo_opalo()
        render_thumb(cursor);

        int x = LIST_W + 4;
        int y = 105;
        char buf[24];

        int t = (int)gema_tipo(&items[cursor]);
        if (t < 0 || t >= 6) t = 0;

        if (items[cursor].etapa == ETAPA_PULIDA) {
            draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
            sprintf(buf, "QUILATES: %d", items[cursor].quilates);
            draw_text(vram, x, y, buf, 255); y += 12;
            sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
            draw_text(vram, x, y, buf, 255); y += 14;
        } else if (items[cursor].etapa == ETAPA_CORTADA) {
            draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
            sprintf(buf, "PESO: %d", items[cursor].quilates);
            draw_text(vram, x, y, buf, 255); y += 12;
            sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
            draw_text(vram, x, y, buf, 255); y += 14;
        } else {
            draw_text(vram, x, y, "SIN IDENTIFICAR", 255); y += 14;
            sprintf(buf, "PESO: %d", items[cursor].quilates);
            draw_text(vram, x, y, buf, 255); y += 12;
            sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
            draw_text(vram, x, y, buf, 255); y += 14;
        }

        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista == VISTA_SUBMENU) {
        int sm_x = LIST_W + 6;
        int sm_y = 82;
        int sm_w = 113;
        int sm_h = 66;
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        draw_text(vram, sm_x + 6, sm_y + 6,
                  opcion_submenu == 0 ? "> VER FICHA" : "  VER FICHA", 255);

        char txt_evolucion[20];
        if (items[cursor].etapa == ETAPA_BRUTA) {
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> CORTAR" : "  CORTAR");
        } else if (items[cursor].etapa == ETAPA_CORTADA) {
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> PULIR" : "  PULIR");
        } else {
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> MAX EVOL" : "  MAX EVOL");
        }
        draw_text(vram, sm_x + 6, sm_y + 20, txt_evolucion, 255);

        draw_text(vram, sm_x + 6, sm_y + 34,
                  opcion_submenu == 2 ? "> A VITRINA" : "  A VITRINA", 255);

        draw_text(vram, sm_x + 6, sm_y + 48,
                  opcion_submenu == 3 ? "> VENDER" : "  VENDER", 255);
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);

    // --- 2-4. PALETA + FLIP ---
    // Todo el contenido está ya en el back buffer. Ahora, en VBlank,
    // escribimos la paleta y hacemos el page swap de forma atómica.
    vsync_only();
    init_paleta_ui();

    // CORRECCIÓN: Cargamos los colores procedurales del ópalo seleccionado en Palette RAM
    if (num_items > 0 && cursor < num_items) {
        generar_paleta_gema(&items[cursor]);
    }

    flip_no_vsync();
}

// ============================================================
// RENDER FICHA
// ============================================================

static void render_ficha(int idx)
{
    uint16_t* vram = get_vram();

    for (int y = 0; y < 160; y++)
        for (int x = 0; x < LIST_W; x += 2)
            vram[(y * 120) + (x / 2)] = 0;

    int y = 20;
    char buf[40];

    draw_text(vram, 5, y, "FICHA TECNICA", 255); y += 20;

    int t = (int)gema_tipo(&items[idx]);
    if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;
    draw_text(vram, 5, y, (char*)NOMBRE_TIPO[t], 255); y += 14;

    sprintf(buf, "BRILLO: %d", gema_brillo(&items[idx]));
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "PUREZA: %d", gema_pureza(&items[idx]));
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "PESO: %d K", items[idx].quilates);
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(idx));
    draw_text(vram, 5, y, buf, 255);

    draw_text(vram, 5, 140, "B: VOLVER", 255);

    // render_ficha no hace flip propio: render_lista() ya lo hizo antes
    // y render_ficha solo pinta la zona izquierda encima del mismo back buffer.
    // El flip definitivo lo hace galeria_input() tras render_lista() + render_ficha().
}

// ============================================================
// VENDER G1 -> G3
// ============================================================

static void vender_item_seleccionado(int idx)
{
    if (idx < 0 || idx >= num_items) return;

    uint32_t valor = valor_gema(idx);
    modificar_dinero((int32_t)valor);

    guardar_gema_galeria3(&items[idx]);

    for (int i = idx; i < num_items - 1; i++) {
        actualizar_gema_en_sram(i, &items[i + 1]);
        items[i] = items[i + 1];
    }
    decrementar_num_gemas();
    num_items--;

    if (cursor >= num_items && cursor > 0) cursor--;
    if (cursor < scroll) scroll = cursor;
    if (scroll + LIST_ITEMS > num_items && num_items > LIST_ITEMS)
        scroll = num_items - LIST_ITEMS;
    if (scroll < 0) scroll = 0;
}

// ============================================================
// RENDER G2 (Vitrina)
// ============================================================

#define G2_COLS     3
#define G2_CELL_W  72
#define G2_CELL_H  46
#define G2_OX       8
#define G2_OY      16

static void render_g2(void)
{
    uint16_t* vram = get_vram();

    dibujar_fondo_texturizado_optimizado(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA 2  L/R:CAMBIAR", 255);

    if (g2_num == 0) {
        draw_text(vram, 10, 70, "VITRINA VACIA", 255);
        draw_text(vram, 0, 152, "CURSOR:MOVER  START:MENU", 255);
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    for (int i = 0; i < g2_num; i++) {
        int col = i % G2_COLS;
        int row = i / G2_COLS;

        /* Centro exacto de la celda — renderizar_opalo_pequeno_celda
         * usa x_pos/y_pos como centro del cabujón, no como esquina. */
        int cx = G2_OX + col * G2_CELL_W + G2_CELL_W / 2;
        int cy = G2_OY + row * G2_CELL_H + G2_CELL_H / 2;

        /* Marco de selección alrededor de la celda completa */
        if (i == g2_cursor) {
            int fx = G2_OX + col * G2_CELL_W;
            int fy = G2_OY + row * G2_CELL_H;
            fill_rect(vram, fx, fy, G2_CELL_W - 2, G2_CELL_H - 2, 2);
        }

        /* Banco de paleta exclusivo para cada gema: 16 colores por slot */
        int num_colores = 16;
        int base_paleta = 16 + (i * num_colores);

        renderizar_gema_celda(cx, cy, &g2_items[i], base_paleta, num_colores);
    }

    draw_text(vram, 0, 152, "CURSOR:MOVER  START:MENU", 255);

    vsync_only();
    init_paleta_ui();

    /* Cargar paleta de cada gema en su banco asignado dentro de PALRAM */
    for (int i = 0; i < g2_num; i++) {
        int base_paleta = 16 + (i * 16);
        generar_paleta_gema_rango(&g2_items[i], base_paleta, 16);
    }

    flip_no_vsync();
}

// ============================================================
// RENDER G3 (Mercado)
// ============================================================

#define G3_COLS     3
#define G3_CELL_W  72
#define G3_CELL_H  46
#define G3_OX       8
#define G3_OY      16

static void render_g3(void)
{
    uint16_t* vram = get_vram();

    dibujar_fondo_texturizado_optimizado(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "MERCADO  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);

    if (g3_num == 0) {
        draw_text(vram, 10, 60, "MERCADO VACIO", 255);
        draw_text(vram, 10, 75, "VENDE DESDE G1", 255);
        draw_text(vram, 0, 152, "A:COMPRAR  START:MENU", 255);
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    for (int i = 0; i < g3_num; i++) {
        int col = i % G3_COLS;
        int row = i / G3_COLS;

        /* Centro exacto de la celda */
        int cx = G3_OX + col * G3_CELL_W + G3_CELL_W / 2;
        int cy = G3_OY + row * G3_CELL_H + G3_CELL_H / 2;

        if (i == g3_cursor) {
            int fx = G3_OX + col * G3_CELL_W;
            int fy = G3_OY + row * G3_CELL_H;
            fill_rect(vram, fx, fy, G3_CELL_W - 2, G3_CELL_H - 2, 2);
        }

        int num_colores = 16;
        int base_paleta = 16 + (i * num_colores);

        renderizar_gema_celda(cx, cy, &g3_items[i], base_paleta, num_colores);
    }

    if (g3_cursor < g3_num) {
        char buf[24];
        int t = (int)gema_tipo(&g3_items[g3_cursor]);
        if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;
        draw_text(vram, 4, 130, (char*)NOMBRE_TIPO[t], 255);
        sprintf(buf, "PRECIO: %u", (unsigned int)valor_g3(g3_cursor));
        draw_text(vram, 4, 142, buf, 255);
    }

    draw_text(vram, 0, 152, "A:COMPRAR  START:MENU", 255);

    vsync_only();
    init_paleta_ui();

    /* Cargar paleta de cada gema en su banco asignado dentro de PALRAM */
    for (int i = 0; i < g3_num; i++) {
        int base_paleta = 16 + (i * 16);
        generar_paleta_gema_rango(&g3_items[i], base_paleta, 16);
    }

    flip_no_vsync();
}

// ============================================================
// COMPRAR DESDE G3 -> G1
// ============================================================

static void comprar_desde_g3(int idx)
{
    if (idx < 0 || idx >= g3_num) return;

    uint32_t valor  = valor_g3(idx);
    uint32_t dinero = obtener_dinero();
    if (dinero < valor) return;

    modificar_dinero(-(int32_t)valor);

    guardar_gema(&g3_items[idx]);

    for (int i = idx; i < g3_num - 1; i++) {
        actualizar_gema_galeria3(i, &g3_items[i + 1]);
        g3_items[i] = g3_items[i + 1];
    }
    decrementar_num_gemas_galeria3();
    g3_num--;

    if (g3_cursor >= g3_num && g3_cursor > 0) g3_cursor--;
}

// ============================================================
// CAMBIAR GALERÍA
// ============================================================

static void cambiar_galeria(int dir)
{
    int next = ((int)galeria_activa + dir + 3) % 3;
    galeria_activa = (GaleriaID)next;

    switch (galeria_activa) {
        case GAL_G1:
            num_items = cargar_gemas(items);
            cursor = 0; scroll = 0;
            vista = VISTA_LISTA;
            render_lista();
            break;
        case GAL_G2:
            g2_num    = cargar_gemas_galeria2(g2_items);
            g2_cursor = 0;
            vista = VISTA_G2;
            render_g2();
            break;
        case GAL_G3:
            g3_num    = cargar_gemas_galeria3(g3_items);
            g3_cursor = 0;
            vista = VISTA_G3;
            render_g3();
            break;
    }
}

// ============================================================
// INIT
// ============================================================

void galeria_init(void)
{
    num_items = cargar_gemas(items);
    g2_num = cargar_gemas_galeria2(g2_items);
    g3_num = cargar_gemas_galeria3(g3_items);

    cursor         = 0;
    scroll         = 0;
    g2_cursor      = 0;
    g3_cursor      = 0;
    opcion_submenu = 0;
    galeria_activa = GAL_G1;
    vista          = VISTA_LISTA;

    // render_lista() ya llama a init_paleta_ui() en el VBlank
    render_lista();
}

// Variable estática para guardar qué botones estaban pulsados en el frame anterior
static uint16_t keys_old = 0;

// ============================================================
// INPUT
//
// Regla de render tras input:
//   - render_lista() / render_g2() / render_g3() hacen su propio
//     vsync + paleta + flip internamente.
//   - render_ficha() solo pinta encima del back buffer que render_lista()
//     dejó preparado; necesita un flip adicional propio.
//   - Un solo render por evento de input. Sin render duplicado.
// ============================================================
void galeria_input(uint16_t keys)
{
    uint16_t keys_pressed = keys & ~keys_old;
    keys_old = keys;

    if (keys_pressed & KEY_START) {
        volver_menu();
        return;
    }
    if (keys_pressed & KEY_L) { cambiar_galeria(-1); return; }
    if (keys_pressed & KEY_R) { cambiar_galeria(+1); return; }

    if (galeria_activa == GAL_G1) {
        int moved = 0;

        if (vista == VISTA_LISTA) {
            if ((keys_pressed & KEY_DOWN) && cursor + 1 < num_items) {
                cursor++;
                if (cursor >= scroll + LIST_ITEMS)
                    scroll = cursor - LIST_ITEMS + 1;
                moved = 1;
            }
            if ((keys_pressed & KEY_UP) && cursor > 0) {
                cursor--;
                if (cursor < scroll) scroll = cursor;
                moved = 1;
            }
            if ((keys_pressed & KEY_A) && num_items > 0) {
                vista = VISTA_SUBMENU;
                opcion_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                volver_menu();
                return;
            }
        } else if (vista == VISTA_SUBMENU) {
            if (keys_pressed & KEY_UP) {
                if (opcion_submenu > 0) opcion_submenu--;
                else opcion_submenu = 3;
                moved = 1;
            }
            if (keys_pressed & KEY_DOWN) {
                if (opcion_submenu < 3) opcion_submenu++;
                else opcion_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                vista = VISTA_LISTA;
                moved = 1;
            }
            if (keys_pressed & KEY_A) {
                moved = 1;
                if (opcion_submenu == 0) {
                    vista = VISTA_FICHA;
                } else if (opcion_submenu == 1) {
                    evolucionar_gema(cursor);
                    vista = VISTA_LISTA;
                } else if (opcion_submenu == 2) {
                    mover_a_vitrina(cursor);
                    vista = VISTA_LISTA;
                } else if (opcion_submenu == 3) {
                    vender_item_seleccionado(cursor);
                    vista = VISTA_LISTA;
                }
            }
        } else if (vista == VISTA_FICHA) {
            if (keys_pressed & KEY_B) {
                vista = VISTA_LISTA;
                moved = 1;
            }
        }

        if (moved) {
            if (vista == VISTA_FICHA) {
                // render_lista() pinta el fondo + preview + flip.
                // render_ficha() pinta la zona izquierda encima del mismo back buffer
                // y necesita su propio flip para presentarlo.
                render_lista();
                render_ficha(cursor);
                vsync_only();
                init_paleta_ui();
                flip_no_vsync();
            } else {
                // render_lista() gestiona su propio vsync+paleta+flip.
                render_lista();
            }
        }
    }
    else if (galeria_activa == GAL_G2) {
        int moved = 0;
        if ((keys_pressed & KEY_LEFT)  && g2_cursor > 0)                { g2_cursor--;           moved = 1; }
        if ((keys_pressed & KEY_RIGHT) && g2_cursor < g2_num - 1)       { g2_cursor++;           moved = 1; }
        if ((keys_pressed & KEY_UP)    && g2_cursor >= G2_COLS)          { g2_cursor -= G2_COLS;  moved = 1; }
        if ((keys_pressed & KEY_DOWN)  && g2_cursor + G2_COLS < g2_num) { g2_cursor += G2_COLS;  moved = 1; }

        if (keys_pressed & KEY_B) {
            volver_menu();
            return;
        }

        if (moved) {
            render_g2();
        }
    }
    else if (galeria_activa == GAL_G3) {
        int moved = 0;
        if ((keys_pressed & KEY_LEFT)  && g3_cursor > 0)                { g3_cursor--;           moved = 1; }
        if ((keys_pressed & KEY_RIGHT) && g3_cursor < g3_num - 1)       { g3_cursor++;           moved = 1; }
        if ((keys_pressed & KEY_UP)    && g3_cursor >= G3_COLS)          { g3_cursor -= G3_COLS;  moved = 1; }
        if ((keys_pressed & KEY_DOWN)  && g3_cursor + G3_COLS < g3_num) { g3_cursor += G3_COLS;  moved = 1; }

        if ((keys_pressed & KEY_A) && g3_num > 0) {
            comprar_desde_g3(g3_cursor);
            moved = 1;
        }
        if (keys_pressed & KEY_B) {
            volver_menu();
            return;
        }

        if (moved) {
            render_g3();
        }
    }
}
