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

/* ----------------------------------------------------------------
   PATRON_HARLEQUIN
   Rombos duros tipo diamante/harlequin.
   Cada rombo tiene tamaño fijo de ~12px, con color uniforme
   por celda determinado por el plasma del centro del rombo.
   El borde entre rombos es un píxel oscuro (índice bajo).
---------------------------------------------------------------- */
static uint8_t pixel_harlequin(int x, int y, uint8_t off) {
    // Coordenadas en espacio rombo (rotación 45°)
    int u = x + y;
    int v = x - y;
    int cell_size = 12;

    // Celda
    int cu = u / cell_size;
    int cv = v / cell_size;

    // Posición dentro de la celda
    int ru = u - cu * cell_size;
    int rv = v - cv * cell_size;
    if (ru < 0) { ru += cell_size; cu--; }
    if (rv < 0) { rv += cell_size; cv--; }

    // Borde de 1px entre rombos
    if (ru == 0 || rv == 0)
        return 16; // borde oscuro

    // Color del rombo: plasma evaluado en el centro de la celda
    int cx_cell = (cu * cell_size + cell_size / 2);
    int cy_cell = (cv * cell_size + cell_size / 2);
    // Volver a coordenadas xy desde uv
    int sx = (cx_cell + cy_cell) / 2;
    int sy = (cx_cell - cy_cell) / 2;

    int p = (lut_s((sx + off) >> 2) + lut_c((sy + off) >> 2) + 128) & 255;
    // Cuantizar a 8 niveles para que cada rombo tenga color sólido
    p = (p >> 5) << 5;
    return (uint8_t)(16 + ((p * 239) >> 8));
}

/* ----------------------------------------------------------------
   PATRON_MATRIX
   Base gris de la roca con manchas iridiscentes encima.
   La "mancha" se activa donde el plasma supera un umbral
   variable por seed — da manchas grandes o pequeñas.
   La intensidad iridiscente de cada mancha varía con una
   segunda capa de plasma, creando alta variabilidad visual.
---------------------------------------------------------------- */
static uint8_t pixel_matrix(int x, int y, uint8_t off, const Opalo* o) {
    // Capa base: gris de roca (igual que renderizar_roca)
    uint8_t v_roca = pixel_nebula(x, y, off);
    uint8_t base   = 10 + (v_roca >> 4) % 20; // índice gris 10..29

    // Capa de manchas: plasma con offset diferente
    int p1 = (lut_s((x + off + 37) >> 3) + lut_c((y + off + 37) >> 3) + 128) & 255;
    // Segunda capa para forma de mancha más orgánica
    int p2 = (lut_s((x * 2 + y + off) >> 4) + lut_c((y * 2 - x + off) >> 4) + 128) & 255;
    int mancha = (p1 + p2) >> 1;

    // Umbral: seed controla qué tan "manchado" es el ópalo
    // color_offset bajo = pocas manchas, alto = muchas manchas
    int umbral = 80 + (o->color_offset >> 1); // 80..207
    if (umbral > 200) umbral = 200;

    if (mancha > umbral) {
        // Dentro de la mancha: color iridiscente del plasma normal
        uint8_t p = pixel_nebula(x, y, off + 73);
        // Mezcla con la base gris según intensidad de la mancha
        // manchas más centradas (mancha muy alto) = más color
        int intensidad = mancha - umbral; // 0..~175
        if (intensidad > 120) intensidad = 120;
        // mezcla: más intensidad = más plasma, menos gris
        int mezclado = ((int)base * (120 - intensidad) + (int)p * intensidad) / 120;
        if (mezclado < 16)  mezclado = 16;
        if (mezclado > 254) mezclado = 254;
        return (uint8_t)mezclado;
    }

    return base;
}

/* ---------------- CORE PIXEL ---------------- */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Opalo* o) {
    switch (o->patron) {
        case PATRON_VENAS:      return pixel_venas(x, y, off);
        case PATRON_MOSAICO:    return pixel_mosaico(x, y, off);
        case PATRON_CHAOS:      return pixel_chaos(x, y, off);
        case PATRON_HARLEQUIN:  return pixel_harlequin(x, y, off);
        case PATRON_MATRIX:     return pixel_matrix(x, y, off, o);
        default:                return pixel_nebula(x, y, off);
    }
}

/* ---------------- BILINEAR + DOBLE CAPA ---------------- */

