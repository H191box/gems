#include <stdint.h>
#include "save.h"

#include "data.h"
#include "gema.h"

#define SRAM_BASE ((uint8_t *)0x0E000000)

#define SAVE_MAGIC    "OPAL"
#define SAVE_VERSION  11

/* ------------------------------------------------------------------ */
/* TAMAÑO Y OFFSETS SRAM                                              */
/* ------------------------------------------------------------------ */

#define GEMA_SIZE     8
#define SRAM_HEADER   0x0000
#define SRAM_GAL1     0x0040
#define SRAM_GAL2     0x0200
#define SRAM_GAL3     0x0300
#define SRAM_DINERO   0x1000

/* ------------------------------------------------------------------ */
/* ACCESO SRAM — bajo nivel                                           */
/* ------------------------------------------------------------------ */

static void sram_read(void *dst, const void *src, uint32_t size)
{
    volatile const uint8_t *s = (volatile const uint8_t *)src;
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

static void sram_write(void *dst, const void *src, uint32_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

/* ------------------------------------------------------------------ */
/* HEADER                                                             */
/* ------------------------------------------------------------------ */

void leer_header(SaveHeader *h)
{
    sram_read(h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));
}

void escribir_header(const SaveHeader *h)
{
    sram_write(SRAM_BASE + SRAM_HEADER, h, sizeof(SaveHeader));
}

static int save_valido(void)
{
    SaveHeader h;
    sram_read(&h, SRAM_BASE + SRAM_HEADER, sizeof(SaveHeader));

    return h.magic[0] == 'O' && h.magic[1] == 'P' &&
           h.magic[2] == 'A' && h.magic[3] == 'L' &&
           h.version  == SAVE_VERSION;
}

/* ------------------------------------------------------------------ */
/* INIT                                                               */
/* ------------------------------------------------------------------ */

void save_init(void)
{
    static int init = 0;
    if (init) return;
    init = 1;

    if (!save_valido()) {
        SaveHeader h = {0};
        h.magic[0] = 'O'; h.magic[1] = 'P';
        h.magic[2] = 'A'; h.magic[3] = 'L';
        h.version    = SAVE_VERSION;
        h.num_gal    = 0;
        h.num_gal2   = 0;
        h.num_gal3   = 0;
        escribir_header(&h);

        uint32_t dinero = 500;
        sram_write(SRAM_BASE + SRAM_DINERO, &dinero, 4);
    }
}

/* ------------------------------------------------------------------ */
/* SEED                                                               */
/* ------------------------------------------------------------------ */

void guardar_seed(uint32_t seed)
{
    SaveHeader h; leer_header(&h);
    h.current_seed = seed;
    escribir_header(&h);
}

uint32_t cargar_seed(void)
{
    SaveHeader h; leer_header(&h);
    return h.current_seed;
}

/* ------------------------------------------------------------------ */
/* HELPERS — serialización gemas                                      */
/* ------------------------------------------------------------------ */

static void sram_escribir_gema(uint32_t offset, const Gema *g)
{
    uint8_t buf[GEMA_SIZE];
    gema_serializar(g, buf);
    sram_write(SRAM_BASE + offset, buf, GEMA_SIZE);
}

void sram_leer_gema(Gema *g, uint32_t offset)
{
    uint8_t buf[GEMA_SIZE];
    sram_read(buf, SRAM_BASE + offset, GEMA_SIZE);
    gema_deserializar(g, buf);
}

/* ------------------------------------------------------------------ */
/* GALERÍAS                                                           */
/* ------------------------------------------------------------------ */

// Galería 1
int cargar_gemas(Gema *slots)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal > MAX_GALERIA) h.num_gal = 0;   /* FIX: SRAM corrupta */
    for (uint32_t i = 0; i < h.num_gal; i++)
        sram_leer_gema(&slots[i], SRAM_GAL1 + i * GEMA_SIZE);
    return (int)h.num_gal;
}

void guardar_gema(const Gema *g)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal >= MAX_GALERIA) return;
    sram_escribir_gema(SRAM_GAL1 + h.num_gal * GEMA_SIZE, g);
    h.num_gal++;
    escribir_header(&h);
}

void actualizar_gema_en_sram(int i, const Gema *g)
{
    sram_escribir_gema(SRAM_GAL1 + (uint32_t)i * GEMA_SIZE, g);
}

void decrementar_num_gemas(void)
{
    SaveHeader h; leer_header(&h);
    if (h.num_gal > 0) h.num_gal--;
    escribir_header(&h);
}

// Galería 2
int cargar_gemas_galeria2(Gema *slots)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal2 > MAX_GALERIA2) h.num_gal2 = 0;   /* FIX: SRAM corrupta */
    for (uint32_t i = 0; i < h.num_gal2; i++)
        sram_leer_gema(&slots[i], SRAM_GAL2 + i * GEMA_SIZE);
    return (int)h.num_gal2;
}

void guardar_gema_galeria2(const Gema *g)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal2 >= MAX_GALERIA2) return;
    sram_escribir_gema(SRAM_GAL2 + h.num_gal2 * GEMA_SIZE, g);
    h.num_gal2++;
    escribir_header(&h);
}

void actualizar_gema_galeria2(int i, const Gema *g)
{
    sram_escribir_gema(SRAM_GAL2 + (uint32_t)i * GEMA_SIZE, g);
}

void decrementar_num_gemas_galeria2(void)
{
    SaveHeader h; leer_header(&h);
    if (h.num_gal2 > 0) h.num_gal2--;
    escribir_header(&h);
}

// Galería 3
int cargar_gemas_galeria3(Gema *slots)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal3 > MAX_GALERIA3) h.num_gal3 = 0;   /* FIX: SRAM corrupta */
    for (uint32_t i = 0; i < h.num_gal3; i++)
        sram_leer_gema(&slots[i], SRAM_GAL3 + i * GEMA_SIZE);
    return (int)h.num_gal3;
}

void guardar_gema_galeria3(const Gema *g)
{
    save_init();
    SaveHeader h; leer_header(&h);
    if (h.num_gal3 >= MAX_GALERIA3) return;
    sram_escribir_gema(SRAM_GAL3 + h.num_gal3 * GEMA_SIZE, g);
    h.num_gal3++;
    escribir_header(&h);
}

void actualizar_gema_galeria3(int i, const Gema *g)
{
    sram_escribir_gema(SRAM_GAL3 + (uint32_t)i * GEMA_SIZE, g);
}

void decrementar_num_gemas_galeria3(void)
{
    SaveHeader h; leer_header(&h);
    if (h.num_gal3 > 0) h.num_gal3--;
    escribir_header(&h);
}

/* ------------------------------------------------------------------ */
/* ECONOMÍA Y ESTADO MUNDIAL                                          */
/* ------------------------------------------------------------------ */

uint32_t obtener_dinero(void)
{
    uint32_t d = 0;
    sram_read(&d, SRAM_BASE + SRAM_DINERO, 4);
    return d;
}

void modificar_dinero(int32_t cantidad)
{
    uint32_t d = obtener_dinero();
    int32_t r  = (int32_t)d + cantidad;
    if (r < 0) r = 0;
    sram_write(SRAM_BASE + SRAM_DINERO, &r, 4);
}

void sync_save_world_state(void)
{
    SaveHeader h; leer_header(&h);
    escribir_header(&h);
}
