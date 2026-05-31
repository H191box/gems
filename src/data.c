#include "data.h"

// Variables globales de tiempo y ubicación
uint8_t dia_actual = 1;
uint8_t mes_actual = 1;

int guardando_estado = 0; // 0: libre, 1: bloqueado

// --- UBICACIÓN ---
// Mantenemos las coordenadas para el mapa
int pos_x = 2; 
int pos_y = 2; 

// --- NUEVA VARIABLE PARA EL SISTEMA DE BIOMAS ---
// Esta es la variable que causaba el error. 
// La definimos aquí para que sea accesible desde todo el proyecto.
// El jugador empieza en CENTRO (pos_x=2, pos_y=2), que es el índice 12 en el array.
// Fórmula: idx = pos_y * 5 + pos_x  →  2*5+2 = 12
int ciudad_actual_idx = 12;

// Función para avanzar el calendario
void avanzar_tiempo(void) {
    dia_actual++;
    if (dia_actual > 30) {
        dia_actual = 1;
        mes_actual++;
        if (mes_actual > 12) {
            mes_actual = 1;
        }
    }
}
