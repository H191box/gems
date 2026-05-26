#include <stdint.h>
#include <gba_video.h>
#include "video.h"
#include "plasma.h"
#include "opalo.h"

/* ---------------- LUT ---------------- */

static const int16_t sin_lut[64] = {
    0,100,201,301,401,501,601,700,
    799,897,995,1092,1189,1285,1380,1474,
    1567,1660,1751,1842,1931,2019,2106,2191,
    2275,2357,2438,2517,2594,2669,2743,2814,
    2884,2951,3016,3080,3140,3199,3255,3309,
    3360,3409,3456,3500,3541,3579,3615,3648,
    3678,3706,3730,3752,3770,3786,3798,3807,
    3814,3817,3817,3814,3807,3798,3786,3770
};

static inline int16_t lut_s(int i) { return sin_lut[i & 63]; }
static inline int16_t lut_c(int i) { return sin_lut[(i + 16) & 63]; }

/* ---------------- PIXELS ---------------- */

static uint8_t pixel_nebula(int x, int y, uint8_t off) {
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    return (uint8_t)(16 + ((v * 239) >> 8));
}

static uint8_t pixel_venas(int x, int y, uint8_t off) {
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    v += (lut_s((x * 3 + y + off) >> 3) >> 4);
    v &= 255;
    return (uint8_t)(16 + ((v * 239) >> 8));
}

static uint8_t pixel_mosaico(int x, int y, uint8_t off) {
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    v = (v >> 5) << 5;
    return (uint8_t)(16 + ((v * 239) >> 8));
}

static uint8_t pixel_chaos(int x, int y, uint8_t off) {
    x += lut_s((y + off) >> 3) >> 6;
    y += lut_c((x + off) >> 3) >> 6;
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    return (uint8_t)(16 + ((v * 239) >> 8));
}

/* ---------------- PALETA OPALO (IRIDISCENCIA INTEGRADA) ---------------- */

