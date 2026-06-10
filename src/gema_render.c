#include <stdint.h>
#include <string.h>
#include "video.h"
#include "gema.h"
#include "render.h"
#include "plasma.h"
#include "gema_render.h"

/* ================================================================
   BUFFER DE GRIETAS Y ANIMACIÓN
================================================================ */
uint8_t grieta_buf[160][240] __attribute__((section(".ewram")));

static uint8_t anim_buf_a[160 * 240] __attribute__((section(".ewram")));
static uint8_t anim_buf_b[160 * 240] __attribute__((section(".ewram")));

uint8_t* get_anim_buf_a(void) { return anim_buf_a; }
uint8_t* get_anim_buf_b(void) { return anim_buf_b; }

/* ================================================================
   CACHÉ DE RENDER PRO
   El ópalo grande (240x160) es el render más caro del juego.
   Se precalcula una sola vez en anim_buf_b y se reutiliza mientras
   la gema no cambie. dirty se activa cuando cambia cualquier campo.
   Coste cuando no hay cambio: una comparación de 4 campos.
================================================================ */
static uint32_t cache_seed     = 0;
static uint16_t cache_quilates = 0;
static uint8_t  cache_etapa    = 0xFF;  /* valor imposible — fuerza primer cálculo */
static uint8_t  cache_flags    = 0;

static int cache_pro_dirty(const Gema *g)
{
    return (g->seed     != cache_seed     ||
            g->quilates != cache_quilates  ||
            g->etapa    != cache_etapa    ||
            g->flags    != cache_flags);
}

