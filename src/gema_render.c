#include <stdint.h>
#include <string.h>
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

    // Colores base para la matriz de roca (Índices 10 a 29)
    for (int i = 0; i < 20; i++) {
        int v = 8 + i;
        pal[10 + i] = (v & 31) | ((v & 31) << 5) | ((v & 31) << 10);
    }

    pal[0]   = 0x0000; // Transparente / Fondo oscuro
    pal[255] = 0x7FFF; // Blanco puro

    // Índice 9: Color característico del núcleo visible según tipo de ópalo
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

    // Mapeo no lineal para que los ópalos pequeños sigan siendo visibles
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

    /* Sprint 1: Highlight reducido — aspecto pulido, no plástico */
    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = (a * 4 / 10);
    int hb = (b * 2 / 10);
    if (ha < 2) ha = 2; if (hb < 2) hb = 2;
    int ha2 = ha * ha;
    int hb2 = hb * hb;
    int ha2hb2 = ha2 * hb2;

    /* Sprint 2: Rim-light — elipse fina justo dentro del borde,
     * solo en el cuadrante superior-izquierdo (zona iluminada).
     * Radio: 92% del cabujón para que sea un anillo estrecho.    */
    int rim_thresh = a2b2 * 92 / 100;

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

                /* Sprint 2: Oscurecimiento radial — más oscuro cerca del borde.
                 * edge en [0, 256]: 0=centro, 256=borde.
                 * Por encima de 180 (~70% del radio) se oscurece gradualmente. */
                int edge = (dx2 * b2 + dy2 * a2) * 256 / a2b2;
                if (edge > 180) {
                    int fade = (edge - 180);          /* [0, 76]        */
                    int c = (int)color - (fade >> 1); /* resta suave    */
                    color = (c < 16) ? 16 : (uint8_t)c;
                }

                /* Sprint 1: Destello especular (highlight pequeño) */
                int hdx = dx - hx_off;
                int hdy = dy - hy_off;
                if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= ha2hb2) {
                    color = 254;
                }
                /* Sprint 1: Sombra inferior-derecha correcta (oscura, no blanca) */
                else if ((dx2 * b2 + dy2 * a2) > (a2b2 * 85 / 100) && dx >= 0 && dy >= 0) {
                    color = 253;
                }
                /* Sprint 2: Rim-light superior-izquierda — línea fina brillante */
                else if ((dx2 * b2 + dy2 * a2) > rim_thresh && dx <= 0 && dy <= 0) {
                    int c = (int)color + 30;
                    color = (c > 253) ? 253 : (uint8_t)c;
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

/**
 * Renderiza el ópalo en bruto a pantalla completa (240x160).
 * Genera una costra exterior de roca (matriz) rota que deja ver el ópalo por dentro.
 */
void renderizar_roca(const Gema* g) {
    uint8_t* vram = (uint8_t*)get_vram();
    aplicar_paleta_roca(g);

    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    // Incrementamos el radio para simular la capa rugosa de la roca exterior
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

            // ¿Está dentro del contorno exterior de la roca?
            if (dx2 * rb2 + dy2 * ra2 <= ra2rb2) {
                // Pseudo-RNG rápido sin llamadas pesadas para dar textura rugosa a la roca
                s_rng = s_rng * 1103515245 + 12345;
                int ruido = (s_rng >> 16) % 6;

                // Generamos una "grieta/ventana" central donde asoma el núcleo cristalino
                // Modulado por el ruido matemático para que no sea una elipse perfecta
                int limit_gema = (ra2rb2 * 45 / 100) + (ruido * 2000);

                if (dx2 * rb2 + dy2 * ra2 <= limit_gema) {
                    // Ventana del ópalo interno pulido
                    int src_x = (dx + a) * 120 / (a * 2 + 1);
                    int src_y = (dy + b) * 80 / (b * 2 + 1);
                    vram[y * 240 + x] = plasma_pixel_smooth(src_x, src_y, off, g);
                } else {
                    // Matriz de la roca externa (asigna índices de paleta 10-29 con ruido)
                    vram[y * 240 + x] = 10 + ((x + y + ruido) % 20);
                }
            } else {
                vram[y * 240 + x] = 0; // Fondo de pantalla limpio
            }
        }
    }
}

/**
 * Renderiza una roca pequeña texturizada en coordenadas arbitrarias.
 * Ideal para representar sacos recién abiertos o animaciones de inventario.
 */
