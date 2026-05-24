#include <stdint.h>
#include <string.h>
#include "save.h"
#include "opalo.h"

#define TAMANIO_RAM_DATA 256 
#define SRAM_HEADER    0x00
#define SRAM_NUM       0x10
#define SRAM_SLOTS     0x20
#define SRAM_DINERO    0xA0 

static uint8_t sram_virtual[TAMANIO_RAM_DATA];
static uint8_t inicializado_ram = 0;

void save_init(void) {
    if (inicializado_ram) return;
    
    memset(sram_virtual, 0, TAMANIO_RAM_DATA);

    // Valores iniciales en RAM
    uint32_t seed = 1234567;
    uint32_t n = 0;
    uint32_t dinero = 500;

    // Escribir en memoria virtual
    memcpy(&sram_virtual[SRAM_HEADER + 8], &seed, sizeof(uint32_t)); 
    memcpy(&sram_virtual[SRAM_NUM], &n, sizeof(uint32_t));
    memcpy(&sram_virtual[SRAM_DINERO], &dinero, sizeof(uint32_t));

    inicializado_ram = 1;
}

uint32_t cargar_seed(void) {
    save_init();
    uint32_t seed;
    memcpy(&seed, &sram_virtual[SRAM_HEADER + 8], sizeof(uint32_t));
    return seed;
}

void guardar_seed(uint32_t seed) {
    save_init();
    memcpy(&sram_virtual[SRAM_HEADER + 8], &seed, sizeof(uint32_t));
}

int cargar_chunks(Chunk* slots) {
    save_init();
    uint32_t n;
    memcpy(&n, &sram_virtual[SRAM_NUM], sizeof(uint32_t));
    if (n > MAX_CAPTURAS) n = MAX_CAPTURAS;
    
    memcpy(slots, &sram_virtual[SRAM_SLOTS], n * sizeof(Chunk));
    return (int)n; 
}

void guardar_chunk(const Chunk* c) {
    save_init();
    uint32_t n;
    memcpy(&n, &sram_virtual[SRAM_NUM], sizeof(uint32_t));
    if (n >= MAX_CAPTURAS) return;
    
    memcpy(&sram_virtual[SRAM_SLOTS + (n * sizeof(Chunk))], c, sizeof(Chunk));
    n++;
    memcpy(&sram_virtual[SRAM_NUM], &n, sizeof(uint32_t));
}

void sobreescribir_chunk(int slot, const Chunk* c) {
    save_init();
    if (slot < 0 || slot >= MAX_CAPTURAS) return;
    memcpy(&sram_virtual[SRAM_SLOTS + (slot * sizeof(Chunk))], c, sizeof(Chunk));
}

void decrementar_num_chunks(void) {
    save_init();
    uint32_t n = 0;
    memcpy(&n, &sram_virtual[SRAM_NUM], sizeof(uint32_t));
    if (n > 0) {
        n--;
        memcpy(&sram_virtual[SRAM_NUM], &n, sizeof(uint32_t));
    }
}

uint32_t obtener_dinero(void) {
    save_init();
    uint32_t dinero;
    memcpy(&dinero, &sram_virtual[SRAM_DINERO], sizeof(uint32_t));
    return dinero;
}

void modificar_dinero(int32_t cantidad) {
    save_init();
    uint32_t dinero_actual = obtener_dinero();
    int32_t total = (int32_t)dinero_actual + cantidad;
    if (total < 0) total = 0;
    uint32_t nuevo_dinero = (uint32_t)total;
    memcpy(&sram_virtual[SRAM_DINERO], &nuevo_dinero, sizeof(uint32_t));
}
