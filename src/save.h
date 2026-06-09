#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

#include "gema.h"


/* ------------------------------------------------------------------ */
/* ESTRUCTURAS DE CONTROL DE SRAM                                     */
/* ------------------------------------------------------------------ */

#define GEMA_SIZE 8
typedef struct {
    char     magic[4];
    uint32_t version;
    uint32_t current_seed;

    uint8_t  dia;
    uint8_t  mes;
    uint8_t  ciudad;
    int8_t   pos_x;
    int8_t   pos_y;

    uint8_t  _pad[3];

    uint32_t num_gal;
    uint32_t num_gal2;
    uint32_t num_gal3;
    // num_taller eliminado
} __attribute__((packed)) SaveHeader;

// Prototipos necesarios para el blindaje de seguridad
void leer_header(SaveHeader *h);
void sram_leer_gema(Gema *g, uint32_t offset);



/* ------------------------------------------------------------------ */
/* CAPACIDADES                                                         */
/* ------------------------------------------------------------------ */

#define MAX_GALERIA    32
#define MAX_GALERIA2    9   /* vitrina fija 3x3 */
#define MAX_GALERIA3   32   /* rejilla muestra top 9; resto es reserva en SRAM */
#define MAX_TALLER     64

/* ------------------------------------------------------------------ */
/* INIT / SEED                                                         */
/* ------------------------------------------------------------------ */

void     save_init(void);
uint32_t cargar_seed(void);
void     guardar_seed(uint32_t seed);
 

/* ------------------------------------------------------------------ */
/* GALERÍA 1 (inventario principal)                                   */
/* ------------------------------------------------------------------ */

int  cargar_gemas(Gema *slots);
void guardar_gema(const Gema *g);
void actualizar_gema_en_sram(int index, const Gema *g);
void decrementar_num_gemas(void);

/* ------------------------------------------------------------------ */
/* GALERÍA 2 (vitrina fija)                                           */
/* ------------------------------------------------------------------ */

int  cargar_gemas_galeria2(Gema *slots);
void guardar_gema_galeria2(const Gema *g);
void actualizar_gema_galeria2(int index, const Gema *g);
void decrementar_num_gemas_galeria2(void);

/* ------------------------------------------------------------------ */
/* GALERÍA 3 (mercado)                                                */
/* ------------------------------------------------------------------ */

int  cargar_gemas_galeria3(Gema *slots);
void guardar_gema_galeria3(const Gema *g);
void actualizar_gema_galeria3(int index, const Gema *g);
void decrementar_num_gemas_galeria3(void);

/* ------------------------------------------------------------------ */
/* ECONOMÍA                                                            */
/* ------------------------------------------------------------------ */

uint32_t obtener_dinero(void);
void     modificar_dinero(int32_t cantidad);

/* ------------------------------------------------------------------ */
/* ESTADO MUNDIAL                                                      */
/* ------------------------------------------------------------------ */

void sync_save_world_state(void);

#endif /* SAVE_H */
