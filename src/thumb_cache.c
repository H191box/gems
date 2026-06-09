/*
 * thumb_cache.c
 * Sistema de miniaturas cacheadas — Gacha de Ópalos GBA
 *
 * OPTIMIZACIÓN (Fase 2 del Plan de Rendimiento):
 *   thumb_generar_desde_gema() usa plasma_cache_get() en lugar de
 *   plasma_pixel(). El render de una miniatura pasa de ~3000 llamadas
 *   a plasma a ~3000 lecturas de array en IWRAM — ~100x más rápido.
 *
 *   Además incorpora caché dirty por gema: si la seed y los quilates
 *   no cambiaron, thumb_dibujar() puede usarse directamente sin
 *   regenerar nada.
 */

#include "thumb_cache.h"
#include "plasma.h"
#include "gema.h"

/* ------------------------------------------------------------------ */
/* Helper interno — escribe un píxel en modo 4 (4bpp)                 */
/* ------------------------------------------------------------------ */

static void put_pixel(uint16_t *vram, int x, int y, uint8_t color)
{
    if (x < 0 || x >= 240 || y < 0 || y >= 160) return;

    int idx = y * 120 + x / 2;
    if (x & 1)
        vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)color << 8);
    else
        vram[idx] = (vram[idx] & 0xFF00) | color;
}

/* ------------------------------------------------------------------ */
/* Generación de mini-paleta por tipo                                  */
/* ------------------------------------------------------------------ */

static void generar_minipaleta(ThumbCache *t, TipoOpalo tipo)
{
    for (int i = 0; i < 16; i++) {
        int r, gc, b;
        switch (tipo) {
            case OPALO_NEGRO:
                r = i; gc = i; b = 31; break;
            case OPALO_CRISTAL:
                r = 16 + i / 2; gc = 16 + i / 2; b = 31; break;
            case OPALO_FUEGO:
                r = 31; gc = i * 2; b = i / 2; break;
            case OPALO_ROSA:
                r = 31; gc = 10 + i / 2; b = 20 + i / 2; break;
            case OPALO_GRIS:
                r = 10 + i; gc = 10 + i; b = 10 + i; break;
            default:
                r = i * 2; gc = i * 2; b = 31 - i; break;
        }
        if (r  > 31) r  = 31;
        if (gc > 31) gc = 31;
        if (b  > 31) b  = 31;
        t->palette[i] = (r & 31) | ((gc & 31) << 5) | ((b & 31) << 10);
    }
}

/* ------------------------------------------------------------------ */
/* Render de píxeles del thumbnail usando plasma_cache                 */
/* ------------------------------------------------------------------ */

static void render_pixels(ThumbCache *t, const Gema *g)
{
    int a, b_r;

    /*
     * Calcular radio proporcional a los quilates para que el tamaño
     * de la miniatura refleje el tamaño real de la gema.
     * Máximo: THUMB_W/2 - 2 para no salir del borde.
     */
    int q = g->quilates;
    if (q < 1)   q = 1;
    if (q > 225) q = 225;

    a   = 6 + (q * (THUMB_W / 2 - 8)) / 225;
    b_r = (a * 65 + 50) / 100;
    if (b_r < 3) b_r = 3;

    int cx = THUMB_W / 2;
    int cy = THUMB_H / 2;
    int a2   = a * a;
    int b2   = b_r * b_r;
    int a2b2 = a2 * b2;

    /* Limpiar buffer */
    for (int i = 0; i < THUMB_W * THUMB_H; i++) t->pixels[i] = 0;

    for (int y = 0; y < THUMB_H; y++) {
        int dy  = y - cy;
        int dy2 = dy * dy;

        for (int x = 0; x < THUMB_W; x++) {
            int dx  = x - cx;
            int dx2 = dx * dx;

            if (dx2 * b2 + dy2 * a2 <= a2b2) {
                /*
                 * Mapear coordenadas del thumb al espacio 120x80 del caché.
                 * plasma_cache_get() hace la lectura en IWRAM — ~1 ciclo.
                 */
                int src_x = ((dx + a) * 119) / (a * 2);
                int src_y = ((dy + b_r) * 79) / (b_r * 2);

                uint8_t p = plasma_cache_get(src_x, src_y);

                /* Reducir [16,254] → [0,15] para la mini-paleta de 16 colores */
                t->pixels[y * THUMB_W + x] = (p - 16) >> 4;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

void thumb_generar_desde_gema(ThumbCache *t, const Gema *g)
{
    /* Caché dirty: si la gema no cambió, no hacer nada */
    if (t->ready &&
        t->cached_seed     == g->seed     &&
        t->cached_quilates == g->quilates)
        return;

    /* Asegurar que el plasma base esté actualizado para esta gema */
    plasma_cache_rebuild(g);

    TipoOpalo tipo = gema_tipo(g);
    generar_minipaleta(t, tipo);
    render_pixels(t, g);

    t->cached_seed     = g->seed;
    t->cached_quilates = g->quilates;
    t->ready           = 1;
}

void thumb_generar(ThumbCache *t, uint32_t seed)
{
    /* Compatibilidad: construir Gema mínima desde seed */
    Gema g;
    gema_init(&g);
    g.seed     = seed;
    g.quilates = 50;
    g.etapa    = ETAPA_PULIDA;
    thumb_generar_desde_gema(t, &g);
}

void thumb_dibujar(ThumbCache *t, uint16_t *vram, int ox, int oy)
{
    if (!t->ready) return;

    volatile uint16_t *pal = (volatile uint16_t *)0x05000000;
    for (int i = 0; i < 16; i++)
        pal[i + 16] = t->palette[i];

    for (int y = 0; y < THUMB_H; y++) {
        for (int x = 0; x < THUMB_W; x++) {
            uint8_t c = t->pixels[y * THUMB_W + x];
            if (c > 0)  /* no pintar transparente */
                put_pixel(vram, ox + x, oy + y, (uint8_t)(c + 16));
        }
    }
}
