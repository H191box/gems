#include <stdint.h>
#include "video.h"
#include "gema.h"
#include "render.h"      // Funciones generales y utilidades (get_vram, etc.)
#include "plasma.h"      // Motor matemático puro (plasma_pixel_smooth)
#include "gema_render.h" // Prototipos locales 

/* ================================================================
   BUFFER DE GRIETAS Y ANIMACIÓN
================================================================ */
uint8_t grieta_buf[160][240] __attribute__((section(".ewram")));

static uint8_t anim_buf_a[160 * 240] __attribute__((section(".ewram")));
static uint8_t anim_buf_b[160 * 240] __attribute__((section(".ewram")));

uint8_t* get_anim_buf_a(void) { return anim_buf_a; }
uint8_t* get_anim_buf_b(void) { return anim_buf_b; }

/* ================================================================
   PALETA ROCA (Adaptada a Gema)
================================================================ */
static void aplicar_paleta_roca(const Gema* g) {
    uint16_t* pal = (uint16_t*)0x05000000;

    for (int i = 0; i < 20; i++) {
        int v = 8 + i;
        pal[10 + i] = (v & 31) | ((v & 31) << 5) | ((v & 31) << 10);
    }

    pal[0]   = 0x0000;
    pal[255] = 0x7FFF;

    // Usamos el tipo_real persistente de la Gema
    switch (g->tipo_real) {
        case 0: // OPALO_NEGRO
            pal[9] = (2 & 31) | ((1 & 31) << 5) | ((4 & 31) << 10); break;
        case 1: // OPALO_CRISTAL
            pal[9] = (3 & 31) | ((3 & 31) << 5) | ((6 & 31) << 10); break;
        case 2: // OPALO_FUEGO
            pal[9] = (6 & 31) | ((2 & 31) << 5) | ((1 & 31) << 10); break;
        case 3: // OPALO_ROSA
            pal[9] = (8 & 31) | ((3 & 31) << 5) | ((6 & 31) << 10); break; 
        case 4: // OPALO_GRIS
            pal[9] = (4 & 31) | ((4 & 31) << 5) | ((4 & 31) << 10); break; 
        default:
            pal[9] = (4 & 31) | ((4 & 31) << 5) | ((5 & 31) << 10); break;
    }
}

/* ================================================================
   GEOMETRÍA Y UTILIDADES
================================================================ */
void quilates_to_radii(uint16_t quilates, int* out_a, int* out_b) {
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
   MOTOR DE RENDERIZADO PRO (Dinámico)
================================================================ */
void renderizar_opalo_pro(uint8_t *buffer, int w, int h, const Gema *g) {
    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    int a2 = a * a;
    int b2 = b * b;
    int a2b2 = a2 * b2;

    // Cálculo dinámico del Highlight (sin campos fijos)
    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = (a * 7 / 10);
    int hb = (b * 3 / 10);
    if (ha < 2) ha = 2; if (hb < 2) hb = 2;
    int ha2 = ha * ha;
    int hb2 = hb * hb;
    int ha2hb2 = ha2 * hb2;

    int cx = w / 2;
    int cy = h / 2;
    uint8_t off = (uint8_t)(g->seed_visual ^ (g->seed_visual >> 16));

    for (int y = 0; y < h; y++) {
        int dy = y - cy;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - cx;
            int dx2 = dx * dx;
            
            if (dx2 * b2 + dy2 * a2 <= a2b2) {
                int src_x = (dx + a) * 120 / (a * 2);
                int src_y = (dy + b) * 80 / (b * 2);
                uint8_t color = plasma_pixel_smooth(src_x, src_y, off, g);

                // Highlight especular dinámico
                int hdx = dx - hx_off;
                int hdy = dy - hy_off;
                if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= ha2hb2) {
                    color = 254; 
                }
                else if ((dx2 * b2 + dy2 * a2) > (a2b2 * 85 / 100) && dx >= 0 && dy >= 0) {
                    color = (color < 36) ? 16 : (color - 20);
                }
                buffer[y * w + x] = color;
            } else {
                buffer[y * w + x] = 0;
            }
        }
    }
}

void renderizar_opalo_grande(uint8_t* buf, const Gema* g) {
    renderizar_opalo_pro(buf, 240, 160, g);
}

void renderizar_opalo_mediano(uint8_t* buf, const Gema* g) {
    renderizar_opalo_pro(buf, 120, 80, g);
}

