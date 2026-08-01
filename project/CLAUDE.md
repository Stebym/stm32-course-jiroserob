# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Beat Clash

Consola de juego arcade tipo "Simón Dice / Guitar Hero" para 2 jugadores cara a cara, sobre una NUCLEO-F411RE con pantalla ILI9341, 2 joysticks, 8 botones arcade (con LED) y un buzzer. Es el proyecto principal del curso (45% de la nota de Taller V).

Este directorio (`project/`) es la copia "oficial" del proyecto — se portó aquí, ya compilando, desde `../practica_pantalla/`, que sigue existiendo como banco de pruebas de diseño visual y puede tener cambios más recientes sin portar todavía. Ver `BASE_PROYECTO.txt` para el detalle de cableado físico y `CABLEADO_BOTONES.txt` / `CABLEADO_JOYSTICK2.txt` para el detalle pin a pin de los botones arcade y el segundo joystick.

## Target hardware

- Board: **NUCLEO-F411RE** (STM32F411RETx — Cortex-M4, 512 KB Flash, 128 KB RAM)
- Toolchain: **GNU Tools for STM32 13.3.rel1** (`arm-none-eabi-gcc`)
- CMSIS/device headers: `/home/jimmy/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0/`
- El toolchain no está en el `PATH` por defecto de la shell; vive en
  `/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_*/tools/bin`.
  Para compilar por terminal hay que anteponerlo al `PATH`; STM32CubeIDE ya lo trae configurado internamente.

## Build

El sistema de build es un set de makefiles generados dentro de `Debug/` (estilo STM32CubeIDE / Eclipse CDT managed build). Se construye desde esa carpeta:

```bash
TOOLBIN=/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_*/tools/bin
PATH="$TOOLBIN:$PATH" make -C Debug/ -j4 all

# Clean
PATH="$TOOLBIN:$PATH" make -C Debug/ clean
```

Output artifact: `Debug/<nombre-del-proyecto>.elf` (más `.map` y `.list`) — el nombre depende de `BUILD_ARTIFACT_NAME` en `Debug/makefile`, que STM32CubeIDE regenera solo (con build automático) tomando el nombre del proyecto Eclipse (`.project`, hoy "Semana0") cada vez que detecta cambios en los fuentes. Si compilas por terminal justo después de que el IDE reconstruyó, revisa ese nombre antes de asumirlo fijo.

Key compiler flags usados (de `Debug/Src/subdir.mk`):
- `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb`
- `-std=gnu11 -O0 -g3`
- Defines: `DEBUG NUCLEO_F411RE STM32 STM32F4 STM32F411RETx STM32F411xE`
- Include: `Inc/` + `Drivers/STM32F4xx_HAL_Driver/Inc` + CMSIS Core + STM32F4xx device headers

Flasheo: STM32CubeIDE (abriendo el proyecto y usando Run/Debug) u OpenOCD con un ST-Link.

## Code architecture

Este es un proyecto **basado en HAL** (STM32Cube HAL, sin RTOS ni CubeMX/.ioc — el HAL se usa a mano). Toda la lógica de aplicación vive en `Src/main.c`; `Src/renderer.c` es la capa de dibujo sobre el driver de pantalla.

### Modelo de ejecución

`main()` inicializa periféricos (GPIO, SPI1, TIM3 como disparador del ADC, ADC1, TIM4 para el buzzer, la pantalla), arranca TIM3 y el ADC en modo interrupción, y entra en un `while(1)` que hace polling a ~30 fps (`HAL_Delay(RENDER_TICK_MS)` al final de cada vuelta). No hay lógica de juego dentro de ISRs — las ISRs solo alimentan variables (`stm32f4xx_it.c`):

| ISR | Fuente | Propósito |
|-----|--------|-----------|
| `SysTick_Handler` | SysTick | `HAL_IncTick()` — requerido por `HAL_Delay`/timeouts del HAL |
| `ADC_IRQHandler` | ADC1, disparado por TIM3 cada 20 ms | Fin de conversión de un canal del joystick; `HAL_ADC_ConvCpltCallback` (en `main.c`) lee el canal y encadena el siguiente |
| `TIM4_IRQHandler` | TIM4 | Alterna PA6 por software para generar el tono del buzzer (`Buzzer_SetSalida`/`Buzzer_Actualizar`) |

### Consola de depuración (USART2 / VCP del ST-Link)

