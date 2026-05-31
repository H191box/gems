#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "galeria.h"
#include "save.h"
#include "plasma.h"     /* generar_paleta, generar_paleta_rango, volcar_buf_solo_opalo,*/
#include "render.h"      /*                     dibujar_fondo_texturizado_optimizado, precalcular_fondo,
                           volcar_frame, draw_ui_sobre_buffer, pixel_nebula,
                           renderizar_opalo_pequeno, renderizar_opalo_pequeno_celda */
#include "video.h"      /* get_vram, flip */
#include "font.h"
#include "opalo.h"
#include "gema.h"
#include "gema_render.h" /* renderizar_gema_a_buffer, renderizar_roca_pequena,
                            get_anim_buf_a, get_anim_buf_b */
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

// Galería 2: rejilla 3x3
#define G2_COLS        3
#define G2_ROWS        3
#define G2_CELL_W     72
#define G2_CELL_H     46
#define G2_ORIGIN_X    8
#define G2_ORIGIN_Y   16
#define G2_THUMB_W    60
#define G2_THUMB_H    36

// ============================================================
// TABLAS DE NOMBRES
// ============================================================
static const char* NOMBRE_TIPO[7] = {
    "OPALO NEGRO",
    "OPALO CRISTAL",
    "OPALO FUEGO",
    "OPALO BLANCO",
    "OPALO ROSA",
    "OPALO GRIS",
    "???"
};
static const char* NOMBRE_PATRON[6] = {
    "NEBULA", "VENAS", "MOSAICO", "CHAOS", "MATRIX", "HARLEQUIN"
};

// ============================================================
// ESTADOS
// ============================================================
typedef enum {
    VISTA_LISTA,
    VISTA_SUBMENU,
    VISTA_FICHA,
    VISTA_IMAGEN,
    VISTA_G2,
    VISTA_G2_SUBMENU
} VistaGaleria;

// ============================================================
// ESTADO GALERÍA 1
// ============================================================
static VistaGaleria vista;
static Chunk        items[MAX_GALERIA];
static int          num_items;
static int          cursor;
static int          scroll;
static int          opcion_submenu;

// ============================================================
// ESTADO GALERÍA 2
// ============================================================
static Chunk  g2_items[MAX_GALERIA2];
static int    g2_num;
static int    g2_cursor;
static int    g2_opcion;
static VistaGaleria vista_anterior;

// ============================================================
// HELPER: Chunk → Gema temporal (sin persistir, solo para render/UI)
//
// ID = 0 indica temporal. Si el chunk ya estaba cortado, avanzamos
// la etapa a CORTADA para que la visibilidad funcione correctamente.
// ============================================================
static void chunk_a_gema_temp(Gema *g, const Chunk *c)
{
    uint8_t bioma = 0;
    if (c->ciudad_id < NUM_TOTAL_CIUDADES)
        bioma = ciudades[c->ciudad_id].bioma_id;
    crear_gema_desde_chunk(g, c, 0, bioma);
    if (c->cortado)
        g->etapa = ETAPA_CORTADA;
}

// HELPER: Gema → Opalo temporal (para las funciones de paleta que
// aún reciben Opalo*: generar_paleta y generar_paleta_rango)
// ============================================================
// HELPERS UI
// ============================================================
static void init_paleta_ui(void)
{
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0]  = 0x0000;
    pal[1]  = 0x294A;
    pal[2]  = 0x681F;
    pal[3]  = 0x4210;
    pal[4]  = 0x6810;
    pal[5]  = 0x3DEF;
    pal[6]  = 0x56B5;
    for (int i = 7; i <= 15; i++) pal[i] = 0x0000;
    pal[255] = 0x7FFF;
}

