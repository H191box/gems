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
#include "opalo.h"
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

static void init_paleta_ui(void)
{
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0]   = 0x0000;
    pal[1]   = 0x294A;
    pal[2]   = 0x681F;
    pal[3]   = 0x4210;
    pal[4]   = 0x6810;
    pal[5]   = 0x1F00;
    for (int i = 6; i <= 15; i++) pal[i] = 0x0000;
    pal[255] = 0x7FFF;
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

// FIX: persiste la etapa en SRAM al evolucionar
static void evolucionar_gema(int idx)
{
    if (num_items <= 0 || idx < 0 || idx >= num_items) return;

    if (items[idx].etapa < ETAPA_PULIDA) {
        items[idx].etapa++;
        actualizar_gema_en_sram(idx, &items[idx]);  // <- FIX
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
// ============================================================

// FIX: guard contra etapa inválida
static void render_thumb(int idx)
{
    limpiar_buffer_render();

    if (items[idx].etapa > ETAPA_PULIDA)    // <- FIX: etapa corrupta, no renderizar
        return;

    if (items[idx].etapa == ETAPA_PULIDA) {
        renderizar_opalo_pequeno_celda(THUMB_X, THUMB_Y, &items[idx], 16, 100);
    } else {
        renderizar_roca_pequena(THUMB_X, THUMB_Y, &items[idx]);
    }
    init_paleta_ui();
}

// ============================================================
// RENDER LISTA G1
// ============================================================

static void render_lista(void)
{
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    uint16_t* vram = get_vram();

    limpiar_buffer_render();


    clear_vram(vram);

   
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA 1  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10, 70, "GALERIA VACIA", 255);
        draw_text(vram, 10, 85, "USA EL TALLER", 255);
        init_paleta_ui();
        flip();
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
        if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;
        draw_text(vram, 20, y + 3,
                  items[idx].etapa == ETAPA_PULIDA
                      ? (char*)NOMBRE_TIPO[t]
                      : "CHUNK BRUTO", 255);
    }


    if (cursor < num_items) {
        render_thumb(cursor);

     
        int x = LIST_W + 4;
        int y = 105;
        char buf[24];
        int t = (int)gema_tipo(&items[cursor]);
        if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;

        if (items[cursor].etapa == ETAPA_PULIDA) {

            draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
            sprintf(buf, "QUILATES: %d", items[cursor].quilates);
            draw_text(vram, x, y, buf, 255); y += 12;
            sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
            draw_text(vram, x, y, buf, 255); y += 14;
        } else {
            draw_text(vram, x, y, "CHUNK SIN CORTAR", 255); y += 14;
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
        draw_text(vram, sm_x + 6, sm_y + 20,
                  opcion_submenu == 1 ? "> CORTAR" : "  CORTAR", 255);
        draw_text(vram, sm_x + 6, sm_y + 34,
                  opcion_submenu == 2 ? "> A VITRINA" : "  A VITRINA", 255);
        draw_text(vram, sm_x + 6, sm_y + 48,
                  opcion_submenu == 3 ? "> VENDER" : "  VENDER", 255);
    }

  
    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
    flip();
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
    flip();
}

// ============================================================
// VENDER G1 → G3 (suma dinero, mueve a galería 3)
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
// RENDER G2
// ============================================================

#define G2_COLS     3
#define G2_CELL_W  72
#define G2_CELL_H  46
#define G2_OX       8
#define G2_OY      16

static void render_g2(void)
{
    uint16_t* vram = get_vram();
    clear_vram(vram);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA 2  L/R:CAMBIAR", 255);

    if (g2_num == 0) {
        draw_text(vram, 10, 70, "VITRINA VACIA", 255);
        init_paleta_ui();
        flip();
        return;
    }

    for (int i = 0; i < g2_num; i++) {
        int col = i % G2_COLS;
        int row = i / G2_COLS;
        int cx  = G2_OX + col * G2_CELL_W;
        int cy  = G2_OY + row * G2_CELL_H;

        if (i == g2_cursor)
            fill_rect(vram, cx, cy, G2_CELL_W - 2, G2_CELL_H - 2, 2);

        if (g2_items[i].etapa == ETAPA_PULIDA)
            renderizar_opalo_pequeno_celda(cx + 4, cy + 4, &g2_items[i], 16, 100);
        else
            renderizar_roca_pequena(cx + 4, cy + 4, &g2_items[i]);
    }

    init_paleta_ui();
    draw_text(vram, 0, 152, "CURSOR:MOVER  START:MENU", 255);
    flip();
}

// ============================================================
// RENDER G3
// ============================================================

#define G3_COLS     3
#define G3_CELL_W  72
#define G3_CELL_H  46
#define G3_OX       8
#define G3_OY      16

static void render_g3(void)
{
    uint16_t* vram = get_vram();
    clear_vram(vram);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "MERCADO  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);

    if (g3_num == 0) {
        draw_text(vram, 10, 60, "MERCADO VACIO", 255);
        draw_text(vram, 10, 75, "VENDE DESDE G1", 255);
        init_paleta_ui();
        flip();
        return;
    }

    for (int i = 0; i < g3_num; i++) {
        int col = i % G3_COLS;
        int row = i / G3_COLS;
        int cx  = G3_OX + col * G3_CELL_W;
        int cy  = G3_OY + row * G3_CELL_H;

        if (i == g3_cursor)
            fill_rect(vram, cx, cy, G3_CELL_W - 2, G3_CELL_H - 2, 2);

        if (g3_items[i].etapa == ETAPA_PULIDA)
            renderizar_opalo_pequeno_celda(cx + 4, cy + 4, &g3_items[i], 16, 100);
        else
            renderizar_opalo_pequeno_celda(cx + 4, cy + 4, &g3_items[i], 16, 100);
    }

    if (g3_cursor < g3_num) {
        char buf[24];
        int t = (int)gema_tipo(&g3_items[g3_cursor]);
        if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;
        draw_text(vram, 4, 130, (char*)NOMBRE_TIPO[t], 255);
        sprintf(buf, "PRECIO: %u", (unsigned int)valor_g3(g3_cursor));
        draw_text(vram, 4, 142, buf, 255);
    }

    init_paleta_ui();
    draw_text(vram, 0, 152, "A:COMPRAR  START:MENU", 255);
    flip();
}

// ============================================================
// COMPRAR DESDE G3 → G1 (resta dinero)
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
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;


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



    init_paleta_ui();
    render_lista();
}
// ============================================================
// INPUT
// ============================================================
void galeria_input(uint16_t keys)
{
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;

    if (keys & KEY_START) {
        volver_menu();
        return;
    }
    if (keys & KEY_L) { cambiar_galeria(-1); return; }
    if (keys & KEY_R) { cambiar_galeria(+1); return; }

    // -------------------------
    // G1
    // -------------------------
    if (galeria_activa == GAL_G1) {

        if (vista == VISTA_LISTA) {

            if ((keys & KEY_DOWN) && cursor + 1 < num_items) {
                cursor++;
                if (cursor >= scroll + LIST_ITEMS)
                    scroll = cursor - LIST_ITEMS + 1;
                render_lista();
            }
            if ((keys & KEY_UP) && cursor > 0) {
                cursor--;
                if (cursor < scroll) scroll = cursor;
                render_lista();
            }
            if ((keys & KEY_A) && num_items > 0) {
                vista = VISTA_SUBMENU;
                opcion_submenu = 0;
                render_lista();
            }
            if (keys & KEY_B) {
                volver_menu();
            }

        } else if (vista == VISTA_SUBMENU) {

            if (keys & KEY_UP) {
                if (opcion_submenu > 0) opcion_submenu--;
                else opcion_submenu = 3;
                render_lista();
            }
            if (keys & KEY_DOWN) {
                if (opcion_submenu < 3) opcion_submenu++;
                else opcion_submenu = 0;
                render_lista();
            }
            if (keys & KEY_B) {
                vista = VISTA_LISTA;
                render_lista();
            }

            if (keys & KEY_A) {

                if (opcion_submenu == 0) {
                    // VER FICHA — no sospechoso pero lo marcamos

                    vista = VISTA_FICHA;
                    render_lista();
              
                    render_ficha(cursor);
            

                } else if (opcion_submenu == 1) {
                    // CORTAR — zona de máxima sospecha
               

          

                    evolucionar_gema(cursor);

            

                    vista = VISTA_LISTA;

         
                    render_lista();

                } else if (opcion_submenu == 2) {
                    // A VITRINA
            
                    mover_a_vitrina(cursor);
                
                    vista = VISTA_LISTA;
                    render_lista();
            

                } else if (opcion_submenu == 3) {
                    // VENDER
          
                    vender_item_seleccionado(cursor);
          
                    vista = VISTA_LISTA;
                    render_lista();
            
                }
            }

        } else if (vista == VISTA_FICHA) {
            if (keys & KEY_B) {
                vista = VISTA_LISTA;
                render_lista();
            }
        }
    }

    // -------------------------
    // G2
    // -------------------------
    else if (galeria_activa == GAL_G2) {
        int moved = 0;
        if ((keys & KEY_LEFT)  && g2_cursor > 0)                { g2_cursor--;           moved = 1; }
        if ((keys & KEY_RIGHT) && g2_cursor < g2_num - 1)       { g2_cursor++;           moved = 1; }
        if ((keys & KEY_UP)    && g2_cursor >= G2_COLS)          { g2_cursor -= G2_COLS;  moved = 1; }
        if ((keys & KEY_DOWN)  && g2_cursor + G2_COLS < g2_num) { g2_cursor += G2_COLS;  moved = 1; }
        if (moved) render_g2();
        if (keys & KEY_B) {
            volver_menu();
        }
    }

    // -------------------------
    // G3
    // -------------------------
    else if (galeria_activa == GAL_G3) {
        int moved = 0;
        if ((keys & KEY_LEFT)  && g3_cursor > 0)                { g3_cursor--;           moved = 1; }
        if ((keys & KEY_RIGHT) && g3_cursor < g3_num - 1)       { g3_cursor++;           moved = 1; }
        if ((keys & KEY_UP)    && g3_cursor >= G3_COLS)          { g3_cursor -= G3_COLS;  moved = 1; }
        if ((keys & KEY_DOWN)  && g3_cursor + G3_COLS < g3_num) { g3_cursor += G3_COLS;  moved = 1; }
        if (moved) render_g3();
        if ((keys & KEY_A) && g3_num > 0) {
            comprar_desde_g3(g3_cursor);
            render_g3();
        }
        if (keys & KEY_B) {
            volver_menu();
        }
    }
}
