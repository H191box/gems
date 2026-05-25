#include <stdint.h>
#include <stdio.h> // Para sprintf
#include <gba_video.h>
#include <gba_input.h>
#include "galeria.h"
#include "save.h"
#include "plasma.h"
#include "video.h"
#include "font.h"
#include "opalo.h"
#include "thumb_cache.h"

// -------------------------------------------------------
// CONFIG LAYOUT
// Pantalla 240x160
// Izquierda: 0-114   (lista)
// Divisor:   115-116
// Derecha:   117-239 (descripcion)
// -------------------------------------------------------
#define LIST_W       115
#define LIST_ITEM_H  14
#define LIST_ITEMS    8   // filas visibles
#define SCROLL_MAX   (MAX_CAPTURAS - LIST_ITEMS)

// Nombres de tipo
static const char* NOMBRE_TIPO[4] = {
    "OPALO NEGRO",
    "OPALO CRISTAL",
    "OPALO FUEGO",
    "OPALO BLANCO"
};

// Nombres de patrón
static const char* NOMBRE_PATRON[4] = {
    "NEBULA",
    "VENAS",
    "MOSAICO",
    "CHAOS"
};

// Nombres de tamaño
static const char* NOMBRE_TAM[5] = {
    "S", "M", "L", "XL", "XXL"
};

// -------------------------------------------------------
// STATE
// -------------------------------------------------------
typedef enum {
    VISTA_LISTA,
    VISTA_SUBMENU, // NUEVO ESTADO para elegir Ver o Vender
    VISTA_IMAGEN
} VistaGaleria;

static VistaGaleria vista;
static Chunk        items[MAX_CAPTURAS];
static int          num_items;
static int          cursor;
static int          scroll;    // primera fila visible
static int          opcion_submenu; // 0 = VER, 1 = VENDER

// -------------------------------------------------------
// PALETA UI
// -------------------------------------------------------
static void init_paleta_ui(void) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[0]  = 0x0000;                               // negro fondo
    pal[1]  = 0x294A;                               // gris oscuro fila normal
    pal[2]  = 0x681F;                               // morado fila seleccionada
    pal[3]  = 0x4210;                               // gris divisor
    pal[4]  = 0x6810;                               // azul cabecera
    pal[5]  = 0x1F00;                               // NUEVO: Azul oscuro para el fondo del submenú
    pal[255]= 0x7FFF;                               // blanco texto
}

// -------------------------------------------------------
// HELPERS DE DIBUJO
// -------------------------------------------------------
static void clear_vram(uint16_t* vram) {
    for (int i = 0; i < 19200; i++) vram[i] = 0;
}

static void put_pixel(uint16_t* vram, int x, int y, uint8_t c) {
    if (x < 0 || x >= 240 || y < 0 || y >= 160) return;
    int idx = y * 120 + x / 2;
    if (x & 1) vram[idx] = (vram[idx] & 0x00FF) | ((uint16_t)c << 8);
    else        vram[idx] = (vram[idx] & 0xFF00) | c;
}

static void fill_rect(uint16_t* vram, int x, int y,
                      int w, int h, uint8_t c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            put_pixel(vram, xx, yy, c);
}

static void vline(uint16_t* vram, int x, int y, int h, uint8_t c) {
    for (int yy = y; yy < y + h; yy++) put_pixel(vram, x, yy, c);
}

static void u8_to_dec(uint8_t v, char* buf) {
    buf[0] = '0' + (v / 10);
    buf[1] = '0' + (v % 10);
}

