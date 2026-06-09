#ifndef THUMB_CACHE_H
#define THUMB_CACHE_H

#include <stdint.h>
#include "gema.h"

#define THUMB_W 52
#define THUMB_H 64

typedef struct {
    uint8_t  pixels[THUMB_W * THUMB_H];
    uint16_t palette[16];
    uint8_t  ready;
    uint32_t cached_seed;      /* seed de la última gema renderizada   */
    uint16_t cached_quilates;  /* quilates del último render           */
} ThumbCache;

/*
 * thumb_generar_desde_gema()
 * Genera el thumbnail desde una Gema completa.
 * Usa quilates reales para el tamaño visual y el caché dirty interno.
 * Solo recalcula si la gema cambió desde el último render.
 */
void thumb_generar_desde_gema(ThumbCache *t, const Gema *g);

/*
 * thumb_generar()
 * Compatibilidad: genera desde seed con quilates neutros (50).
 * Usar thumb_generar_desde_gema() para galerías con tamaños reales.
 */
void thumb_generar(ThumbCache *t, uint32_t seed);

/*
 * thumb_dibujar()
 * Vuelca el thumbnail cacheado a VRAM. Coste: memcpy + paleta.
 * Solo actúa si ready == 1.
 */
void thumb_dibujar(ThumbCache *t, uint16_t *vram, int ox, int oy);

#endif /* THUMB_CACHE_H */