void renderizar_roca_pequena(int x_pos, int y_pos, const Gema* g) {
    uint8_t* vram = (uint8_t*)get_vram();

    int r = 16; // Radio base para el mineral en bruto miniatura
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

                // Ventanita pequeña que expone destellos del tipo de gema (índice 9)
                if (x2 + y2 <= (r2 * 25 / 100) + ruido) {
                    vram[screen_y * 240 + screen_x] = 9;
                } else {
                    // Cuerpo rugoso de la piedra
                    vram[screen_y * 240 + screen_x] = 12 + ((x + y + ruido) % 15);
                }
            }
        }
    }
}

/**
 * Renderiza un icono o miniatura de ópalo directamente acotado dentro de una caja (BBox).
 * Esencial para los buffers del inventario (render_lista) o los slots de la vitrina.
 */
void renderizar_opalo_pequeno_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    uint8_t* vram = (uint8_t*)get_vram();
    uint32_t s_rng = g->seed ^ (x_pos * y_pos); // Semilla única por posición para variar la textura

    // ================================================================
    // FASE 1: MINERAL EN BRUTO (Icono de roca rugosa)
    // ================================================================
    if (g->etapa == ETAPA_BRUTA) {
        int r = 13; // Radio ideal para que encaje perfectamente en la cuadrícula de 72x46
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
                    // Generador rápido de ruido para romper los bordes perfectos
                    s_rng = s_rng * 1103515245 + 12345;
                    int ruido = (s_rng >> 16) % 4;

                    // Una pequeña "ventana" en el corazón de la piedra revela el color oculto
                    if (x2 + y2 <= 5 + ruido) {
                        vram[screen_y * 240 + screen_x] = 9; // Color característico del tipo de gema
                    } else {
                        // Matriz de roca rugosa (Colores 12 a 27)
                        vram[screen_y * 240 + screen_x] = 12 + ((x + y + ruido) % 15);
                    }
                }
            }
        }
        return; // Terminamos temprano para la roca
    }

    // ================================================================
    // FASES 2 Y 3: CORTADA Y PULIDA (Forma de Cabujón Elíptico)
    // ================================================================
    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    /* Escala para rejilla 72x46 px con centro en la celda.
     * Factor 71/100 para que 225q rece los bordes (~32px).
     * Mínimos bajos para preservar diferencia visual entre tamaños. */
    a = (a * 71) / 100;
    b = (b * 71) / 100;
    if (a < 3) a = 3;
    if (b < 2) b = 2;

    int a2   = a * a;
    int b2   = b * b;
    int a2b2 = a2 * b2;
    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

    /* Borde físico: elipse 1px mayor desplazada +1 abajo-derecha.
     * Los píxeles en el borde pero fuera del interior reciben color 1
     * (gris azulado de init_paleta_ui), creando un contorno con sombra
     * natural — técnica de la versión clásica que daba el mejor aspecto. */
    int ab = a + 1;
    int bb = b + 1;
    int ab2 = ab * ab;
    int bb2 = bb * bb;

    /* Highlight con variación por seed — igual que versión clásica */
    int hl_activo = (a >= 5);
    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = a * 7 / 10;
    int hb = b * 3 / 10;
    int seed_var     = (int)(g->seed & 7);
    int ha_var       = ha - (ha / 10) + (seed_var * ha / 40);
    int hb_var       = hb - (hb / 5)  + (((g->seed >> 3) & 7) * hb / 20);
    if (ha_var < 1) ha_var = 1;
    if (hb_var < 1) hb_var = 1;
    int ha_var2      = ha_var * ha_var;
    int hb_var2      = hb_var * hb_var;
    int total_elipse = ha_var2 * hb_var2;

    /* Iterar sobre bounding box del borde (a+1, b+1) */
    for (int y = -(b + 1); y <= (b + 1); y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int dy2 = y * y;
        for (int x = -(a + 1); x <= (a + 1); x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int dx2 = x * x;
            int inside_main = (dx2 * b2 + dy2 * a2 <= a2b2);

            /* Borde: elipse mayor desplazada +1 px abajo-derecha */
            int shadow_dx  = x - 1;
            int shadow_dy  = y - 1;
            int inside_borde = (shadow_dx * shadow_dx * bb2 +
                                shadow_dy * shadow_dy * ab2 <= ab2 * bb2);

            uint8_t c;

            if (inside_main) {
                int src_x = (x + a) * 120 / ((a * 2) + 1);
                int src_y = (y + b) * 80  / ((b * 2) + 1);
                c = plasma_pixel_smooth(src_x, src_y, off, g);

                /* Sombra inferior-derecha: resta directa sobre el color
                 * del plasma — nunca índice fijo, nunca negro puro.     */
                int edge_dist2 = dx2 * b2 + dy2 * a2;
                if (edge_dist2 > (a2b2 * 85 / 100) && x >= 0 && y >= 0) {
                    int cv = (int)c - 20;
                    if (cv < 16) cv = 16;
                    c = (uint8_t)cv;
                }

                /* Highlight con variación por seed, solo en PULIDA */
                if (hl_activo && g->etapa == ETAPA_PULIDA) {
                    int hdx = x - hx_off;
                    int hdy = y - hy_off;
                    int val_elipse = hb_var2 * hdx * hdx + ha_var2 * hdy * hdy;
                    if (val_elipse <= total_elipse) {
                        if (val_elipse < (total_elipse * 15 / 100)) {
                            c = 254; /* núcleo sólido */
                        } else {
                            if (((x + y) & 1) == 0) c = 254; /* borde dithered */
                        }
                    }
                }

                /* Grietas procedurales */
                if (g->flags & GEMA_FLAG_GRIETAS) {
                    if (((x + y * 2) % 11 == 0 && x > -a && x < a / 2) ||
                        ((x - y) == 1 && y > -b / 2 && y < b)) {
                        c = 11;
                    }
                }

                /* Remap al banco de paleta asignado */
                if (num_colores > 0 && c >= 16 && c < 254) {
                    c = (uint8_t)(base + (c % num_colores));
                }

            } else if (inside_borde) {
                /* Borde físico: color 1 = gris azulado oscuro de init_paleta_ui */
                c = 1;
            } else {
                continue; /* fuera del borde — no pintar, dejar fondo */
            }

            vram[screen_y * 240 + screen_x] = c;
        }
    }
}

