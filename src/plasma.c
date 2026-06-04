/*
 * plasma.c
 * Motor de renderizado procedural de patrones de ópalo — GBA
 *
 * RESTRICCIONES DE HARDWARE (GBA / ARM7TDMI):
 *   - Sin FPU: cero floats.
 *   - División entera cara: minimizada con shifts y tablas.
 *   - VRAM limitada: sin buffers intermedios.
 *   - Todos los patrones devuelven índice de paleta en [16, 254].
 *     · 255 reservado para highlight especular (gema_render.c)
 *     · 253 reservado para sombra oscura    (gema_render.c)
 */

#include <stdint.h>
#include "gema.h"
#include "plasma.h"

/* ================================================================== */
/* LUT TRIGONOMÉTRICA                                                 */
/* 64 entradas, sin(i * 2π/64) escalado a [0, 3817]                  */
/* ================================================================== */

static const int16_t sin_lut[64] = {
    0,   100,  201,  301,  401,  501,  601,  700,
  799,   897,  995, 1092, 1189, 1285, 1380, 1474,
 1567,  1660, 1751, 1842, 1931, 2019, 2106, 2191,
 2275,  2357, 2438, 2517, 2594, 2669, 2743, 2814,
 2884,  2951, 3016, 3080, 3140, 3199, 3255, 3309,
 3360,  3409, 3456, 3500, 3541, 3579, 3615, 3648,
 3678,  3706, 3730, 3752, 3770, 3786, 3798, 3807,
 3814,  3817, 3817, 3814, 3807, 3798, 3786, 3770
};

inline int16_t lut_s(int i) { return sin_lut[i & 63]; }
inline int16_t lut_c(int i) { return sin_lut[(i + 16) & 63]; }

/* ================================================================== */
/* UTILIDAD: clamp a rango de paleta [16, 254]                        */
/* ================================================================== */

static inline uint8_t pal_clamp(int v)
{
    if (v < 16)  return 16;
    if (v > 254) return 254;
    return (uint8_t)v;
}

/* ================================================================== */
/* HASH LIGERO                                                         */
/* XOR-shift de 16 bits — O(1), cero divisiones, cero multiplicaciones
 * de 32 bits. Suficiente entropía para patrones visuales.            */
/* ================================================================== */

static inline uint16_t xhash(uint16_t x)
{
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 3);
    return x;
}

/* ================================================================== */
/* PATRÓN 0 — NEBULA                                                  */
/* Base fluida de plasma. Usada por todos los demás patrones.         */
/* Coste: 2 accesos LUT + 2 sumas + 1 shift.                         */
/* ================================================================== */

uint8_t pixel_nebula(int x, int y, uint8_t off)
{
    int v = (lut_s((x + off) >> 2) + lut_c((y + off) >> 2) + 128) & 255;
    return pal_clamp(16 + ((v * 239) >> 8));
}

/* ================================================================== */
/* PATRÓN 1 — VENAS                                                   */
/* Bandas largas mineralizadas.                                       */
/*                                                                    */
/* Técnica: dos ondas de baja frecuencia con ejes asimétricos.        */
/* X se mueve muy lento (>> 5), Y más rápido (>> 2).                 */
/* El valor absoluto de la resta crea "crestas" estrechas brillantes  */
/* sobre un fondo oscuro, que simulan venas de mineral.               */
/* Umbral fijo en 40: venas delgadas y definidas.                     */
/* ================================================================== */

