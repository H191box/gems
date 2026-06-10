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

// Preview lazy: frames de inactividad antes de renderizar
#define PREVIEW_DELAY_FRAMES  120   /* 2 segundos a 60fps */

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

// Pool completo (cargado una vez)
static Gema  pool[MAX_POOL];
static int   pool_num;

// Vista filtrada de la caja activa
static int   vista_idx[MAX_POOL];   /* pool_idx de cada entrada visible */
static int   vista_num;

// Navegación caja activa
static int   cursor;
static int   scroll;
static int   opcion_submenu;

// Cajas
static CajaFiltro cajas[NUM_CAJAS];
static int        caja_activa;      /* 0..NUM_CAJAS-1 */

// G3
static Gema  g3_items[MAX_GALERIA3];
static int   g3_num;
static int   g3_cursor;
static int   g3_scroll;
static int   g3_submenu;

// Preview lazy
static int   preview_timer  = 0;   /* frames desde el último movimiento  */
static int   preview_listo  = 0;   /* 1 = preview válido en pantalla      */

extern uint8_t grieta_buf[160][240];

static void limpiar_buffer_render(void) {
    memset(grieta_buf, 0, sizeof(grieta_buf));
}

/* Forward declare — usada antes de su definición */
static void reconstruir_vista(void);
static void render_lista(void);

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

// Abreviaturas para el resumen de filtro en cabecera
static const char* TIPO_ABR[6]   = {"NGR","CRI","FUE","BLA","ROS","GRS"};
static const char* PAT_ABR[6]    = {"NEB","VEN","MAT","MOS","CHA","HAR"};

// ============================================================
// HELPERS VALOR
// ============================================================

static uint32_t valor_gema(int vis_idx)
{
    return gema_valor_estimado(&pool[vista_idx[vis_idx]]);
}

static uint32_t valor_g3(int idx)
{
    return gema_valor_estimado(&g3_items[idx]);
}

// ============================================================
// RESUMEN FILTRO — para la cabecera
// ============================================================

/* Construye un string legible del filtro activo de una caja.
   Formato: "NGR|FUE · NEB|MAT" o "TODO" si sin filtro. */
static void caja_resumen_filtro(const CajaFiltro *f, char *buf, int buf_size)
{
    if (caja_filtro_vacia(f)) {
        int i = 0;
        const char *s = "TODO";
        while (*s && i < buf_size - 1) buf[i++] = *s++;
        buf[i] = '\0';
        return;
    }

    char tmp[48];
    int pos = 0;

    /* Tipos con al menos un bit activo */
    int primero = 1;
    for (int t = 0; t < CF_NUM_TIPOS; t++) {
        int tiene = 0;
        for (int p = 0; p < CF_NUM_PATRONES; p++)
            if (caja_filtro_activo(f, t, p)) { tiene = 1; break; }
        if (!tiene) continue;
        if (!primero && pos < 44) tmp[pos++] = '|';
        const char *a = TIPO_ABR[t];
        while (*a && pos < 44) tmp[pos++] = *a++;
        primero = 0;
    }

    /* Separador central */
    if (pos < 44) tmp[pos++] = ' ';
    if (pos < 44) tmp[pos++] = '-';   /* GBA no tiene mid-dot en font ASCII */
    if (pos < 44) tmp[pos++] = ' ';

    /* Patrones con al menos un bit activo */
    primero = 1;
    for (int p = 0; p < CF_NUM_PATRONES; p++) {
        int tiene = 0;
        for (int t = 0; t < CF_NUM_TIPOS; t++)
            if (caja_filtro_activo(f, t, p)) { tiene = 1; break; }
        if (!tiene) continue;
        if (!primero && pos < 44) tmp[pos++] = '|';
        const char *a = PAT_ABR[p];
        while (*a && pos < 44) tmp[pos++] = *a++;
        primero = 0;
    }
    tmp[pos] = '\0';

    int i = 0;
    while (tmp[i] && i < buf_size - 1) { buf[i] = tmp[i]; i++; }
    buf[i] = '\0';
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

static void evolucionar_gema(int vis_idx)
{
    if (vista_num <= 0 || vis_idx < 0 || vis_idx >= vista_num) return;
    int pi = vista_idx[vis_idx];

    if (pool[pi].etapa == ETAPA_BRUTA) {
        gema_cortar(&pool[pi]);
        actualizar_gema_pool(pi, &pool[pi]);
    }
    else if (pool[pi].etapa == ETAPA_CORTADA) {
        uint8_t dado = (uint8_t)(rand() % 100);
        gema_pulir(&pool[pi], dado, 20);
        actualizar_gema_pool(pi, &pool[pi]);
    }
}

static void vender_item_seleccionado(int vis_idx)
{
    if (vis_idx < 0 || vis_idx >= vista_num) return;
    int pi = vista_idx[vis_idx];

    uint32_t val = gema_valor_estimado(&pool[pi]);
    modificar_dinero((int32_t)val);

    /* Registrar en G3 */
    if (g3_num >= MAX_GALERIA3) {
        uint32_t valor_ultima = gema_valor_estimado(&g3_items[g3_num - 1]);
        if (val > valor_ultima) {
            actualizar_gema_galeria3(g3_num - 1, &pool[pi]);
            g3_items[g3_num - 1] = pool[pi];
            ordenar_g3();
        }
    } else {
        guardar_gema_galeria3(&pool[pi]);
        g3_items[g3_num] = pool[pi];
        g3_num++;
        ordenar_g3();
    }

    /* Compactar pool físico en SRAM */
    for (int i = pi; i < pool_num - 1; i++) {
        actualizar_gema_pool(i, &pool[i + 1]);
        pool[i] = pool[i + 1];
    }
    decrementar_pool();
    pool_num--;

    /* Reconstruir vista e invalidar preview */
    reconstruir_vista();
    preview_listo = 0;
    preview_timer = 0;

    if (cursor >= vista_num && cursor > 0) cursor = vista_num - 1;
    if (cursor < scroll) scroll = cursor;
    if (scroll + LIST_ITEMS > vista_num && vista_num > LIST_ITEMS)
        scroll = vista_num - LIST_ITEMS;
    if (scroll < 0) scroll = 0;
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
    guardar_gema_pool(&g3_items[idx]);

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
// HELPER RENDER THUMB
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

    int t = (int)gema_tipo(&pool[vista_idx[idx]]);
    if (t < 0 || t >= NUM_TIPOS_OPALO) t = 0;
    draw_text(vram, 5, y, (char*)NOMBRE_TIPO[t], 255); y += 14;

    AtributosGema attr;
    gema_calcular_atributos(&pool[vista_idx[idx]], &attr);

    sprintf(buf, "BRILLO: %d", attr.brillo_aparente);
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "PUREZA: %d", gema_pureza(&pool[vista_idx[idx]]));
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "PESO: %d K", gema_quilates_aparentes(&pool[vista_idx[idx]]));
    draw_text(vram, 5, y, buf, 255); y += 12;

    sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(idx));
    draw_text(vram, 5, y, buf, 255);

    draw_text(vram, 5, 140, "B: VOLVER", 255);
}

