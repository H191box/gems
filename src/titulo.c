#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_dma.h>
#include "titulo.h"
#include "title.h"
#include "video.h"
#include "gema.h"
#include "gema_render.h"
#include "plasma.h"
#include "save.h"

/* ------------------------------------------------------------------ */
/* MODE 3: framebuffer RGB555 directo, sin paleta, sin doble buffer   */
/* ------------------------------------------------------------------ */

#define MODE3_VRAM  ((volatile uint16_t*)0x06000000)
#define PAL_BG_MEM  ((volatile uint16_t*)0x05000000)

/* ------------------------------------------------------------------ */
/* Paleta interna del ópalo — 239 entradas [16..254]                  */
/* Se construye en RAM y se usa para el volcado MODE3, no en PALRAM   */
/* ------------------------------------------------------------------ */
static uint16_t pal_opalo[256];

/* Buffer del ópalo en EWRAM — reutiliza anim_buf_b de gema_render    */
extern uint8_t* get_anim_buf_b(void);

/* ------------------------------------------------------------------ */
/* Generar una gema aleatoria decorativa a partir de la seed guardada */
/* ------------------------------------------------------------------ */
static Gema gema_titulo;

static void crear_gema_titulo(void)
{
    uint32_t seed = cargar_seed();
    if (seed == 0) seed = 0xDEAD1234;

    gema_init(&gema_titulo);
    gema_titulo.seed     = seed;
    gema_titulo.quilates = 180;   /* grande pero no máximo */
    gema_titulo.etapa    = ETAPA_PULIDA;
}

/* ------------------------------------------------------------------ */
/* Volcar bitmap del título al framebuffer MODE 3                     */
/* ------------------------------------------------------------------ */
static void volcar_titulo(void)
{
    volatile uint16_t *dst = MODE3_VRAM;
    const uint16_t    *src = titleBitmap;
    /* DMA32: 38400 pixels × 2 bytes = 76800 bytes = 19200 words de 32 bits */
    DMA3COPY(src, dst, 19200 | DMA32 | DMA_ENABLE);
}

/* ------------------------------------------------------------------ */
/* Renderizar el ópalo y volcarlo sobre el framebuffer MODE 3         */
/*                                                                     */
/* El ópalo se renderiza en un buffer 8bpp con renderizar_opalo_pro.  */
/* Cada píxel no-cero se convierte a RGB555 usando pal_opalo[]        */
/* y se escribe directamente sobre el framebuffer RGB555 del título.  */
/*                                                                     */
/* Zona: ópalo centrado en x=120, centrado entre y=38 y y=118        */
/* (entre "PIEDRAS" y "PULSA START").                                 */
/* ------------------------------------------------------------------ */
static void renderizar_opalo_titulo(void)
{
    /* 1. Construir paleta del ópalo en RAM */
    generar_paleta_gema(&gema_titulo);

    /* Copiar PALRAM → pal_opalo (generar_paleta_gema escribió ahí) */
    volatile uint16_t *palram = PAL_BG_MEM;
    for (int i = 0; i < 256; i++)
        pal_opalo[i] = palram[i];

    /* 2. Renderizar el ópalo en buffer 8bpp */
    uint8_t *buf = get_anim_buf_b();
    /* Limpiar buffer */
    for (int i = 0; i < 240 * 160; i++) buf[i] = 0;

    plasma_cache_rebuild(&gema_titulo);

    /* Renderizar en ventana reducida centrada en la zona libre.
       Usamos un sub-buffer virtual: pasamos w=240, h=80 para que
       renderizar_opalo_pro centre el ópalo en (120, 40) dentro
       del buffer, luego lo volcamos a y_base=38 en pantalla.     */
    renderizar_opalo_pro(buf, 240, 80, &gema_titulo);

    /* 3. Volcar sobre MODE3 convirtiendo índice → RGB555 */
    int y_base = 50;   /* fila de inicio en pantalla */
    int h_buf  = 80;   /* altura del sub-buffer      */

    volatile uint16_t *dst = MODE3_VRAM;

    for (int y = 0; y < h_buf; y++) {
        int screen_y = y_base + y;
        if (screen_y < 0 || screen_y >= 160) continue;

        for (int x = 0; x < 240; x++) {
            uint8_t idx = buf[y * 240 + x];
            if (idx != 0) {
                dst[screen_y * 240 + x] = pal_opalo[idx];
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

void titulo_init(void)
{
    /* Activar MODE 3 */
    REG_DISPCNT = MODE_3 | BG2_ENABLE;

    crear_gema_titulo();

    /* Volcar fondo del título */
    volcar_titulo();

    /* Renderizar el ópalo encima */
    renderizar_opalo_titulo();

    fade_in();
}

uint8_t titulo_update(void)
{
    scanKeys();
    return (keysDown() & KEY_START) ? 1 : 0;
}

void titulo_free(void)
{
    /* Al salir restaurar MODE 4 para el resto del juego */
    REG_DISPCNT = MODE_4 | BG2_ENABLE;
}
