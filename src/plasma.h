/*
 * plasma.h
 * Motor de renderizado procedural de patrones de ópalo — GBA
 *
 * FILOSOFÍA:
 * Todas las funciones pixel_*() devuelven un índice de paleta en el rango
 * [16, 254]. El índice 255 está reservado para el highlight y el 253 para
 * la sombra oscura en gema_render.c.
 *
 * La LUT trigonométrica (lut_s / lut_c) se expone en el header para que
 * gema_render.c pueda reutilizarla en cálculos de highlight y rim-light
 * sin duplicar la tabla.
 */

#ifndef PLASMA_H
#define PLASMA_H

#include <stdint.h>
#include "gema.h"

/* ------------------------------------------------------------------ */
/* LUT trigonométrica (64 entradas, escala x3817)                     */
/* ------------------------------------------------------------------ */

int16_t lut_s(int i);   /* sin  — i se toma mod 64                   */
int16_t lut_c(int i);   /* cos  — equivale a lut_s(i + 16)           */

/* ------------------------------------------------------------------ */
/* Generadores de píxel por patrón (rango salida: 16–254)             */
/* ------------------------------------------------------------------ */

/* Nebula: base fluida de plasma, usada internamente por el resto     */
uint8_t pixel_nebula(int x, int y, uint8_t off);

/* Venas: bandas largas mineralizadas                                 */
uint8_t pixel_venas(int x, int y, uint8_t off);

/* Mosaico: células irregulares tipo boulder opal                     */
uint8_t pixel_mosaico(int x, int y, uint8_t off);

/* Chaos: manchas fragmentadas de alto contraste                      */
uint8_t pixel_chaos(int x, int y, uint8_t off);

/* Harlequin: bloques geométricos desplazados, patrón más raro        */
uint8_t pixel_harlequin(int x, int y, uint8_t off);

/* ------------------------------------------------------------------ */
/* API pública principal                                              */
/* ------------------------------------------------------------------ */

/*
 * plasma_pixel()
 * Despacha al generador correcto según gema_patron(g).
 * Usa pixel_matrix() para PATRON_MATRIX (requiere acceso a g).
 */
uint8_t plasma_pixel(int x, int y, uint8_t off, const Gema *g);

/*
 * plasma_pixel_smooth()
 * Versión suavizada: promedia 2x2 vecinos y mezcla una segunda capa
 * desplazada. Mezcla conservadora (8:2) para preservar detalle.
 */
uint8_t plasma_pixel_smooth(int x, int y, uint8_t off, const Gema *g);

#endif /* PLASMA_H */