// ============================================================================
// ENLACE PARA EL LINKER
// ============================================================================

void renderizar_gema_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores) {
    renderizar_opalo_pequeno_celda(x_pos, y_pos, g, base, num_colores);
}

void renderizar_gema_celda_vieja(int x_pos, int y_pos, const Gema* g) {
    renderizar_opalo_pequeno_celda(x_pos, y_pos, g, 0, 0);
}

/* ================================================================
   RENDER PREVIEW — Bug #1 fix
   Renderiza la gema al buffer software (anim_buf_a) y luego lo
   vuelca sobre el back buffer con volcar_buf_solo_opalo().
   Nunca escribe directo al frame visible.
================================================================ */
void renderizar_gema_preview(int x_pos, int y_pos, const Gema* g) {
    uint8_t* buf = get_anim_buf_a();
    memset(buf, 0, 240 * 160);

    int a, b;
    quilates_to_radii(g->quilates, &a, &b);

    int a2 = a * a;
    int b2 = b * b;
    int a2b2 = a2 * b2;

    /* Sprint 1: Highlight reducido — aspecto pulido, no plástico */
    int hx_off = -(a / 4);
    int hy_off = -(b * 4 / 10);
    int ha = (a * 4 / 10);
    int hb = (b * 2 / 10);
    if (ha < 2) ha = 2; if (hb < 2) hb = 2;
    int ha2 = ha * ha;
    int hb2 = hb * hb;
    int ha2hb2 = ha2 * hb2;

    /* Sprint 2: Rim-light — anillo fino en cuadrante superior-izquierdo */
    int rim_thresh = a2b2 * 92 / 100;

    uint8_t off = (uint8_t)(g->seed ^ (g->seed >> 16));

    for (int y = -b; y <= b; y++) {
        int screen_y = y_pos + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        int dy2 = y * y;
        for (int x = -a; x <= a; x++) {
            int screen_x = x_pos + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int dx2 = x * x;
            if (dx2 * b2 + dy2 * a2 <= a2b2) {
                int src_x = (x + a) * 120 / ((a * 2) + 1);
                int src_y = (y + b) * 80 / ((b * 2) + 1);

                uint8_t color = plasma_pixel_smooth(src_x, src_y, off, g);

                /* Sprint 2: Oscurecimiento radial hacia los bordes */
                int edge = (dx2 * b2 + dy2 * a2) * 256 / a2b2;
                if (edge > 180) {
                    int fade = (edge - 180);
                    int c = (int)color - (fade >> 1);
                    color = (c < 16) ? 16 : (uint8_t)c;
                }

                /* Sprint 1: Destello especular reducido */
                int hdx = x - hx_off;
                int hdy = y - hy_off;
                if ((hb2 * hdx * hdx + ha2 * hdy * hdy) <= ha2hb2) {
                    color = 254;
                }
                /* Sprint 1: Sombra inferior-derecha correcta (oscura, no blanca) */
                else if ((dx2 * b2 + dy2 * a2) > (a2b2 * 85 / 100) && x >= 1 && y >= 1) {
                    color = 253;
                }
                /* Sprint 2: Rim-light superior-izquierda */
                else if ((dx2 * b2 + dy2 * a2) > rim_thresh && x <= 0 && y <= 0) {
                    int c = (int)color + 30;
                    color = (c > 253) ? 253 : (uint8_t)c;
                }

                buf[screen_y * 240 + screen_x] = color;
            }
        }
    }

    // Volcado sobre el back buffer: solo píxeles no-cero, sin offset
    volcar_buf_solo_opalo(buf, 0);
}

