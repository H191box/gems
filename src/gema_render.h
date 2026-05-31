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

// --- CONTROL DE BUFFERS DE ANIMACIÓN (EWRAM) ---
uint8_t* get_anim_buf_a(void);
uint8_t* get_anim_buf_b(void);

// --- ESCENA DE LA MINA Y PIEDRA BRUTA ---
// Dibuja la roca en la pantalla de minado (corrige el 'implicit declaration' en src/mina.c)
void renderizar_roca(const Gema* g);

// Versión reducida de la piedra bruta para menús o inventario de la mina
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
