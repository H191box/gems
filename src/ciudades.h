typedef struct {
    char nombre[16];
    uint16_t color_paleta; // O índice de paleta
    char descripcion[64];
    uint8_t dificultad;    // Probabilidad de mejores ópalos
    uint16_t tasa_venta;   // Multiplicador de precio
} Ciudad;

// Definición de las 3 ciudades
static const Ciudad ciudades[3] = {
    {"Valle Ceniza",   0x738F, "Tierra quemada, rica en gemas oscuras.", 1, 100},
    {"Costa Rosa",     0x7C1F, "Brillantes cristales pulidos por el mar.", 2, 150},
    {"Llanura Cristal", 0x5FFF, "Vastos campos de cuarzo puro.", 3, 200}
};
