#include <stdint.h>
#include "save.h"
#include "opalo.h"
#include "data.h"
#include "gema.h"

#define GEMA_SIZE sizeof(Gema)

#define SRAM_BASE ((uint8_t*)0x0E000000)

// Layout SRAM
#define SRAM_HEADER        0x0000
#define SRAM_SLOTS_GAL     0x0040
#define SRAM_SLOTS_GAL2    0x0400
#define SRAM_SLOTS_TALLER  0x0800
#define SRAM_DINERO        0x1000
#define SRAM_GEMAS         0x1004   // Justo después del dinero (4 bytes)

// Límites
#define MAX_GEMAS 64

#define SAVE_MAGIC   "OPAL"
#define SAVE_VERSION 10  // Sistema de gemas + persistencia espacial

// 🟢 DECLARACIÓN DE VARIABLES GLOBALES EXTERNAS
extern uint8_t dia_actual;
extern uint8_t mes_actual;
extern int pos_x;
extern int pos_y;
extern int ciudad_actual_idx;


// ============================================================
// SAVE HEADER STRUCT
// ============================================================

typedef struct {
    char     magic[4];      // 0x00
    uint32_t version;       // 0x04
    uint32_t current_seed;  // 0x08

    uint8_t  dia;           // 0x0C
    uint8_t  mes;           // 0x0D
    uint8_t  ciudad;        // 0x0E
    int8_t   pos_x;         // 0x0F
    int8_t   pos_y;         // 0x10

    uint8_t  _pad[3];       // 0x11-0x13

    uint32_t num_gal;       // 0x14
    uint32_t num_taller;    // 0x18
    uint32_t num_gal2;      // 0x1C
    uint32_t num_gemas;     // 0x20 

    uint32_t next_gema_id;  // 0x24   
} __attribute__((packed)) SaveHeader; // 👈 ¡PÓNLO JUSTO AQUÍ!

static int sram_inicializada = 0;

// ============================================================
// SRAM LOW LEVEL
// ============================================================

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

// ============================================================
// HEADER HELPERS
// ============================================================

static void leer_header(SaveHeader* h) {
    sram_read(h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));
}

static void escribir_header(const SaveHeader* h) {
    sram_write(SRAM_BASE + SRAM_HEADER, h, sizeof(SaveHeader));
}

// ============================================================
// VALIDACIÓN
// ============================================================

static int save_valido(void) {
    SaveHeader h;
    sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));

    return h.magic[0]=='O' && h.magic[1]=='P' &&
           h.magic[2]=='A' && h.magic[3]=='L' &&
           h.version == SAVE_VERSION;
}

void save_init(void) {
    if (sram_inicializada) return;
    sram_inicializada = 1;

    // Acceso directo a la paleta de la GBA para pintar el fondo de la pantalla
    volatile uint16_t* paleta_bg = (volatile uint16_t*)0x05000000;

    if (save_valido()) {
        // 🟢 ¡SI ENTRA AQUÍ, EL SAVE ES VÁLIDO Y SE ENTIENDE!
        // Pintamos el fondo de VERDE para saber que ha leído bien la firma "OPAL"
        paleta_bg[0] = (31 << 5); // Verde puro en formato BGR555

        SaveHeader h;
        sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));

        dia_actual = h.dia;
        mes_actual = h.mes;
        pos_x = h.pos_x;
        pos_y = h.pos_y;
        ciudad_actual_idx = h.ciudad;

        // Congelar el juego 2 segundos (aproximadamente 120 frames de VBlank)
        // para que nos dé tiempo a ver el color verde en el emulador
        for(int i = 0; i < 120; i++) {
            while(*(volatile uint16_t*)0x04000006 >= 160);
            while(*(volatile uint16_t*)0x04000006 < 160);
        }
        return;
    }

    // 🔴 ¡SI ENTRA AQUÍ, EL SAVE SE CONSIDERA CORRUPTO O NUEVO!
    // Pintamos el fondo de ROJO para saber que ha fallado la validación
    paleta_bg[0] = 31; // Rojo puro en formato BGR555

    // Congelar el juego 2 segundos para ver el color rojo
    for(int i = 0; i < 120; i++) {
        while(*(volatile uint16_t*)0x04000006 >= 160);
        while(*(volatile uint16_t*)0x04000006 < 160);
    }

    // Inicializar nueva partida limpia de forma normal...
    SaveHeader h = {0};
    h.magic[0]='O'; h.magic[1]='P'; h.magic[2]='A'; h.magic[3]='L';
    h.version      = SAVE_VERSION;
    h.current_seed = 1234567;
    h.dia = 1;
    h.mes = 1;
    h.pos_x = 2;
    h.pos_y = 2;
    h.ciudad = 12;

    escribir_header(&h);

    uint32_t dinero = 500;
    sram_write(SRAM_BASE + SRAM_DINERO, &dinero, sizeof(uint32_t));

    pos_x = 2;
    pos_y = 2;
    ciudad_actual_idx = 12;
}
// ============================================================
// GEMAS API
// ============================================================

