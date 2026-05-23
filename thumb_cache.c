#include "thumb_cache.h"

#include "plasma.h"

// --------------------------------------------------
// VRAM helper
// --------------------------------------------------

static void put_pixel(
    uint16_t* vram,
    int x,
    int y,
    uint8_t color
) {

    if (x < 0 || x >= 240 || y < 0 || y >= 160)
        return;

    int idx = y * 120 + x / 2;

    if (x & 1) {

        vram[idx] =
            (vram[idx] & 0x00FF) |
            ((uint16_t)color << 8);

    } else {

        vram[idx] =
            (vram[idx] & 0xFF00) |
            color;
    }
}

// --------------------------------------------------
// Thumbnail generation
// --------------------------------------------------

void thumb_generar(
    ThumbCache* t,
    uint32_t seed
) {

    Opalo o;

    generar_opalo(&o, seed);

    // mini-paleta simple
    for (int i = 0; i < 16; i++) {

        int r, g, b;

        switch (o.tipo) {

            case OPALO_NEGRO:
                r = i;
                g = i;
                b = 31;
                break;

            case OPALO_CRISTAL:
                r = 16 + i / 2;
                g = 16 + i / 2;
                b = 31;
                break;

            case OPALO_FUEGO:
                r = 31;
                g = i * 2;
                b = i / 2;
                break;

            default:
                r = i * 2;
                g = i * 2;
                b = 31 - i;
                break;
        }

        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (b > 31) b = 31;

        t->palette[i] =
            (r & 31) |
            ((g & 31) << 5) |
            ((b & 31) << 10);
    }

    uint8_t off =
        (uint8_t)o.color_offset;

    // render offline
    for (int y = 0; y < THUMB_H; y++) {

        int py =
            (y * 160) / THUMB_H;

        for (int x = 0; x < THUMB_W; x++) {

            int px =
                (x * 240) / THUMB_W;

            uint8_t p =
                plasma_pixel(
                    px,
                    py,
                    off,
                    &o
                );

            // reducir 0-255 → 0-15
            t->pixels[
                y * THUMB_W + x
            ] = p >> 4;
        }
    }

    t->ready = 1;
}

// --------------------------------------------------
// Draw cached thumbnail
// --------------------------------------------------

void thumb_dibujar(
    ThumbCache* t,
    uint16_t* vram,
    int ox,
    int oy
) {

    if (!t->ready)
        return;

    volatile uint16_t* pal =
        (volatile uint16_t*)0x05000000;

    // subir mini-paleta
    for (int i = 0; i < 16; i++) {
        pal[i + 16] = t->palette[i];
    }

    // dibujar pixels
    for (int y = 0; y < THUMB_H; y++) {

        for (int x = 0; x < THUMB_W; x++) {

            uint8_t c =
                t->pixels[
                    y * THUMB_W + x
                ];

            put_pixel(
                vram,
                ox + x,
                oy + y,
                c + 16
            );
        }
    }
}
