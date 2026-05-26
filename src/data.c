#include "data.h"

// Definición de las 3 ciudades
const Ciudad ciudades[3] = {
    {"Valle Ceniza",    0x7C1F, "Tierra quemada, rica en gemas."},
    {"Costa Rosa",      0x7E3F, "Cristales pulidos por el mar."},
    {"Llanura Cristal", 0x7FFF, "Vastos campos de cuarzo puro."}
};

// Variables globales de tiempo y ubicación
uint8_t dia_actual = 1;
uint8_t mes_actual = 1;
uint8_t ciudad_actual_idx = 0; // Por defecto: Valle Ceniza

// Función para avanzar el calendario
void avanzar_tiempo(void) {
    dia_actual++;
    if (dia_actual > 30) { // Meses de 30 días para simplificar
        dia_actual = 1;
        mes_actual++;
        if (mes_actual > 12) {
            mes_actual = 1;
        }
    }
}