static void cache_pro_marcar(const Gema *g)
{
    cache_seed     = g->seed;
    cache_quilates = g->quilates;
    cache_etapa    = g->etapa;
    cache_flags    = g->flags;
}

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

    switch (gema_tipo(g)) {
        case 0: pal[9] = (2 & 31) | ((1 & 31) << 5) | ((4 & 31) << 10); break;
        case 1: pal[9] = (3 & 31) | ((3 & 31) << 5) | ((6 & 31) << 10); break;
        case 2: pal[9] = (6 & 31) | ((2 & 31) << 5) | ((1 & 31) << 10); break;
        case 3: pal[9] = (8 & 31) | ((3 & 31) << 5) | ((6 & 31) << 10); break;
        case 4: pal[9] = (4 & 31) | ((4 & 31) << 5) | ((4 & 31) << 10); break;
        default: pal[9] = (4 & 31) | ((4 & 31) << 5) | ((5 & 31) << 10); break;
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

/* quilates_to_radii_pro — versión para vista pantalla completa (240x160).
   A 200 quilates el ópalo llega a 5px del margen superior e inferior,
   es decir b_max = 80 - 5 = 75. El resto de quilates escalan linealmente
   desde un mínimo de b=10 para gemas muy pequeñas.
   Relación a/b fija: a = b * 100 / 65 (misma proporción que la normal). */
static void quilates_to_radii_pro(uint16_t quilates, int* out_a, int* out_b)
{
    int q = quilates;
    if (q < 1)   q = 1;
    if (q > 225) q = 225;

    /* Escala lineal: q=1 → b=10, q=200 → b=75 */
    int b = 10 + ((q - 1) * 65) / 199;
    if (b > 75) b = 75;

    /* a mantiene la proporción 100:65 */
    int a = (b * 100 + 32) / 65;

    /* Clamp horizontal: no sobrepasar 115px del centro (margen 5px a cada lado) */
    if (a > 115) a = 115;

    *out_a = a;
    *out_b = b;
}

/* ================================================================
   MOTOR DE RENDERIZADO PRO (Dinámico)

   OPTIMIZACIÓN v2:
   - Usa plasma_pixel() en lugar de plasma_pixel_smooth().
     A 240x160 el suavizado 2x2 no es perceptible y cuesta 4x.
     Ahorro: ~90.000 llamadas a plasma_pixel() por frame evitadas.
   - El resultado se guarda en anim_buf_b (EWRAM).
     renderizar_opalo_grande() comprueba el flag dirty antes de
     recalcular — si la gema no cambió, reutiliza el buffer.
================================================================ */
void renderizar_opalo_pro(uint8_t *buffer, int w, int h, const Gema *g)
{
    // SISTEMA DE PESO APARENTE: La roca se dibujará más grande o pequeña de lo real en Fase 1
    int q = (g->etapa == ETAPA_BRUTA) ? gema_quilates_aparentes(g) : g->quilates;

    int a, b;
    /* Vista pantalla completa: escalar para aprovechar todo el espacio.
       Para tamaños menores (preview, celdas) usar la escala compacta normal. */
    if (w == 240 && h == 160)
        quilates_to_radii_pro((uint16_t)q, &a, &b);
    else
        quilates_to_radii((uint16_t)q, &a, &b);

    int a2 = a * a;
    int b2 = b * b;
    int a2b2 = a2 * b2;

    int sa = a + 1;
    int sb = b + 1;
    int sa2 = sa * sa;
    int sb2 = sb * sb;
    int sa2sb2 = sa2 * sb2;

    int shadow_off_x = 1;
    int shadow_off_y = 1;

    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = (a * 4 / 10);
    int hb = (b * 2 / 10);

    if (ha < 2) ha = 2;
    if (hb < 2) hb = 2;

    int ha2 = ha * ha;
    int hb2 = hb * hb;
    int ha2hb2 = ha2 * hb2;

    int cx = w / 2;
    int cy = h / 2;

    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

    // GENERACIÓN DE FORMA IRREGULAR (Squircle asimétrico por cuadrantes)
    int k_q1 = 0, k_q2 = 0, k_q3 = 0, k_q4 = 0;
    if (g->etapa == ETAPA_BRUTA) {
        k_q1 = 80 + ((g->seed >> 2) % 140);
        k_q2 = 80 + ((g->seed >> 5) % 140);
        k_q3 = 80 + ((g->seed >> 8) % 140);
        k_q4 = 80 + ((g->seed >> 11) % 140);
    }

    for (int y = 0; y < h; y++)
    {
        int dy = y - cy;
        int dy2 = dy * dy;

        for (int x = 0; x < w; x++)
        {
            int dx = x - cx;
            int dx2 = dx * dx;

            int inside_main = 0;
            int inside_shadow = 0;
            int dist_val = 0;

            if (g->etapa == ETAPA_BRUTA) {
                /* Deformación tipo patata — squircle asimétrico por cuadrantes.
                   Para evitar desborde int32 con radios grandes (vista pro),
                   el término de deformación se calcula en espacio reducido:
                   se dividen dx y dy entre 2 antes de elevar al cuadrado,
                   lo que reduce el producto 16x y lo mantiene dentro de int32
                   incluso con a=115, b=75. El factor k se ajusta (*4) para
                   compensar la reducción y conservar la forma visual. */
                int k = (dx > 0) ? ((dy > 0) ? k_q1 : k_q4) : ((dy > 0) ? k_q2 : k_q3);
                int dxs = dx >> 1;
                int dys = dy >> 1;
                int dxdy2 = (dxs * dys) * (dxs * dys);   /* máx ~(58*40)^2 = 5.382.400 — seguro */
                int subtract = (dxdy2 >> 6) * k;          /* >>6 en lugar de >>8, compensa el /4 */

                dist_val = (dx2 * b2 + dy2 * a2) - subtract;
                inside_main = (dist_val <= a2b2);

                int sdx = dx - shadow_off_x;
                int sdy = dy - shadow_off_y;
                int sk = (sdx > 0) ? ((sdy > 0) ? k_q1 : k_q4) : ((sdy > 0) ? k_q2 : k_q3);
                int sdxs = sdx >> 1;
                int sdys = sdy >> 1;
                int sdxdy2 = (sdxs * sdys) * (sdxs * sdys);
                int sub_s = (sdxdy2 >> 6) * sk;
                inside_shadow = ((sdx * sdx * sb2 + sdy * sdy * sa2) - sub_s <= sa2sb2);
            } else {
                dist_val = (dx2 * b2 + dy2 * a2);
                inside_main = (dist_val <= a2b2);
                
                int sdx = dx - shadow_off_x;
                int sdy = dy - shadow_off_y;
                inside_shadow = ((sdx * sdx * sb2 + sdy * sdy * sa2) <= sa2sb2);
            }

            if (inside_main)
            {
                int src_x = (dx + a) * 120 / (a * 2);
                int src_y = (dy + b) * 80  / (b * 2);
                uint8_t color = plasma_pixel(src_x, src_y, off, g);

                if (g->etapa == ETAPA_BRUTA) {
                    // ROCA BRUTA: Textura granulada y sin highlights pulidos
                    int ruido = ((dx * 17) ^ (dy * 31) ^ g->seed) & 31;
                    if (dist_val > (a2b2 * 65 / 100)) {
                        // Sombra rugosa en los bordes opuestos a la luz
                        if (ruido > 15 && dx >= -a/2 && dy >= -b/2) {
                            int cv = (int)color - 20;
                            color = (cv < 16) ? 16 : (uint8_t)cv;
                        }
                    }
                    // Microporos aleatorios en el cuerpo de la gema
                    if (ruido == 0) {
                        int cv = (int)color - 15;
                        color = (cv < 16) ? 16 : (uint8_t)cv;
                    }
                } else {
                    // GEMA PULIDA/CORTADA: Cabujón perfecto con reflejos (Código Original)
                    int edge = dist_val * 256 / a2b2;
                    if (edge > 180) {
                        int fade = edge - 180;
                        int c = (int)color - (fade >> 1);
                        color = (c < 16) ? 16 : (uint8_t)c;
                    }

                    int hdx = dx - hx_off;
                    int hdy = dy - hy_off;
                    if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= ha2hb2) {
                        color = 254; // Reflejo especular
                    } else if (dist_val > (a2b2 * 82 / 100) && dx >= 0 && dy >= 0) {
                        int cv = (int)color - 30;
                        color = (cv < 16) ? 16 : (uint8_t)cv;
                    }
                }

                buffer[y * w + x] = color;
            }
            else if (inside_shadow) {
                buffer[y * w + x] = 255;
            } else {
                buffer[y * w + x] = 0;
            }
        }
    }
}

