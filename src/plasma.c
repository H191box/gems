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

/* ---------------- CORE PIXEL ---------------- */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o) {
    switch (o->patron) {
        case PATRON_VENAS:   return pixel_venas(x, y, off);
        case PATRON_MOSAICO: return pixel_mosaico(x, y, off);
        case PATRON_CHAOS:   return pixel_chaos(x, y, off);
        default:             return pixel_nebula(x, y, off);
    }
}

/* ---------------- BILINEAR + DOBLE CAPA (suavizado de plasma) ---------------- */
// Muestra el plasma interpolando 4 vecinos (bilinear aproximado en enteros)
// y mezcla una segunda capa con frecuencia diferente para iridiscencia fluida.

static uint8_t plasma_pixel_smooth(int fx, int fy, uint8_t off, const Opalo* o) {
    // fx, fy en espacio de coordenadas "full res" (pueden ser fraccionarios en 4x4)
    // Usamos punto fijo: fx y fy son enteros pero representan coordenada*4 implícita.
    // En la práctica: llamamos con coordenadas reales escaladas,
    // e interpolamos entre (x,y), (x+1,y), (x,y+1), (x+1,y+1).

    uint8_t c00 = plasma_pixel(fx,     fy,     off, o);
    uint8_t c10 = plasma_pixel(fx + 1, fy,     off, o);
    uint8_t c01 = plasma_pixel(fx,     fy + 1, off, o);
    uint8_t c11 = plasma_pixel(fx + 1, fy + 1, off, o);

    // Promedio simple de los 4 vecinos (bilinear 50/50)
    int avg = ((int)c00 + c10 + c01 + c11) >> 2;

    // Segunda capa: plasma con coordenadas rotadas 45° y escala diferente
    // para simular la profundidad del ópalo (estructura interna difractiva).
    // Rotación 45°: x' = (x+y)/2, y' = (y-x)/2
    int rx = (fx + fy) >> 1;
    int ry = (fy - fx + 256) >> 1;  // +256 para evitar negativos
    uint8_t layer2 = plasma_pixel(rx, ry, off + 37, o);  // offset distinto → desfase de fase

    // Mezcla 60% capa principal suavizada + 40% capa secundaria
    int blended = (avg * 6 + (int)layer2 * 4) / 10;
    if (blended < 16)  blended = 16;
    if (blended > 254) blended = 254;
    return (uint8_t)blended;
}

/* ---------------- PALETA OPALO (IRIDISCENCIA INTEGRADA) ---------------- */

