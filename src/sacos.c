#include "sacos.h"
#include "save.h"
#include "opalo.h"

const Saco SACOS[4] = {
    {"SACO POBRE", 50, 3, 1},
    {"SACO MINERO", 200, 6, 2},
    {"SACO INDUSTRIAL", 800, 12, 3},
    {"MEGA CARGAMENTO", 3000, 24, 4}
};

static uint32_t saco_seed = 0x12345678;

void comprar_saco(int idx) {
    if (idx < 0 || idx >= 4) return;

    const Saco* s = &SACOS[idx];

    if (obtener_dinero() < s->precio)
        return;

    modificar_dinero(-(int32_t)s->precio);

    Chunk temp[MAX_TALLER];
    int n = cargar_chunks_taller(temp);

    for (int i = 0; i < s->cantidad_chunks; i++) {
        if (n >= MAX_TALLER)
            break;

        Chunk c;
        generar_chunk(&c, saco_seed);

        if (s->tier >= 2 && c.grietas == 0)
            c.grietas = 1;

        if (s->tier >= 3 && c.tamanyo < 2)
            c.tamanyo = 2;

        if (s->tier >= 4 && c.peso < 5)
            c.peso += 5;

        guardar_chunk_taller(&c);

        saco_seed += 0x9E3779B9;
        n++;
    }
}
