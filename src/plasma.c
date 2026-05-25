// 2.0

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
    return (uint8_t)((lut_s((x + off) >> 2) +
                      lut_c((y + off) >> 2) +
                      128) & 255);
}

static uint8_t pixel_venas(int x, int y, uint8_t off) {
    uint8_t v = pixel_nebula(x, y, off);
    v += (lut_s((x * 3 + y + off) >> 3) >> 4);
    return v;
}

static uint8_t pixel_mosaico(int x, int y, uint8_t off) {
    uint8_t v = pixel_nebula(x, y, off);
    return (v >> 5) << 5;
}

static uint8_t pixel_chaos(int x, int y, uint8_t off) {
    x += lut_s((y + off) >> 3) >> 6;
    y += lut_c((x + off) >> 3) >> 6;
    return pixel_nebula(x, y, off);
}

/* ---------------- PALETA OPALO ---------------- */

void generar_paleta(Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;
    for (int i = 0; i < 256; i++) {
        int r, g, b;
        switch (o->tipo) {
            case OPALO_NEGRO:
                r = (lut_s(i + o->color_offset) + 4096) >> 8;
                g = (lut_c(i * 2 + o->color_offset) + 4096) >> 8;
                b = 31;
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s(i + o->color_offset) + 4096) >> 9);
                g = 20 + ((lut_c(i + o->color_offset) + 4096) >> 9);
                b = 31;
                break;
            case OPALO_FUEGO:
                r = 31;
                g = (lut_s(i + o->color_offset) + 4096) >> 8;
                b = (lut_c(i + o->color_offset) + 4096) >> 10;
                break;
            default:
                r = (lut_s(i + o->color_offset) + 4096) >> 8;
                g = (lut_s(i * 2 + o->color_offset) + 4096) >> 8;
                b = (lut_c(i + o->color_offset) + 4096) >> 8;
                break;
        }
        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (b > 31) b = 31;
        pal[i] = (r & 31) | ((g & 31) << 5) | ((b & 31) << 10);
    }
    pal[255] = 0x7FFF;
}

/* ---------------- PALETA ROCA ---------------- */

static void aplicar_paleta_roca(const Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;

    // Grises para la textura (índices 10-29)
    for (int i = 0; i < 20; i++) {
        int v = 8 + i;
        pal[10 + i] = (v & 31) | ((v & 31) << 5) | ((v & 31) << 10);
    }

    pal[0]   = 0x0000; // negro
    pal[255] = 0x7FFF; // blanco texto

    // Color de grieta según tipo de ópalo interior
    // Saturados y visibles sobre la roca gris
// Color de grieta según tipo de ópalo interior
    uint16_t color_grieta;
    switch (o->tipo) {
        case OPALO_NEGRO:
            // Rojo muy oscuro
            color_grieta = (14 & 31) | ((2 & 31) << 5) | ((2 & 31) << 10);
            break;
        case OPALO_CRISTAL:
            // Blanco azulado
            color_grieta = (24 & 31) | ((26 & 31) << 5) | ((31 & 31) << 10);
            break;
        case OPALO_FUEGO:
            // Marrón amarillento / gris anaranjado cálido
            color_grieta = (26 & 31) | ((20 & 31) << 5) | ((10 & 31) << 10);
            break;
        default:
            // Blanco con ligero toque verdoso
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
// Longitud reducida: ahora tamanyo 1 → (15-25px), tamanyo 5 → (55-85px)
    int len_base = 15 + (tamanyo - 1) * 10;
    int len_rand = 10 + (tamanyo - 1) * 5;

    // Grosor máximo limitado a 2: 
    // tamanyo 1-2 → 1px, tamanyo 3-5 → 2px
    int grosor = (tamanyo >= 3) ? 2 : 1;

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

            // Dibujar con grosor máximo 2
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

/* ---------------- RENDER ROCA ---------------- */

void renderizar_roca(const Chunk* c) {
    // Generamos el ópalo interior para conocer su tipo y colorear las grietas
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

            if (grieta_buf[y][x])     c1 = 31; // índice 31 = color del tipo de ópalo
            if (grieta_buf[y][x + 1]) c2 = 31;

            vram[(y * 120) + (x / 2)] = (c2 << 8) | c1;
        }
    }
}

/* ---------------- RENDER OPALO ---------------- */

void renderizar_opalo(Opalo* o) {
    uint16_t* vram = get_vram();
    uint8_t off = o->seed ^ (o->seed >> 16);
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x += 2) {
            uint8_t c1 = plasma_pixel(x,     y, off, o);
            uint8_t c2 = plasma_pixel(x + 1, y, off, o);
            vram[(y * 120) + (x / 2)] = (c2 << 8) | c1;
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

/* ---------------- FLASH ---------------- */

void flash_guardado(void) {
    uint16_t* pal = (uint16_t*)0x05000000;
    for (int i = 0; i < 256; i++) pal[i] = 0x7FFF;
}