/*
 * renderizar_opalo_grande — versión con caché.
 * Solo llama a renderizar_opalo_pro() cuando la gema cambia.
 * En frames sucesivos con la misma gema: cero cálculo de plasma.
 */
void renderizar_opalo_grande(uint8_t* buf, const Gema* g) {
    if (cache_pro_dirty(g)) {
        renderizar_opalo_pro(buf, 240, 160, g);
        cache_pro_marcar(g);
    }
    /* Si no es dirty, buf ya contiene el resultado correcto —
     * el llamador es responsable de no limpiar el buffer entre frames. */
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
void renderizar_roca(const Gema* g) {
    uint8_t* vram = (uint8_t*)get_vram();
    aplicar_paleta_roca(g);

    int a, b;
    quilates_to_radii(gema_quilates_aparentes(g), &a, &b);

    int ra = a + 8;
    int rb = b + 6;
    int ra2 = ra * ra;
    int rb2 = rb * rb;
    int ra2rb2 = ra2 * rb2;

    int cx = 120;
    int cy = 80;
    uint32_t s_rng = g->seed;
    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

    for (int y = 0; y < 160; y++) {
        int dy = y - cy;
        int dy2 = dy * dy;
        for (int x = 0; x < 240; x++) {
            int dx = x - cx;
            int dx2 = dx * dx;

            if (dx2 * rb2 + dy2 * ra2 <= ra2rb2) {
                s_rng = s_rng * 1103515245 + 12345;
                int ruido = (s_rng >> 16) % 6;

                int limit_gema = (ra2rb2 * 45 / 100) + (ruido * 2000);

                if (dx2 * rb2 + dy2 * ra2 <= limit_gema) {
                    int src_x = (dx + a) * 120 / (a * 2 + 1);
                    int src_y = (dy + b) * 80  / (b * 2 + 1);
                    /*
                     * plasma_pixel() en lugar de plasma_pixel_smooth().
                     * La ventana interior de la roca es pequeña y el
                     * smooth no aporta detalle visible aquí.
                     */
                    vram[y * 240 + x] = plasma_pixel(src_x, src_y, off, g);
                } else {
                    vram[y * 240 + x] = 10 + ((x + y + ruido) % 20);
                }
            } else {
                vram[y * 240 + x] = 0;
            }
        }
    }
}

void renderizar_roca_pequena(int x_pos, int y_pos, const Gema* g) {
    uint8_t* vram = (uint8_t*)get_vram();

    int r = 16;
    int r2 = r * r;
    uint32_t s_rng = g->seed ^ (x_pos * y_pos);

    for (int y = -r; y <= r; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int y2 = y * y;
        for (int x = -r; x <= r; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int x2 = x * x;
            if (x2 + y2 <= r2) {
                s_rng = s_rng * 1103515245 + 12345;
                int ruido = (s_rng >> 16) % 4;

                if (x2 + y2 <= (r2 * 25 / 100) + ruido) {
                    vram[screen_y * 240 + screen_x] = 9;
                } else {
                    vram[screen_y * 240 + screen_x] = 12 + ((x + y + ruido) % 15);
                }
            }
        }
    }
}

void renderizar_opalo_pequeno_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    uint8_t* vram = (uint8_t*)get_vram();
    uint32_t s_rng = g->seed ^ (x_pos * y_pos);

    if (g->etapa == ETAPA_BRUTA) {
        int r = 13;
        int r2 = r * r;

        for (int y = -r; y <= r; y++) {
            int screen_y = y_pos + y;
            if (screen_y < 0 || screen_y >= 160) continue;
            int y2 = y * y;

            for (int x = -r; x <= r; x++) {
                int screen_x = x_pos + x;
                if (screen_x < 0 || screen_x >= 240) continue;
                int x2 = x * x;

                if (x2 + y2 <= r2) {
                    s_rng = s_rng * 1103515245 + 12345;
                    int ruido = (s_rng >> 16) % 4;

                    if (x2 + y2 <= 5 + ruido) {
                        vram[screen_y * 240 + screen_x] = 9;
                    } else {
                        vram[screen_y * 240 + screen_x] = 12 + ((x + y + ruido) % 15);
                    }
                }
            }
        }
        return;
    }

    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    a = (a * 71) / 100;
    b = (b * 71) / 100;
    if (a < 3) a = 3;
    if (b < 2) b = 2;

    int a2   = a * a;
    int b2   = b * b;
    int a2b2 = a2 * b2;
    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

    /* Sombra desplazada: elipse exterior desplazada 1px abajo-derecha.
     * Crea sensación de profundidad: la gema "flota" sobre su sombra.
     * Radio extra 3px en dirección sombra, 1px en dirección luz.      */
    int ab = a + 1;   /* radio elipse de sombra */
    int bb = b + 1;
    int ab2 = ab * ab;
    int bb2 = bb * bb;
    /* Desplazamiento: +1px abajo-derecha para profundidad.
     * Con radio 3 el borde existe en todo el contorno;
     * solo es ~1px más grueso abajo y a la derecha.                   */
    int sombra_dx = 1;
    int sombra_dy = 1;
    (void)off; /* off no se usa en celda — plasma viene del caché      */

    /* highlight eliminado — el borde oscuro es suficiente */

    /* El rango del loop debe cubrir la elipse de sombra (radio ab/bb)
     * más el desplazamiento. Sin esto los píxeles del borde no se visitan. */
    int loop_a = ab + sombra_dx + 1;
    int loop_b = bb + sombra_dy + 1;

    for (int y = -loop_b; y <= loop_b; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int dy2 = y * y;
        for (int x = -loop_a; x <= loop_a; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int dx2 = x * x;
            int inside_main = (dx2 * b2 + dy2 * a2 <= a2b2);

            /* Sombra desplazada abajo-derecha: restar el offset antes del test */
            int shadow_dx  = x - sombra_dx;
            int shadow_dy  = y - sombra_dy;
            int inside_borde = (shadow_dx * shadow_dx * bb2 +
                                shadow_dy * shadow_dy * ab2 <= ab2 * bb2);

            uint8_t c;

            if (inside_main) {
                int src_x = (x + a) * 119 / ((a * 2) + 1);
                int src_y = (y + b) * 79  / ((b * 2) + 1);

                /*
                 * plasma_cache_get() — cero cálculo de plasma en caliente.
                 * Lee del buffer precalculado en EWRAM.
                 * generar_paleta_gema() ya habrá mapeado los índices [16,254]
                 * a los colores reales de esta gema en PALRAM.
                 */
                c = plasma_cache_get(src_x, src_y);

                /* Sombra en borde inferior-derecho */
                int edge_dist2 = dx2 * b2 + dy2 * a2;
                if (edge_dist2 > (a2b2 * 85 / 100) && x >= 0 && y >= 0) {
                    int cv = (int)c - 20;
                    if (cv < 16) cv = 16;
                    c = (uint8_t)cv;
                }

                if (g->flags & GEMA_FLAG_GRIETAS) {
                    if (((x + y * 2) % 11 == 0 && x > -a && x < a / 2) ||
                        ((x - y) == 1 && y > -b / 2 && y < b)) {
                        c = 11;
                    }
                }

                /*
                 * Remap al banco de paleta de esta gema.
                 * base=0/num_colores=0 → paleta global (G1 preview).
                 * base=16+i*16/num_colores=16 → banco propio (G2/G3).
                 */
                if (num_colores > 0 && c >= 16 && c < 254) {
                    c = (uint8_t)(base + ((c - 16) % num_colores));
                }

            } else if (inside_borde) {
                /* En modo banco (G2/G3): primer color del banco = tono oscuro de esa gema.
                 * En modo paleta global (G1 preview): pal[255] = 0x4210 antracita.       */
                c = (num_colores > 0) ? (uint8_t)base : 255;
            } else {
                continue;
            }

            vram[screen_y * 240 + screen_x] = c;
        }
    }
}