void renderizar_opalo_pequeno(uint8_t* buf, const Gema* g) {
    renderizar_opalo_pro(buf, 80, 80, g);
}

void renderizar_gema_a_buffer(uint8_t *buffer, int w, int h, const Gema *g) {
    renderizar_opalo_pro(buffer, w, h, g);
}

/* ================================================================
   RENDER ROCA (FASE 1 - BRUTO)
================================================================ */
#define OFF_INTERIOR_XOR 0x5A

void renderizar_roca(const Gema* g) {
    int ra, rb;
    quilates_to_radii(g->quilates, &ra, &rb);
    int cx = 120, cy = 80;

    uint8_t num_grietas = (g->seed_visual >> 24) % 6; 
    precalcular_grietas(g->seed_visual, num_grietas, cx, cy, ra, rb);

    uint16_t* vram = get_vram();
    uint8_t off      = (uint8_t)(g->seed_visual ^ (g->seed_visual >> 16));
    uint8_t off_int  = off ^ OFF_INTERIOR_XOR;

    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x += 2) {
            uint8_t z1 = grieta_buf[y][x];
            uint8_t z2 = grieta_buf[y][x + 1];
            uint8_t c1, c2;

            if (z1 == 0x01) {
                c1 = 9;
            } else if (z1 >= 0x02) {
                // Desempaquetado corregido
                uint8_t p  = plasma_pixel_smooth(x, y, off_int, g); 
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
                // Desempaquetado corregido
                uint8_t p = plasma_pixel_smooth(x + 1, y, off_int, g);
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

    aplicar_paleta_roca(g);
}

void renderizar_roca_pequena(int x_pos, int y_pos, const Gema* g) {
    int ra, rb;
    quilates_to_radii(g->quilates, &ra, &rb);
    int cx = 120, cy = 80;

    uint8_t num_grietas = (g->seed_visual >> 24) % 6; 
    precalcular_grietas(g->seed_visual, num_grietas, cx, cy, ra, rb);

    uint16_t* vram = get_vram();
    uint8_t off     = (uint8_t)(g->seed_visual ^ (g->seed_visual >> 16));
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
                // Desempaquetado corregido
                uint8_t p = plasma_pixel_smooth(src_x, src_y, off_int, g);
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

    aplicar_paleta_roca(g);
}

void renderizar_opalo_pequeno_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    uint16_t* vram = get_vram();
    uint8_t off = (uint8_t)(g->seed_visual ^ (g->seed_visual >> 16));

    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    int cx = 30; 
    int cy = 18;
    int a2 = a * a;
    int b2 = b * b;
    
    // Cálculos automáticos: eliminamos brillo_x, brillo_y, brillo_tam
    // El brillo se sitúa en la parte superior izquierda de la elipse
    int hx_off = -(a / 3); 
    int hy_off = -(b / 3);
    
    // El tamaño del brillo es proporcional al tamaño de la gema (aprox 1/2)
    int ha = a / 2;
    int hb = b / 2;
    if (ha < 2) ha = 2;
    if (hb < 2) hb = 2;
    
    int ha2 = ha * ha;
    int hb2 = hb * hb;

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

            int inside_main = (dx2 * b2 + dy2 * a2 <= a2 * b2);
            
            uint8_t color = 0;
            int pintar = 0;

            if (inside_main) {
                int src_x = (a > 0) ? ((dx + a) * 120 / (a * 2)) : 60;
                int src_y = (b > 0) ? ((dy + b) * 80 / (b * 2)) : 40;

                uint8_t p = plasma_pixel_smooth(src_x, src_y, off, g);

                // Aplicamos el brillo especular usando las variables calculadas
                int hdx = dx - hx_off;
                int hdy = dy - hy_off;
                if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= (ha2 * hb2)) {
                    color = 254; 
                }

                // Sombra de borde
                if ((dx2 * b2 + dy2 * a2) > (a2 * b2 * 85 / 100) && dx >= 0 && dy >= 0) {
                    color = (color < 36) ? 16 : (color - 20);
                }

                // Ajuste de paleta según celda
                if (color >= 16 && color <= 254) {
                    int rel = color - 16;
                    color = base + ((rel * num_colores) / 239);
                }
                pintar = 1;
            }

            if (pintar) {
                int idx = screen_y * 120 + screen_x / 2;
                if (screen_x & 1) vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)color << 8);
                else              vram[idx] = (vram[idx] & 0xFF00) | color;
            }
        }
    }
}