`printf()` está retargeteado a USART2 (PA2=TX, PA3=RX, 115200 8N1) vía `__io_putchar()` en `main.c` — no requiere cableado, sale por el mismo cable USB del ST-Link (en Linux, `/dev/ttyACM0`; abrir con `screen /dev/ttyACM0 115200` o similar). Requiere `HAL_UART_MODULE_ENABLED` en `Inc/stm32f4xx_hal_conf.h` (ya habilitado) y compilar `Drivers/.../stm32f4xx_hal_uart.c`.

Los logs están puestos SOLO en eventos discretos (cambio de pantalla en `Demo_Enter`, selección de jugadores/modo, arranque de partida, cada input aceptado con acierto/fallo, game over/retry, golpes de Guitar Hero) — nunca en cada vuelta del loop de ~33ms, para no inundar la consola ni frenar el juego con la transmisión bloqueante del UART. Hay además un log periódico `[ADC] j1=(x,y) j2=(x,y)` cada 500ms (diagnóstico temporal, corre siempre) y el LED rojo de J1 parpadea como heartbeat cada 500ms cuando no hay una partida real de BOTONES en curso.

La pantalla (SPI1) se maneja por **polling**, no por interrupción ni DMA.

### Calibración del centro del joystick (2026-07-31)

El joystick físico de este montaje **no descansa en 2048** (mitad de escala teórica) — se midió por consola en ~(2950,3145) para J1 y ~(2985,3070) para J2. `main()` mide el centro real de cada eje 300 ms después de arrancar el ADC (`centro_j1x/y`, `centro_j2x/y`, log `[CALIB]`) y todos los umbrales de dirección se calculan relativos a ese centro medido (`Joy_Umbrales()`, `JOY_UMBRAL_DESVIO`), no a un valor fijo. **El usuario no debe tocar los joysticks durante ese medio segundo tras encender/resetear la placa**, o la calibración sale mal.

### B1 (Nucleo) queda FISICAMENTE INACCESIBLE (2026-07-31)

El cabinet ya está armado y B1 quedó sellado dentro de la caja — no se puede presionar nunca más. `Boton_B1_Flanco()` sigue en el código (con debounce) pero en la práctica `avanzar` nunca vale 1. Cualquier flujo que dependiera SOLO de B1 sin alternativa quedó como **código muerto inalcanzable** (p. ej. las ramas `DEMO_MODO_SIMON`/`DEMO_MODO_SIMONJOY` dentro del `if (avanzar)`, o `GuitarHero_IntentarGolpe()` vía `DEMO_JUGANDO`) — inofensivo, pero no confiar en textos de ayuda o comentarios viejos que digan "B1=confirmar" en pantallas nuevas.

### Salir/reset con combo de botones

Rojo+Amarillo del mismo jugador, mantenidos 3s (`ComboSalir_Detectado`), resetean duro a la pantalla de splash desde **cualquier** pantalla o modo — se revisa con máxima prioridad al inicio del loop principal, antes que cualquier otra lógica. Es la única forma de salir de una partida real ahora que B1 no es alcanzable.

### Menú de selección de jugadores

Joystick (cualquiera de los 2) navega el cursor (alterna 1/2 jugadores) y **cualquier** botón arcade confirma — no depende de B1.

### Menú de selección de modo — SELECCIÓN DIRECTA

A diferencia de JUGADORES, este menú no usa "mover cursor + confirmar": en cualquiera de las 3 pantallas `DEMO_MODO_*`, presionar **cualquier botón arcade** arranca BOTONES de una vez, y mover **cualquier joystick** arranca SIMONJOY de una vez — el método de entrada ES la elección, sin pasos intermedios. Exige ver el stick centrado al entrar a la pantalla antes de aceptar un movimiento (evita heredar el "arrastre" del menú anterior y disparar SIMONJOY sin querer). Guitar Hero no tiene mapeo directo (no es un modo real todavía).

### Máquina de estados de pantallas (`DemoScreen_t`, en `main.c`)

El flujo de la aplicación es un recorrido de pantallas navegado con los botones arcade (avanzar) y confirmado con **B1** (PC13, único botón de "click" del sistema — los SW de ambos joystick se retiraron físicamente el 2026-07-30):

```
DEMO_SPLASH → DEMO_JUGADORES_{1,2} → DEMO_MODO_{SIMON,SIMONJOY,GUITAR}
→ DEMO_CONTEO_{3,2,1,GO} → [juego real] → DEMO_RESULTADO
```

`Demo_Enter(screen)` prepara cada pantalla; `Renderer_Update*` la redibuja incrementalmente cuando solo cambia el cursor, para no repintar toda la pantalla por SPI en cada frame.