/* ================================================================
   PALETA POR BANCO — para G2/G3 con múltiples gemas simultáneas
   Escribe num_colores entradas a partir de pal[base].
   Cada gema tiene su banco propio — no interfieren entre sí.
================================================================ */
void generar_paleta_banco(const Gema *g, int base, int num_colores)
{
    volatile uint16_t *pal = (volatile uint16_t *)0x05000000;
    TipoOpalo tipo          = gema_tipo(g);
    uint8_t   color_offset  = (uint8_t)(g->seed & 0xFF);
    uint8_t   iridiscencia  = gema_iridiscencia(g);
    int       spread        = 1 + (iridiscencia / 8);

    for (int i = 0; i < num_colores; i++) {
        /* Mapear el índice del banco al rango [16,254] del plasma */
        int src = 16 + (i * 238) / num_colores;
        int r, gc, b;

        switch (tipo) {
            case OPALO_NEGRO:
                r  = (lut_s((src * spread + color_offset) & 63) + 4096) >> 8;
                gc = (lut_c((src * spread * 2 + color_offset) & 63) + 4096) >> 8;
                b  = 20 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 10);
                break;
            case OPALO_CRISTAL:
                r  = 18 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 9);
                gc = 18 + ((lut_c((src * spread + color_offset) & 63) + 4096) >> 9);
                b  = 24 + ((lut_c((src * spread + color_offset) & 63) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r  = 28 + ((lut_c((src * spread + color_offset) & 63) + 4096) >> 9);
                gc = (lut_s((src * spread + color_offset) & 63) + 4096) >> 8;
                b  = (lut_c((src * spread + color_offset) & 63) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r  = 24 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 9);
                gc = 12 + ((lut_c((src * spread + color_offset) & 63) + 4096) >> 10);
                b  = 18 + ((lut_c((src * spread + color_offset) & 63) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r  = 14 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 10);
                gc = 14 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 10);
                b  = 14 + ((lut_s((src * spread + color_offset) & 63) + 4096) >> 10);
                break;
            default:
                r  = (lut_s((src * spread + color_offset) & 63) + 4096) >> 8;
                gc = (lut_s((src * spread * 2 + color_offset) & 63) + 4096) >> 8;
                b  = (lut_c((src * spread + color_offset) & 63) + 4096) >> 8;
                break;
        }
        if (r  > 31) r  = 31;
        if (gc > 31) gc = 31;
        if (b  > 31) b  = 31;

        pal[base + i] = (uint16_t)((r & 31) | ((gc & 31) << 5) | ((b & 31) << 10));
    }
}

