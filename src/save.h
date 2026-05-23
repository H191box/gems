#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

#define MAX_CAPTURAS 8

// -----------------------------------------------------
// SAVE SYSTEM
// -----------------------------------------------------

void save_init(void);

// -----------------------------------------------------
// CURRENT SEED
// -----------------------------------------------------

uint32_t cargar_seed(void);
void guardar_seed(uint32_t seed);

// -----------------------------------------------------
// GALLERY
// -----------------------------------------------------

int cargar_capturas(uint32_t* slots);

void guardar_captura(uint32_t seed);

void sobreescribir_captura(
    int slot,
    uint32_t seed
);

#endif
