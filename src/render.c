#include <stdint.h>
#include <gba_dma.h>
#include <gba_video.h>
#include "font.h"
#include "plasma.h"
#include "render.h"
#include "gema_render.h"
#include "gema.h"
#include "video.h"  // Corrección: Evita la declaración implícita de get_vram

// Buffer para el patrón de dithering en EWRAM
static uint16_t fondo_lineas[16][120]; 
static uint16_t linea_tmp[120];

// Matriz de Bayer 4x4 para el Dithering ordenado
const uint8_t bayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},                
    {15,  7, 13,  5}
};

// RNG Local para desvincular el renderizado de estructuras pesadas
static uint32_t rng_next(uint32_t* s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

/* --- OPTIMIZACIÓN ARM7TDMI: Eliminados los int64_t por software --- */
static void plot_grieta_pixel(int px, int py, int halo, int cx, int cy, int ra2, int rb2) {
    if (px < 0 || px >= 240 || py < 0 || py >= 160) return;
    
    int dx = px - cx;
    int dy = py - cy;
    
    if (dx*dx * rb2 + dy*dy * ra2 > ra2 * rb2) return;
    
    if (grieta_buf[py][px] < 0x01) grieta_buf[py][px] = 0x01;
    
    if (halo >= 1) {
        const int offs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int k = 0; k < 4; k++) {
            int nx = px + offs[k][0];
            int ny = py + offs[k][1];
            if (nx < 0 || nx >= 240 || ny < 0 || ny >= 160) continue;
            int dx2 = nx - cx, dy2 = ny - cy;
            if (dx2*dx2 * rb2 + dy2*dy2 * ra2 > ra2 * rb2) continue;
            if (grieta_buf[ny][nx] < 0x02) grieta_buf[ny][nx] = 0x02;
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
            if (dx2*dx2 * rb2 + dy2*dy2 * ra2 > ra2 * rb2) continue;
            if (grieta_buf[ny][nx] < 0x03) grieta_buf[ny][nx] = 0x03;
        }
    }
}