/* ================================================================
   ENLACE PARA EL LINKER
================================================================ */
void renderizar_gema_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    renderizar_opalo_pequeno_celda(x_pos, y_pos, g, base, num_colores);
}

void renderizar_gema_celda_vieja(int x_pos, int y_pos, const Gema* g) {
    renderizar_opalo_pequeno_celda(x_pos, y_pos, g, 0, 0);
}

/* ================================================================
   RENDER PREVIEW
   Usa el buffer cacheado si la gema no cambió desde el último
   renderizar_opalo_grande(). Si cambió, recalcula y actualiza caché.
================================================================ */
void renderizar_gema_preview(int x_pos, int y_pos, const Gema* g) {
    uint8_t* buf = get_anim_buf_a();

    if (cache_pro_dirty(g)) {
        memset(buf, 0, 240 * 160);

        // PESO APARENTE
        int q = (g->etapa == ETAPA_BRUTA) ? gema_quilates_aparentes(g) : g->quilates;
        
        int a, b;
        quilates_to_radii(q, &a, &b);

        int a2 = a * a;
        int b2 = b * b;
        int a2b2 = a2 * b2;

        int ab = a + 1, bb_r = b + 1;
        int ab2 = ab * ab, bb2 = bb_r * bb_r;
        int sombra_dx = 1, sombra_dy = 1;
        int loop_a = ab + sombra_dx + 1;
        int loop_b = bb_r + sombra_dy + 1;

        int hx_off = -(a / 4);
        int hy_off = -(b * 4 / 10);
        int ha = (a * 4 / 10);
        int hb = (b * 2 / 10);
        if (ha < 2) ha = 2; if (hb < 2) hb = 2;
        int ha2 = ha * ha;
        int hb2 = hb * hb;
        int ha2hb2 = ha2 * hb2;

        uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

        // FACTORES DE FORMA IRREGULAR
        int k_q1 = 0, k_q2 = 0, k_q3 = 0, k_q4 = 0;
        if (g->etapa == ETAPA_BRUTA) {
            k_q1 = 80 + ((g->seed >> 2) % 140);
            k_q2 = 80 + ((g->seed >> 5) % 140);
            k_q3 = 80 + ((g->seed >> 8) % 140);
            k_q4 = 80 + ((g->seed >> 11) % 140);
        }

        for (int y = -loop_b; y <= loop_b; y++) {
            int screen_y = y_pos + y;
            if (screen_y < 0 || screen_y >= 160) continue;

            int dy2 = y * y;
            for (int x = -loop_a; x <= loop_a; x++) {
                int screen_x = x_pos + x;
                if (screen_x < 0 || screen_x >= 240) continue;

                int dx2 = x * x;
                int inside_main = 0;
                int inside_borde = 0;
                int dist_val = 0;

                if (g->etapa == ETAPA_BRUTA) {
                    int k = (x > 0) ? ((y > 0) ? k_q1 : k_q4) : ((y > 0) ? k_q2 : k_q3);
                    int dxdy2 = (x * y) * (x * y);
                    int subtract = (dxdy2 >> 8) * k;
                    
                    dist_val = (dx2 * b2 + dy2 * a2) - subtract;
                    inside_main = (dist_val <= a2b2);
                    
                    int shadow_dx_v = x - sombra_dx;
                    int shadow_dy_v = y - sombra_dy;
                    int sk = (shadow_dx_v > 0) ? ((shadow_dy_v > 0) ? k_q1 : k_q4) : ((shadow_dy_v > 0) ? k_q2 : k_q3);
                    int sdxdy2 = (shadow_dx_v * shadow_dy_v) * (shadow_dx_v * shadow_dy_v);
                    int sub_s = (sdxdy2 >> 8) * sk;
                    
                    int dist_shadow = (shadow_dx_v * shadow_dx_v * bb2 + shadow_dy_v * shadow_dy_v * ab2) - sub_s;
                    inside_borde = (dist_shadow <= ab2 * bb2);
                } else {
                    dist_val = (dx2 * b2 + dy2 * a2);
                    inside_main = (dist_val <= a2b2);

                    int shadow_dx_v = x - sombra_dx;
                    int shadow_dy_v = y - sombra_dy;
                    inside_borde = (shadow_dx_v * shadow_dx_v * bb2 + shadow_dy_v * shadow_dy_v * ab2 <= ab2 * bb2);
                }

                if (inside_main) {
                    int src_x = (x + a) * 120 / ((a * 2) + 1);
                    int src_y = (y + b) * 80  / ((b * 2) + 1);
                    uint8_t color = plasma_pixel(src_x, src_y, off, g);

                    if (g->etapa == ETAPA_BRUTA) {
                        int ruido = ((x * 17) ^ (y * 31) ^ g->seed) & 31;
                        if (dist_val > (a2b2 * 65 / 100)) {
                            if (ruido > 15 && x >= -a/2 && y >= -b/2) {
                                int cv = (int)color - 20;
                                color = (cv < 16) ? 16 : (uint8_t)cv;
                            }
                        }
                        if (ruido == 0) {
                            int cv = (int)color - 15;
                            color = (cv < 16) ? 16 : (uint8_t)cv;
                        }
                    } else {
                        int edge = dist_val * 256 / a2b2;
                        if (edge > 180) {
                            int fade = (edge - 180);
                            int c = (int)color - (fade >> 1);
                            color = (c < 16) ? 16 : (uint8_t)c;
                        }

                        int hdx = x - hx_off;
                        int hdy = y - hy_off;
                        if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= ha2hb2) {
                            color = 254;
                        }
                        else if (dist_val > (a2b2 * 82 / 100) && (x + 1) >= 0 && (y + 1) >= 0) {
                            int cv = (int)color - 30;
                            color = (cv < 16) ? 16 : (uint8_t)cv;
                        }
                    }

                    buf[screen_y * 240 + screen_x] = color;
                } else if (inside_borde) {
                    buf[screen_y * 240 + screen_x] = 255;
                }
            }
        }

        cache_pro_marcar(g);
    }

    volcar_buf_solo_opalo(buf, 0);
}
/* ================================================================
   RENDER GEMA (versión simple sin celda)
================================================================ */
void renderizar_gema(int x_pos, int y_pos, const Gema* g) {
    renderizar_opalo_pequeno_celda(x_pos, y_pos, g, 0, 0);
}

