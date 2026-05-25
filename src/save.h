#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include "opalo.h"

// Definiciones de capacidad
#define MAX_GALERIA 32
#define MAX_TALLER 64

// Funciones de inicialización y semillas
void     save_init(void); 
uint32_t cargar_seed(void);
void     guardar_seed(uint32_t seed);

// Gestión de Chunks: GALERÍA
int      cargar_chunks(Chunk* slots);
void     guardar_chunk(const Chunk* c);
void     sobreescribir_chunk(int slot, const Chunk* c);
void     decrementar_num_chunks(void);

// Gestión de Chunks: TALLER (Nuevas funciones independientes)
int      cargar_chunks_taller(Chunk* slots);
void     guardar_chunk_taller(const Chunk* c);
void     reset_taller(void);

// Gestión de Economía
uint32_t obtener_dinero(void);
void     modificar_dinero(int32_t cantidad);

#endif // SAVE_H