int cargar_gemas(Gema* coleccion) {
    save_init();

    SaveHeader h;
    leer_header(&h);

    uint32_t n = h.num_gemas;
    if (n > MAX_GEMAS) n = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint8_t buffer[GEMA_SIZE];
        sram_read(buffer,
                  SRAM_BASE + SRAM_GEMAS + i * GEMA_SIZE,
                  GEMA_SIZE);

        gema_deserializar(&coleccion[i], buffer);
    }

    return (int)n;
}

void guardar_gema(const Gema* g) {
    save_init();

    SaveHeader h;
    leer_header(&h);

    if (h.num_gemas >= MAX_GEMAS) return;

    uint8_t buffer[GEMA_SIZE];
    gema_serializar(g, buffer);

    sram_write(SRAM_BASE + SRAM_GEMAS + h.num_gemas * GEMA_SIZE,
               buffer,
               GEMA_SIZE);

    h.num_gemas++;
    escribir_header(&h);
}

void actualizar_gema_en_sram(int index, const Gema* g) {
    save_init();

    SaveHeader h;
    leer_header(&h);

    if (index < 0 || index >= (int)h.num_gemas) return;

    uint8_t buffer[GEMA_SIZE];
    gema_serializar(g, buffer);

    sram_write(SRAM_BASE + SRAM_GEMAS + index * GEMA_SIZE,
               buffer,
               GEMA_SIZE);
}

// ============================================================
// CHUNKS API (Módulos de Almacenamiento)
// ============================================================

// --- GALERÍA 1 ---
int cargar_chunks(Chunk* slots) {
    save_init();
    SaveHeader h; leer_header(&h);

    uint32_t n = h.num_gal;
    if (n > MAX_GALERIA) n = 0;

    for (uint32_t i = 0; i < n; i++)
        sram_read(&slots[i],
                  SRAM_BASE + SRAM_SLOTS_GAL + i * sizeof(Chunk),
                  sizeof(Chunk));

    return (int)n;
}

void guardar_chunk(const Chunk* c) {
    save_init();
    SaveHeader h; leer_header(&h);

    if (h.num_gal >= MAX_GALERIA) return;

    sram_write(SRAM_BASE + SRAM_SLOTS_GAL + h.num_gal * sizeof(Chunk),
               c,
               sizeof(Chunk));

    h.num_gal++;
    escribir_header(&h);
}

void sobreescribir_chunk(int slot, const Chunk* c) {
    save_init();
    if (slot < 0 || slot >= MAX_GALERIA) return;

    sram_write(SRAM_BASE + SRAM_SLOTS_GAL + slot * sizeof(Chunk),
               c,
               sizeof(Chunk));
}

void decrementar_num_chunks(void) {
    save_init();
    SaveHeader h; 
    leer_header(&h);

    if (h.num_gal > 0) {
        h.num_gal--;
        escribir_header(&h);
    }
}

