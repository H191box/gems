/* caja_filtro.c */
#include "caja_filtro.h"

/* ------------------------------------------------------------------ */
/* Helpers internos                                                   */
/* ------------------------------------------------------------------ */

/*
 * Índice lineal de una celda en la máscara de 64 bits.
 * Layout: bit = tipo + patron * CF_NUM_TIPOS
 *
 *   patron 0: bits  0.. 5  (los 6 tipos)
 *   patron 1: bits  6..11
 *   ...
 *   patron 5: bits 30..35
 */
static int bit_index(int tipo, int patron)
{
    return tipo + patron * CF_NUM_TIPOS;
}

static int rango_valido(int tipo, int patron)
{
    return (tipo   >= 0 && tipo   < CF_NUM_TIPOS) &&
           (patron >= 0 && patron < CF_NUM_PATRONES);
}

/* ------------------------------------------------------------------ */
/* API básica                                                         */
/* ------------------------------------------------------------------ */

void caja_filtro_limpiar(CajaFiltro *f)
{
    f->mascara = 0;
}

void caja_filtro_activar(CajaFiltro *f, int tipo, int patron)
{
    if (!rango_valido(tipo, patron)) return;
    f->mascara |= (uint64_t)1 << bit_index(tipo, patron);
}

void caja_filtro_desactivar(CajaFiltro *f, int tipo, int patron)
{
    if (!rango_valido(tipo, patron)) return;
    f->mascara &= ~((uint64_t)1 << bit_index(tipo, patron));
}

int caja_filtro_activo(const CajaFiltro *f, int tipo, int patron)
{
    if (!rango_valido(tipo, patron)) return 0;
    return (f->mascara >> bit_index(tipo, patron)) & 1;
}

/* ------------------------------------------------------------------ */
/* Selección masiva                                                   */
/* ------------------------------------------------------------------ */

void caja_filtro_toggle_fila(CajaFiltro *f, int tipo)
{
    if (tipo < 0 || tipo >= CF_NUM_TIPOS) return;

    /* ¿Toda la fila ya activa? */
    int todos = 1;
    for (int p = 0; p < CF_NUM_PATRONES; p++) {
        if (!caja_filtro_activo(f, tipo, p)) { todos = 0; break; }
    }

    /* Si todos activos → limpiar fila; si no → activar fila entera */
    for (int p = 0; p < CF_NUM_PATRONES; p++) {
        if (todos) caja_filtro_desactivar(f, tipo, p);
        else       caja_filtro_activar   (f, tipo, p);
    }
}

void caja_filtro_toggle_columna(CajaFiltro *f, int patron)
{
    if (patron < 0 || patron >= CF_NUM_PATRONES) return;

    int todos = 1;
    for (int t = 0; t < CF_NUM_TIPOS; t++) {
        if (!caja_filtro_activo(f, t, patron)) { todos = 0; break; }
    }

    for (int t = 0; t < CF_NUM_TIPOS; t++) {
        if (todos) caja_filtro_desactivar(f, t, patron);
        else       caja_filtro_activar   (f, t, patron);
    }
}

void caja_filtro_reset(CajaFiltro *f)
{
    caja_filtro_limpiar(f);
}

/* ------------------------------------------------------------------ */
/* Aplicación sobre gemas                                             */
/* ------------------------------------------------------------------ */

/*
 * Caja vacía (mascara == 0) = sin filtro configurado → acepta todo.
 * Caja con al menos un bit activo filtra estrictamente.
 */
int caja_filtro_vacia(const CajaFiltro *f)
{
    return f->mascara == 0;
}

int caja_filtro_acepta(const CajaFiltro *f, const Gema *g)
{
    if (caja_filtro_vacia(f)) return 1;   /* sin filtro: acepta todo */

    int tipo   = (int)gema_tipo(g);
    int patron = (int)gema_patron(g);

    if (tipo   < 0 || tipo   >= CF_NUM_TIPOS)    return 0;
    if (patron < 0 || patron >= CF_NUM_PATRONES) return 0;

    return caja_filtro_activo(f, tipo, patron);
}
