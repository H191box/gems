#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "gema.h"

// Prototipos de funciones de renderizado y control de vídeo de la GBA
void precalcular_grietas(uint32_t seed, uint8_t num_grietas, int cx, int cy, int ra, int rb);
void generar_paleta_rango(Opalo* o, int base, int num_colores);
void generar_paleta(Opalo* o);
void volcar_buf_offset(const uint8_t* buf, int offset, uint8_t color_fondo);
void volcar_buf_solo_opalo(const uint8_t* buf, int offset);
void volcar_frame(const uint8_t* buf_opalo, int offset, int offset_bg);
void precalcular_fondo(int offset_bg);
void dibujar_fondo_texturizado_optimizado(uint16_t* vram, int offset_bg);
void renderizar_frame_completo(const uint8_t* buf_opalo, int offset_opalo, int offset_bg);
void vsync(void);
void draw_ui_sobre_buffer(const char* nombre);
void renderizar_escena_completa(const Gema* g, int offset_opalo, int offset_fondo);

#endif // RENDER_H