// NUEVA FUNCIÓN AUXILIAR: Calcula el precio dinámico del ópalo para mostrarlo y venderlo
static uint16_t calcular_valor_opalo(int idx) {
    Opalo o;
    generar_opalo(&o, items[idx].seed);

    // Precio base por karat según tipo (de menos a más valioso)
    uint16_t precio_karat;
    switch (o.tipo) {
        case 0: precio_karat = 8;  break; // OPALO NEGRO   - el más valioso de base
        case 2: precio_karat = 6;  break; // OPALO FUEGO
        case 1: precio_karat = 4;  break; // OPALO CRISTAL
        default:precio_karat = 2;  break; // OPALO BLANCO  - el más común
    }

    // Modificador de brillo: brillo va de 16 a 31
    // brillo 16 → ×0.8,  brillo 31 → ×1.5 aprox
    // Usamos factor entero: (brillo * 10) / 20 → rango 8-15, /10 al final
    uint16_t mod_brillo = (uint16_t)o.brillo * 10 / 20; // 8-15

    // Modificador de saturación: igual de rango
    uint16_t mod_sat = (uint16_t)o.saturacion * 10 / 20; // 8-15

    // Bonus por patrón (algunos patrones son más raros y cotizados)
    uint16_t bonus_patron;
    switch (o.patron) {
        case 3: bonus_patron = 4; break; // CHAOS   - muy raro
        case 1: bonus_patron = 3; break; // VENAS
        case 2: bonus_patron = 2; break; // MOSAICO
        default:bonus_patron = 1; break; // NEBULA  - común
    }

    // precio_karat final = base * mod_brillo * mod_sat / 100 + bonus_patron
    precio_karat = (uint16_t)(precio_karat * mod_brillo * mod_sat / 100) + bonus_patron;
    if (precio_karat < 1) precio_karat = 1;

    // Karats = tamanyo × peso (ambos 1-10 aprox, tamanyo 1-5)
    uint16_t karats = (uint16_t)items[idx].tamanyo * items[idx].peso;

    return precio_karat * karats;
}
// NUEVA FUNCIÓN AUXILIAR: Ejecuta la venta en la RAM virtual y reorganiza los slots
static void vender_opalo_seleccionado(int idx_lista) {
    Chunk todos[MAX_CAPTURAS];
    int n = cargar_chunks(todos);

    // Buscamos cuál es el índice real en save.c comparando la semilla
    int slot_sram = -1;
    for (int i = 0; i < n; i++) {
        if (todos[i].seed == items[idx_lista].seed && todos[i].cortado) {
            slot_sram = i;
            break;
        }
    }

    if (slot_sram == -1) return;

    // 1. Añadimos el dinero en base a su valor
    uint16_t valor = calcular_valor_opalo(idx_lista);
    modificar_dinero((int32_t)valor);

    // 2. Desplazamos todos los chunks hacia atrás en save.c para mantener la estructura secuencial libre de huecos
    for (int i = slot_sram; i < n - 1; i++) {
        sobreescribir_chunk(i, &todos[i + 1]);
    }

    // 3. Limpiamos el último hueco libre que ha quedado duplicado
    Chunk vacio = {0};
    sobreescribir_chunk(n - 1, &vacio);

    // 4. Bajamos el contador general de chunks usando la función que preparamos para save.c
    decrementar_num_chunks();
}

// -------------------------------------------------------
// RENDER LISTA
// -------------------------------------------------------
static void render_lista(void) {
    init_paleta_ui();
    uint16_t* vram = get_vram();
    clear_vram(vram);

    // Cabecera
    fill_rect(vram, 0, 0, 240, 12, 4);
    draw_text(vram, 4,   2, "GALERIA",    255);
    
    // NUEVO: Imprimir el dinero actual en la cabecera (alineado a la derecha)
    char txt_dinero[16];
    sprintf(txt_dinero, "ORO: %d", obtener_dinero());
    draw_text(vram, 175, 2, txt_dinero, 255);

    // Divisor vertical
    vline(vram, LIST_W, 12, 148, 3);

    if (num_items == 0) {
        draw_text(vram, 10, 70, "GALERIA VACIA", 255);
        draw_text(vram, 10, 85, "CORTA CHUNKS", 255);
        draw_text(vram, 10, 100,"EN EL TALLER",  255);
        flip();
        return;
    }

    // Lista: filas visibles según scroll
    for (int i = 0; i < LIST_ITEMS; i++) {
        int idx = scroll + i;
        if (idx >= num_items) break;

        int y   = 13 + i * LIST_ITEM_H;
        uint8_t bg = (idx == cursor) ? 2 : 1;
        fill_rect(vram, 0, y, LIST_W - 1, LIST_ITEM_H - 1, bg);

        // Número + nombre tipo
        char num[4] = "XX.";
        u8_to_dec((uint8_t)(idx + 1), num);
        draw_text(vram, 2, y + 3, num, 255);

        Opalo o;
        generar_opalo(&o, items[idx].seed);
        draw_text(vram, 20, y + 3, NOMBRE_TIPO[o.tipo], 255);
    }

    // Indicadores de scroll
    if (scroll > 0)
        draw_text(vram, LIST_W / 2 - 4, 13,      "^", 255);
    if (scroll + LIST_ITEMS < num_items)
        draw_text(vram, LIST_W / 2 - 4, 150,     "v", 255);

    // Panel derecho: descripción del item seleccionado
    if (cursor < num_items) {
        Opalo o;
        generar_opalo(&o, items[cursor].seed);

        int x = LIST_W + 4;
        int y = 14;

        draw_text(vram, x, y,      NOMBRE_TIPO[o.tipo],       255); y += 12;
        draw_text(vram, x, y,      NOMBRE_PATRON[o.patron],   255); y += 14;

        // Tamaño
        char tam[8] = "TAM: X";
        tam[5] = NOMBRE_TAM[items[cursor].tamanyo - 1][0];
        draw_text(vram, x, y, tam, 255); y += 11;

        // Peso
        char peso[10] = "PESO: XX";
        u8_to_dec(items[cursor].peso, &peso[6]);
        draw_text(vram, x, y, peso, 255); y += 11;

        // Brillo
        char bri[12] = "BRILL: XX";
        u8_to_dec(o.brillo, &bri[7]);
        draw_text(vram, x, y, bri, 255); y += 11;

        // Saturación
        char sat[12] = "SAT:   XX";
        u8_to_dec(o.saturacion, &sat[7]);
        draw_text(vram, x, y, sat, 255); y += 14;

        // Valor estimado
        uint16_t valor = calcular_valor_opalo(cursor);
        char val[14] = "VALOR: XXXX";
        val[7]  = '0' + (valor / 1000) % 10;
        val[8]  = '0' + (valor / 100) % 10;
        val[9]  = '0' + (valor /  10) % 10;
        val[10] = '0' + (valor       ) % 10;
        draw_text(vram, x, y, val, 255); y += 14;

        draw_text(vram, x, y, "A:OPCIONES", 255); // Cambiado para indicar que abre submenú
    }

    // NUEVO: Si estamos en el estado submenú, pintamos la ventana emergente encima del panel derecho
    if (vista == VISTA_SUBMENU) {
        int sm_x = LIST_W + 10;
        int sm_y = 100;
        int sm_w = 105;
        int sm_h = 42;

        // Dibujamos el marco de la cajita del submenú
        fill_rect(vram, sm_x, sm_y, sm_w, sm_h, 3);       // Borde exterior gris
        fill_rect(vram, sm_x + 2, sm_y + 2, sm_w - 4, sm_h - 4, 5); // Fondo azul oscuro

        // Dibujamos las opciones con el indicador astronómico '>'
        if (opcion_submenu == 0) {
            draw_text(vram, sm_x + 6, sm_y + 8,  "> VER IMAGEN", 255);
            draw_text(vram, sm_x + 6, sm_y + 24, "  VENDER OPALO", 255);
        } else {
            draw_text(vram, sm_x + 6, sm_y + 8,  "  VER IMAGEN", 255);
            draw_text(vram, sm_x + 6, sm_y + 24, "> VENDER OPALO", 255);
        }
    }

    // Instrucciones pie
    draw_text(vram, 0, 152, "ARR/ABA:MOVER  START:MENU", 255);
    flip();
}