/* ================================================================
   GENERACIÓN DE PALETAS
================================================================ */
void generar_paleta_gema_rango(const Gema* g, int base, int num_colores) {
    uint16_t* pal = (uint16_t*)0x05000000;

    TipoOpalo tipo = gema_tipo(g);
    uint8_t iridiscencia = gema_iridiscencia(g);
    uint8_t color_offset = (uint8_t)(g->seed & 0xFF);

    int spread = 1 + (iridiscencia / 8);

    for (int i = 0; i < num_colores; i++) {
        int src = 16 + (i * 239) / num_colores;
        int r, gc, b;

        switch (tipo) {
            case OPALO_NEGRO:
                r = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                gc = (lut_c((src * spread * 2) + color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((src * spread * 2) + color_offset) + 4096) >> 11);
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((src * spread) + color_offset) + 4096) >> 9);
                gc = 20 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                gc = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + color_offset) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r = 25 + ((lut_s((src * spread) + color_offset) + 4096) >> 9);
                gc = 15 + ((lut_c((src * spread) + color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                gc = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                break;
            default:
                r = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                gc = (lut_s((src * spread * 2) + color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + color_offset) + 4096) >> 8;
                break;
        }

        if (r > 31) r = 31;
        if (gc > 31) gc = 31;
        if (b > 31) b = 31;

        pal[base + i] = (r & 31) | ((gc & 31) << 5) | ((b & 31) << 10);
    }

    int sh_r, sh_g, sh_b;
    int sh2_r, sh2_g, sh2_b;
    switch (tipo) {
        case OPALO_NEGRO:   sh_r=4;  sh_g=3;  sh_b=8;  sh2_r=7;  sh2_g=6;  sh2_b=12; break;
        case OPALO_CRISTAL: sh_r=6;  sh_g=6;  sh_b=10; sh2_r=10; sh2_g=10; sh2_b=14; break;
        case OPALO_FUEGO:   sh_r=12; sh_g=4;  sh_b=2;  sh2_r=16; sh2_g=7;  sh2_b=4;  break;
        case OPALO_BLANCO:  sh_r=10; sh_g=10; sh_b=10; sh2_r=14; sh2_g=14; sh2_b=14; break;
        case OPALO_ROSA:    sh_r=14; sh_g=6;  sh_b=10; sh2_r=18; sh2_g=9;  sh2_b=14; break;
        default:            sh_r=8;  sh_g=8;  sh_b=9;  sh2_r=12; sh2_g=12; sh2_b=13; break;
    }
    pal[252] = (sh_r  & 31) | ((sh_g  & 31) << 5) | ((sh_b  & 31) << 10);
    pal[251] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10);
}

