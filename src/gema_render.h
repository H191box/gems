#ifndef GEMA_RENDER_H
#define GEMA_RENDER_H

#include <stdint.h>
#include "gema.h"

// Buffer global transitorio para el cálculo de grietas procedimentales
extern uint8_t grieta_buf[160][240];

// --- FUNCIONES PRINCIPALES DE RENDERIZADO (BUFFER LÓGICO) ---
void renderizar_gema_a_buffer(uint8_t *buffer, int w, int h, const Gema *g);
void renderizar_opalo_pro(uint8_t *buffer, int w, int h, const Gema *g);
void renderizar_transicion(uint8_t *buf_sal, uint8_t *buf_ent, int progreso);

void renderizar_gema_preview(int x_pos, int y_pos, const Gema* g);



void generar_paleta_gema(const Gema* g);
void generar_paleta_gema_rango(const Gema* g, int base, int num_colores);


// --- CONTROL DE BUFFERS DE ANIMACIÓN (EWRAM) ---
uint8_t* get_anim_buf_a(void);
uint8_t* get_anim_buf_b(void);

// --- MANEJO DE CELDAS ---
// Lógica completa (usada por render_lista)
void renderizar_gema_celda(int x_pos, int y_pos, const Gema* g, int base, int num_colores);

// Versión simplificada (usada por render_thumb, render_g2, render_g3)
void renderizar_gema(int x_pos, int y_pos, const Gema* g);
void renderizar_gema_celda_vieja(int x_pos, int y_pos, const Gema* g);

// --- ESCENA DE LA MINA Y PIEDRA BRUTA ---
void renderizar_roca(const Gema* g);
void renderizar_roca_pequena(int x_pos, int y_pos, const Gema* g);

// --- MINIATURAS E INVENTARIO ---
void renderizar_opalo_pequeno(uint8_t* buf, const Gema* g);

void renderizar_opalo_pequeno_celda(
    int x,
    int y,
    const Gema* g,
    int pal_base,
    int num_colores
);

// --- CÁLCULO DE RUIDO PROCEDIMENTAL (NEBULA/PLASMA) ---
uint8_t pixel_nebula(int x, int y, uint8_t offset);

#endif // GEMA_RENDER_H
