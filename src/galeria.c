#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <gba_video.h>
#include <gba_input.h>
#include <stdlib.h>
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
static int   g2_scroll;
static int   g2_submenu;

// G3
static Gema  g3_items[MAX_GALERIA3];
static int   g3_num;
static int   g3_cursor;
static int   g3_scroll;
static int   g3_submenu;

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

    pal[0]   = 27 | (25 << 5) | (21 << 10);
    pal[1]   = 0x294A;
    pal[2]   = 0x681F;
    pal[3]   = 0x4210;
    pal[4]   = 0x6810;
    pal[5]   = 27 | (25 << 5) | (21 << 10);
    pal[6]   = 30 | (29 << 5) | (25 << 10);

    for (int i = 7; i <= 14; i++)
        pal[i] = 0x0000;

    pal[15]  = 0x7FFF;
    pal[254] = 0x7FFF;
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
// HELPERS VALOR
// ============================================================

static uint32_t valor_gema(int idx_g1)
{
    return gema_valor_estimado(&items[idx_g1]);
}

static uint32_t valor_g2(int idx)
{
    return gema_valor_estimado(&g2_items[idx]);
}

static uint32_t valor_g3(int idx)
{
    return gema_valor_estimado(&g3_items[idx]);
}

// ============================================================
// ORDENAR G3
// ============================================================

static void ordenar_g3(void)
{
    for (int i = 0; i < g3_num - 1; i++) {
        for (int j = 0; j < g3_num - 1 - i; j++) {
            if (gema_valor_estimado(&g3_items[j]) < gema_valor_estimado(&g3_items[j + 1])) {
                Gema tmp      = g3_items[j];
                g3_items[j]   = g3_items[j + 1];
                g3_items[j+1] = tmp;
            }
        }
    }
    for (int i = 0; i < g3_num; i++)
        actualizar_gema_galeria3(i, &g3_items[i]);
}

// ============================================================
// ACCIONES G1
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

    if (cursor >= num_items && cursor > 0) cursor = num_items - 1;
    if (scroll > 0 && scroll + LIST_ITEMS > num_items) scroll--;
    if (scroll < 0) scroll = 0;
}

