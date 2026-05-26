#include "opalo.h"

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

// Distribución de quilates exacta según tu petición:
// 30% [1-5], 20% [5-10], 10% [10-25], 5% [25-50], 2.5% [50-100], 1% [100-110], resto [111-225]
static uint16_t calcular_quilates_exacto(uint32_t h) {
    uint32_t r = h % 1000000; // Normalizamos a 1 millón

    if (r < 300000) return 1 + (h % 5);           // 30% : 1-5
    if (r < 500000) return 5 + (h % 6);           // 20% : 5-10
    if (r < 600000) return 10 + (h % 16);         // 10% : 10-25
    if (r < 650000) return 25 + (h % 26);         // 5%  : 25-50
    if (r < 675000) return 50 + (h % 51);         // 2.5%: 50-100
    if (r < 685000) return 100 + (h % 11);        // 1%  : 100-110
    
    // Rarezas extremas (el 31.5% restante para llegar a 225)
    // Esto mapea el rango [685000, 999999] a [111, 225]
    return 111 + (uint16_t)(((r - 685000) * 114) / 315000);
}

void generar_opalo(Opalo* o, uint32_t seed) {
    uint32_t h = hash32(seed);
    o->seed = seed;
    o->tipo = h & 3;
    o->patron = (h >> 2) & 3;

    o->brillo = 16 + (uint8_t)(((h >> 4) & 15) * ((h >> 4) & 15) / 15);
    o->saturacion = 16 + (uint8_t)(((h >> 8) & 15) * ((h >> 8) & 15) / 15);
    o->iridiscencia = 16 + (uint8_t)(((h >> 13) & 15) * ((h >> 13) & 15) / 15);
    
    o->color_offset = (h >> 17) & 255;
    o->quilates = calcular_quilates_exacto(hash32(h));
}

void generar_chunk(Chunk* c, uint32_t seed) {
    uint32_t h = hash32(seed ^ 0xDEADBEEF);
    c->seed = seed;
    c->cortado = 0;
    c->tamanyo = 1 + ((h >> 4) & 3);
    c->quilates = calcular_quilates_exacto(h);
    c->grietas = ((h >> 8) & 0xFF) > 200 ? 1 : 0;
    c->pista = (c->grietas > 0) ? 1 + ((h >> 16) & 0xFF) % 4 : 0;
}

uint32_t calcular_valor_opalo(const Opalo* o) {
    // 1. Base exponencial: (quilates^2) / 20. 
    // Para 168 ct: 28224 / 20 = 1411.
    uint32_t base = (uint32_t)o->quilates * o->quilates / 20;

    // 2. Tipo
    uint32_t mult_tipo = (o->tipo == OPALO_NEGRO) ? 50 : (o->tipo == OPALO_FUEGO ? 30 : (o->tipo == OPALO_CRISTAL ? 20 : 10));

    // 3. Belleza (10 a 55)
    uint32_t b = (o->brillo - 16) + (o->saturacion - 16) + (o->iridiscencia - 16);
    uint32_t factor_belleza = 10 + b;

    // 4. CÁLCULO SEGURO (Previene el valor ridículo de 48 para piezas grandes)
    // Multiplicamos paso a paso para evitar desbordamiento y pérdida de valor.
    // Usamos uint64_t internamente si fuera necesario, pero esto es seguro en 32 bits:
    uint32_t valor = (base * mult_tipo) / 10; 
    valor = (valor * factor_belleza) / 10;

    // Bonus patrón
    if (o->patron == PATRON_CHAOS) valor = (valor * 3) / 2;

    return (valor < 1) ? 1 : valor;
}
