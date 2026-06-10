/* save.h */
#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include "gema.h"
#include "caja_filtro.h"

/* ------------------------------------------------------------------ */
/* Capacidades                                                         */
/* ------------------------------------------------------------------ */

#define MAX_POOL      500
#define MAX_GALERIA3   32

/* Alias de compatibilidad — eliminar cuando galeria.c esté limpio */
#define MAX_GALERIA   MAX_POOL

/* ------------------------------------------------------------------ */
/* Header SRAM                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char     magic[4];       /* "OPAL"            */
    uint8_t  version;        /* SAVE_VERSION       */
    uint8_t  _pad[3];
    uint16_t num_pool;       /* gemas en el pool   */
    uint8_t  num_gal3;       /* gemas en G3        */
    uint8_t  _pad2[5];
    uint32_t current_seed;
} SaveHeader;               /* 20 bytes, cabe en 64 */

/* ------------------------------------------------------------------ */
/* API — header                                                        */
/* ------------------------------------------------------------------ */

void     save_init       (void);
void     leer_header     (SaveHeader *h);
void     escribir_header (const SaveHeader *h);

/* ------------------------------------------------------------------ */
/* API — pool principal                                                */
/* ------------------------------------------------------------------ */

int  cargar_pool         (Gema *slots);               /* devuelve count */
void guardar_gema_pool   (const Gema *g);             /* añade al final  */
void actualizar_gema_pool(int i, const Gema *g);
void decrementar_pool    (void);

/* ------------------------------------------------------------------ */
/* API — G3 (mercado)                                                  */
/* ------------------------------------------------------------------ */

int  cargar_gemas_galeria3    (Gema *slots);
void guardar_gema_galeria3    (const Gema *g);
void actualizar_gema_galeria3 (int i, const Gema *g);
void decrementar_num_gemas_galeria3(void);

/* ------------------------------------------------------------------ */
/* API — filtros de cajas                                              */
/* ------------------------------------------------------------------ */

void guardar_cajas (const CajaFiltro cajas[NUM_CAJAS]);
void cargar_cajas  (CajaFiltro cajas[NUM_CAJAS]);

/* ------------------------------------------------------------------ */
/* API — economía                                                       */
/* ------------------------------------------------------------------ */

uint32_t obtener_dinero  (void);
void     modificar_dinero(int32_t cantidad);

/* ------------------------------------------------------------------ */
/* API — seed                                                           */
/* ------------------------------------------------------------------ */

void     guardar_seed (uint32_t seed);
uint32_t cargar_seed  (void);

/* legado — mantener hasta limpiar llamadas externas */
void sync_save_world_state(void);

#endif /* SAVE_H */