// ============================================================
// RENDER OBSERVAR — pantalla completa
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

    /* --- Cabecera doble línea --- */
    fill_rect(vram, 0, 0, 240, 20, 4);

    /* Línea 1: nombre de caja + conteo X/Y + dinero */
    {
        char cab[32];
        sprintf(cab, "CAJA %d  [%d/%d]", caja_activa + 1, vista_num, pool_num);
        draw_text(vram, 4, 2, cab, 255);

        char txt_dinero[20];
        sprintf(txt_dinero, "ORO:%d", obtener_dinero());
        draw_text(vram, 170, 2, txt_dinero, 255);
    }

    /* Línea 2: resumen del filtro activo */
    {
        char filtro[48];
        caja_resumen_filtro(&cajas[caja_activa], filtro, sizeof(filtro));
        draw_text(vram, 4, 11, filtro, 6);
    }

    vline(vram, LIST_W, 20, 132, 3);

    if (vista_num == 0) {
        draw_text(vram, 10, 70, "SIN RESULTADOS", 255);
        draw_text(vram, 10, 85, "AJUSTA EL FILTRO", 255);
        draw_text(vram, 0, 152, "IZQ/DER:x8  L/R:CAJA", 255);
        vsync_only();
        init_paleta_ui();
        flip_no_vsync();
        return;
    }

    /* Lista de items */
    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= vista_num) break;

        int y      = 21 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        char num[4] = "XX.";
        num[0] = '0' + ((idx + 1) / 10);
        num[1] = '0' + ((idx + 1) % 10);
        draw_text(vram, 2, y + 3, num, 255);

        int t = (int)gema_tipo(&pool[vista_idx[idx]]);
        if (t < 0 || t >= 6) t = 0;

        char etiqueta[24];
        if (pool[vista_idx[idx]].etapa == ETAPA_PULIDA)
            sprintf(etiqueta, "%s", NOMBRE_TIPO[t]);
        else if (pool[vista_idx[idx]].etapa == ETAPA_CORTADA)
            sprintf(etiqueta, "CABUJON %s", NOMBRE_TIPO[t]);
        else
            sprintf(etiqueta, "RAW %s", NOMBRE_TIPO[t]);

        draw_text(vram, 20, y + 3, etiqueta, 255);
    }

    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 21, "^", 255);
    if (scroll + LIST_ITEMS < vista_num)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    /* --- Zona de preview (derecha) --- */
    if (cursor < vista_num) {
        if (preview_listo) {
            /* Preview válido: dibujarlo */
            render_thumb_gema(&pool[vista_idx[cursor]]);

            int x = LIST_W + 4;
            int y = 105;
            char buf[24];

            int t = (int)gema_tipo(&pool[vista_idx[cursor]]);
            if (t < 0 || t >= 6) t = 0;

            if (pool[vista_idx[cursor]].etapa == ETAPA_PULIDA) {
                draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
                sprintf(buf, "QUILATES: %d", pool[vista_idx[cursor]].quilates);
                draw_text(vram, x, y, buf, 255); y += 12;
                sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
                draw_text(vram, x, y, buf, 255); y += 14;
            } else if (pool[vista_idx[cursor]].etapa == ETAPA_CORTADA) {
                draw_text(vram, x, y, (char*)NOMBRE_TIPO[t], 255); y += 12;
                sprintf(buf, "PESO: %d", pool[vista_idx[cursor]].quilates);
                draw_text(vram, x, y, buf, 255); y += 12;
                sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
                draw_text(vram, x, y, buf, 255); y += 14;
            } else {
                draw_text(vram, x, y, "SIN IDENTIFICAR", 255); y += 14;
                sprintf(buf, "PESO: %d", gema_quilates_aparentes(&pool[vista_idx[cursor]]));
                draw_text(vram, x, y, buf, 255); y += 12;
                sprintf(buf, "VALOR: %u", (unsigned int)valor_gema(cursor));
                draw_text(vram, x, y, buf, 255); y += 14;
            }

            draw_text(vram, x, y, "A:OPCIONES", 255);

        } else {
            /* Telón: rectángulo negro + CARGANDO */
            fill_rect(vram, LIST_W + 1, 20, 240 - LIST_W - 1, 132, 0);
            draw_text(vram, LIST_W + 10, 78, "CARGANDO...", 255);
        }
    }

    /* Submenu opciones G1 */
    if (vista == VISTA_SUBMENU) {
        int sm_x = LIST_W + 6;
        int sm_y = 82;
        int sm_w = 113;
        int sm_h = 54;
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        draw_text(vram, sm_x + 6, sm_y + 6,
                  opcion_submenu == 0 ? "> VER FICHA" : "  VER FICHA", 255);

        char txt_evolucion[20];
        if (pool[vista_idx[cursor]].etapa == ETAPA_BRUTA)
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> CORTAR" : "  CORTAR");
        else if (pool[vista_idx[cursor]].etapa == ETAPA_CORTADA)
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> PULIR" : "  PULIR");
        else
            sprintf(txt_evolucion, "%s", opcion_submenu == 1 ? "> MAX EVOL" : "  MAX EVOL");
        draw_text(vram, sm_x + 6, sm_y + 20, txt_evolucion, 255);

        draw_text(vram, sm_x + 6, sm_y + 34,
                  opcion_submenu == 2 ? "> VENDER" : "  VENDER", 255);
    }

    draw_text(vram, 0, 152, "ARR/ABA:1  IZQ/DER:8  L/R:CAJA", 255);

    vsync_only();
    init_paleta_ui();
    if (vista_num > 0 && cursor < vista_num && preview_listo)
        generar_paleta_gema(&pool[vista_idx[cursor]]);
    flip_no_vsync();
}

