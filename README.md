# 🧠 Arquitectura Final de Renderizado GBA (Versión Estable Sin Flicker)

## 🎯 Objetivo

Eliminar completamente:
- Flicker de paleta
- Parpadeo de fondo
- Inestabilidad visual por DMA
- Mutaciones de VRAM durante el frame
- Conflictos entre UI / fondo / gema

---

# 🧱 Arquitectura General del Renderer

El sistema se divide en 3 capas fijas e independientes:


CAPA 1 → FONDO (dithering beige)
CAPA 2 → GEMAS (buffer software)
CAPA 3 → UI (texto/overlay)


---

# 🧩 Problema Actual

El flicker viene de:

## ❌ Problemas detectados

- Escritura constante a `0x05000000` (paleta global)
- Regeneración de fondo cada frame
- Uso de DMA sin sincronización VBlank estricta
- Mezcla de UI + fondo + gema en estado mutable
- Falta de doble buffer real
- Render no determinista por frame

---

# 🧠 Solución Global

## 🔵 1. DOBLE BUFFER REAL

```c
uint8_t* bufA = get_anim_buf_a();
uint8_t* bufB = get_anim_buf_b();

uint8_t* buf_front;
uint8_t* buf_back;
Swap por frame:
static inline void swap_buffers(void)
{
    uint8_t* tmp = buf_front;
    buf_front = buf_back;
    buf_back = tmp;
}
🔵 2. PIPELINE DE RENDER ESTABLE

Orden obligatorio del frame:

void render_frame(const Gema* g, int offset_opalo, int offset_bg)
{
    vsync(); // sincronización obligatoria con VBlank

    swap_buffers();

    static int last_bg = -999;
    if (offset_bg != last_bg) {
        precalcular_fondo(offset_bg);
        last_bg = offset_bg;
    }

    renderizar_gema_a_buffer(buf_back, 240, 160, g);

    volcar_frame(buf_back, offset_opalo, offset_bg);

    draw_ui_sobre_buffer("GALERIA");
}
🔵 3. PALETA ESTABLE (CRÍTICO)
❌ Prohibido:
Modificar paleta global en runtime desde múltiples funciones
❌ NO HACER:
pal[xx] = ...
✅ Solución: Paleta doble buffer
static uint16_t pal_front[256];
static uint16_t pal_back[256];

void swap_paleta(void)
{
    memcpy((void*)0x05000000, pal_back, 512);
}
🔵 4. DIVISIÓN DE PALETA
0–15     → UI / fondo
16–200   → gemas / plasma
201–255  → efectos / brillo
🔵 5. FONDO OPTIMIZADO
❌ Antes:
precalcular_fondo(offset_bg); // cada frame
✅ Ahora:
static int last_bg = -1;

if (offset_bg != last_bg) {
    precalcular_fondo(offset_bg);
    last_bg = offset_bg;
}
🔵 6. DMA SEGURO (VBLANK ONLY)

Regla:

DMA SOLO después de vsync, nunca durante render

void dma_copy_line(uint16_t* src, uint16_t* dst)
{
    REG_DMA3SAD = (uint32_t)src;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = 120 | (1 << 31);
}
🔵 7. FRAME FINAL (ESTABLE)
void render_frame(const Gema* g, int offset_opalo, int offset_bg)
{
    vsync();

    swap_buffers();

    if (offset_bg != last_bg) {
        precalcular_fondo(offset_bg);
        last_bg = offset_bg;
    }

    renderizar_gema_a_buffer(buf_back, 240, 160, g);

    volcar_frame(buf_back, offset_opalo, offset_bg);

    draw_ui_sobre_buffer("GALERIA");
}
🚨 ERRORES CRÍTICOS A EVITAR
❌ Nunca hacer:
Escribir paleta en múltiples funciones por frame
Regenerar fondo sin cache
Usar DMA fuera de VBlank
Mezclar UI con paleta de gema
Render directo a VRAM sin buffer
🧠 Resultado esperado

Con esta arquitectura:

✔ Cero flicker de paleta
✔ Fondo estable sin parpadeo
✔ Render determinista por frame
✔ Pipeline tipo juego comercial GBA
✔ Escalabilidad para sistemas complejos (galería / inventario / animaciones)
🚀 Extensiones futuras recomendadas
Tile engine híbrido para fondo
Cache de gemas por seed (no render cada frame)
UI en OAM (sprites reales)
Sistema de materiales precalculados
Pipeline de render multietapa tipo consola

---

Si quieres, el siguiente paso lógico es que te lo convierta en:

- 🔧 :contentReference[oaicite:0]{index=0}
- o directamente un **:contentReference[oaicite:1]{index=1}**

Solo dime.
