#ifndef SACOS_H
#define SACOS_H

#include <stdint.h>

typedef struct {
    char nombre[20];
    uint16_t precio;
    uint8_t cantidad_chunks;
    uint8_t tier;
} Saco;

extern const Saco SACOS[4];

void comprar_saco(int idx);

#endif
