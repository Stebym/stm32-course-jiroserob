# Análisis comparativo: `Mert-Yazgan/guitar_hero` vs. modo Guitar Hero de `practica_pantalla/`

**Repositorio analizado:** https://github.com/Mert-Yazgan/guitar_hero.git
**Fecha:** 2026-07-28
**Método:** se clonó temporalmente en el scratchpad de la sesión (no en el proyecto) solo para lectura, y se descartó después de este análisis. Esta carpeta se dejó vacía, sin el código fuente del repo.

## 1. Plataforma de destino (diferencia fundamental)

| | Repo de referencia | `practica_pantalla/` (este proyecto) |
|---|---|---|
| Hardware | **DE1-SoC** (FPGA + ARM Cortex-A9, HPS) | STM32F411 (Nucleo), Cortex-M4 |
| Video | Framebuffer VGA mapeado en memoria (`PIXEL_ADRESS`, escritura directa de píxel) | Panel **ILI9341 por SPI**, sin framebuffer propio — cada primitiva (`FillRect`, texto, imagen) es una transacción SPI |
| Audio | Códec de audio con FIFO + DMA, reproduce PCM de 16 bits precomputado | Buzzer activo/piezo en PA6, tono cuadrado generado por software vía interrupción de TIM4 (sin DAC) |
| Entrada | 4 pulsadores dedicados del DE1-SoC, polling por registro GPIO | Joystick ADC + (pendiente) botones arcade |

**Conclusión:** no hay código directamente portable. El modelo de dibujo (escribir píxeles en un buffer en RAM) y el de audio (streaming PCM por DMA) no existen en este hardware — ya se resolvieron ambos con enfoques distintos y ya funcionando (SPI + `ILI9341_DrawImage`/primitivas propias, y TIM4 + buzzer por software, ver memoria de `prueba_de_sonido`).

## 2. Modelo de "chart" de notas — la única idea rescatable

El repo de referencia codifica la canción completa como un arreglo compacto:

```c
uint8_t notes_array[90] = {2, 5, 8, 1, 4, 7, ...}; // 90 valores, 1 por segundo
```

Cada valor es una **máscara de 4 bits** (bit0=verde, bit1=rojo, bit2=amarillo, bit3=azul) que indica qué carriles tienen nota en ese segundo de la canción. `update_vga_screen()` calcula el índice con `current_time / 1000` y dibuja las notas activas + las del segundo anterior (para el efecto de caída), sin ningún generador aleatorio ni temporizador de spawn — la posición de cada nota está 100% ligada al tiempo de la canción.

Esto contrasta con el spawn actual en `practica_pantalla/`: `EstadoJugador_t.tick_ultimo_spawn` + intervalos fijos por nivel (`SPAWN_INTERVAL_L1/L2/L3`), que generan notas periódicas mientras el juego corre — no están sincronizadas con ninguna melodía real, solo con el reloj del juego.

**Idea aplicable:** ya existe en el proyecto una fuente de "canción real" — las melodías `PasoSonido_t{freq_hz,dur_ms}` portadas en `prueba_de_sonido/` y usadas en `DEMO_MENU_CANCIONES` (Estrellita, Himno de la Alegría, etc., ver memoria de `project_practica_pantalla`). Se podría definir, para una de esas canciones, un arreglo paralelo tipo `chart[]` (una entrada de máscara de carril por nota o por paso de tiempo) para que las notas que caen en el modo Guitar Hero **coincidan con las notas reales que suena el buzzer**, en vez de spawnear con temporizador independiente del audio. Esto es exactamente lo que hace `notes_array` del repo de referencia, adaptado a "una entrada por nota de la melodía" en lugar de "una entrada por segundo de PCM".

Esto es una idea de diseño, no código — no se implementó nada todavía (no fue pedido).

## 3. Lo demás: `practica_pantalla/` ya está en un punto más avanzado

- **Puntaje:** el repo de referencia solo hace ±1 por acierto/fallo con piso en 0. El proyecto actual ya tiene ventanas de precisión escalonadas (`HIT_PERFECT/GOOD/OK` → `SCORE_PERFECT/GOOD/OK` = 100/50/25) y combo — más granular.
- **Detección de golpe:** el repo de referencia solo verifica el estado del botón una vez cada 100 ms (refresco de pantalla) contra una ventana de posición Y fija. `GuitarHero_IntentarGolpe()` ya calcula distancia continua de la nota más cercana al centro de la zona de presión, más flexible.
- **Carriles por color:** ambos usan el mismo concepto (verde/rojo/amarillo/azul = 4 carriles), coincidencia de diseño, no algo que se copió.

## Recomendación

No se recomienda clonar ni portar código de este repo — la arquitectura de hardware es demasiado distinta (FPGA con framebuffer y DMA de audio vs. MCU con SPI y sin DAC). Lo único con valor real es la idea conceptual de **notas ligadas a una tabla de tiempos de la canción real** en vez de spawn periódico aleatorio, aplicable el día que se quiera sincronizar el modo Guitar Hero con una de las melodías de `prueba_de_sonido/`.