void precalcular_grietas(uint32_t seed, uint8_t num_grietas, int cx, int cy, int ra, int rb) {
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            grieta_buf[y][x] = 0;
        }
    }

    if (num_grietas == 0) return;

    int ra2 = ra * ra;
    int rb2 = rb * rb;
    
    uint32_t s_init = seed;
    int tamanyo = (rng_next(&s_init) % 3) + 1;
    int forma_grieta = rng_next(&s_init) & 3;
    int brillo = 16 + (rng_next(&s_init) % 16);
    int halo = (brillo >= 24) ? 2 : 1;

    for (uint8_t g = 0; g < num_grietas; g++) {
        uint32_t s = seed ^ (0x1234u * (uint32_t)(g + 1));

        int ox = cx, oy = cy;
        for (int tries = 0; tries < 8; tries++) {
            int tx = cx + (int)(rng_next(&s) % (ra * 2 + 1)) - ra;
            int ty = cy + (int)(rng_next(&s) % (rb * 2 + 1)) - rb;
            int ddx = tx - cx, ddy = ty - cy;
            if (ddx*ddx * rb2 + ddy*ddy * ra2 <= ra2 * rb2) {
                ox = tx; oy = ty; 
                break;
            }
        }

        int len = 10 + (tamanyo - 1) * 5 + (int)(rng_next(&s) % 11);

        int dx_fp, dy_fp;
        switch (forma_grieta & 3) {
            case 0:  dx_fp = 256; dy_fp =   0; break;
            case 1:  dx_fp = 181; dy_fp = 181; break;
            case 2:  dx_fp =   0; dy_fp = 256; break;
            default: dx_fp = 181; dy_fp =-181; break;
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

/* ---------------- GENERACIÓN DE PALETAS ---------------- */

void generar_paleta_rango(Opalo* o, int base, int num_colores) {
    uint16_t* pal = (uint16_t*)0x05000000;
    int spread = 1 + (o->iridiscencia / 8);

    for (int i = 0; i < num_colores; i++) {
        int src = 16 + (i * 239) / num_colores;
        int r, g, b;

        switch (o->tipo) {
            case OPALO_NEGRO:
                r = (lut_s((src * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_c((src * spread * 2) + o->color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((src * spread * 2) + o->color_offset) + 4096) >> 11);
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((src * spread) + o->color_offset) + 4096) >> 9);
                g = 20 + ((lut_c((src * spread) + o->color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((src * spread) + o->color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((src * spread) + o->color_offset) + 4096) >> 9);
                g = (lut_s((src * spread) + o->color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + o->color_offset) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r = 25 + ((lut_s((src * spread) + o->color_offset) + 4096) >> 9);
                g = 15 + ((lut_c((src * spread) + o->color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((src * spread) + o->color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((src * spread) + o->color_offset) + 4096) >> 10);
                g = 15 + ((lut_s((src * spread) + o->color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((src * spread) + o->color_offset) + 4096) >> 10);
                break;
            default:
                r = (lut_s((src * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_s((src * spread * 2) + o->color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + o->color_offset) + 4096) >> 8;
                break;
        }

        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (b > 31) b = 31;

        pal[base + i] = (r & 31) | ((g & 31) << 5) | ((b & 31) << 10);
    }
}

void generar_paleta(Opalo* o) {
    uint16_t* pal = (uint16_t*)0x05000000;
    int spread = 1 + (o->iridiscencia / 8);

    for (int i = 16; i < 254; i++) {
        int r, g, b;
        switch (o->tipo) {
            case OPALO_NEGRO:
                r = (lut_s((i * spread) + o->color_offset) + 4096) >> 8;
                g = (lut_c((i * spread * 2) + o->color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((i * spread * 2) + o->color_offset) + 4096) >> 11);
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
            case OPALO_ROSA:
                r = 25 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 9);
                g = 15 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((i * spread) + o->color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 10);
                g = 15 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((i * spread) + o->color_offset) + 4096) >> 10);
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
    int tinte = (o->color_offset & 3) - 1;
    int borde_r = borde_v + tinte;
    int borde_b = borde_v - tinte;
    if (borde_r > 30) borde_r = 30; if (borde_r < 22) borde_r = 22;
    if (borde_b > 30) borde_b = 30; if (borde_b < 22) borde_b = 22;
    pal[15] = (borde_r & 31) | ((borde_v & 31) << 5) | ((borde_b & 31) << 10);

    int hl_base = 29 + ((o->brillo - 16) * 3 / 15);
    int hl_r, hl_g, hl_b;
    switch (o->tipo) {
        case OPALO_CRISTAL: hl_r = 31; hl_g = 31; hl_b = 31; break;
        case OPALO_FUEGO:
            hl_r = 30;
            hl_g = 29 + ((o->brillo - 16) * 6 / 15);
            hl_b = 12 + ((o->brillo - 16) * 4 / 15);
            break;
        case OPALO_NEGRO:
            hl_r = hl_base - 2; hl_g = hl_base - 1; hl_b = hl_base;
            if (hl_r < 30) hl_r = 30;
            break;
        case OPALO_ROSA:
            hl_r = 31;
            hl_g = 25 + ((o->brillo - 16) * 4 / 15);
            hl_b = 30;
            break;
        case OPALO_GRIS:
            hl_r = hl_base; hl_g = hl_base; hl_b = hl_base;
            break;
        default:
            hl_r = hl_base; hl_g = hl_base - 1; hl_b = hl_base - 2;
            if (hl_g < 30) hl_g = 30;
            if (hl_b < 30) hl_b = 30;
            break;
    }
    pal[254] = (hl_r & 31) | ((hl_g & 31) << 5) | ((hl_b & 31) << 10);
    pal[255] = 0x4210; 
}

/* ---------------- VOLCADOS DE BUFFER ---------------- */

void volcar_buf_offset(const uint8_t* buf, int offset, uint8_t color_fondo) {
    uint16_t* vram = get_vram();
    for (int y = 0; y < 160; y++) {
        const uint8_t* row = buf + y*240;
        int vrow = y*120;
        for (int x = 0; x < 240; x++) {
            int sx = x - offset;
            uint8_t px;
            if (sx < 0 || sx >= 240) px = color_fondo;
            else { px = row[sx]; if (px==0) px = color_fondo; }
            int idx = vrow + x/2;
            if (x&1) vram[idx]=(vram[idx]&0x00FF)|((uint16_t)px<<8);
            else     vram[idx]=(vram[idx]&0xFF00)|px;
        }
    }
}

void volcar_buf_solo_opalo(const uint8_t* buf, int offset) {
    uint16_t* vram = get_vram();
    for (int y = 0; y < 160; y++) {
        const uint8_t* row = buf + y*240;
        for (int x = 0; x < 240; x++) {
            int sx = x - offset;
            if (sx >= 0 && sx < 240) {
                uint8_t px = row[sx];
                if (px != 0) {
                    int idx = (y * 120) + (x / 2);
                    if (x & 1) vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)px << 8);
                    else       vram[idx] = (vram[idx] & 0xFF00) | px;
                }
            }
        }
    }
}

// Volcado crítico optimizado por línea en sección IWRAM
void __attribute__((section(".iwram"), long_call)) volcar_frame(const uint8_t* buf_opalo, int offset, int offset_bg) {
    uint16_t* vram = get_vram();
    for (int y = 0; y < 160; y++) {
        const uint8_t* row = buf_opalo + y * 240;
        for (int x = 0; x < 120; x++) {
            uint16_t fondo_word = fondo_lineas[y & 15][x];
            uint8_t c0 = fondo_word & 0xFF;
            uint8_t c1 = fondo_word >> 8;
            
            int sx0 = (x * 2)     - offset;
            int sx1 = (x * 2 + 1) - offset;
            
            if (sx0 >= 0 && sx0 < 240 && row[sx0] != 0) c0 = row[sx0];
            if (sx1 >= 0 && sx1 < 240 && row[sx1] != 0) c1 = row[sx1];
            
            linea_tmp[x] = (uint16_t)c0 | ((uint16_t)c1 << 8);
        }
        REG_DMA3SAD = (uint32_t)linea_tmp;   // CORRECCIÓN: REG_ en lugar de MEM_
        REG_DMA3DAD = (uint32_t)(vram + y * 120);
        REG_DMA3CNT = 0x80000000 | 120;
    }
}

void precalcular_fondo(int offset_bg) {
    const uint8_t IDX_FONDO_OSCURO = 5; 
    const uint8_t IDX_FONDO_CLARO  = 6;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 120; x++) {
            int px0 = x * 2;
            int px1 = x * 2 + 1;

            int intensidad0 = ((px0 + offset_bg) / 8) % 16; 
            int intensidad1 = ((px1 + offset_bg) / 8) % 16; 

            uint8_t c0 = (intensidad0 > bayer4x4[y % 4][px0 % 4]) ? IDX_FONDO_CLARO : IDX_FONDO_OSCURO;
            uint8_t c1 = (intensidad1 > bayer4x4[y % 4][px1 % 4]) ? IDX_FONDO_CLARO : IDX_FONDO_OSCURO;

            fondo_lineas[y][x] = c0 | (c1 << 8);
        }
    }
}

void dibujar_fondo_texturizado_optimizado(uint16_t* vram, int offset_bg) {
    precalcular_fondo(offset_bg);
    for (int y = 0; y < 160; y++) {
        REG_DMA3SAD = (uint32_t)fondo_lineas[y & 15]; // CORRECCIÓN: REG_ en lugar de MEM_
        REG_DMA3DAD = (uint32_t)(vram + y * 120);
        REG_DMA3CNT = 0x80000000 | 120;
    }
}

void renderizar_frame_completo(const uint8_t* buf_opalo, int offset_opalo, int offset_bg) {
    precalcular_fondo(offset_bg);
    volcar_frame(buf_opalo, offset_opalo, offset_bg);
}

void vsync(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

void draw_ui_sobre_buffer(const char* nombre) {
    uint16_t* vram = get_vram();
    for (int x = 0; x < 120; x++) {
        vram[x]           = 0;
        vram[120+x]       = 0;
        vram[19080+x]     = 0;
        vram[19200-120+x] = 0;
    }
    draw_text(vram, 2, 2,   (char*)nombre,              255);
    draw_text(vram, 2, 150, "B:VOLVER  IZQ/DER:CAMBIAR",  255);
}

void renderizar_escena_completa(const Gema* g, int offset_opalo, int offset_fondo) {
    precalcular_fondo(offset_fondo);
    renderizar_gema_a_buffer(get_anim_buf_a(), 240, 160, g);
    volcar_frame(get_anim_buf_a(), offset_opalo, offset_fondo);
}