static uint8_t plasma_pixel_smooth(int fx, int fy, uint8_t off, const Opalo* o) {
    uint8_t c00 = plasma_pixel(fx,     fy,     off, o);
    uint8_t c10 = plasma_pixel(fx + 1, fy,     off, o);
    uint8_t c01 = plasma_pixel(fx,     fy + 1, off, o);
    uint8_t c11 = plasma_pixel(fx + 1, fy + 1, off, o);

    int avg = ((int)c00 + c10 + c01 + c11) >> 2;

    int rx = (fx + fy) >> 1;
    int ry = (fy - fx + 256) >> 1;
    uint8_t layer2 = plasma_pixel(rx, ry, off + 37, o);

    int blended = (avg * 6 + (int)layer2 * 4) / 10;
    if (blended < 16)  blended = 16;
    if (blended > 254) blended = 254;
    return (uint8_t)blended;
}

/* ---------------- PALETA OPALO ---------------- */

void generar_paleta(Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;
    int spread = 1 + (o->iridiscencia / 8);
    for (int i = 16; i < 255; i++) {
        int r, g, b;
        switch (o->tipo) {
            case OPALO_NEGRO:
                r = (lut_s((i * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_c((i * spread * 2) + o->color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((i * spread * 2) + o->color_offset) + 4096) >> 11); // 26..27
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 9);
                g = 20 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 9);
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

    int borde_v = 24 + ((o->brillo - 16) * 4 / 15);
    int tinte   = (o->color_offset & 3) - 1;
    int borde_r = borde_v + tinte;
    int borde_b = borde_v - tinte;
    if (borde_r > 30) borde_r = 30; if (borde_r < 22) borde_r = 22;
    if (borde_b > 30) borde_b = 30; if (borde_b < 22) borde_b = 22;
    pal[15] = (borde_r & 31) | ((borde_v & 31) << 5) | ((borde_b & 31) << 10);

    int hl_base = 29 + ((o->brillo - 16) * 3 / 15);
    int hl_r, hl_g, hl_b;
    switch (o->tipo) {
        case OPALO_CRISTAL:
            hl_r = 31; hl_g = 31; hl_b = 31; break;
        case OPALO_FUEGO:
            hl_r = 30;
            hl_g = 29 + ((o->brillo - 16) * 6 / 15);
            hl_b = 12  + ((o->brillo - 16) * 4 / 15);
            break;
        case OPALO_NEGRO:
            hl_r = hl_base - 2; hl_g = hl_base - 1; hl_b = hl_base;
            if (hl_r < 30) hl_r = 30;
            break;
        default:
            hl_r = hl_base; hl_g = hl_base - 1; hl_b = hl_base - 2;
            if (hl_g < 30) hl_g = 30;
            if (hl_b < 30) hl_b = 30;
            break;
    }
    pal[254] = (hl_r & 31) | ((hl_g & 31) << 5) | ((hl_b & 31) << 10);
}

/* ================================================================
   PALETA ROCA
================================================================ */

static void aplicar_paleta_roca(Opalo* o_interior) {
    generar_paleta(o_interior);

    uint16_t* pal = (uint16_t*)0x05000000;

    for (int i = 0; i < 20; i++) {
        int v = 8 + i;
        pal[10 + i] = (v & 31) | ((v & 31) << 5) | ((v & 31) << 10);
    }

    pal[0]   = 0x0000;
    pal[255] = 0x7FFF;

    switch (o_interior->tipo) {
        case OPALO_NEGRO:
            pal[9] = (2 & 31) | ((1 & 31) << 5) | ((4 & 31) << 10); break;
        case OPALO_CRISTAL:
            pal[9] = (3 & 31) | ((3 & 31) << 5) | ((6 & 31) << 10); break;
        case OPALO_FUEGO:
            pal[9] = (6 & 31) | ((2 & 31) << 5) | ((1 & 31) << 10); break;
        default:
            pal[9] = (4 & 31) | ((4 & 31) << 5) | ((5 & 31) << 10); break;
    }
}

/* ================================================================
   BUFFER DE GRIETAS
================================================================ */

static uint8_t grieta_buf[160][240] __attribute__((section(".ewram")));

static uint32_t rng_next(uint32_t* s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

static void plot_grieta_pixel(int px, int py,
                              int halo,
                              int cx, int cy, int ra2, int rb2) {
    if (px < 0 || px >= 240 || py < 0 || py >= 160) return;

    int dx = px - cx;
    int dy = py - cy;
    if ((int64_t)dx*dx * rb2 + (int64_t)dy*dy * ra2 > (int64_t)ra2 * rb2)
        return;

    if (grieta_buf[py][px] < 0x01)
        grieta_buf[py][px] = 0x01;

    if (halo >= 1) {
        const int offs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int k = 0; k < 4; k++) {
            int nx = px + offs[k][0];
            int ny = py + offs[k][1];
            if (nx < 0 || nx >= 240 || ny < 0 || ny >= 160) continue;
            int dx2 = nx - cx, dy2 = ny - cy;
            if ((int64_t)dx2*dx2 * rb2 + (int64_t)dy2*dy2 * ra2
                    > (int64_t)ra2 * rb2) continue;
            if (grieta_buf[ny][nx] < 0x02)
                grieta_buf[ny][nx] = 0x02;
        }
    }

    if (halo >= 2) {
        const int offs2[8][2] = {
            {2,0},{-2,0},{0,2},{0,-2},
            {1,1},{1,-1},{-1,1},{-1,-1}
        };
        for (int k = 0; k < 8; k++) {
            int nx = px + offs2[k][0];
            int ny = py + offs2[k][1];
            if (nx < 0 || nx >= 240 || ny < 0 || ny >= 160) continue;
            int dx2 = nx - cx, dy2 = ny - cy;
            if ((int64_t)dx2*dx2 * rb2 + (int64_t)dy2*dy2 * ra2
                    > (int64_t)ra2 * rb2) continue;
            if (grieta_buf[ny][nx] < 0x03)
                grieta_buf[ny][nx] = 0x03;
        }
    }
}

static void precalcular_grietas(uint32_t seed,
                                uint8_t  num_grietas,
                                const Chunk* c,
                                const Opalo* o,
                                int cx, int cy, int ra, int rb) {
    for (int y = 0; y < 160; y++)
        for (int x = 0; x < 240; x++)
            grieta_buf[y][x] = 0;

    if (num_grietas == 0) return;

    int ra2 = ra * ra;
    int rb2 = rb * rb;
    int halo = (o->brillo >= 24) ? 2 : 1;

    for (uint8_t g = 0; g < num_grietas; g++) {
        uint32_t s = seed ^ (0x1234u * (uint32_t)(g + 1));

        int ox = cx, oy = cy;
        for (int tries = 0; tries < 8; tries++) {
            int tx = cx + (int)(rng_next(&s) % (ra * 2 + 1)) - ra;
            int ty = cy + (int)(rng_next(&s) % (rb * 2 + 1)) - rb;
            int ddx = tx - cx, ddy = ty - cy;
            if ((int64_t)ddx*ddx * rb2 + (int64_t)ddy*ddy * ra2
                    <= (int64_t)ra2 * rb2) {
                ox = tx; oy = ty; break;
            }
        }

        int len = 10 + (int)(c->tamanyo - 1) * 5
                     + (int)(rng_next(&s) % 11);

        int dx_fp, dy_fp;
        switch (c->forma_grieta & 3) {
            case 0: dx_fp = 256; dy_fp =   0; break;
            case 1: dx_fp = 181; dy_fp = 181; break;
            case 2: dx_fp =   0; dy_fp = 256; break;
            default:dx_fp = 181; dy_fp =-181; break;
        }

        if (rng_next(&s) & 1) dx_fp = -dx_fp;
        if (rng_next(&s) & 1) dy_fp = -dy_fp;

        int px_fp = ox << 8;
        int py_fp = oy << 8;

        for (int i = 0; i < len; i++) {
            int px = px_fp >> 8;
            int py = py_fp >> 8;

            if (px < 0 || px >= 240 || py < 0 || py >= 160) break;

            plot_grieta_pixel(px, py, halo, cx, cy, ra2, rb2);

            px_fp += dx_fp;
            py_fp += dy_fp;

            if ((i & 3) == 3) {
                uint8_t off_g = (uint8_t)(seed >> 8);
                int pv = (int)pixel_nebula(px >> 2, py >> 2, off_g);
                int nudge = ((pv - 135) >> 6);
                dy_fp += nudge * 64;
                if (dy_fp >  256) dy_fp =  256;
                if (dy_fp < -256) dy_fp = -256;
            }
        }
    }
}

/* ================================================================
   RENDER ROCA COMPLETA
================================================================ */

#define OFF_INTERIOR_XOR 0x5A

void renderizar_roca(const Chunk* c) {
    Opalo o_interior;
    generar_opalo(&o_interior, c->seed);

    int ra, rb;
    {
        int q = c->quilates;
        if (q < 1)   q = 1;
        if (q > 225) q = 225;
        ra = 5 + (q * 33) / 225;
        rb = (ra * 65 + 50) / 100;
        if (rb < 3) rb = 3;
    }
    int cx = 120, cy = 80;

    precalcular_grietas(c->seed, c->grietas, c, &o_interior, cx, cy, ra, rb);

    uint16_t* vram = get_vram();
    uint8_t off      = (uint8_t)(c->seed ^ (c->seed >> 16));
    uint8_t off_int  = off ^ OFF_INTERIOR_XOR;

    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x += 2) {
            uint8_t z1 = grieta_buf[y][x];
            uint8_t z2 = grieta_buf[y][x + 1];
            uint8_t c1, c2;

            if (z1 == 0x01) {
                c1 = 9;
            } else if (z1 >= 0x02) {
                uint8_t p  = plasma_pixel(x, y, off_int, &o_interior);
                uint8_t vr = pixel_nebula(x, y, off);
                uint8_t cr = 10 + (vr >> 4) % 20;
                c1 = (z1 == 0x02)
                   ? (uint8_t)(((int)p * 3 + cr) >> 2)
                   : (uint8_t)(((int)p     + cr * 3) >> 2);
                if (c1 < 16) c1 = 16;
            } else {
                uint8_t v1 = pixel_nebula(x, y, off);
                c1 = 10 + (v1 >> 4) % 20;
            }

            if (z2 == 0x01) {
                c2 = 9;
            } else if (z2 >= 0x02) {
                uint8_t p  = plasma_pixel(x + 1, y, off_int, &o_interior);
                uint8_t vr = pixel_nebula(x + 1, y, off);
                uint8_t cr = 10 + (vr >> 4) % 20;
                c2 = (z2 == 0x02)
                   ? (uint8_t)(((int)p * 3 + cr) >> 2)
                   : (uint8_t)(((int)p     + cr * 3) >> 2);
                if (c2 < 16) c2 = 16;
            } else {
                uint8_t v2 = pixel_nebula(x + 1, y, off);
                c2 = 10 + (v2 >> 4) % 20;
            }

            vram[(y * 120) + (x / 2)] = ((uint16_t)c2 << 8) | c1;
        }
    }

    aplicar_paleta_roca(&o_interior);
}