static void evolucionar_gema(int idx)
{
    if (num_items <= 0 || idx < 0 || idx >= num_items) return;

    if (items[idx].etapa == ETAPA_BRUTA) {
        // Fase 1 -> Fase 2: Cortar (100% seguro)
        gema_cortar(&items[idx]);
        actualizar_gema_en_sram(idx, &items[idx]);
    } 
    else if (items[idx].etapa == ETAPA_CORTADA) {
        // Fase 2 -> Fase 3: Pulir (20% de riesgo de grietas)
        uint8_t dado = (uint8_t)(rand() % 100);
        uint8_t umbral_fallo = 20; // 20% de probabilidad de fallo
        
        // gema_pulir cambia la etapa a ETAPA_PULIDA e inyecta 
        // el FLAG_GRIETAS si el dado cae por debajo de 20
        gema_pulir(&items[idx], dado, umbral_fallo);
        actualizar_gema_en_sram(idx, &items[idx]);
    }
}
static void vender_item_seleccionado(int idx)
{
    if (idx < 0 || idx >= num_items) return;

    uint32_t val = valor_gema(idx);
    modificar_dinero((int32_t)val);

    if (g3_num >= MAX_GALERIA3) {
        uint32_t valor_nueva  = gema_valor_estimado(&items[idx]);
        uint32_t valor_ultima = gema_valor_estimado(&g3_items[g3_num - 1]);
        if (valor_nueva > valor_ultima) {
            actualizar_gema_galeria3(g3_num - 1, &items[idx]);
            g3_items[g3_num - 1] = items[idx];
            ordenar_g3();
        }
    } else {
        guardar_gema_galeria3(&items[idx]);
        g3_items[g3_num] = items[idx];
        g3_num++;
        ordenar_g3();
    }

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
// ACCIONES G2
// ============================================================

static void devolver_a_g1(int idx)
{
    if (idx < 0 || idx >= g2_num) return;

    guardar_gema(&g2_items[idx]);

    for (int i = idx; i < g2_num - 1; i++) {
        actualizar_gema_galeria2(i, &g2_items[i + 1]);
        g2_items[i] = g2_items[i + 1];
    }
    decrementar_num_gemas_galeria2();
    g2_num--;

    if (g2_cursor >= g2_num && g2_cursor > 0) g2_cursor--;
    if (g2_scroll > 0 && g2_scroll + LIST_ITEMS > g2_num) g2_scroll--;
    if (g2_scroll < 0) g2_scroll = 0;
}

// ============================================================
// ACCIONES G3
// ============================================================

static void comprar_desde_g3(int idx)
{
    if (idx < 0 || idx >= g3_num) return;

    uint32_t val    = valor_g3(idx);
    uint32_t dinero = obtener_dinero();
    if (dinero < val) return;

    modificar_dinero(-(int32_t)val);
    guardar_gema(&g3_items[idx]);

    for (int i = idx; i < g3_num - 1; i++) {
        actualizar_gema_galeria3(i, &g3_items[i + 1]);
        g3_items[i] = g3_items[i + 1];
    }
    decrementar_num_gemas_galeria3();
    g3_num--;

    if (g3_cursor >= g3_num && g3_cursor > 0) g3_cursor--;
    if (g3_scroll > 0 && g3_scroll + LIST_ITEMS > g3_num) g3_scroll--;
    if (g3_scroll < 0) g3_scroll = 0;
}

// ============================================================
// HELPER RENDER THUMB — reutilizado por las 3 galerías
// Renderiza la preview de una gema al cuadrante superior derecho.
// ============================================================

static void render_thumb_gema(const Gema* g)
{
    limpiar_buffer_render();

    if (g->etapa > ETAPA_PULIDA)
        return;

    plasma_cache_rebuild(g);
    generar_paleta_gema(g);
    renderizar_gema_preview(180, 40, g);
}

// ============================================================
// RENDER FICHA G1
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

    // AUDITORÍA DE BRILLO: Calculamos los atributos percibidos para evitar fugas de información
    AtributosGema attr;
    gema_calcular_atributos(&items[idx], &attr);

    // CORREGIDO: Ahora muestra el brillo aparente (con sesgo) en lugar del brillo real de la semilla
    sprintf(buf, "BRILLO: %d", attr.brillo_aparente);
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "PUREZA: %d", gema_pureza(&items[idx]));
    draw_text(vram, 5, y, buf, 255); y += 12;

    // SISTEMA DE PESO APARENTE: Se mantiene tu cambio con la función al vuelo
    sprintf(buf, "PESO: %d K", gema_quilates_aparentes(&items[idx]));
    draw_text(vram, 5, y, buf, 255); y += 12;
    
    sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(idx));
    draw_text(vram, 5, y, buf, 255);

    draw_text(vram, 5, 140, "B: VOLVER", 255);
}

// ============================================================
// RENDER OBSERVAR — pantalla completa bloqueante (G2 y G3)
// ============================================================

static void render_observar(const Gema* g)
{
    uint8_t* buf = get_anim_buf_b();

    plasma_cache_rebuild(g);

renderizar_opalo_pro(buf, 240, 160, g);

    vsync_only();
    generar_paleta_gema(g);
    dibujar_fondo_texturizado_optimizado(get_vram(), 0);
    volcar_buf_solo_opalo(buf, 0);

    draw_text(get_vram(), 2, 150, "B: VOLVER", 255);
    flip_no_vsync();
}

// ============================================================
// RENDER LISTA G1
// ============================================================