uint8_t pixel_venas(int x, int y, uint8_t off)
{
    /* Onda primaria: orientación casi horizontal */
    int onda_a = lut_s(((x >> 5) + (y >> 3) + off) & 63);

    /* Onda secundaria: ligera curvatura diagonal */
    int onda_b = lut_c(((x >> 4) - (y >> 5) + off + 17) & 63);

    /* Cresta = diferencia absoluta entre las dos ondas */
    int diff = onda_a - onda_b;
    if (diff < 0) diff = -diff;

    /* diff en [0, ~3817]. Normalizar a [0, 255] con shift */
    diff = (diff * 255) >> 12;  /* >> 12 ≈ / 3817 * 255, sin división */

    if (diff < 40) {
        /* Zona de vena: brillo alto, índice alto en paleta */
        return pal_clamp(180 + (diff * 2));
    }

    /* Fondo mineral: base nebula oscurecida */
    int base = pixel_nebula(x, y, off);
    return pal_clamp(16 + ((base - 16) >> 1));  /* mitad de brillo */
}

/* ================================================================== */
/* PATRÓN 2 — MOSAICO                                                 */
/* Grandes células irregulares, tipo boulder opal.                   */
/*                                                                    */
/* Técnica: se divide el espacio en celdas de 16x16.                  */
/* Cada celda tiene un "centro" calculado con hash de sus coordenadas */
/* de celda + offset. El color del píxel es función de su distancia   */
/* al centro de la celda, creando manchas con bordes suaves.          */
/* ================================================================== */

uint8_t pixel_mosaico(int x, int y, uint8_t off)
{
    /* Celda a la que pertenece este píxel (16x16 píxeles por celda) */
    int cx = x >> 4;
    int cy = y >> 4;

    /* Hash de la celda: genera un centro pseudo-aleatorio dentro     */
    uint16_t h = xhash((uint16_t)(cx * 31 + cy * 17 + off));

    /* Centro de la mancha dentro de la celda: [0, 15]               */
    int centro_x = (cx << 4) + (h & 15);
    int centro_y = (cy << 4) + ((h >> 4) & 15);

    /* Distancia Manhattan al centro (más barata que Euclidiana)      */
    int dx = x - centro_x;
    int dy = y - centro_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int dist = dx + dy;  /* [0, ~22] */

    /* Bordes oscuros, centros brillantes                             */
    if (dist > 10) {
        /* Borde de celda: oscuro, base mineral */
        return pal_clamp(20 + (dist >> 1));
    }

    /* Interior de la mancha: color derivado del hash de la celda     */
    uint8_t color_celda = pal_clamp(120 + (int)(h >> 8));
    int suav = ((10 - dist) * (int)(color_celda - 16)) / 10;
    return pal_clamp(16 + suav);
}

/* ================================================================== */
/* PATRÓN 3 — CHAOS                                                   */
/* Manchas fragmentadas de alto contraste.                            */
/*                                                                    */
/* Técnica: hash XOR de coordenadas desplazadas por off.              */
/* Produce islas de color irregulares con bordes abruptos.            */
/* Umbral agresivo: sólo el 25% superior de valores es "activo".     */
/* ================================================================== */

uint8_t pixel_chaos(int x, int y, uint8_t off)
{
    /* Escalar coordenadas para controlar el tamaño de las manchas:
     * >> 3 da manchas de ~8 píxeles, visible y barato.              */
    uint16_t hx = xhash((uint16_t)((x >> 3) ^ (off & 0xFF)));
    uint16_t hy = xhash((uint16_t)((y >> 3) ^ (off >> 1)));

    /* Combinar con XOR y mezcla simple */
    uint16_t ruido = xhash(hx ^ hy);

    /* Normalizar a [0, 255] */
    uint8_t n = (uint8_t)(ruido >> 8);

    /* Umbral agresivo: sólo el cuartil superior brilla               */
    if (n > 192) {
        /* Mancha activa: color vivo derivado del hash                */
        return pal_clamp(160 + (n - 192));
    }
    if (n > 128) {
        /* Zona de transición: semi-apagado                           */
        return pal_clamp(60 + ((n - 128) >> 1));
    }

    /* Zona apagada: muy oscuro, alto contraste con las manchas       */
    return pal_clamp(16 + (n >> 3));
}