void generar_paleta(Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;

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

/* ---------------- PALETA ROCA ---------------- */

static void aplicar_paleta_roca(const Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;

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

void renderizar_roca_pequena(int x_pos, int y_pos, const Chunk* c) {
    Opalo o_interior;
    generar_opalo(&o_interior, c->seed);

    aplicar_paleta_roca(&o_interior);
    precalcular_grietas(c->seed, c->grietas, c->tamanyo);

    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(c->seed ^ (c->seed >> 16));

    int w = 80;
    int h = 80;

    for (int y = 0; y < h; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int src_y = y * 2;
        if (src_y >= 160) src_y = 159;

        for (int x = 0; x < w; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int src_x = x * 2;
            if (src_x >= 240) src_x = 238;

            uint8_t v  = pixel_nebula(src_x, src_y, off);
            uint8_t ci = 10 + (v >> 4) % 20;

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
            uint8_t c1 = plasma_pixel_smooth(x,     y, off, o);
            uint8_t c2 = plasma_pixel_smooth(x + 1, y, off, o);
            vram[(y * 120) + (x / 2)] = ((uint16_t)c2 << 8) | c1;
        }
    }
}

/* ---------------- FLASH GUARDADO ---------------- */

void flash_guardado(void) {
    uint16_t* pal = (uint16_t*)0x05000000;
    for (int i = 0; i < 256; i++) pal[i] = 0x7FFF;
}

/* ---------------- RENDER OPALO PEQUEÑO CON CABUJÓN 3D ---------------- */
// Elipse cabujón con:
//   - Máscara elíptica (solo pinta dentro)
//   - Borde de 2px con color claro (índice de paleta 254 ≈ blanco iridiscente)
//   - Highlight semitransparente: elipse interior desplazada arriba-izquierda,
//     dibujada con paleta color 254 cada N píxeles para simular reflejo especular
//   - Plasma suavizado (bilinear + doble capa)
//
// Tamaños de radio mayor según tamanyo (1..5):
//   S(1)=15, M(2)=24, L(3)=33, XL(4)=42, XXL(5)=38 (radio mayor)
//   Radio menor = radio_mayor * 0.65 para forma cabujón natural.

void renderizar_opalo_pequeno(int x_pos, int y_pos, Opalo* o, uint8_t tamanyo) {
    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(o->seed ^ (o->seed >> 16));

    // Tabla de radios: índice 0..4 → tamanyo 1..5
    // Radio mayor (a) y menor (b) de la elipse principal
    static const uint8_t rad_a[5] = { 15, 24, 31, 38, 38 };
    static const uint8_t rad_b[5] = { 10, 16, 20, 25, 25 };

    int ti = (tamanyo >= 1 && tamanyo <= 5) ? (tamanyo - 1) : 2;
    int a  = rad_a[ti];  // radio horizontal
    int b  = rad_b[ti];  // radio vertical

    // Centro de la elipse dentro del panel de 80x80
    int cx = 40;
    int cy = 42;

    // Pre-calculamos a² y b² para la ecuación de la elipse
    int a2 = a * a;
    int b2 = b * b;

    // Radio del borde: 2px hacia fuera (usamos a+2, b+2)
    int ab = a + 2;
    int bb = b + 2;
    int ab2 = ab * ab;
    int bb2 = bb * bb;

    // Highlight: elipse interior desplazada (-a*0.25, -b*0.30)
    int hx_off = -(a / 4);
    int hy_off = -(b * 3 / 10);
    int ha  = a * 6 / 10;   // radio horizontal del highlight
    int hb  = b * 4 / 10;   // radio vertical del highlight
    int ha2 = ha * ha;
    int hb2 = hb * hb;

    for (int y = 0; y < 80; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int dy  = y - cy;
        int dy2 = dy * dy;

        for (int x = 0; x < 80; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int dx  = x - cx;
            int dx2 = dx * dx;

            // Test de pertenencia: dentro del ópalo, borde, o highlight
            // Ecuación elipse: (dx²/a²) + (dy²/b²) <= 1
            // En enteros: dx²*b² + dy²*a² <= a²*b²

            int inside_main   = (dx2 * b2 + dy2 * a2 <= a2 * b2);
            int inside_border  = (dx2 * bb2 + dy2 * ab2 <= ab2 * bb2);

            // Highlight: centrado en (cx+hx_off, cy+hy_off)
            int hdx  = dx - hx_off;
            int hdy  = dy - hy_off;
            int inside_hl = (hb2 * hdx * hdx + ha2 * hdy * hdy <= ha2 * hb2);

            uint8_t color;

            if (inside_main) {
                // Plasma suavizado: escalamos coordenadas al espacio 240x160
                int src_x = (dx + a) * 120 / (a * 2);
                int src_y = (dy + b) *  80 / (b * 2);
                if (src_x < 0)   src_x = 0;
                if (src_x > 238) src_x = 238;
                if (src_y < 0)   src_y = 0;
                if (src_y > 158) src_y = 158;

                color = plasma_pixel_smooth(src_x, src_y, off, o);

                // Highlight especular: sobrescribimos con blanco parcial
                // Solo en la mitad superior-izquierda del highlight
                if (inside_hl && hdy < 0) {
                    // Fade: cuanto más en el centro del highlight más blanco
                    // Usamos índice 254 (casi blanco en paleta iridiscente)
                    // con densidad proporcional a la distancia al centro del hl
                    int dist2 = hdx * hdx + hdy * hdy;
                    int max2  = (ha > hb ? ha : hb);
                    max2 = max2 * max2;
                    // Solo pintamos si dist < 60% del radio máximo
                    if (dist2 < (max2 * 6 / 10)) {
                        color = 254;
                    }
                }

                // Sombra de volumen: píxeles en el borde inferior-derecho
                // se oscurecen mezclando hacia el índice 16 (más oscuro)
                int edge_dist2 = (dx2 * b2 + dy2 * a2);
                int full2      = a2 * b2;
                // Si estamos en el 15% exterior del ópalo y en cuadrante inf-der
                if (edge_dist2 > (full2 * 85 / 100) && dx >= 0 && dy >= 0) {
                    // Oscurecer: bajar índice hacia 16
                    int c_int = (int)color - 20;
                    if (c_int < 16) c_int = 16;
                    color = (uint8_t)c_int;
                }

            } else if (inside_border) {
                // Borde brillante: índice 254 (blanco iridiscente en paleta opalo)
                color = 254;
            } else {
                // Fuera de la elipse: no pintamos (dejamos lo que había)
                continue;
            }

            int idx = screen_y * 120 + screen_x / 2;
            if (screen_x & 1)
                vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)color << 8);
            else
                vram[idx] = (vram[idx] & 0xFF00) | color;
        }
    }
}