static void clear_vram(uint16_t* vram)
{
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

// ============================================================
// VALOR PARA UI (siempre estimado hasta etapa PULIDA)
// ============================================================
static uint32_t valor_visible_chunk(const Chunk *c)
{
    if (!c->cortado)
        return (uint32_t)(c->tamanyo * c->quilates * 2) + (c->grietas * 5);
    Gema g;
    chunk_a_gema_temp(&g, c);
    return gema_valor_estimado(&g);
}

// ============================================================
// VENDER (galería 1)
// ============================================================
static void vender_item_seleccionado(int idx_lista)
{
    Chunk todos[MAX_GALERIA];
    int n = cargar_chunks(todos);
    int slot_sram = -1;
    for (int i = 0; i < n; i++) {
        if (todos[i].seed == items[idx_lista].seed) { slot_sram = i; break; }
    }
    if (slot_sram == -1) return;

    uint32_t valor = valor_visible_chunk(&items[idx_lista]);
    modificar_dinero((int32_t)valor);

    for (int i = slot_sram; i < n - 1; i++)
        sobreescribir_chunk(i, &todos[i + 1]);
    Chunk vacio = {0};
    sobreescribir_chunk(n - 1, &vacio);
    decrementar_num_chunks();
}

// ============================================================
// MOVER G1 → G2
// ============================================================
static void mover_a_galeria2(int idx_lista)
{
    if (g2_num >= MAX_GALERIA2) return;
    guardar_chunk_galeria2(&items[idx_lista]);

    Chunk todos[MAX_GALERIA];
    int n = cargar_chunks(todos);
    int slot_sram = -1;
    for (int i = 0; i < n; i++) {
        if (todos[i].seed == items[idx_lista].seed) { slot_sram = i; break; }
    }
    if (slot_sram == -1) return;
    for (int i = slot_sram; i < n - 1; i++)
        sobreescribir_chunk(i, &todos[i + 1]);
    Chunk vacio = {0};
    sobreescribir_chunk(n - 1, &vacio);
    decrementar_num_chunks();
}

// ============================================================
// MOVER G2 → G1
// ============================================================
static void devolver_a_galeria1(int g2_idx)
{
    guardar_chunk(&g2_items[g2_idx]);

    Chunk todos2[MAX_GALERIA2];
    int n2 = cargar_chunks_galeria2(todos2);
    int slot_sram = -1;
    for (int i = 0; i < n2; i++) {
        if (todos2[i].seed == g2_items[g2_idx].seed) { slot_sram = i; break; }
    }
    if (slot_sram == -1) return;
    for (int i = slot_sram; i < n2 - 1; i++)
        sobreescribir_chunk_galeria2(i, &todos2[i + 1]);
    Chunk vacio = {0};
    sobreescribir_chunk_galeria2(n2 - 1, &vacio);
    decrementar_num_chunks_galeria2();
}

// ============================================================
// RENDER THUMBNAIL (galería 1, zona derecha de la lista)
// ============================================================
static void render_thumb(int idx)
{
    if (items[idx].cortado) {
        Gema g;
        chunk_a_gema_temp(&g, &items[idx]);

        Opalo o;
        gema_a_opalo_temp(&o, &g);
        generar_paleta(&o);

        // renderizar_opalo_pequeno(buf, Gema*) rellena un buffer 80x80;
        // aquí usamos el buffer de animación a como scratch
        uint8_t *buf = get_anim_buf_a();
        renderizar_opalo_pequeno(buf, &g);

        // Volcado manual a la posición THUMB_X, THUMB_Y
        uint16_t *vram = get_vram();
        for (int y = 0; y < 80; y++) {
            int sy = THUMB_Y + y;
            if (sy < 0 || sy >= 160) continue;
            for (int x = 0; x < 80; x++) {
                int sx = THUMB_X + x;
                if (sx < 0 || sx >= 240) continue;
                uint8_t px = buf[y * 80 + x];
                if (px == 0) continue;
                int vidx = sy * 120 + sx / 2;
                if (sx & 1)
                    vram[vidx] = (vram[vidx] & 0x00FF) | ((uint16_t)px << 8);
                else
                    vram[vidx] = (vram[vidx] & 0xFF00) | px;
            }
        }
    }
    init_paleta_ui();
}

// ============================================================
// RENDER FICHA (reutilizable G1 y G2)
// es_grande=1 → pantalla completa (G1), 0 → miniatura (G2)
// ============================================================
static void render_ficha_chunk(const Chunk* c, int es_grande)
{
    uint16_t* vram = get_vram();
    clear_vram(vram);
    init_paleta_ui();

    vline(vram, 118, 0, 160, 3);

    int y = 10;
    draw_text(vram, 8, y, "FICHA TECNICA", 255);
    y += 18;

    char buf[40];
    sprintf(buf, "FECHA: %d/%d", c->dia, c->mes);
    draw_text(vram, 8, y, buf, 255);
    y += 14;

    draw_text(vram, 8, y, "ORIGEN:", 255);
    y += 12;
    if (c->ciudad_id >= 0 && c->ciudad_id < 25)
        draw_text(vram, 8, y, (char*)ciudades[c->ciudad_id].nombre, 255);
    y += 16;

    sprintf(buf, "PESO: %d K", c->quilates);
    draw_text(vram, 8, y, buf, 255);
    y += 16;

    if (c->cortado) {
        Gema g;
        chunk_a_gema_temp(&g, c);

        // Patrón: visible en CORTADA
        if (gema_campo_visible(&g, CAMPO_PATRON)) {
            int pat = (g.patron_real < 6) ? g.patron_real : 0;
            draw_text(vram, 8, y, (char*)NOMBRE_PATRON[pat], 255);
        } else {
            draw_text(vram, 8, y, "PATRON: ???", 255);
        }
        y += 14;

        // Tipo: solo en PULIDA
        if (gema_campo_visible(&g, CAMPO_TIPO)) {
            int t = (g.tipo_real < NUM_TIPOS_OPALO) ? g.tipo_real : 0;
            draw_text(vram, 8, y, (char*)NOMBRE_TIPO[t], 255);
        } else {
            draw_text(vram, 8, y, "TIPO: ???", 255);
        }
        y += 14;

        // Valor
        if (gema_campo_visible(&g, CAMPO_VALOR))
            sprintf(buf, "VALOR:%lu", gema_valor_real(&g));
        else
            sprintf(buf, "EST:%lu?", gema_valor_estimado(&g));
        draw_text(vram, 8, y, buf, 255);

        // Render visual
        Opalo o;
        gema_a_opalo_temp(&o, &g);
        generar_paleta(&o);
        volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
        pal[255] = 0x7FFF;

        if (es_grande) {
            uint8_t *rbuf = get_anim_buf_a();
            renderizar_gema_a_buffer(rbuf, 240, 160, &g);
            volcar_buf_solo_opalo(rbuf, 0);
        } else {
            uint8_t *rbuf = get_anim_buf_a();
            renderizar_opalo_pequeno(rbuf, &g);
            // Volcar en posición 138, 42
            for (int ry = 0; ry < 80; ry++) {
                int sy = 42 + ry;
                if (sy < 0 || sy >= 160) continue;
                for (int rx = 0; rx < 80; rx++) {
                    int sx = 138 + rx;
                    if (sx < 0 || sx >= 240) continue;
                    uint8_t px = rbuf[ry * 80 + rx];
                    if (px == 0) continue;
                    int vidx = sy * 120 + sx / 2;
                    if (sx & 1) vram[vidx] = (vram[vidx] & 0x00FF) | ((uint16_t)px << 8);
                    else        vram[vidx] = (vram[vidx] & 0xFF00) | px;
                }
            }
        }
    } else {
        draw_text(vram, 8, y, "CHUNK BRUTO", 255); y += 14;
        draw_text(vram, 8, y, "TIPO: ???",   255); y += 14;
        sprintf(buf, "EST:%lu?", valor_visible_chunk(c));
        draw_text(vram, 8, y, buf, 255);
        
        Gema g_temp;
        chunk_a_gema_temp(&g_temp, c);
        
        // Ahora pasamos el puntero a la gema temporal
        renderizar_roca_pequena(138, 42, &g_temp);
        
   
    }

    draw_text(vram, 8, 148, "B: VOLVER", 255);
    flip();
}

// ============================================================
// RENDER LISTA (galería 1)
// ============================================================
static void render_lista(void)
{
    uint16_t* vram = get_vram();
    dibujar_fondo_texturizado_optimizado(vram, 0);

    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4, 2, "GALERIA", 255);

    char txt_g2[12];
    sprintf(txt_g2, "G2:%d/9", g2_num);
    draw_text(vram, 130, 2, txt_g2, 255);

    char txt_dinero[16];
    sprintf(txt_dinero, "ORO:%d", obtener_dinero());
    draw_text(vram, 175, 2, txt_dinero, 255);

    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10,  70, "GALERIA VACIA", 255);
        draw_text(vram, 10,  85, "CORTA O MUEVE", 255);
        draw_text(vram, 10, 100, "EN EL TALLER",  255);
        draw_text(vram, 0,  152, "SELECT:VITRINA  START:MENU", 255);
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

        // En lista no revelamos el tipo (etapa CORTADA), solo "OPALO ???"
        if (items[idx].cortado)
            draw_text(vram, 20, y + 3, "OPALO ???", 255);
        else
            draw_text(vram, 20, y + 3, "CHUNK BRUTO", 255);
    }

    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13,  "^", 255);
    if (scroll + LIST_ITEMS < num_items)
        draw_text(vram, LIST_W / 2 - 4, 150, "v", 255);

    if (cursor < num_items) {
        render_thumb(cursor);

        int x = LIST_W + 4;
        int y = 105;
        char val_buf[20];

        if (items[cursor].cortado) {
            Gema g;
            chunk_a_gema_temp(&g, &items[cursor]);

            // Patrón visible en CORTADA
            if (gema_campo_visible(&g, CAMPO_PATRON)) {
                int pat = (g.patron_real < 6) ? g.patron_real : 0;
                draw_text(vram, x, y, NOMBRE_PATRON[pat], 255);
            } else {
                draw_text(vram, x, y, "PATRON ???", 255);
            }
            y += 12;

            // Tipo oculto hasta PULIDA
            draw_text(vram, x, y, "TIPO: ???", 255);
            y += 14;

            // Valor estimado
            sprintf(val_buf, "EST:%lu?", gema_valor_estimado(&g));
            draw_text(vram, x, y, val_buf, 255);
            y += 14;
        } else {
            char pbuf[24];
            draw_text(vram, x, y, "CHUNK BRUTO", 255); y += 14;
            sprintf(pbuf, "PESO:%d", items[cursor].quilates);
            draw_text(vram, x, y, pbuf, 255); y += 12;
            sprintf(val_buf, "EST:%lu?", valor_visible_chunk(&items[cursor]));
            draw_text(vram, x, y, val_buf, 255);
            y += 14;
        }
        draw_text(vram, x, y, "A:OPCIONES", 255);
    }

    if (vista == VISTA_SUBMENU) {
        int sm_x = LIST_W + 4;
        int sm_y = 18;
        int sm_w = 112;
        int sm_h = 56;

        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);

        int llena     = (g2_num >= MAX_GALERIA2);
        const char* op0 = llena ? "  VITRINA(LLENA)"
                                : (opcion_submenu == 0 ? "> A VITRINA" : "  A VITRINA");
        const char* op1 = (opcion_submenu == 1) ? "> VER FICHA" : "  VER FICHA";
        const char* op2 = (opcion_submenu == 2) ? "> VENDER"    : "  VENDER";

        draw_text(vram, sm_x + 4, sm_y + 6,  (char*)op0, 255);
        draw_text(vram, sm_x + 4, sm_y + 22, (char*)op1, 255);
        draw_text(vram, sm_x + 4, sm_y + 38, (char*)op2, 255);
    }

    draw_text(vram, 0, 152, "RIGHT:VITRINA START:MENU", 255);
    flip();
}