static void render_lista(void)
{
    uint16_t* vram = get_vram();
    limpiar_buffer_render();

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
        render_thumb_gema(&items[cursor]);

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
            // FASE 1 (ETAPA_BRUTA): Muestra los datos sin identificar con el peso sesgado
            draw_text(vram, x, y, "SIN IDENTIFICAR", 255); y += 14;
            
            // MODIFICACIÓN: Cambiado items[cursor].quilates por gema_quilates_aparentes
            sprintf(buf, "PESO: %d", gema_quilates_aparentes(&items[cursor]));
            
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
        if (items[cursor].etapa == ETAPA_BRUTA)
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> CORTAR" : "  CORTAR");
        else if (items[cursor].etapa == ETAPA_CORTADA)
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> PULIR" : "  PULIR");
        else
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> MAX EVOL" : "  MAX EVOL");
        draw_text(vram, sm_x + 6, sm_y + 20, txt_evolucion, 255);

        draw_text(vram, sm_x + 6, sm_y + 34,
                  opcion_submenu == 2 ? "> A VITRINA" : "  A VITRINA", 255);
        draw_text(vram, sm_x + 6, sm_y + 48,
                  opcion_submenu == 3 ? "> VENDER" : "  VENDER", 255);
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);

    vsync_only();
    init_paleta_ui();
    if (num_items > 0 && cursor < num_items)
        generar_paleta_gema(&items[cursor]);
    flip_no_vsync();
}
// ============================================================
// RENDER LISTA G2 (Vitrina) — misma UI que G1
// ============================================================

static void render_g2(void)
{
    uint16_t* vram = get_vram();
    limpiar_buffer_render();

    dibujar_fondo_texturizado_optimizado(vram, 0);
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "VITRINA  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (g2_num == 0) {
        draw_text(vram, 10, 70, "VITRINA VACIA", 255);
        draw_text(vram, 10, 85, "MUEVE DESDE G1", 255);
        draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = g2_scroll + i;
        if (idx >= g2_num) break;

        int y      = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == g2_cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char num[4] = "XX.";
        num[0] = '0' + ((idx + 1) / 10);
        num[1] = '0' + ((idx + 1) % 10);
        draw_text(vram, 2, y + 3, num, 255);

        int t = (int)gema_tipo(&g2_items[idx]);
        if (t < 0 || t >= 6) t = 0;

        char etiqueta[24];
        if (g2_items[idx].etapa == ETAPA_PULIDA)
            sprintf(etiqueta, "%s", NOMBRE_TIPO[t]);
        else if (g2_items[idx].etapa == ETAPA_CORTADA)
            sprintf(etiqueta, "CABUJON %s", NOMBRE_TIPO[t]);
        else
            sprintf(etiqueta, "RAW %s", NOMBRE_TIPO[t]);

        draw_text(vram, 20, y + 3, etiqueta, 255);
    }

    if (g2_scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13, "^", 255);
    if (g2_scroll + LIST_ITEMS < g2_num)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (g2_cursor < g2_num) {
        render_thumb_gema(&g2_items[g2_cursor]);

        int x = LIST_W + 4;
        int y = 105;
        char buf[24];

        int t = (int)gema_tipo(&g2_items[g2_cursor]);
        if (t < 0 || t >= 6) t = 0;

        draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
        sprintf(buf, "QUILATES: %d", g2_items[g2_cursor].quilates);
        draw_text(vram, x, y, buf, 255); y += 12;
        sprintf(buf, "VALOR: %u", (unsigned int)valor_g2(g2_cursor));
        draw_text(vram, x, y, buf, 255); y += 14;
        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista == VISTA_G2_SUBMENU && g2_num > 0) {
        int sm_x = LIST_W + 6;
        int sm_y = 82;
        int sm_w = 113;
        int sm_h = 40;
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        draw_text(vram, sm_x + 6, sm_y + 6,
                  g2_submenu == 0 ? "> OBSERVAR"    : "  OBSERVAR",    255);
        draw_text(vram, sm_x + 6, sm_y + 20,
                  g2_submenu == 1 ? "> DEVOLVER G1" : "  DEVOLVER G1", 255);
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);

    vsync_only();
    init_paleta_ui();
    if (g2_num > 0 && g2_cursor < g2_num)
        generar_paleta_gema(&g2_items[g2_cursor]);
    flip_no_vsync();
}

// ============================================================
// RENDER LISTA G3 (Mercado) — misma UI que G1
// ============================================================

