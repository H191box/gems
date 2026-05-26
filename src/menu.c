#include <stdint.h>
#include <stdio.h>
#include <gba_video.h>
#include <gba_input.h>
#include "video.h"
#include "font.h"
#include "save.h"
#include "data.h"

// Eliminamos la referencia a 'ciudad_actual' y usamos la global de 'data.h'
// 'ciudades' ya viene importado vía 'data.h'

void aplicar_paleta_segun_ciudad(int idx) {
    volatile uint16_t* pal = (volatile uint16_t*)0x05000000;
    uint16_t color = ciudades[idx].color_paleta;

    pal[0]   = 0x0000;
    pal[1]   = color;
    pal[2]   = (color >> 1) & 0x3DEF; 
    pal[3]   = color + 0x0318;
    pal[4]   = 0x4210; // Gris para el divisor
    pal[255] = 0x7FFF;
}

void dibujar_menu(int opcion) {
    // Usamos 'ciudad_actual_idx' que definimos en data.c
    aplicar_paleta_segun_ciudad(ciudad_actual_idx);
    
    uint16_t* vram = get_vram();
    clear(vram, 0); 

    // Cabecera
    fill_rect(vram, 0, 0, 240, 12, 1);
    draw_text(vram, 4, 2, "MENU", 255);
    
    // Divisor vertical
    vline(vram, 115, 12, 160, 4);

    // Opciones (Izquierda)
    const char* opciones_txt[] = {"FARMEO", "TALLER", "GALERIA", "TIENDA", "VIAJAR"};
    for(int i = 0; i < 5; i++) {
        int y = 20 + (i * 20);
        uint8_t col = (opcion == i) ? 3 : 2;
        fill_rect(vram, 0, y, 114, 18, col);
        draw_text(vram, 10, y + 5, (char*)opciones_txt[i], 255);
    }

    // Panel Derecho (Información)
    int x = 125;
    
    // Mostrar Dinero
    char txt_oro[16];
    sprintf(txt_oro, "ORO: %d", obtener_dinero());
    draw_text(vram, x, 20, txt_oro, 255);

    // Mostrar Ciudad usando la variable global correcta
    draw_text(vram, x, 35, (char*)ciudades[ciudad_actual_idx].nombre, 255);

    // Descripción dinámica
    const char* desc_txt[] = {
        "BUSCA ROCAS", 
        "CORTA CHUNKS", 
        "TUS OPALOS", 
        "COMPRAR SACOS", 
        "CAMBIAR ZONA"
    };
    
    draw_text(vram, x, 60, "ACCION:", 255);
    draw_text(vram, x, 75, (char*)desc_txt[opcion], 255);
    
    // Nota al pie
    draw_text(vram, x, 140, "A: CONFIRMAR", 255);
}