// ============================================================
// RENDER IMAGEN COMPLETA (galería 1)
// ============================================================
static void render_imagen(int idx)
{
    Gema g;
    chunk_a_gema_temp(&g, &items[idx]);

    Opalo o;
    gema_a_opalo_temp(&o, &g);
    generar_paleta(&o);

    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[255] = 0x7FFF;

    uint8_t* buf = get_anim_buf_a();
    renderizar_gema_a_buffer(buf, 240, 160, &g);

    dibujar_fondo_texturizado_optimizado(get_vram(), 0);
    volcar_buf_solo_opalo(buf, 0);

    uint16_t *vram = get_vram();
    // Tipo oculto hasta PULIDA
    if (gema_campo_visible(&g, CAMPO_TIPO)) {
        int t = (g.tipo_real < NUM_TIPOS_OPALO) ? g.tipo_real : 0;
        draw_text(vram, 2, 2, NOMBRE_TIPO[t], 255);
    } else {
        draw_text(vram, 2, 2, "OPALO ???", 255);
    }
    draw_text(vram, 2, 150, "B:VOLVER  IZQ/DER:CAMBIAR", 255);
    flip();
}

// ============================================================
// RENDER GALERÍA 2 (rejilla 3x3)
// ============================================================
static void render_g2(void)
{
    uint16_t* vram = get_vram();
    dibujar_fondo_texturizado_optimizado(vram, 0);

    fill_rect(vram, 0, 0, 240, 14, 4);
    draw_text(vram, 4, 3, "VITRINA", 255);
    char buf[16];
    sprintf(buf, "%d/9", g2_num);
    draw_text(vram, 200, 3, buf, 255);

    for (int row = 0; row < G2_ROWS; row++) {
        for (int col = 0; col < G2_COLS; col++) {
            int idx  = row * G2_COLS + col;
            int cx   = G2_ORIGIN_X + col * G2_CELL_W;
            int cy   = G2_ORIGIN_Y + row * G2_CELL_H;
            int sel  = (idx == g2_cursor);

            uint8_t col_borde = sel ? 3 : 1;
            rect(vram, cx, cy, G2_CELL_W, G2_CELL_H, col_borde);
            if (sel)
                rect(vram, cx + 1, cy + 1, G2_CELL_W - 2, G2_CELL_H - 2, 2);

            if (idx < g2_num) {
                int tx = cx + (G2_CELL_W - G2_THUMB_W) / 2;
                int ty = cy + (G2_CELL_H - G2_THUMB_H) / 2;

                if (g2_items[idx].cortado) {
                    Gema g;
                    chunk_a_gema_temp(&g, &g2_items[idx]);

                    // Paleta por rango: misma fórmula que el código original
                    int base = 16 + (idx * 26);
                    Opalo o;
                    gema_a_opalo_temp(&o, &g);
                    generar_paleta_rango(&o, base, 26);

                    // renderizar_opalo_pequeno_celda ya recibe Gema*
                    renderizar_opalo_pequeno_celda(tx, ty, &g, base, 26);
                }
            } else {
                draw_text(vram, cx + G2_CELL_W/2 - 2,
                                cy + G2_CELL_H/2 - 4, "-", 3);
            }
        }
    }

    init_paleta_ui();

    // Panel inferior
    if (g2_cursor < g2_num) {
        Chunk* c = &g2_items[g2_cursor];
        int iy = G2_ORIGIN_Y + G2_ROWS * G2_CELL_H + 4;
        if (c->cortado) {
            Gema g;
            chunk_a_gema_temp(&g, c);

            if (gema_campo_visible(&g, CAMPO_TIPO)) {
                int t = (g.tipo_real < NUM_TIPOS_OPALO) ? g.tipo_real : 0;
                draw_text(vram, 4, iy, (char*)NOMBRE_TIPO[t], 255);
            } else {
                draw_text(vram, 4, iy, "OPALO ???", 255);
            }
            char vbuf[20];
            if (gema_campo_visible(&g, CAMPO_VALOR))
                sprintf(vbuf, "VAL:%lu", gema_valor_real(&g));
            else
                sprintf(vbuf, "EST:%lu?", gema_valor_estimado(&g));
            draw_text(vram, 140, iy, vbuf, 255);
        } else {
            draw_text(vram, 4, iy, "CHUNK BRUTO", 255);
        }
    }

    if (vista == VISTA_G2_SUBMENU) {
        int sm_x = 60, sm_y = 50, sm_w = 120, sm_h = 42;
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5);
        const char* op0 = (g2_opcion == 0) ? "> DEVOLVER G1" : "  DEVOLVER G1";
        const char* op1 = (g2_opcion == 1) ? "> VER FICHA"   : "  VER FICHA";
        draw_text(vram, sm_x + 6, sm_y + 8,  (char*)op0, 255);
        draw_text(vram, sm_x + 6, sm_y + 24, (char*)op1, 255);
    }

    draw_text(vram, 0, 152, "RIGHT:GALERIA A:OPCIONES", 255);
    flip();
}

