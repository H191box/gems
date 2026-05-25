#include <stdint.h>
#include <gba_video.h>
#include <gba_input.h>
#include "video.h" // Aquí ya están declaradas rect() y clear()
#include "font.h"
#include "save.h"
#include "data.h"

// Declaración de variables globales externas
extern int ciudad_actual;
extern const Ciudad ciudades[3];

// Nueva función de paleta dinámica
void aplicar_paleta_segun_ciudad(int idx) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    uint16_t color = ciudades[idx].color_paleta;

    pal[0] = 0x0000;          // Negro
    pal[1] = color & 0x7BE0;  // Variante oscura
    pal[2] = color;           // Color principal
    pal[3] = color | 0x0200;  // Tono extra
    pal[255] = 0x7FFF;        // Blanco
}

void dibujar_menu(int opcion) {
    aplicar_paleta_segun_ciudad(ciudad_actual);
    
    uint16_t* vram = get_vram();
    // Como ya incluiste "video.h", puedes usar clear y rect directamente
    clear(vram, 0); 

    rect(vram, 0, 0, 240, 20, 1);
    draw_text(vram, 10, 6, (char*)ciudades[ciudad_actual].nombre, 255);
    draw_text(vram, 180, 6, "MENU", 255);

    int h_celda = 28; 
    int y_start = 28;
    int espacio = 32;

    char* opciones_txt[] = {"FARMEO", "TALLER", "GALERIA", "VIAJAR"};
    char* desc_txt[] = {"BUSCA ROCAS", "CORTA CHUNKS", "TUS OPALOS", "CAMBIAR ZONA"};

    for(int i = 0; i < 4; i++) {
        uint8_t col = (opcion == i) ? 3 : 2;
        rect(vram, 8, y_start + (espacio * i), 224, h_celda, col);
        draw_text(vram, 16, y_start + (espacio * i) + 4, opciones_txt[i], 255);
        draw_text(vram, 100, y_start + (espacio * i) + 4, desc_txt[i], 255);
    }
}