// --- GALERÍA 2 ---
int cargar_chunks_galeria2(Chunk* slots) {
    save_init();
    SaveHeader h; leer_header(&h);

    uint32_t n = h.num_gal2;
    if (n > MAX_GALERIA2) n = 0;

    for (uint32_t i = 0; i < n; i++)
        sram_read(&slots[i],
                  SRAM_BASE + SRAM_SLOTS_GAL2 + i * sizeof(Chunk),
                  sizeof(Chunk));

    return (int)n;
}

void guardar_chunk_galeria2(const Chunk* c) {
    save_init();
    SaveHeader h; leer_header(&h);

    if (h.num_gal2 >= MAX_GALERIA2) return;

    sram_write(SRAM_BASE + SRAM_SLOTS_GAL2 + h.num_gal2 * sizeof(Chunk),
               c,
               sizeof(Chunk));

    h.num_gal2++;
    escribir_header(&h);
}

void sobreescribir_chunk_galeria2(int slot, const Chunk* c) {
    save_init();
    if (slot < 0 || slot >= MAX_GALERIA2) return;

    sram_write(SRAM_BASE + SRAM_SLOTS_GAL2 + slot * sizeof(Chunk),
               c,
               sizeof(Chunk));
}

void decrementar_num_chunks_galeria2(void) {
    save_init();
    SaveHeader h; 
    leer_header(&h);

    if (h.num_gal2 > 0) {
        h.num_gal2--;
        escribir_header(&h);
    }
}

// --- TALLER ---
int cargar_chunks_taller(Chunk* slots) {
    save_init();
    SaveHeader h; leer_header(&h);

    uint32_t n = h.num_taller;
    if (n > MAX_TALLER) n = 0;

    for (uint32_t i = 0; i < n; i++)
        sram_read(&slots[i],
                  SRAM_BASE + SRAM_SLOTS_TALLER + i * sizeof(Chunk),
                  sizeof(Chunk));

    return (int)n;
}

void guardar_chunk_taller(const Chunk* c) {
    save_init();
    SaveHeader h; leer_header(&h);

    if (h.num_taller >= MAX_TALLER) return;

    sram_write(SRAM_BASE + SRAM_SLOTS_TALLER + h.num_taller * sizeof(Chunk),
               c,
               sizeof(Chunk));

    h.num_taller++;
    escribir_header(&h);
}

void reset_taller(void) {
    save_init();
    SaveHeader h;
    leer_header(&h);
    
    h.num_taller = 0; 
    escribir_header(&h);
}

// ============================================================
// ESTADO MUNDIAL Y ECONOMÍA
// ============================================================

void sync_save_world_state(void) {
    save_init();
    SaveHeader h;
    leer_header(&h);

    extern int pos_x, pos_y;
    extern int ciudad_actual_idx;

    h.dia = dia_actual;
    h.mes = mes_actual;
    h.pos_x = (int8_t)pos_x;
    h.pos_y = (int8_t)pos_y;
    h.ciudad = (uint8_t)ciudad_actual_idx;

    escribir_header(&h);
}

uint32_t cargar_seed(void) {
    save_init();
    SaveHeader h; leer_header(&h);
    return h.current_seed;
}

void guardar_seed(uint32_t seed) {
    save_init();
    SaveHeader h; leer_header(&h);
    h.current_seed = seed;
    escribir_header(&h);
}

uint32_t obtener_dinero(void) {
    save_init();
    uint32_t dinero = 0;
    sram_read(&dinero, SRAM_BASE + SRAM_DINERO, sizeof(uint32_t));
    return dinero;
}
void modificar_dinero(int32_t cantidad) {
    save_init();

    uint32_t d = obtener_dinero();
    int32_t total = (int32_t)d + cantidad;

    if (total < 0) total = 0;

    sram_write(SRAM_BASE + SRAM_DINERO, &total, sizeof(uint32_t));
}