// -------------------------------------------------------
// RENDER IMAGEN COMPLETA
// -------------------------------------------------------
static void render_imagen(int idx) {
    Opalo o;
    generar_opalo(&o, items[idx].seed);
    generar_paleta(&o);
    renderizar_opalo(&o);

    // Aseguramos que el blanco esté disponible para el texto
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    pal[255] = 0x7FFF;

    uint16_t* vram = get_vram();
    draw_text(vram, 2, 2, NOMBRE_TIPO[o.tipo], 255);
    draw_text(vram, 2, 150, "B:VOLVER  IZQ/DER:CAMBIAR", 255);
    flip();
}

// -------------------------------------------------------
// API PUBLICA
// -------------------------------------------------------
void galeria_init(void) {
    // Solo mostramos los chunks ya cortados
    Chunk todos[MAX_CAPTURAS];
    int n = cargar_chunks(todos);
    num_items = 0;
    for (int i = 0; i < n; i++) {
        if (todos[i].cortado) {
            items[num_items++] = todos[i];
        }
    }

    cursor = 0;
    scroll = 0;
    vista  = VISTA_LISTA;
    opcion_submenu = 0;
    render_lista();
}

void galeria_input(uint16_t keys) {

    if (vista == VISTA_LISTA) {

        if ((keys & KEY_DOWN) && cursor + 1 < num_items) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS)
                scroll = cursor - LIST_ITEMS + 1;
            render_lista();
        }
        if ((keys & KEY_UP) && cursor > 0) {
            cursor--;
            if (cursor < scroll) scroll = cursor;
            render_lista();
        }
        if ((keys & KEY_A) && num_items > 0) {
            // NUEVO: Al pulsar A abrimos el submenú en lugar de saltar directo a la imagen
            vista = VISTA_SUBMENU;
            opcion_submenu = 0; // Apunta a "VER" por defecto
            render_lista();
        }

    } else if (vista == VISTA_SUBMENU) { // NUEVO BLOQUE DE CONTROL DEL SUBMENÚ

        if ((keys & KEY_UP) || (keys & KEY_DOWN)) {
            opcion_submenu = !opcion_submenu; // Cambia entre 0 (Ver) y 1 (Vender)
            render_lista();
        }
        if (keys & KEY_B) {
            vista = VISTA_LISTA; // Cancela el submenú y vuelve a la lista normal
            render_lista();
        }
        if (keys & KEY_A) {
            if (opcion_submenu == 0) {
                // Opción VER IMAGEN
                vista = VISTA_IMAGEN;
                render_imagen(cursor);
            } else {
                // Opción VENDER ÓPALO
                vender_opalo_seleccionado(cursor);
                // Tras venderlo, refrescamos la galería completa para actualizar la lista
                galeria_init(); 
            }
        }

    } else {  // VISTA_IMAGEN

        if (keys & KEY_B) {
            vista = VISTA_LISTA;
            render_lista();
        }
        if ((keys & KEY_LEFT) && cursor > 0) {
            cursor--;
            if (cursor < scroll) scroll = cursor;
            render_imagen(cursor);
        }
        if ((keys & KEY_RIGHT) && cursor + 1 < num_items) {
            cursor++;
            if (cursor >= scroll + LIST_ITEMS)
                scroll = cursor - LIST_ITEMS + 1;
            render_imagen(cursor);
        }
    }
}