### Estado de los 3 modos de juego

| Modo | Estado | Notas |
|------|--------|-------|
| **Simón Clásico** (4 botones arcade, cara a cara, retrato 240×320) | ✅ Jugable, 2 jugadores real | `Botones_*` en `main.c` |
| **Simón + Joystick** (1 o 2 jugadores) | ✅ Jugable | `SimonJoy_*` (1p) / `SimonJoy2_*` (2p) en `main.c` |
| **Guitar Hero** (notas cayendo, se golpean con los botones del carril) | 🚧 Solo maqueta visual (`DEMO_JUGANDO` + `GuitarHero_IntentarGolpe`, un único botón de prueba B1, sin carriles reales ni sincronía con canción) | **Foco actual del proyecto** — ver `guitar_hero/ANALISIS_REFERENCIA.md` en la raíz del curso para ideas de diseño (chart de notas ligado al tiempo real de una melodía, en vez de spawn periódico) |

El reintento tras game over ya no depende de ningún botón dedicado: en Simón+Joystick se dispara moviendo el propio stick, y en Simón con botones, presionando cualquiera de los 4 botones propios del jugador.

### Mapa de pines (`Inc/board_pins.h`)

- **Pantalla ILI9341** — SPI1 solo-escritura (sin MISO): SCK=PA5, MOSI=PA7, CS=PB6, DC=PC7, RST=PA9. VCC a 3V3 (no 5V).
- **B1** (PC13) — único botón de click, pull-up ya presente en la Nucleo.
- **Joystick 1**: VRy=PA1 (ADC1_IN1), VRx=PA4 (ADC1_IN4). Sin pin SW (retirado).
- **Joystick 2**: VRy2=PC0 (ADC1_IN10), VRx2=PC1 (ADC1_IN11). Sin pin SW2 (retirado).
- **Botones arcade J1** (switch + LED vía ULN2003A #1): ROJO=PB12/PB8, VERDE=PA12/PC8, AZUL=PC6/PC5, AMARILLO=PC9/PA11.
- **Botones arcade J2** (switch + LED vía ULN2003A #2): ROJO=PC10/PB5, VERDE=PA10/PC4, AZUL=PB13/PB14, AMARILLO=PB15/PB1.
- **Buzzer**: PA6, GPIO de salida normal alternado por software desde TIM4 (el F411 no tiene TIM13/TIM14, y el único timer con canal en PA6, TIM3_CH1, ya está ocupado disparando el ADC).

### Constantes de juego (`Inc/game_state.h`)

Dimensiones de pantalla/carriles, velocidades y ventanas de puntuación (`HIT_PERFECT/GOOD/OK` → 100/50/25 puntos + combo) están centralizadas ahí. Algunos campos de `GameState_t`/`EstadoJugador_t` (p. ej. `ESTADO_MENU_NIVEL`, lectura del joystick por DMA, polling de botones por TIM5) son de un diseño anterior y **no reflejan el mecanismo real actual** (polling en el loop principal de `main()`, ADC disparado por TIM3+IRQ) — al tocar este archivo, verificar contra `main.c`, no asumir que los comentarios del header están al día.

## File layout

```
Src/
  main.c              — TODA la lógica de aplicación: init de periféricos, máquina de
                         estados de pantallas, los 3 modos de juego, buzzer, botones, joysticks
  renderer.c           — capa de dibujo (pantallas, texto, HUD, notas) sobre ili9341.c
  ili9341.c             — driver de la pantalla (SPI, comandos del controlador, primitivas)
  stm32f4xx_it.c        — ISRs: SysTick, ADC, TIM4 (buzzer)
  syscalls.c / sysmem.c / system_stm32f4xx.c  — generados, no editar
Inc/
  board_pins.h          — TODOS los #define de pines
  game_state.h          — constantes de juego + tipos de datos (GameState_t, Nota_t, ...)
  renderer.h / ili9341.h — prototipos
  splash_bg.h            — imagen de bienvenida (RGB565 big-endian, 320×240)
  stm32f4xx_hal_conf.h / stm32f4xx_it.h — config del HAL
Drivers/                — HAL de ST (STM32Cube), no tocar
Startup/                — arranque assembly + vector de interrupciones
STM32F411RETX_FLASH.ld / _RAM.ld — linker scripts
Debug/                  — carpeta de build generada (make -C Debug/ desde ahí)
BASE_PROYECTO.txt, CABLEADO_BOTONES.txt, CABLEADO_JOYSTICK2.txt — documentación de cableado
```