void generar_paleta(Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;
    
    // La iridiscencia controla qué tan amplio es el espectro de colores
    // Escala el factor para que 0 sea un rango estrecho y 31 sea espectro completo
    int spread = 1 + (o->iridiscencia / 8); 

    for (int i = 16; i < 255; i++) {
        int r, g, b;
        switch (o->tipo) {
            case OPALO_NEGRO:
                r = (lut_s((i * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_c((i * spread * 2) + o->color_offset) + 4096) >> 8;
                b = 31;
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 9);
                g = 20 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 9);
                b = 31;
                break;
            case OPALO_FUEGO:
                r = 31;
                g = (lut_s((i * spread) + o->color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + o->color_offset) + 4096) >> 10;
                break;
            default:
                r = (lut_s((i * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_s((i * spread * 2) + o->color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + o->color_offset) + 4096) >> 8;
                break;
        }
        if (r > 31) r = 31; if (g > 31) g = 31; if (b > 31) b = 31;
        pal[i] = (r & 31) | ((g & 31) << 5) | ((b & 31) << 10);
    }
}

/* ... resto de funciones de renderizado (se mantienen iguales) ... */

// Nota: He omitido el resto de las funciones (renderizar_roca, precalcular_grietas, etc.)
// para brevedad, pero úsalas tal cual las tenías, ya que no requieren cambios.
/* ---------------- PALETA ROCA ---------------- */

static void aplicar_paleta_roca(const Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;

    // Grises para textura: índices 10-29
    for (int i = 0; i < 20; i++) {
        int v = 8 + i;
        pal[10 + i] = (v & 31) | ((v & 31) << 5) | ((v & 31) << 10);
    }

    pal[0]   = 0x0000;
    pal[255] = 0x7FFF;

    uint16_t color_grieta;
    switch (o->tipo) {
        case OPALO_NEGRO:
            color_grieta = (14 & 31) | ((2 & 31) << 5) | ((2 & 31) << 10);
            break;
        case OPALO_CRISTAL:
            color_grieta = (24 & 31) | ((26 & 31) << 5) | ((31 & 31) << 10);
            break;
        case OPALO_FUEGO:
            color_grieta = (26 & 31) | ((20 & 31) << 5) | ((10 & 31) << 10);
            break;
        default:
            color_grieta = (28 & 31) | ((31 & 31) << 5) | ((28 & 31) << 10);
            break;
    }
    pal[31] = color_grieta;
}

/* ---------------- GRIETAS ---------------- */

static uint8_t grieta_buf[160][240] __attribute__((section(".ewram")));

static uint32_t rng_next(uint32_t* s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

static void precalcular_grietas(uint32_t seed, uint8_t num_grietas, uint8_t tamanyo) {
    for (int y = 0; y < 160; y++)
        for (int x = 0; x < 240; x++)
            grieta_buf[y][x] = 0;

    if (num_grietas == 0) return;

    int len_base = 15 + (tamanyo - 1) * 10;
    int len_rand = 10 + (tamanyo - 1) * 5;
    int grosor   = (tamanyo >= 3) ? 2 : 1;

    for (uint8_t g = 0; g < num_grietas; g++) {
        uint32_t s = seed ^ (0x1234 * (g + 1));

        int cx      = (int)(rng_next(&s) % 200) + 20;
        int cy      = (int)(rng_next(&s) % 120) + 20;
        int len     = len_base + (int)(rng_next(&s) % len_rand);
        int dx      = (rng_next(&s) & 1) ? 1 : -1;
        int dy_base = (int)(rng_next(&s) % 3) - 1;

        for (int i = 0; i < len; i++) {
            int dy = dy_base;
            if ((i & 3) == 0)
                dy += (int)(rng_next(&s) % 3) - 1;

            for (int gy = 0; gy < grosor; gy++) {
                for (int gx = 0; gx < grosor; gx++) {
                    int px = cx + gx;
                    int py = cy + gy;
                    if (px >= 0 && px < 240 && py >= 0 && py < 160)
                        grieta_buf[py][px] = 1;
                }
            }

            cx += dx;
            cy += dy;
            if (cx < 0 || cx >= 240 || cy < 0 || cy >= 160) break;
        }
    }
}

/* ---------------- RENDER ROCA COMPLETA ---------------- */

void renderizar_roca(const Chunk* c) {
    Opalo o_interior;
    generar_opalo(&o_interior, c->seed);

    aplicar_paleta_roca(&o_interior);
    precalcular_grietas(c->seed, c->grietas, c->tamanyo);

    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(c->seed ^ (c->seed >> 16));

    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x += 2) {
            uint8_t v1 = pixel_nebula(x,     y, off);
            uint8_t v2 = pixel_nebula(x + 1, y, off);
            uint8_t c1 = 10 + (v1 >> 4) % 20;
            uint8_t c2 = 10 + (v2 >> 4) % 20;

            if (grieta_buf[y][x])     c1 = 31;
            if (grieta_buf[y][x + 1]) c2 = 31;

            vram[(y * 120) + (x / 2)] = ((uint16_t)c2 << 8) | c1;
        }
    }
}

/* ---------------- RENDER ROCA PEQUEÑA ---------------- */
// Miniatura 80x80 de la roca, posicionable en cualquier (x_pos, y_pos).
// x_pos debe ser par para evitar desplazamiento de medio word.
// Las grietas se escalan igual que la textura (muestreo 2x2).
// Usa la paleta de roca (índices 10-29 y 31); llamar a init_paleta_ui()
// después si se necesitan los colores UI de vuelta.

void renderizar_roca_pequena(int x_pos, int y_pos, const Chunk* c) {
    Opalo o_interior;
    generar_opalo(&o_interior, c->seed);

    aplicar_paleta_roca(&o_interior);
    // Precalculamos las grietas en coordenadas de pantalla completa;
    // luego muestreamos en 2x para la miniatura.
    precalcular_grietas(c->seed, c->grietas, c->tamanyo);

    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(c->seed ^ (c->seed >> 16));

    int w = 80;
    int h = 80;

    for (int y = 0; y < h; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        // Coordenada de muestreo en espacio pantalla-completa (escala 2x)
        int src_y = y * 2;
        if (src_y >= 160) src_y = 159;

        for (int x = 0; x < w; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int src_x = x * 2;
            if (src_x >= 240) src_x = 238;

            uint8_t v  = pixel_nebula(src_x, src_y, off);
            uint8_t ci = 10 + (v >> 4) % 20;

            // Grieta: si cualquiera de los 4 subpíxeles es grieta, la marcamos
            if (grieta_buf[src_y][src_x] ||
                (src_x + 1 < 240 && grieta_buf[src_y][src_x + 1]) ||
                (src_y + 1 < 160 && grieta_buf[src_y + 1][src_x]))
                ci = 31;

            int idx = screen_y * 120 + screen_x / 2;
            if (screen_x & 1)
                vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)ci << 8);
            else
                vram[idx] = (vram[idx] & 0xFF00) | ci;
        }
    }
}

/* ---------------- RENDER OPALO COMPLETO ---------------- */

void renderizar_opalo(Opalo* o) {
    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(o->seed ^ (o->seed >> 16));
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x += 2) {
            uint8_t c1 = plasma_pixel(x,     y, off, o);
            uint8_t c2 = plasma_pixel(x + 1, y, off, o);
            vram[(y * 120) + (x / 2)] = ((uint16_t)c2 << 8) | c1;
        }
    }
}

/* ---------------- CORE PIXEL ---------------- */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o) {
    switch (o->patron) {
        case PATRON_VENAS:   return pixel_venas(x, y, off);
        case PATRON_MOSAICO: return pixel_mosaico(x, y, off);
        case PATRON_CHAOS:   return pixel_chaos(x, y, off);
        default:             return pixel_nebula(x, y, off);
    }
}

/* ---------------- FLASH GUARDADO ---------------- */

void flash_guardado(void) {
    uint16_t* pal = (uint16_t*)0x05000000;
    for (int i = 0; i < 256; i++) pal[i] = 0x7FFF;
}

/* ---------------- RENDER OPALO PEQUEÑO ---------------- */

void renderizar_opalo_pequeno(int x_pos, int y_pos, Opalo* o) {
    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(o->seed ^ (o->seed >> 16));
    int w = 80;
    int h = 80;

    for (int y = 0; y < h; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        for (int x = 0; x < w; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            uint8_t c = plasma_pixel(x * 2, y * 2, off, o);

            int idx = screen_y * 120 + screen_x / 2;
            if (screen_x & 1)
                vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)c << 8);
            else
                vram[idx] = (vram[idx] & 0xFF00) | c;
        }
    }
}