// ============================================================
// TRANSICIÓN PARALLAX ENTRE DOS ÓPALOS
// direccion: +1 = saliente sale por derecha, -1 = por izquierda
// ============================================================
#define ANIM_FRAMES 12

static void transicion_gema_relevo(const Gema *sal, const Gema *ent,
                                   int direccion,
                                   const char *nom_sal, const char *nom_ent)
{
    uint8_t* buf_sal = get_anim_buf_a();
    uint8_t* buf_ent = get_anim_buf_b();

    // Prerenderizar los dos ópalos
    {
        Opalo o;
        gema_a_opalo_temp(&o, sal);
        generar_paleta(&o);
    }
    renderizar_gema_a_buffer(buf_sal, 240, 160, sal);
    renderizar_gema_a_buffer(buf_ent, 240, 160, ent);

    precalcular_fondo(0);

    // FASE 1: Salida del ópalo saliente
    for (int f = 1; f <= ANIM_FRAMES; f++) {
        int offset = (f * 240 / ANIM_FRAMES) * direccion;
        volcar_frame(buf_sal, offset, -offset);
        draw_ui_sobre_buffer(nom_sal);
        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
        flip();
    }

    // FASE 2: Cambio de paleta al ópalo entrante
    {
        Opalo o;
        gema_a_opalo_temp(&o, ent);
        generar_paleta(&o);
    }

    // FASE 3: Entrada del ópalo entrante
    for (int f = 1; f <= ANIM_FRAMES; f++) {
        int offset = ((f * 240 / ANIM_FRAMES) - 240) * direccion;
        volcar_frame(buf_ent, offset, -offset);
        draw_ui_sobre_buffer(nom_ent);
        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
        flip();
    }
}

