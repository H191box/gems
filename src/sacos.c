#include "sacos.h"
#include "save.h"
#include "opalo.h"

// Variables externas
extern uint8_t ciudad_actual_idx; 
extern uint8_t dia_actual;
extern uint8_t mes_actual;

const Saco SACOS[4] = {
    {"SACO POBRE", 50, 3, 1},
    {"SACO MINERO", 200, 6, 2},
    {"SACO INDUSTRIAL", 800, 12, 3},
    {"MEGA CARGAMENTO", 3000, 24, 4}
};

// Semilla base que evoluciona con el tiempo
static uint32_t saco_seed = 0x12345678;
static uint32_t contador_global = 0;

void comprar_saco(int idx) {
    if (idx < 0 || idx >= 4) return;

    const Saco* s = &SACOS[idx];

    if (obtener_dinero() < (int32_t)s->precio)
        return;

    modificar_dinero(-(int32_t)s->precio);

    Chunk temp[MAX_TALLER];
    int n = cargar_chunks_taller(temp);

    for (int i = 0; i < s->cantidad_chunks; i++) {
        if (n >= MAX_TALLER)
            break;

        Chunk c;
        // Mezclamos semillas para asegurar que cada chunk sea único
        uint32_t seed_dinamica = saco_seed + (contador_global++) + (i * 0x9E3779B9);
        generar_chunk(&c, seed_dinamica);

        // --- INYECCIÓN DE DATOS PARA LA FICHA ---
        c.dia = dia_actual;
        c.mes = mes_actual;
        c.ciudad_id = ciudad_actual_idx;
        // ----------------------------------------

        // Tier alto: Aumentamos la probabilidad de calidad/rareza 
        // en lugar de sumar peso artificialmente.
        if (s->tier >= 2 && c.grietas == 0)
            c.grietas = 1;

        if (s->tier >= 3 && c.tamanyo < 2)
            c.tamanyo = 2;

        // NOTA: Eliminamos la suma artificial de quilates para 
        // mantener la integridad de la distribución exponencial.
        // Si el usuario quiere piezas pesadas en un saco, debe ser 
        // por pura suerte estadística de la semilla.

        guardar_chunk_taller(&c);
        n++;
    }

    // Refrescamos la semilla base para que la siguiente compra sea impredecible
    saco_seed += 0x9E3779B9;
}