/* ================================================================
   SUCIEDAD DE PALETA (Fase 1 y Fase 2)

   Mezcla cada color del plasma hacia un blanco lechoso según
   el nivel de suciedad derivado de la seed [0..3].

   Se aplica en ETAPA_BRUTA y ETAPA_CORTADA.
   En ETAPA_PULIDA no se llama — los colores reales quedan al
   descubierto al pulir la gema.

   Mezcla: color_lechoso = color_real * (8 - peso) / 8
                         + leche      * peso        / 8
   donde peso es [2..5] (más suave que antes para no oscurecer).

   La leche es blanco casi puro (R=30, G=30, B=30) que desatura
   sin oscurecer — efecto traslúcido/nevado en lugar de lodoso.

   ETAPA_BRUTA: peso fijo = 5 (más cubierto, se ve poco color)
   ETAPA_CORTADA: peso según nivel [2..4]

   Índices afectados: [16..253] — colores del plasma.
   Respetados: pal[0], pal[254] (especular), pal[255] (sombra).
================================================================ */
static void aplicar_suciedad_paleta(uint16_t *pal, const Gema *g)
{
    int peso;

    if (g->etapa == ETAPA_BRUTA) {
        /* Fase 1: siempre muy lechoso — el color real apenas asoma */
        peso = 5;
    } else {
        /* Fase 2: peso según nivel de suciedad [0..3] → [2..4] */
        uint8_t nivel = gema_suciedad(g);
        peso = (int)nivel + 2;   /* [2..4] sobre escala de 8 */
    }

    /* Leche: blanco cálido casi puro (R=30, G=29, B=28) en BGR555 */
    int leche_r = 30;
    int leche_g = 29;
    int leche_b = 28;

    for (int i = 16; i < 251; i++) {
        uint16_t c = pal[i];
        int r = ( c        & 31);
        int g = ((c >>  5) & 31);
        int b = ((c >> 10) & 31);

        r = (r * (8 - peso) + leche_r * peso) >> 3;
        g = (g * (8 - peso) + leche_g * peso) >> 3;
        b = (b * (8 - peso) + leche_b * peso) >> 3;

        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (b > 31) b = 31;

        pal[i] = (uint16_t)((r & 31) | ((g & 31) << 5) | ((b & 31) << 10));
    }
}

