#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include "opalo.h"
#include "gema.h"   // ✅ necesario para Gema

// ============================================================
// CAPACIDADES
// ============================================================

#define MAX_GALERIA   32
#define MAX_TALLER    64
#define MAX_GALERIA2   9
#define MAX_GEMAS     64   // ✅ NUEVO

// ============================================================
// INIT / SEED
// ============================================================

void     save_init(void);
uint32_t cargar_seed(void);
void     guardar_seed(uint32_t seed);

// ============================================================
// GALERÍA 1
// ============================================================

int      cargar_chunks(Chunk* slots);
void     guardar_chunk(const Chunk* c);
void     sobreescribir_chunk(int slot, const Chunk* c);
void     decrementar_num_chunks(void);

// ============================================================
// GALERÍA 2 (vitrina 3x3)
// ============================================================

int      cargar_chunks_galeria2(Chunk* slots);
void     guardar_chunk_galeria2(const Chunk* c);
void     sobreescribir_chunk_galeria2(int slot, const Chunk* c);
void     decrementar_num_chunks_galeria2(void);

// ============================================================
// TALLER
// ============================================================

int      cargar_chunks_taller(Chunk* slots);
void     guardar_chunk_taller(const Chunk* c);
void     reset_taller(void);

// ============================================================
// GEMAS (NUEVO SISTEMA)
// ============================================================

int      cargar_gemas(Gema* coleccion);
void     guardar_gema(const Gema* g);
void     actualizar_gema_en_sram(int index, const Gema* g);

// ============================================================
// ECONOMÍA
// ============================================================

uint32_t obtener_dinero(void);
void     modificar_dinero(int32_t cantidad);

// ============================================================
// ESTADO MUNDIAL
// ============================================================

void     sync_save_world_state(void);
void     obtener_posicion(int* x, int* y);
void     guardar_posicion(int x, int y);

#endif // SAVE_H
