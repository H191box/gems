#include <stdint.h>
#include "save.h"

#define SRAM_BASE ((uint8_t*)0x0E000000)

// -----------------------------------------------------
// SRAM LAYOUT
// -----------------------------------------------------

#define SRAM_HEADER 0x00
#define SRAM_NUM    0x10
#define SRAM_SLOTS  0x20

#define SAVE_MAGIC  "OPAL"
#define SAVE_VERSION 1

// -----------------------------------------------------
// SAVE HEADER
// -----------------------------------------------------

typedef struct {

    char magic[4];

    uint32_t version;

    uint32_t current_seed;

} SaveHeader;

// -----------------------------------------------------
// SRAM IO
// -----------------------------------------------------

static void sram_read(
    void* dst,
    const void* src,
    uint32_t size
) {
    volatile uint8_t* s = (volatile uint8_t*)src;

    uint8_t* d = (uint8_t*)dst;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static void sram_write(
    void* dst,
    const void* src,
    uint32_t size
) {
    volatile uint8_t* d = (volatile uint8_t*)dst;

    const uint8_t* s = (const uint8_t*)src;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

// -----------------------------------------------------
// VALIDATION
// -----------------------------------------------------

static int save_valido(void) {

    SaveHeader h;

    sram_read(
        &h,
        SRAM_BASE + SRAM_HEADER,
        sizeof(SaveHeader)
    );

    return
        h.magic[0] == 'O' &&
        h.magic[1] == 'P' &&
        h.magic[2] == 'A' &&
        h.magic[3] == 'L' &&
        h.version == SAVE_VERSION;
}

// -----------------------------------------------------
// INIT
// -----------------------------------------------------

void save_init(void) {

    if (save_valido()) {
        return;
    }

    SaveHeader h;

    h.magic[0] = 'O';
    h.magic[1] = 'P';
    h.magic[2] = 'A';
    h.magic[3] = 'L';

    h.version = SAVE_VERSION;

    h.current_seed = 1234567;

    sram_write(
        SRAM_BASE + SRAM_HEADER,
        &h,
        sizeof(SaveHeader)
    );

    uint32_t cero = 0;

    sram_write(
        SRAM_BASE + SRAM_NUM,
        &cero,
        sizeof(uint32_t)
    );
}

// -----------------------------------------------------
// CURRENT SEED
// -----------------------------------------------------

uint32_t cargar_seed(void) {

    save_init();

    SaveHeader h;

    sram_read(
        &h,
        SRAM_BASE + SRAM_HEADER,
        sizeof(SaveHeader)
    );

    return h.current_seed;
}

void guardar_seed(uint32_t seed) {

    save_init();

    SaveHeader h;

    sram_read(
        &h,
        SRAM_BASE + SRAM_HEADER,
        sizeof(SaveHeader)
    );

    h.current_seed = seed;

    sram_write(
        SRAM_BASE + SRAM_HEADER,
        &h,
        sizeof(SaveHeader)
    );
}

// -----------------------------------------------------
// GALLERY
// -----------------------------------------------------

int cargar_capturas(uint32_t* slots) {

    save_init();

    uint32_t n = 0;

    sram_read(
        &n,
        SRAM_BASE + SRAM_NUM,
        sizeof(uint32_t)
    );

    if (n > MAX_CAPTURAS) {
        n = 0;
    }

    for (uint32_t i = 0; i < n; i++) {

        sram_read(
            &slots[i],
            SRAM_BASE + SRAM_SLOTS + i * 4,
            sizeof(uint32_t)
        );
    }

    return (int)n;
}

void guardar_captura(uint32_t seed) {

    save_init();

    uint32_t n = 0;

    sram_read(
        &n,
        SRAM_BASE + SRAM_NUM,
        sizeof(uint32_t)
    );

    if (n > MAX_CAPTURAS) {
        n = 0;
    }

    if (n >= MAX_CAPTURAS) {
        return;
    }

    sram_write(
        SRAM_BASE + SRAM_SLOTS + n * 4,
        &seed,
        sizeof(uint32_t)
    );

    n++;

    sram_write(
        SRAM_BASE + SRAM_NUM,
        &n,
        sizeof(uint32_t)
    );
}

void sobreescribir_captura(
    int slot,
    uint32_t seed
) {
    save_init();

    if (slot < 0 || slot >= MAX_CAPTURAS) {
        return;
    }

    sram_write(
        SRAM_BASE + SRAM_SLOTS + slot * 4,
        &seed,
        sizeof(uint32_t)
    );
}