// ============================================================
// RENDER LISTA G3 (Mercado)
// ============================================================

static void render_g3(void)
{
    uint16_t* vram = get_vram();
    limpiar_buffer_render();

    dibujar_fondo_texturizado_optimizado(vram, 0);
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "MERCADO  L/R:VOLVER", 255);

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
// RECONSTRUIR VISTA FILTRADA
// ============================================================

static void reconstruir_vista(void)
{
    vista_num = 0;
    for (int i = 0; i < pool_num; i++) {
        if (caja_filtro_acepta(&cajas[caja_activa], &pool[i])) {
            vista_idx[vista_num++] = i;
        }
    }
    /* Ajustar cursor/scroll al nuevo tamaño */
    if (cursor >= vista_num) cursor = vista_num > 0 ? vista_num - 1 : 0;
    if (scroll > cursor)     scroll = cursor;

    /* Invalidar preview al cambiar contenido */
    preview_listo = 0;
    preview_timer = 0;
}

// ============================================================
// CAMBIAR CAJA / NAVEGAR A G3
// ============================================================

static void cambiar_caja(int dir)
{
    caja_activa = (caja_activa + dir + NUM_CAJAS) % NUM_CAJAS;

    fade_out();
    reconstruir_vista();
    cursor = 0; scroll = 0;
    vista  = VISTA_LISTA;
    render_lista();
    fade_in();
}