// ============================================================
// INIT
// ============================================================
void galeria_init(void)
{
    Chunk todos[MAX_GALERIA];
    int n = cargar_chunks(todos);
    num_items = 0;
    for (int i = 0; i < n; i++) items[num_items++] = todos[i];

    g2_num = cargar_chunks_galeria2(g2_items);

    cursor = 0; scroll = 0;
    vista  = VISTA_LISTA;
    opcion_submenu = 0;
    g2_cursor = 0; g2_opcion = 0;
    render_lista();
}

// ============================================================
// INPUT
// ============================================================
void galeria_input(uint16_t keys)
{
    if ((keys & KEY_START) && vista != VISTA_FICHA && vista != VISTA_IMAGEN) {
        volver_menu();
        return;
    }

    // ---- GALERÍA 2: REJILLA ----
    if (vista == VISTA_G2) {
        if (keys & KEY_L) {
            Chunk todos[MAX_GALERIA];
            int n = cargar_chunks(todos);
            num_items = 0;
            for (int i = 0; i < n; i++) items[num_items++] = todos[i];
            vista = VISTA_LISTA;
            render_lista();
            return;
        }
        if (keys & KEY_B) {
            Chunk todos[MAX_GALERIA];
            int n = cargar_chunks(todos);
            num_items = 0;
            for (int i = 0; i < n; i++) items[num_items++] = todos[i];
            vista = VISTA_LISTA;
            render_lista();
            return;
        }
        if (keys & KEY_UP)    { if (g2_cursor >= G2_COLS) g2_cursor -= G2_COLS; render_g2(); }
        if (keys & KEY_DOWN)  { if (g2_cursor + G2_COLS < MAX_GALERIA2) g2_cursor += G2_COLS; render_g2(); }
        if (keys & KEY_LEFT)  { if (g2_cursor % G2_COLS > 0) g2_cursor--; render_g2(); }
        if (keys & KEY_RIGHT) { if (g2_cursor % G2_COLS < G2_COLS - 1) g2_cursor++; render_g2(); }
        if ((keys & KEY_A) && g2_cursor < g2_num) {
            vista = VISTA_G2_SUBMENU;
            g2_opcion = 0;
            render_g2();
        }
        return;
    }

    // ---- GALERÍA 2: SUBMENÚ ----
    if (vista == VISTA_G2_SUBMENU) {
        if ((keys & KEY_UP) || (keys & KEY_DOWN)) {
            g2_opcion = !g2_opcion;
            render_g2();
        }
        if (keys & KEY_B) { vista = VISTA_G2; render_g2(); }
        if (keys & KEY_A) {
            if (g2_opcion == 0) {
                devolver_a_galeria1(g2_cursor);
                g2_num = cargar_chunks_galeria2(g2_items);
                if (g2_cursor >= g2_num && g2_cursor > 0) g2_cursor--;
                vista = VISTA_G2;
                render_g2();
            } else {
                vista_anterior = VISTA_G2;
                vista = VISTA_FICHA;
                render_ficha_chunk(&g2_items[g2_cursor], 0);
            }
        }
        return;
    }

    // ---- GALERÍA 1: LISTA ----
    if (vista == VISTA_LISTA) {
        if (keys & KEY_R) {
            vista = VISTA_G2;
            g2_cursor = 0;
            render_g2();
            return;
        }
        if ((keys & KEY_DOWN) && cursor + 1 < num_items) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS) scroll = cursor - LIST_ITEMS + 1;
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
        return;
    }

    // ---- GALERÍA 1: SUBMENÚ ----
    if (vista == VISTA_SUBMENU) {
        if (keys & KEY_UP)   { if (opcion_submenu > 0) opcion_submenu--; render_lista(); }
        if (keys & KEY_DOWN) { if (opcion_submenu < 2) opcion_submenu++; render_lista(); }
        if (keys & KEY_B)    { vista = VISTA_LISTA; render_lista(); }
        if (keys & KEY_A) {
            if (opcion_submenu == 0) {
                if (g2_num < MAX_GALERIA2) {
                    mover_a_galeria2(cursor);
                    galeria_init();
                }
            } else if (opcion_submenu == 1) {
                if (items[cursor].cortado) {
                    vista = VISTA_IMAGEN;
                    render_imagen(cursor);
                } else {
                    vista_anterior = VISTA_LISTA;
                    vista = VISTA_FICHA;
                    render_ficha_chunk(&items[cursor], 1);
                }
            } else {
                vender_item_seleccionado(cursor);
                galeria_init();
            }
        }
        return;
    }

    // ---- FICHA (compartida G1 y G2) ----
    if (vista == VISTA_FICHA) {
        if (keys & KEY_B) {
            vista = vista_anterior;
            if (vista == VISTA_G2) render_g2();
            else                   render_lista();
        }
        return;
    }

    // ---- IMAGEN ----
    if (vista == VISTA_IMAGEN) {
        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
            return;
        }

        if ((keys & KEY_LEFT) && cursor > 0) {
            int prev = cursor - 1;
            while (prev >= 0 && !items[prev].cortado) prev--;
            if (prev >= 0) {
                Gema sal, ent;
                chunk_a_gema_temp(&sal, &items[cursor]);
                chunk_a_gema_temp(&ent, &items[prev]);

                const char* nom_sal = gema_campo_visible(&sal, CAMPO_TIPO)
                    ? NOMBRE_TIPO[sal.tipo_real < NUM_TIPOS_OPALO ? sal.tipo_real : 0]
                    : "OPALO ???";
                const char* nom_ent = gema_campo_visible(&ent, CAMPO_TIPO)
                    ? NOMBRE_TIPO[ent.tipo_real < NUM_TIPOS_OPALO ? ent.tipo_real : 0]
                    : "OPALO ???";

                transicion_gema_relevo(&sal, &ent, 1, nom_sal, nom_ent);
                cursor = prev;
                if (cursor < scroll) scroll = cursor;
                render_imagen(cursor);
            }
        }

        if ((keys & KEY_RIGHT) && cursor + 1 < num_items) {
            int next = cursor + 1;
            while (next < num_items && !items[next].cortado) next++;
            if (next < num_items) {
                Gema sal, ent;
                chunk_a_gema_temp(&sal, &items[cursor]);
                chunk_a_gema_temp(&ent, &items[next]);

                const char* nom_sal = gema_campo_visible(&sal, CAMPO_TIPO)
                    ? NOMBRE_TIPO[sal.tipo_real < NUM_TIPOS_OPALO ? sal.tipo_real : 0]
                    : "OPALO ???";
                const char* nom_ent = gema_campo_visible(&ent, CAMPO_TIPO)
                    ? NOMBRE_TIPO[ent.tipo_real < NUM_TIPOS_OPALO ? ent.tipo_real : 0]
                    : "OPALO ???";

                transicion_gema_relevo(&sal, &ent, -1, nom_sal, nom_ent);
                cursor = next;
                if (cursor >= scroll + LIST_ITEMS) scroll = cursor - LIST_ITEMS + 1;
                render_imagen(cursor);
            }
        }
        return;
    }
}

