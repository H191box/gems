PLAN DE REFACTORIZACIÓN DEL SISTEMA DE GEMAS
Proyecto: Gacha de Ópalos GBA
Objetivo principal

Reducir el tamaño de las partidas guardadas y eliminar datos redundantes almacenados en Gema.

La filosofía nueva será:

Una gema almacena únicamente información persistente.

Toda información derivable se recalcula cuando se necesite.

Estado actual

Actualmente una Gema almacena:

id
etapa

tipo_real
patron_real

brillo_real
pureza_real
iridiscencia
saturacion

pista_color
pista_intensidad
pista_patron

quilates

seed_visual

Muchos de estos valores:

Nunca cambian.
Se generan una sola vez.
Son completamente deterministas.

Por tanto no necesitan almacenarse.

Nuevo modelo conceptual
Chunk

Representa un hallazgo en bruto.

Contiene:

seed
bioma
profundidad
etc

Sólo existe durante la generación.

Gema

Representa un objeto coleccionable persistente.

Debe almacenar únicamente:

typedef struct
{
    uint32_t seed;

    uint16_t quilates;

    uint8_t etapa;

    uint8_t flags;
}
Gema;

Tamaño aproximado:

8 bytes

o

12 bytes

según alineación.

Opalo

Representa una reconstrucción temporal para renderizado.

No se guarda.

Se reconstruye desde:

Gema
↓
generar_opalo_desde_gema()
↓
Opalo temporal
Principio fundamental

Toda propiedad visual o comercial debe derivarse de:

seed
+
quilates
+
etapa

Nunca almacenarse.

FASE 1 — Unificar la semilla
Objetivo

Eliminar seed_visual.

Guardar una única seed canónica.

Archivos a modificar
gema.h

Eliminar:

seed_visual

Añadir:

seed

como semilla principal.

gema.c

Modificar:

crear_gema_desde_chunk()

para guardar directamente la seed original.

opalo.c

Modificar:

generar_opalo_desde_gema()

para usar:

g->seed

en lugar de:

g->seed_visual
save.c

Actualizar serialización.

FASE 2 — Eliminar atributos permanentes
Objetivo

Eliminar atributos derivados.

Campos eliminados
tipo_real
patron_real

brillo_real
pureza_real
iridiscencia
saturacion
Nuevo enfoque

Crear funciones puras:

tipo_opalo(seed)

patron_opalo(seed)

pureza_opalo(seed)

brillo_opalo(seed)

iridiscencia_opalo(seed)

saturacion_opalo(seed)
Archivos a modificar
gema.h

Eliminar los campos anteriores.

gema.c

Crear funciones derivadas.

opalo.c

Reemplazar lecturas directas de la estructura.

Antes:

g->tipo_real

Después:

tipo_opalo(g->seed)
galeria.c

Actualizar todas las pantallas de información.

FASE 3 — Eliminar pistas almacenadas
Objetivo

Las pistas no ocupan memoria.

Se generan dinámicamente.

Campos eliminados
pista_color
pista_patron
pista_intensidad
Nuevo sistema

Funciones:

obtener_pista_color(seed)

obtener_pista_patron(seed)

obtener_pista_intensidad(seed)
Archivos a modificar
gema.h

Eliminar los tres campos.

gema.c

Crear generadores de pistas.

galeria.c

Mostrar pistas generadas al vuelo.

FASE 4 — Reescribir el cálculo de valor
Objetivo

Hacer que los ópalos memorables sean los más valiosos.

Problema actual

La valoración depende demasiado de:

pureza
brillo
saturación

y poco de:

tipo
patrón
Nuevo enfoque

Valor base:

tipo
+
patrón

Modificadores:

quilates
pureza
brillo

Bonificaciones especiales:

combinaciones raras
tipos extremadamente raros
patrones legendarios
Archivos a modificar
gema.c

Reescribir:

calcular_valor_gema()
galeria.c

Actualizar textos de valoración.

FASE 5 — Introducir información oculta real
Objetivo

Dar sentido al proceso de trabajo.

BRUTA

Información visible:

quilates aproximados
pistas
CORTADA

Información visible:

tipo confirmado
patrón confirmado
PULIDA

Información visible:

todos los atributos
valor definitivo
Ventaja

Ahora sí existe progresión real:

BRUTA
↓
INCERTIDUMBRE

CORTADA
↓
DESCUBRIMIENTO

PULIDA
↓
REVELACIÓN COMPLETA
Archivos a modificar
galeria.c

Pantallas de información.

gema.c

Funciones de consulta por etapa.

FASE 6 — Compactación final de SRAM
Objetivo

Preparar el juego para miles de gemas.

Posible estructura final
typedef struct
{
    uint32_t seed;
    uint16_t quilates;

    uint8_t etapa;
    uint8_t flags;
}
Gema;
Resultado esperado

Situación actual:

20 bytes por gema

Objetivo:

8 bytes por gema

Reducción:

60% menos memoria
Orden recomendado de implementación
1. Unificar seed
2. Eliminar seed_visual
3. Eliminar atributos derivados
4. Eliminar pistas
5. Reescribir valoración
6. Rehacer UI de descubrimiento
7. Compactar save
Resultado final

La gema se convierte en un objeto extremadamente pequeño y estable:

Seed  → Identidad completa del ópalo
Etapa → Qué conoce el jugador
Quilates → Tamaño físico
Flags → Estado especial

Todo lo demás se calcula cuando se necesita.

Eso reduce SRAM, simplifica el código, facilita el equilibrado económico y permite añadir nuevas propiedades en el futuro sin cambiar el formato de guardado.