void generar_paleta_gema(const Gema* g) {
    uint16_t* pal = (uint16_t*)0x05000000;

    TipoOpalo tipo = gema_tipo(g);
    uint8_t iridiscencia = gema_iridiscencia(g);
    uint8_t color_offset = (uint8_t)(g->seed & 0xFF);

    // CORREGIDO: Extraemos los atributos calculados para evitar que los destellos 
    // visuales revelen el brillo real antes de limpiar o tasar la gema.
    AtributosGema attr;
    gema_calcular_atributos(g, &attr);
    uint8_t brillo = attr.brillo_aparente; 

    int spread = 1 + (iridiscencia / 8);

    for (int i = 16; i < 254; i++) {
        int r, gc, b;
        switch (tipo) {
            case OPALO_NEGRO:
                r = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                gc = (lut_c((i * spread * 2) + color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((i * spread * 2) + color_offset) + 4096) >> 11);
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((i * spread) + color_offset) + 4096) >> 9);
                gc = 20 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                gc = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + color_offset) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r = 25 + ((lut_s((i * spread) + color_offset) + 4096) >> 9);
                gc = 15 + ((lut_c((i * spread) + color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                gc = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                break;
            default:
                r = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                gc = (lut_s((i * spread * 2) + color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + color_offset) + 4096) >> 8;
                break;
        }
        if (r > 31) r = 31; if (gc > 31) gc = 31; if (b > 31) b = 31;
        pal[i] = (r & 31) | ((gc & 31) << 5) | ((b & 31) << 10);
    }

    // A partir de aquí todo el código original usa la variable 'brillo' corregida:
    int borde_v = 24 + ((brillo - 16) * 4 / 15);
    int tinte = (color_offset & 3) - 1;
    int borde_r = borde_v + tinte;
    int borde_b = borde_v - tinte;
    if (borde_r > 30) borde_r = 30; if (borde_r < 22) borde_r = 22;
    if (borde_b > 30) borde_b = 30; if (borde_b < 22) borde_b = 22;
    pal[15] = (borde_r & 31) | ((borde_v & 31) << 5) | ((borde_b & 31) << 10);

    // ================================================================
    // CÁLCULO DE HIGHLIGHTS ESPECULARES (CORREGIDO)
    // ================================================================
    int hl_base = 29 + ((brillo - 16) * 3 / 15);
    
    // Control preventivo sobre la base
    if (hl_base > 31) hl_base = 31;
    if (hl_base < 0)  hl_base = 0;

    int hl_r, hl_g, hl_b;
    switch (tipo) {
        case OPALO_CRISTAL: 
            hl_r = 31; hl_g = 31; hl_b = 31; 
            break;
        case OPALO_FUEGO:
            hl_r = 31; 
            hl_g = 30; // Un pelín menos de verde para un destello cálido
            hl_b = 27 + ((brillo - 16) * 4 / 15);
            break;
        case OPALO_NEGRO:
            hl_r = hl_base - 2; 
            hl_g = hl_base - 1; 
            hl_b = hl_base; // Destello con tinte azulado/frío
            break;
        case OPALO_ROSA:
            hl_r = 31;
            hl_g = 26 + ((brillo - 16) * 3 / 15); // Suavizado
            hl_b = 30;
            break;
        case OPALO_GRIS:
            hl_r = hl_base; hl_g = hl_base; hl_b = hl_base;
            break;
        default: // Ópalo Blanco y otros
            hl_r = hl_base; 
            hl_g = hl_base - 1; 
            hl_b = hl_base - 2;
            break;
    }

    // REGLA DE CONTROL 1: Clamping real para erradicar desbordamientos
    if (hl_r > 31) hl_r = 31; else if (hl_r < 0) hl_r = 0;
    if (hl_g > 31) hl_g = 31; else if (hl_g < 0) hl_g = 0;
    if (hl_b > 31) hl_b = 31; else if (hl_b < 0) hl_b = 0;

    // REGLA DE CONTROL 2: Suelo estético anti-saturación (min 24)
    if (hl_r < 24) hl_r = 24;
    if (hl_g < 24) hl_g = 24;
    if (hl_b < 24) hl_b = 24;

    // Inserción segura sin usar máscara binaria (& 31)
    pal[254] = hl_r | (hl_g << 5) | (hl_b << 10);

    // ================================================================
    // SOMBRAS
    // ================================================================
    int sh_r, sh_g, sh_b;
    int sh2_r, sh2_g, sh2_b;
    switch (tipo) {
        case OPALO_NEGRO:   sh_r=4;  sh_g=3;  sh_b=8;  sh2_r=7;  sh2_g=6;  sh2_b=12; break;
        case OPALO_CRISTAL: sh_r=6;  sh_g=6;  sh_b=10; sh2_r=10; sh2_g=10; sh2_b=14; break;
        case OPALO_FUEGO:   sh_r=12; sh_g=4;  sh_b=2;  sh2_r=16; sh2_g=7;  sh2_b=4;  break;
        case OPALO_BLANCO:  sh_r=10; sh_g=10; sh_b=10; sh2_r=14; sh2_g=14; sh2_b=14; break;
        case OPALO_ROSA:    sh_r=14; sh_g=6;  sh_b=10; sh2_r=18; sh2_g=9;  sh2_b=14; break;
        default:            sh_r=8;  sh_g=8;  sh_b=9;  sh2_r=12; sh2_g=12; sh2_b=13; break;
    }
    
    // Aquí sí se puede mantener el & 31 por limpieza, ya que los valores asignados arriba están totalmente controlados
    pal[252] = (sh_r  & 31) | ((sh_g  & 31) << 5) | ((sh_b  & 31) << 10);
    pal[251] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10);
    pal[253] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10);
    pal[255] = 0x4210;

    /* ----------------------------------------------------------------
       SUCIEDAD (Fase 1 y 2): desatura la paleta hacia blanco lechoso.
       En Fase 1 (BRUTA) el filtro es fuerte — color apenas visible.
       En Fase 2 (CORTADA) varía según nivel de suciedad de la gema.
       En Fase 3 (PULIDA) no se aplica — colores reales al descubierto.
    ---------------------------------------------------------------- */
    if (g->etapa == ETAPA_BRUTA || g->etapa == ETAPA_CORTADA) {
        aplicar_suciedad_paleta(pal, g);
    }
}
