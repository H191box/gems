#include <stdint.h>
#include "gema.h"
#include "plasma.h"

/* ---------------- LUT ---------------- */
static const int16_t sin_lut[64] = {
    0,100,201,301,401,501,601,700,799,897,995,1092,1189,1285,1380,1474,
    1567,1660,1751,1842,1931,2019,2106,2191,2275,2357,2438,2517,2594,2669,2743,2814,
    2884,2951,3016,3080,3140,3199,3255,3309,3360,3409,3456,3500,3541,3579,3615,3648,
    3678,3706,3730,3752,3770,3786,3798,3807,3814,3817,3817,3814,3807,3798,3786,3770
};

inline int16_t lut_s(int i) { return sin_lut[i & 63]; }
inline int16_t lut_c(int i) { return sin_lut[(i + 16) & 63]; }

/* ---------------- PIXELS (Privados) ---------------- */
uint8_t pixel_nebula(int x, int y, uint8_t off) {
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    return (uint8_t)(16 + ((v * 239) >> 8));
}

static uint8_t pixel_venas(int x, int y, uint8_t off) { return 0; }
static uint8_t pixel_mosaico(int x, int y, uint8_t off) { return 0; }
static uint8_t pixel_chaos(int x, int y, uint8_t off) { return 0; }
static uint8_t pixel_harlequin(int x, int y, uint8_t off) { return 0; }

static uint8_t pixel_matrix(int x, int y, uint8_t off, const Gema* g) {
    uint8_t v_roca = pixel_nebula(x, y, off);
    uint8_t base   = 10 + (v_roca >> 4) % 20;

    int p1 = (lut_s((x + off + 37) >> 3) + lut_c((y + off + 37) >> 3) + 128) & 255;
    int p2 = (lut_s((x * 2 + y + off) >> 4) + lut_c((y * 2 - x + off) >> 4) + 128) & 255;
    int mancha = (p1 + p2) >> 1;

    int umbral = 80 + (gema_iridiscencia(g) >> 1);
    if (umbral > 200) umbral = 200;

    if (mancha > umbral) {
        uint8_t p = pixel_nebula(x, y, off + 73);
        int intensidad = mancha - umbral;
        if (intensidad > 120) intensidad = 120;
        
        int mezclado = ((int)base * (120 - intensidad) + (int)p * intensidad) / 120;
        if (mezclado < 16)  mezclado = 16;
        if (mezclado > 254) mezclado = 254;
        return (uint8_t)mezclado;
    }

    return base;
}

/* ---------------- PÚBLICAS ---------------- */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Gema* g) {
    switch (gema_patron(g)) {
        case PATRON_VENAS:     return pixel_venas(x, y, off);
        case PATRON_MOSAICO:   return pixel_mosaico(x, y, off);
        case PATRON_CHAOS:     return pixel_chaos(x, y, off);
        case PATRON_HARLEQUIN: return pixel_harlequin(x, y, off);
        case PATRON_MATRIX:    return pixel_matrix(x, y, off, g);
        default:               return pixel_nebula(x, y, off);
    }
}

uint8_t plasma_pixel_smooth(int x, int y, uint8_t off, const Gema* g) {
    uint8_t c00 = plasma_pixel(x,     y,     off, g);
    uint8_t c10 = plasma_pixel(x + 1, y,     off, g);
    uint8_t c01 = plasma_pixel(x,     y + 1, off, g);
    uint8_t c11 = plasma_pixel(x + 1, y + 1, off, g);

    int avg = ((int)c00 + c10 + c01 + c11) >> 2;

    int rx = (x + y) >> 1;
    int ry = (y - x + 256) >> 1;
    
    uint8_t layer2 = plasma_pixel(rx, ry, off + 37, g);

    int blended = (avg * 6 + (int)layer2 * 4) / 10;
    
    if (blended < 16)  blended = 16;
    if (blended > 254) blended = 254;
    
    return (uint8_t)blended;
}