static void render_g3(void)
{
    uint16_t* vram = get_vram();
    limpiar_buffer_render();

    dibujar_fondo_texturizado_optimizado(vram, 0);
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "MERCADO  L/R:CAMBIAR", 255);

    char txt_dinero[20];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 170, 2, txt_dinero, 255);
    vline(vram, LIST_W, 12, 148, 3);

    if (g3_num == 0) {
        draw_text(vram, 10, 60, "MERCADO VACIO", 255);
        draw_text(vram, 10, 75, "VENDE DESDE G1", 255);
        draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = g3_scroll + i;
        if (idx >= g3_num) break;

        int y      = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == g3_cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char num[4] = "XX.";
        num[0] = '0' + ((idx + 1) / 10);
        num[1] = '0' + ((idx + 1) % 10);
        draw_text(vram, 2, y + 3, num, 255);

        int t = (int)gema_tipo(&g3_items[idx]);
        if (t < 0 || t >= 6) t = 0;

        draw_text(vram, 20, y + 3, (char*)NOMBRE_TIPO[t], 255);
    }

    if (g3_scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13, "^", 255);
    if (g3_scroll + LIST_ITEMS < g3_num)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (g3_cursor < g3_num) {
        render_thumb_gema(&g3_items[g3_cursor]);

        int x = LIST_W + 4;
        int y = 105;
        char buf[24];

        int t = (int)gema_tipo(&g3_items[g3_cursor]);
        if (t < 0 || t >= 6) t = 0;

        draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
        sprintf(buf, "QUILATES: %d", g3_items[g3_cursor].quilates);
        draw_text(vram, x, y, buf, 255); y += 12;
        sprintf(buf, "PRECIO: %u", (unsigned int)valor_g3(g3_cursor));
        draw_text(vram, x, y, buf, 255); y += 14;
        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista == VISTA_G3_SUBMENU && g3_num > 0) {
        int sm_x = LIST_W + 6;
        int sm_y = 82;
        int sm_w = 113;
        int sm_h = 40;
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        draw_text(vram, sm_x + 6, sm_y + 6,
                  g3_submenu == 0 ? "> COMPRAR"  : "  COMPRAR",  255);
        draw_text(vram, sm_x + 6, sm_y + 20,
                  g3_submenu == 1 ? "> OBSERVAR" : "  OBSERVAR", 255);
    }

    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);

    vsync_only();
    init_paleta_ui();
    if (g3_num > 0 && g3_cursor < g3_num)
        generar_paleta_gema(&g3_items[g3_cursor]);
    flip_no_vsync();
}

// ============================================================
// CAMBIAR GALERÍA — con fade para ocultar la carga
// ============================================================

static void cambiar_galeria(int dir)
{
    int next = ((int)galeria_activa + dir + 3) % 3;
    galeria_activa = (GaleriaID)next;

    fade_out();

    switch (galeria_activa) {
        case GAL_G1:
            num_items = cargar_gemas(items);
            cursor = 0; scroll = 0;
            vista = VISTA_LISTA;
            render_lista();
            break;
        case GAL_G2:
            g2_num    = cargar_gemas_galeria2(g2_items);
            g2_cursor = 0; g2_scroll = 0;
            vista = VISTA_G2;
            render_g2();
            break;
        case GAL_G3:
            g3_num    = cargar_gemas_galeria3(g3_items);
            g3_cursor = 0; g3_scroll = 0;
            ordenar_g3();
            vista = VISTA_G3;
            render_g3();
            break;
    }

    fade_in();
}

// ============================================================
// INIT
// ============================================================

void galeria_init(void)
{
    fade_out();

    num_items = cargar_gemas(items);
    g2_num    = cargar_gemas_galeria2(g2_items);
    g3_num    = cargar_gemas_galeria3(g3_items);

    cursor    = 0; scroll    = 0;
    g2_cursor = 0; g2_scroll = 0;
    g3_cursor = 0; g3_scroll = 0;
    opcion_submenu = 0;
    galeria_activa = GAL_G1;
    vista          = VISTA_LISTA;

    ordenar_g3();

    render_lista();
    fade_in();
}

// ============================================================
// INPUT
// ============================================================

static uint16_t keys_old = 0;