/* ================================================================
   RENDER ROCA PEQUEÑA
================================================================ */

void renderizar_roca_pequena(int x_pos, int y_pos, const Chunk* c) {
    Opalo o_interior;
    generar_opalo(&o_interior, c->seed);

    int ra, rb;
    {
        int q = c->quilates;
        if (q < 1)   q = 1;
        if (q > 225) q = 225;
        ra = 5 + (q * 33) / 225;
        rb = (ra * 65 + 50) / 100;
        if (rb < 3) rb = 3;
    }
    int cx = 120, cy = 80;

    precalcular_grietas(c->seed, c->grietas, c, &o_interior, cx, cy, ra, rb);

    uint16_t* vram = get_vram();
    uint8_t off     = (uint8_t)(c->seed ^ (c->seed >> 16));
    uint8_t off_int = off ^ OFF_INTERIOR_XOR;

    for (int y = 0; y < 80; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int src_y = y * 2;
        if (src_y >= 160) src_y = 159;

        for (int x = 0; x < 100; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int src_x = x * 2;
            if (src_x >= 240) src_x = 238;

            uint8_t z = grieta_buf[src_y][src_x];
            uint8_t ci;

            if (z == 0x01) {
                ci = 9;
            } else if (z >= 0x02) {
                uint8_t p  = plasma_pixel(src_x, src_y, off_int, &o_interior);
                uint8_t vr = pixel_nebula(src_x, src_y, off);
                uint8_t cr = 10 + (vr >> 4) % 20;
                ci = (z == 0x02)
                   ? (uint8_t)(((int)p * 3 + cr) >> 2)
                   : (uint8_t)(((int)p     + cr * 3) >> 2);
                if (ci < 16) ci = 16;
            } else {
                uint8_t v = pixel_nebula(src_x, src_y, off);
                ci = 10 + (v >> 4) % 20;
            }

            int idx = screen_y * 120 + screen_x / 2;
            if (screen_x & 1)
                vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)ci << 8);
            else
                vram[idx] = (vram[idx] & 0xFF00) | ci;
        }
    }

    aplicar_paleta_roca(&o_interior);
}

