#include "opalo.h"
#include "data.h"
#include "ciudades.h"
#include "gema.h"

int COLOR_PRUEBA = 2;

// ----------------------------------------------------
// GENERAR ÓPALO (Para renderizado temporal / thumbs)
// ----------------------------------------------------
void generar_opalo(Opalo* o, uint32_t seed) {
    Gema g_temp;
    gema_init(&g_temp);
    
    // Inyectamos la semilla. La magia ocurre al vuelo.
    g_temp.seed = seed;
    g_temp.quilates = 0; 
    
    // Usamos el conversor que ya programaste en gema.c.
    // Esto calculará tipo, patrón, pureza, etc., al momento
    // y rellenará el struct temporal Opalo.
    gema_a_opalo_temp(o, &g_temp);
}

// ----------------------------------------------------
// FUNCIONES OBSOLETAS (Mantenidas como stubs para compilar)
// ----------------------------------------------------

void calcular_atributos_gema(Gema* g, uint32_t seed, uint8_t bioma) {
    // OBSOLETO: Ahora `sacos.c` se encarga directamente de hacer:
    // nueva_gema.seed = (ciudad << 24) | (dia << 16) | entropia;
    // Ya no hay campos g->tipo_real que rellenar.
}

void aplicar_bonus_region(Opalo* o) {
    // OBSOLETO: Las probabilidades de aparición por bioma ya están 
    // integradas matemáticamente en derivar_tipo() dentro de gema.c.
}
