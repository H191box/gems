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

/* ---------------- PALETA ---------------- */

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

        pal[i] = (r & 31) |
                 ((g & 31) << 5) |
                 ((b & 31) << 10);
    }

    pal[255] = 0x7FFF;
}

/* ---------------- RENDER ---------------- */

void renderizar_opalo(Opalo* o) {

    uint16_t* vram = get_vram();

    uint8_t off = o->seed ^ (o->seed >> 16);

    for (int y = 0; y < 160; y++) {

        for (int x = 0; x < 240; x += 2) {

            uint8_t c1 = plasma_pixel(x, y, off, o);
            uint8_t c2 = plasma_pixel(x + 1, y, off, o);

            vram[(y * 120) + (x / 2)] = (c2 << 8) | c1;
        }
    }
}

/* ---------------- CORE PIXEL ---------------- */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o) {

    switch (o->patron) {

        case PATRON_VENAS:
            return pixel_venas(x, y, off);

        case PATRON_MOSAICO:
            return pixel_mosaico(x, y, off);

        case PATRON_CHAOS:
            return pixel_chaos(x, y, off);

        default:
            return pixel_nebula(x, y, off);
    }
}

/* ---------------- FLASH ---------------- */

void flash_guardado(void) {

    uint16_t* pal = (uint16_t*)0x05000000;

    for (int i = 0; i < 256; i++) {
        pal[i] = 0x7FFF;
    }
}
