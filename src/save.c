#include <stdint.h>
#include "save.h"
#include "opalo.h"
#include "data.h" // Necesario para acceder a los globales

#define SRAM_BASE ((uint8_t*)0x0E000000)

#define SRAM_HEADER       0x00
#define SRAM_NUM_GAL      0x10
#define SRAM_NUM_TALLER   0x14
#define SRAM_SLOTS_GAL    0x40
#define SRAM_SLOTS_TALLER 0x400
#define SRAM_DINERO       0x1200

#define SAVE_MAGIC   "OPAL"
#define SAVE_VERSION 4 // Incrementado para incluir datos de tiempo/ciudad

typedef struct {
    char     magic[4];
    uint32_t version;
    uint32_t current_seed;
    uint8_t  dia;      // Guardado de tiempo
    uint8_t  mes;
    uint8_t  ciudad;   // Guardado de ubicación
    uint8_t  padding;  // Alineación
} SaveHeader;

// --- Helpers de SRAM ---
static void sram_read(void* dst, const void* src, uint32_t size) {
    volatile uint8_t* s = (volatile uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

static void sram_write(void* dst, const void* src, uint32_t size) {
    volatile uint8_t* d = (volatile uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

static int save_valido(void) {
    SaveHeader h;
    sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));
    return h.magic[0] == 'O' && h.magic[1] == 'P' && h.magic[2] == 'A' && h.magic[3] == 'L' && h.version == SAVE_VERSION;
}

void save_init(void) {
    if (save_valido()) {
        // Cargar estado mundial al iniciar
        SaveHeader h;
        sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));
        dia_actual = h.dia;
        mes_actual = h.mes;
        ciudad_actual_idx = h.ciudad;
        return;
    }

    // Inicialización por defecto
    SaveHeader h = {"OPAL", SAVE_VERSION, 1234567, 1, 1, 0, 0};
    sram_write(SRAM_BASE + SRAM_HEADER, &h, sizeof(SaveHeader));

    uint32_t cero = 0;
    uint32_t dinero = 500;
    sram_write(SRAM_BASE + SRAM_NUM_GAL, &cero, sizeof(uint32_t));
    sram_write(SRAM_BASE + SRAM_NUM_TALLER, &cero, sizeof(uint32_t));
    sram_write(SRAM_BASE + SRAM_DINERO, &dinero, sizeof(uint32_t));
}

// Función auxiliar para actualizar el header cuando el tiempo/ciudad cambia
void sync_save_world_state(void) {
    SaveHeader h;
    sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));
    h.dia = dia_actual;
    h.mes = mes_actual;
    h.ciudad = ciudad_actual_idx;
    sram_write(SRAM_BASE + SRAM_HEADER, &h, sizeof(SaveHeader));
}
// --- FUNCIONES GALERÍA ---
int cargar_chunks(Chunk* slots) {
    save_init();
    uint32_t n = 0;
    sram_read(&n, SRAM_BASE + SRAM_NUM_GAL, sizeof(uint32_t));
    if (n > MAX_GALERIA) n = 0;
    for (uint32_t i = 0; i < n; i++) {
        sram_read(&slots[i], SRAM_BASE + SRAM_SLOTS_GAL + i * sizeof(Chunk), sizeof(Chunk));
    }
    return (int)n;
}

void guardar_chunk(const Chunk* c) {
    save_init();
    uint32_t n = 0;
    sram_read(&n, SRAM_BASE + SRAM_NUM_GAL, sizeof(uint32_t));
    if (n >= MAX_GALERIA) return;
    sram_write(SRAM_BASE + SRAM_SLOTS_GAL + n * sizeof(Chunk), c, sizeof(Chunk));
    n++;
    sram_write(SRAM_BASE + SRAM_NUM_GAL, &n, sizeof(uint32_t));
}

void sobreescribir_chunk(int slot, const Chunk* c) {
    save_init();
    if (slot < 0 || slot >= MAX_GALERIA) return;
    sram_write(SRAM_BASE + SRAM_SLOTS_GAL + slot * sizeof(Chunk), c, sizeof(Chunk));
}

void decrementar_num_chunks(void) {
    save_init();
    uint32_t n = 0;
    sram_read(&n, SRAM_BASE + SRAM_NUM_GAL, sizeof(uint32_t));
    if (n > 0) {
        n--;
        sram_write(SRAM_BASE + SRAM_NUM_GAL, &n, sizeof(uint32_t));
    }
}

// --- FUNCIONES TALLER ---
int cargar_chunks_taller(Chunk* slots) {
    save_init();
    uint32_t n = 0;
    sram_read(&n, SRAM_BASE + SRAM_NUM_TALLER, sizeof(uint32_t));
    if (n > MAX_TALLER) n = 0;
    for (uint32_t i = 0; i < n; i++) {
        sram_read(&slots[i], SRAM_BASE + SRAM_SLOTS_TALLER + i * sizeof(Chunk), sizeof(Chunk));
    }
    return (int)n;
}

void guardar_chunk_taller(const Chunk* c) {
    save_init();
    uint32_t n = 0;
    sram_read(&n, SRAM_BASE + SRAM_NUM_TALLER, sizeof(uint32_t));
    if (n >= MAX_TALLER) return;
    sram_write(SRAM_BASE + SRAM_SLOTS_TALLER + n * sizeof(Chunk), c, sizeof(Chunk));
    n++;
    sram_write(SRAM_BASE + SRAM_NUM_TALLER, &n, sizeof(uint32_t));
}

void reset_taller(void) {
    save_init();
    uint32_t cero = 0;
    sram_write(SRAM_BASE + SRAM_NUM_TALLER, &cero, sizeof(uint32_t));
}

// --- RESTO (SEED, DINERO Y ESTADO MUNDIAL) ---
uint32_t cargar_seed(void) { save_init(); SaveHeader h; sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader)); return h.current_seed; }
void guardar_seed(uint32_t seed) { save_init(); SaveHeader h; sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader)); h.current_seed = seed; sram_write(SRAM_BASE + SRAM_HEADER, &h, sizeof(SaveHeader)); }
uint32_t obtener_dinero(void) { save_init(); uint32_t dinero = 0; sram_read(&dinero, SRAM_BASE + SRAM_DINERO, sizeof(uint32_t)); return dinero; }
void modificar_dinero(int32_t cantidad) { save_init(); uint32_t d = obtener_dinero(); int32_t total = (int32_t)d + cantidad; if (total < 0) total = 0; sram_write(SRAM_BASE + SRAM_DINERO, &total, sizeof(uint32_t)); }