/* ---------------- GENERACIÓN DE PALETAS PARA GEMA ---------------- */

void generar_paleta_gema_rango(const Gema* g, int base, int num_colores) {
    uint16_t* pal = (uint16_t*)0x05000000;

    TipoOpalo tipo = gema_tipo(g);
    uint8_t iridiscencia = gema_iridiscencia(g);
    uint8_t color_offset = (uint8_t)(g->seed & 0xFF);

    int spread = 1 + (iridiscencia / 8);

    for (int i = 0; i < num_colores; i++) {
        int src = 16 + (i * 239) / num_colores;
        int r, g, b;

        switch (tipo) {
            case OPALO_NEGRO:
                r = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                g = (lut_c((src * spread * 2) + color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((src * spread * 2) + color_offset) + 4096) >> 11);
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((src * spread) + color_offset) + 4096) >> 9);
                g = 20 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                g = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + color_offset) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r = 25 + ((lut_s((src * spread) + color_offset) + 4096) >> 9);
                g = 15 + ((lut_c((src * spread) + color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((src * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                g = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((src * spread) + color_offset) + 4096) >> 10);
                break;
            default:
                r = (lut_s((src * spread) + color_offset) + 4096) >> 8;
                g = (lut_s((src * spread * 2) + color_offset) + 4096) >> 8;
                b = (lut_c((src * spread) + color_offset) + 4096) >> 8;
                break;
        }

        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (b > 31) b = 31;

        pal[base + i] = (r & 31) | ((g & 31) << 5) | ((b & 31) << 10);
    }

    /* Escribir sombras 251/252 con tinte del tipo — mismos valores que generar_paleta_gema.
     * Son índices globales compartidos por todas las gemas de la rejilla.
     * La última gema renderizada gana, lo cual es aceptable visualmente. */
    int sh_r, sh_g, sh_b;
    int sh2_r, sh2_g, sh2_b;
    switch (tipo) {
        case OPALO_NEGRO:
            sh_r=4;  sh_g=3;  sh_b=8;
            sh2_r=7; sh2_g=6; sh2_b=12; break;
        case OPALO_CRISTAL:
            sh_r=6;  sh_g=6;  sh_b=10;
            sh2_r=10; sh2_g=10; sh2_b=14; break;
        case OPALO_FUEGO:
            sh_r=12; sh_g=4;  sh_b=2;
            sh2_r=16; sh2_g=7; sh2_b=4; break;
        case OPALO_BLANCO:
            sh_r=10; sh_g=10; sh_b=10;
            sh2_r=14; sh2_g=14; sh2_b=14; break;
        case OPALO_ROSA:
            sh_r=14; sh_g=6;  sh_b=10;
            sh2_r=18; sh2_g=9; sh2_b=14; break;
        default:
            sh_r=8;  sh_g=8;  sh_b=9;
            sh2_r=12; sh2_g=12; sh2_b=13; break;
    }
    pal[252] = (sh_r  & 31) | ((sh_g  & 31) << 5) | ((sh_b  & 31) << 10);
    pal[251] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10);
}

