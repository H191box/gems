/* caja_filtro.h */
#ifndef CAJA_FILTRO_H
#define CAJA_FILTRO_H

#include <stdint.h>
#include "gema.h"

/* ------------------------------------------------------------------ */
/* Dimensiones de la matriz de filtro                                 */
/* ------------------------------------------------------------------ */

#define NUM_CAJAS       8
#define CF_NUM_TIPOS    NUM_TIPOS_OPALO   /* 6 */
#define CF_NUM_PATRONES NUM_PATRONES      /* 6 */

/* ------------------------------------------------------------------ */
/* Estructura                                                         */
/* 6 tipos × 6 patrones = 36 bits → cabe en uint64_t                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t mascara;
} CajaFiltro;

/* ------------------------------------------------------------------ */
/* API                                                                */
/* ------------------------------------------------------------------ */

void caja_filtro_limpiar   (CajaFiltro *f);
void caja_filtro_activar   (CajaFiltro *f, int tipo, int patron);
void caja_filtro_desactivar(CajaFiltro *f, int tipo, int patron);
int  caja_filtro_activo    (const CajaFiltro *f, int tipo, int patron);

/* Selección masiva (Fase 5) */
void caja_filtro_toggle_fila   (CajaFiltro *f, int tipo);
void caja_filtro_toggle_columna(CajaFiltro *f, int patron);
void caja_filtro_reset         (CajaFiltro *f);

/* Aplicación sobre gemas */
int  caja_filtro_acepta    (const CajaFiltro *f, const Gema *g);

/* Utilidad: 1 si ningún bit activo (caja sin filtro = acepta todo) */
int  caja_filtro_vacia     (const CajaFiltro *f);

#endif /* CAJA_FILTRO_H */