/* ================================================================
   RENDER OPALO COMPLETO
================================================================ */

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

/* ================================================================
   FLASH GUARDADO
================================================================ */

void flash_guardado(void) {
    uint16_t* pal = (uint16_t*)0x05000000;
    for (int i = 0; i < 256; i++) pal[i] = 0x7FFF;
}

/* ================================================================
   RADIO CONTINUO POR QUILATES
================================================================ */

static void quilates_to_radii(uint16_t quilates, int* out_a, int* out_b) {
    int q = quilates;
    if (q < 1)   q = 1;
    if (q > 225) q = 225;

    int a = 3 + ((q - 1) * 42 + 112) / 224;
    int b = (a * 65 + 50) / 100;
    if (b < 2) b = 2;

    *out_a = a;
    *out_b = b;
}
/* ================================================================
    RENDER OPALO PEQUEÑO CON CABUJÓN 3D
================================================================ */

#define FONDO_GRIS_IDX  3

void renderizar_opalo_pequeno(int x_pos, int y_pos, Opalo* o, uint16_t quilates) {
    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(o->seed ^ (o->seed >> 16));

    int a, b;
    quilates_to_radii(quilates, &a, &b);

    int cx = 50;
    int cy = 40;

    int a2 = a * a;
    int b2 = b * b;

    // Borde fijo de 1px
    int ab = a + 1;
    int bb = b + 1;
    int ab2 = ab * ab;
    int bb2 = bb * bb;

    int hl_activo = (a >= 8);
    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = a * 7 / 10;
    int hb = b * 3 / 10;

    for (int y = 0; y < 80; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int dy = y - cy;
        int dy2 = dy * dy;

        for (int x = 0; x < 100; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int dx = x - cx;
            int dx2 = dx * dx;

            // Lógica de sombra: desplazamos el borde 1px hacia abajo y derecha
            int shadow_dx = dx - 1;
            int shadow_dy = dy - 1;

            int inside_main   = (dx2 * b2  + dy2 * a2  <= a2  * b2);
            int inside_border = (shadow_dx * shadow_dx * bb2 + shadow_dy * shadow_dy * ab2 <= ab2 * bb2);

            uint8_t color;

            if (inside_main) {
                int src_x = (a > 0) ? ((dx + a) * 120 / (a * 2)) : 60;
                int src_y = (b > 0) ? ((dy + b) * 80 / (b * 2)) : 40;
                color = plasma_pixel_smooth(src_x, src_y, off, o);

                if (hl_activo) {
                    int hdx = dx - hx_off;
                    int hdy = dy - hy_off;

                    int seed_var = (int)(o->seed & 7);
                    int ha_var = ha - (ha / 10) + (seed_var * ha / 40);
                    int hb_var = hb - (hb / 5) + (((o->seed >> 3) & 7) * hb / 20);
                    if (ha_var < 2) ha_var = 2;
                    if (hb_var < 2) hb_var = 2;

                    int ha_var2 = ha_var * ha_var;
                    int hb_var2 = hb_var * hb_var;

                    int val_elipse   = (hb_var2 * hdx * hdx + ha_var2 * hdy * hdy);
                    int total_elipse = (ha_var2 * hb_var2);

                    if (val_elipse <= total_elipse) {
                        if (val_elipse < (total_elipse * 15 / 100)) {
                            color = 254;
                        } else {
                            if (((x + y) & 1) == 0) color = 254;
                        }
                    }
                }

                int edge_dist2 = dx2 * b2 + dy2 * a2;
                int full2 = a2 * b2;
                if (edge_dist2 > (full2 * 85 / 100) && dx >= 0 && dy >= 0) {
                    int c_int = (int)color - 20;
                    if (c_int < 16) c_int = 16;
                    color = (uint8_t)c_int;
                }

            } else if (inside_border) {
                // Aquí usamos la variable global para las pruebas
                color = 1; 
            } else {
                color = FONDO_GRIS_IDX;
            }

            int idx = screen_y * 120 + screen_x / 2;
            if (screen_x & 1)
                vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)color << 8);
            else
                vram[idx] = (vram[idx] & 0xFF00) | color;
        }
    }

    generar_paleta(o);
}