void generar_paleta_gema(const Gema* g) {
    uint16_t* pal = (uint16_t*)0x05000000;

    TipoOpalo tipo = gema_tipo(g);
    uint8_t iridiscencia = gema_iridiscencia(g);
    uint8_t brillo       = gema_brillo(g);
    uint8_t color_offset = (uint8_t)(g->seed & 0xFF);

    int spread = 1 + (iridiscencia / 8);

    for (int i = 16; i < 254; i++) {
        int r, g, b;
        switch (tipo) {
            case OPALO_NEGRO:
                r = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                g = (lut_c((i * spread * 2) + color_offset) + 4096) >> 8;
                b = 26 + ((lut_s((i * spread * 2) + color_offset) + 4096) >> 11);
                break;
            case OPALO_CRISTAL:
                r = 20 + ((lut_s((i * spread) + color_offset) + 4096) >> 9);
                g = 20 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                b = 25 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_FUEGO:
                r = 28 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                g = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + color_offset) + 4096) >> 10;
                break;
            case OPALO_ROSA:
                r = 25 + ((lut_s((i * spread) + color_offset) + 4096) >> 9);
                g = 15 + ((lut_c((i * spread) + color_offset) + 4096) >> 10);
                b = 20 + ((lut_c((i * spread) + color_offset) + 4096) >> 9);
                break;
            case OPALO_GRIS:
                r = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                g = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                b = 15 + ((lut_s((i * spread) + color_offset) + 4096) >> 10);
                break;
            default:
                r = (lut_s((i * spread) + color_offset) + 4096) >> 8;
                g = (lut_s((i * spread * 2) + color_offset) + 4096) >> 8;
                b = (lut_c((i * spread) + color_offset) + 4096) >> 8;
                break;
        }
        if (r > 31) r = 31; if (g > 31) g = 31; if (b > 31) b = 31;
        pal[i] = (r & 31) | ((g & 31) << 5) | ((b & 31) << 10);
    }

    // Lógica del borde basada en el brillo dinámico
    int borde_v = 24 + ((brillo - 16) * 4 / 15);
    int tinte = (color_offset & 3) - 1;
    int borde_r = borde_v + tinte;
    int borde_b = borde_v - tinte;
    if (borde_r > 30) borde_r = 30; if (borde_r < 22) borde_r = 22;
    if (borde_b > 30) borde_b = 30; if (borde_b < 22) borde_b = 22;
    pal[15] = (borde_r & 31) | ((borde_v & 31) << 5) | ((borde_b & 31) << 10);

    // Lógica del Brillo/Highlight basada en el brillo dinámico
    int hl_base = 29 + ((brillo - 16) * 3 / 15);
    int hl_r, hl_g, hl_b;
    switch (tipo) {
        case OPALO_CRISTAL: hl_r = 31; hl_g = 31; hl_b = 31; break;
        case OPALO_FUEGO:
            /* Siempre blanco cálido. hl_b mínimo 27 para que nunca
             * derive a rojo aunque el brillo de la gema sea bajo.   */
            hl_r = 31;
            hl_g = 31;
            hl_b = 27 + ((brillo - 16) * 4 / 15);
            if (hl_b > 31) hl_b = 31;
            break;
        case OPALO_NEGRO:
            hl_r = hl_base - 2; hl_g = hl_base - 1; hl_b = hl_base;
            if (hl_r < 30) hl_r = 30;
            break;
        case OPALO_ROSA:
            hl_r = 31;
            hl_g = 25 + ((brillo - 16) * 4 / 15);
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

    /* Sombra inferior-derecha: tonos oscurecidos del color propio del ópalo.
     * 251 = capa interior (más suave), 252 = capa exterior (más oscura).
     * Nunca negro puro — siempre con tinte del tipo de gema.            */
    int sh_r, sh_g, sh_b;    /* capa exterior — más oscura  */
    int sh2_r, sh2_g, sh2_b; /* capa interior — más suave  */
    switch (tipo) {
        case OPALO_NEGRO:
            sh_r=4;  sh_g=3;  sh_b=8;
            sh2_r=7; sh2_g=6; sh2_b=12; break;
        case OPALO_CRISTAL:
            sh_r=6;  sh_g=6;  sh_b=10;
            sh2_r=10; sh2_g=10; sh2_b=14; break;
        case OPALO_FUEGO:
            sh_r=12; sh_g=4;  sh_b=2;
            sh2_r=16; sh2_g=7; sh2_b=4; break;
        case OPALO_BLANCO:
            sh_r=10; sh_g=10; sh_b=10;
            sh2_r=14; sh2_g=14; sh2_b=14; break;
        case OPALO_ROSA:
            sh_r=14; sh_g=6;  sh_b=10;
            sh2_r=18; sh2_g=9; sh2_b=14; break;
        default: /* GRIS y resto */
            sh_r=8;  sh_g=8;  sh_b=9;
            sh2_r=12; sh2_g=12; sh2_b=13; break;
    }
    pal[252] = (sh_r  & 31) | ((sh_g  & 31) << 5) | ((sh_b  & 31) << 10);
    pal[251] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10);
    pal[253] = (sh2_r & 31) | ((sh2_g & 31) << 5) | ((sh2_b & 31) << 10); /* Sombra render_pro — mismo tinte que miniaturas */
    pal[255] = 0x4210; /* Texto / Sombras oscuras */
}
