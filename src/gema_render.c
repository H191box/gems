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

    // Usamos el tipo extraído de forma pura mediante la semilla de la Gema
    switch (gema_tipo(g)) {
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
    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

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
    //TODO: JIJI JAJA JUJU
}

void renderizar_roca_pequena(int x_pos, int y_pos, const Gema* g) {
    //TODO: JEJE JAJA QUE GRACIOSO
}

void renderizar_opalo_pequeno_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    //TODO: NOSE NOSE
}