void galeria_input(uint16_t keys)
{
    uint16_t keys_pressed = keys & ~keys_old;
    keys_old = keys;

    if (keys_pressed & KEY_START) {
        volver_menu_con_fade();
        return;
    }
    if (keys_pressed & KEY_L) { cambiar_galeria(-1); return; }
    if (keys_pressed & KEY_R) { cambiar_galeria(+1); return; }

    // --------------------------------------------------------
    // G1
    // --------------------------------------------------------
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
                volver_menu_con_fade();
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
                render_lista();
                render_ficha(cursor);
                vsync_only();
                init_paleta_ui();
                flip_no_vsync();
            } else {
                render_lista();
            }
        }
    }
    // --------------------------------------------------------
    // G2
    // --------------------------------------------------------
    else if (galeria_activa == GAL_G2) {
        int moved = 0;

        if (vista == VISTA_G2) {
            if ((keys_pressed & KEY_DOWN) && g2_cursor + 1 < g2_num) {
                g2_cursor++;
                if (g2_cursor >= g2_scroll + LIST_ITEMS)
                    g2_scroll = g2_cursor - LIST_ITEMS + 1;
                moved = 1;
            }
            if ((keys_pressed & KEY_UP) && g2_cursor > 0) {
                g2_cursor--;
                if (g2_cursor < g2_scroll) g2_scroll = g2_cursor;
                moved = 1;
            }
            if ((keys_pressed & KEY_A) && g2_num > 0) {
                vista = VISTA_G2_SUBMENU;
                g2_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                volver_menu_con_fade();
                return;
            }

        } else if (vista == VISTA_G2_OBSERVAR) {
            if (keys_pressed & KEY_B) {
                vista = VISTA_G2;
                render_g2();
            }

        } else if (vista == VISTA_G2_SUBMENU) {
            if (keys_pressed & KEY_UP)   { g2_submenu = !g2_submenu; moved = 1; }
            if (keys_pressed & KEY_DOWN) { g2_submenu = !g2_submenu; moved = 1; }
            if (keys_pressed & KEY_B)    { vista = VISTA_G2; moved = 1; }
            if (keys_pressed & KEY_A) {
                if (g2_submenu == 0) {
                    vista = VISTA_G2_OBSERVAR;
                    render_observar(&g2_items[g2_cursor]);
                } else {
                    devolver_a_g1(g2_cursor);
                    vista = VISTA_G2;
                    moved = 1;
                }
            }
        }

        if (moved) render_g2();
    }
    // --------------------------------------------------------
    // G3
    // --------------------------------------------------------
    else if (galeria_activa == GAL_G3) {
        int moved = 0;

        if (vista == VISTA_G3) {
            if ((keys_pressed & KEY_DOWN) && g3_cursor + 1 < g3_num) {
                g3_cursor++;
                if (g3_cursor >= g3_scroll + LIST_ITEMS)
                    g3_scroll = g3_cursor - LIST_ITEMS + 1;
                moved = 1;
            }
            if ((keys_pressed & KEY_UP) && g3_cursor > 0) {
                g3_cursor--;
                if (g3_cursor < g3_scroll) g3_scroll = g3_cursor;
                moved = 1;
            }
            if ((keys_pressed & KEY_A) && g3_num > 0) {
                vista = VISTA_G3_SUBMENU;
                g3_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                volver_menu_con_fade();
                return;
            }

        } else if (vista == VISTA_G3_OBSERVAR) {
            if (keys_pressed & KEY_B) {
                vista = VISTA_G3;
                render_g3();
            }

        } else if (vista == VISTA_G3_SUBMENU) {
            if (keys_pressed & KEY_UP)   { g3_submenu = !g3_submenu; moved = 1; }
            if (keys_pressed & KEY_DOWN) { g3_submenu = !g3_submenu; moved = 1; }
            if (keys_pressed & KEY_B)    { vista = VISTA_G3; moved = 1; }
            if (keys_pressed & KEY_A) {
                if (g3_submenu == 0) {
                    /* COMPRAR */
                    comprar_desde_g3(g3_cursor);
                    ordenar_g3();
                    vista = VISTA_G3;
                    moved = 1;
                } else {
                    /* OBSERVAR */
                    vista = VISTA_G3_OBSERVAR;
                    render_observar(&g3_items[g3_cursor]);
                }
            }
        }

        if (moved) render_g3();
    }
}