/* ================================================================== */
/* PATRÓN 4 — HARLEQUIN                                              */
/* Bloques geométricos desplazados. El patrón más raro y valioso.    */
/*                                                                    */
/* Técnica: XOR de coordenadas divididas en bloques de 16px.          */
/* El desplazamiento alterno entre filas pares/impares rompe la      */
/* cuadrícula pura y da el aspecto "diamante" del harlequin real.    */
/* Dos colores alternativos por celda según paridad del hash.         */
/* ================================================================== */

uint8_t pixel_harlequin(int x, int y, uint8_t off)
{
    /* Bloques de 16x16, con offset en filas pares/impares            */
    int fila  = y >> 4;
    int col_x = x + ((fila & 1) ? 8 : 0);  /* desplazamiento alterno */
    int col   = col_x >> 4;

    /* Hash de bloque con off para variedad entre gemas               */
    uint16_t h = xhash((uint16_t)(col * 23 + fila * 41 + off));

    /* Posición dentro del bloque: [0, 15] en cada eje               */
    int bx = col_x & 15;
    int by = y     & 15;

    /* Borde fino del bloque: 1 píxel en cada lado                    */
    if (bx == 0 || by == 0) {
        return pal_clamp(20);  /* borde oscuro, separa los bloques    */
    }

    /* Color del bloque: dos tonos alternos según bit del hash        */
    if (h & 1) {
        /* Bloque "activo": color vivo con ligera variación interna   */
        int var = (bx + by) >> 1;  /* gradiente suave interior       */
        return pal_clamp(160 + (int)(h >> 9) + var);
    } else {
        /* Bloque "apagado": tono complementario oscuro               */
        int var = (bx + by) >> 2;
        return pal_clamp(50 + (int)((h >> 9) & 31) + var);
    }
}

/* ================================================================== */
/* PATRÓN 5 — MATRIX                                                  */
/* Mezcla premium de nebula con manchas de iridiscencia superpuestas. */
/* Requiere acceso a la Gema para leer gema_iridiscencia().           */
/* ================================================================== */

static uint8_t pixel_matrix(int x, int y, uint8_t off, const Gema *g)
{
    uint8_t base = pal_clamp(10 + (pixel_nebula(x, y, off) >> 4) % 20);

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
        return pal_clamp(mezclado);
    }

    return pal_clamp((int)base);
}

/* ================================================================== */
/* API PÚBLICA                                                         */
/* ================================================================== */

uint8_t plasma_pixel(int x, int y, uint8_t off, const Gema *g)
{
    switch (gema_patron(g)) {
        case PATRON_VENAS:     return pixel_venas    (x, y, off);
        case PATRON_MOSAICO:   return pixel_mosaico  (x, y, off);
        case PATRON_CHAOS:     return pixel_chaos     (x, y, off);
        case PATRON_HARLEQUIN: return pixel_harlequin (x, y, off);
        case PATRON_MATRIX:    return pixel_matrix    (x, y, off, g);
        default:               return pixel_nebula    (x, y, off);
    }
}

uint8_t plasma_pixel_smooth(int x, int y, uint8_t off, const Gema *g)
{
    /* Promedio 2x2 */
    int avg = (  (int)plasma_pixel(x,     y,     off, g)
               + (int)plasma_pixel(x + 1, y,     off, g)
               + (int)plasma_pixel(x,     y + 1, off, g)
               + (int)plasma_pixel(x + 1, y + 1, off, g)) >> 2;

    /* Segunda capa desplazada para añadir profundidad */
    int rx = (x + y) >> 1;
    int ry = (y - x + 256) >> 1;
    int layer2 = (int)plasma_pixel(rx, ry, off + 37, g);

    /*
     * Mezcla 8:2 (antes era 6:4).
     * Conserva el 80% del patrón original; la segunda capa
     * sólo añade profundidad sin destruir el detalle.
     */
    int blended = (avg * 8 + layer2 * 2) / 10;

    return pal_clamp(blended);
}
