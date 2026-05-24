#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include "opalo.h"

#define MAX_CAPTURAS 8

// Inicialización y persistencia básica
void     save_init(void); 

// Semillas
uint32_t cargar_seed(void);
void     guardar_seed(uint32_t seed);

// Gestión de chunks
int      cargar_chunks(Chunk* slots);
void     guardar_chunk(const Chunk* c);
void     sobreescribir_chunk(int slot, const Chunk* c);
void     decrementar_num_chunks(void); // Nueva función añadida

// Economía
uint32_t obtener_dinero(void);
void     modificar_dinero(int32_t cantidad);

#endif // SAVE_H
