#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include "opalo.h"

#define MAX_CAPTURAS 8

// -----------------------------------------------------
// SAVE SYSTEM
// -----------------------------------------------------
void save_init(void);

// -----------------------------------------------------
// CURRENT SEED
// -----------------------------------------------------
uint32_t cargar_seed(void);
void     guardar_seed(uint32_t seed);

// -----------------------------------------------------
// CHUNKS (inventario de rocas sin cortar)
// -----------------------------------------------------
int  cargar_chunks(Chunk* slots);
void guardar_chunk(const Chunk* c);
void sobreescribir_chunk(int slot, const Chunk* c);

#endif