static void ir_a_g3(void)
{
    galeria_activa = GAL_G3;
    fade_out();
    g3_num    = cargar_gemas_galeria3(g3_items);
    g3_cursor = 0; g3_scroll = 0;
    ordenar_g3();
    vista = VISTA_G3;
    render_g3();
    fade_in();
}

static void volver_a_cajas(void)
{
    galeria_activa = GAL_G1;
    fade_out();
    reconstruir_vista();
    cursor = 0; scroll = 0;
    vista  = VISTA_LISTA;
    render_lista();
    fade_in();
}

// ============================================================
// INIT
// ============================================================

void galeria_init(void)
{
    fade_out();

    pool_num = cargar_pool(pool);
    g3_num   = cargar_gemas_galeria3(g3_items);
    cargar_cajas(cajas);

    cursor         = 0;
    scroll         = 0;
    g3_cursor      = 0;
    g3_scroll      = 0;
    opcion_submenu = 0;
    caja_activa    = 0;
    galeria_activa = GAL_G1;
    vista          = VISTA_LISTA;
    preview_listo  = 0;
    preview_timer  = 0;

    reconstruir_vista();
    ordenar_g3();

    render_lista();
    fade_in();
}

void galeria_recargar_caja_activa(void)
{
    cargar_cajas(cajas);
    reconstruir_vista();
    cursor = 0; scroll = 0;
    render_lista();
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

    if (keys_pressed & KEY_L) {
        if (galeria_activa == GAL_G3) volver_a_cajas();
        else cambiar_caja(-1);
        return;
    }
    if (keys_pressed & KEY_R) {
        if (galeria_activa == GAL_G3) volver_a_cajas();
        else cambiar_caja(+1);
        return;
    }

    // --------------------------------------------------------
    // G1 — cajas
    // --------------------------------------------------------
    if (galeria_activa == GAL_G1) {
        int moved = 0;

        if (vista == VISTA_LISTA) {

            /* Movimiento de 1 en 1 */
            if ((keys_pressed & KEY_DOWN) && cursor + 1 < vista_num) {
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

            /* Movimiento de 8 en 8 con cruceta izquierda/derecha */
            if (keys_pressed & KEY_LEFT) {
                cursor -= 8;
                if (cursor < 0) cursor = 0;
                scroll = cursor - (LIST_ITEMS / 2);
                if (scroll < 0) scroll = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_RIGHT) {
                cursor += 8;
                if (cursor >= vista_num) cursor = vista_num > 0 ? vista_num - 1 : 0;
                scroll = cursor - (LIST_ITEMS / 2);
                if (scroll < 0) scroll = 0;
                if (scroll + LIST_ITEMS > vista_num)
                    scroll = vista_num - LIST_ITEMS;
                if (scroll < 0) scroll = 0;
                moved = 1;
            }

            if ((keys_pressed & KEY_A) && vista_num > 0) {
                vista = VISTA_SUBMENU;
                opcion_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                volver_menu_con_fade();
                return;
            }

            /* Timer de preview lazy */
            if (moved) {
                /* Movimiento: invalidar preview */
                preview_listo = 0;
                preview_timer = 0;
            } else {
                /* Sin movimiento: avanzar timer si preview pendiente */
                if (!preview_listo && vista_num > 0) {
                    preview_timer++;
                    if (preview_timer >= PREVIEW_DELAY_FRAMES) {
                        preview_listo = 1;
                        preview_timer = 0;
                        render_lista();
                        return;
                    }
                }
            }

        } else if (vista == VISTA_SUBMENU) {
            if (keys_pressed & KEY_UP) {
                if (opcion_submenu > 0) opcion_submenu--;
                else opcion_submenu = 2;
                moved = 1;
            }
            if (keys_pressed & KEY_DOWN) {
                if (opcion_submenu < 2) opcion_submenu++;
                else opcion_submenu = 0;
                moved = 1;
            }
            if (keys_pressed & KEY_B) {
                vista = VISTA_LISTA;
                moved = 1;
            }
            if (keys_pressed & KEY_A) {
                if (opcion_submenu == 0) {
                    vista = VISTA_FICHA;
                    render_observar(&pool[vista_idx[cursor]]);
                    return;   /* salir sin tocar moved — evita render_ficha encima */
                }
                moved = 1;
                if (opcion_submenu == 1) {

                    evolucionar_gema(cursor);
                    preview_listo = 0;   /* gema cambió, invalidar preview */
                    vista = VISTA_LISTA;
                } else if (opcion_submenu == 2) {
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
    // G3 — Mercado
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
                    comprar_desde_g3(g3_cursor);
                    ordenar_g3();
                    vista = VISTA_G3;
                    moved = 1;
                } else {
                    vista = VISTA_G3_OBSERVAR;
                    render_observar(&g3_items[g3_cursor]);
                }
            }
        }

        if (moved) render_g3();
    }
}
