/**
 ******************************************************************************
 * @file    : main.c
 * @author  : Jimmy Stebym Rosero Barrera
 * @brief   : Final Box - Consola arcade de 2 jugadores cara a cara, tipo
 *            "Simon Dice / Guitar Hero", sobre NUCLEO-F411RE + pantalla
 *            ILI9341 (320x240, SPI). Proyecto principal de Taller V (2.0).
 * Nucleo-F411RE (STM32F411RETx) | HAL puro de STM32Cube, sin CubeMX/.ioc,
 * sin RTOS -- toda la aplicacion vive en un bucle mas una maquina de estados.
 *
 * ------------------------------------------------------------------------
 * QUE HACE ESTE PROGRAMA
 * ------------------------------------------------------------------------
 * Dos jugadores, uno a cada lado de la pantalla, compiten en 3 modos de
 * juego distintos que comparten la misma mecanica de fondo ("repetir/atinar
 * una secuencia o una nota a tiempo"):
 *   1. SIMON CON BOTONES ARCADE: 4 botones con LED por jugador (rojo, verde,
 *      azul, amarillo); el sistema enciende una secuencia de colores cada
 *      vez mas larga y el jugador debe repetirla presionando los botones en
 *      el mismo orden.
 *   2. SIMON + JOYSTICK: la misma mecanica que el anterior, pero la entrada
 *      es la direccion del joystick analogico (arriba/abajo/izquierda/
 *      derecha) en vez del color de un boton. Jugable a 1 o 2 jugadores.
 *   3. GUITAR HERO: notas de 4 colores caen por su carril hacia una "zona de
 *      golpe" fija; el jugador debe presionar el boton de ese color justo
 *      cuando la nota cruza la zona, sumando puntaje segun que tan cerca
 *      cayo del centro (PERFECT/GOOD/OK). Jugable a 1 jugador (pantalla
 *      completa) o 2 jugadores (mitad de pantalla cada uno, en espejo).
 *
 * Todo el flujo (splash -> elegir jugadores -> elegir modo -> conteo 3-2-1-GO
 * -> partida real -> resultado) es una maquina de estados de pantallas
 * (DemoScreen_t, ver mas abajo) recorrida por el bucle principal a un ritmo
 * fijo de ~30 fps (RENDER_TICK_MS=33 ms). El dibujo esta separado en
 * renderer.c (capa de presentacion); este archivo (main.c) concentra TODA
 * la logica de aplicacion: inicializacion de perifericos, la maquina de
 * estados de pantallas, los 3 modos de juego, el generador de tonos del
 * buzzer, y la lectura antirrebote de botones/joystick.
 *
 * ------------------------------------------------------------------------
 * MAPA DE PINES
 * ------------------------------------------------------------------------
 * Pin    Funcion                  Modo                Notas
 * PA1    ADC1_IN1  J1 VRy         Analogico           joystick 1, eje vertical
 * PA2    USART2_TX                AF7                 consola de depuracion (VCP del ST-Link)
 * PA3    USART2_RX                AF7                 no usado por la aplicacion (solo TX)
 * PA4    ADC1_IN4  J1 VRx         Analogico           joystick 1, eje horizontal (separado de PA1 a proposito)
 * PA5    SPI1_SCK  LCD            AF5                 reloj de la pantalla, 8 MHz
 * PA6    Buzzer                   Salida PP           libre porque SPI1 no usa MISO; alternado por software desde TIM4
 * PA7    SPI1_MOSI LCD            AF5                 datos hacia la pantalla (driver de solo escritura)
 * PA9    LCD RST                  Salida PP           reset hardware del ILI9341
 * PA10   BTN2 Verde (switch)      Entrada PullUp      jugador 2
 * PA11   BTN1 Azul (LED)          Salida PP           hacia ULN2003A #1
 * PA12   BTN1 Verde (switch)      Entrada PullUp      jugador 1
 * PB1    BTN2 Rojo (LED)          Salida PP           hacia ULN2003A #2
 * PB5    BTN2 Amarillo (LED)      Salida PP           reubicado del canal averiado IN4 al IN5 del ULN2003A #2
 * PB6    SPI1_CS   LCD            Salida PP (NSS soft) Chip Select por software
 * PB8    BTN1 Rojo (LED)          Salida PP           hacia ULN2003A #1
 * PB12   BTN1 Rojo (switch)       Entrada PullUp      jugador 1
 * PB13   BTN2 Azul (switch)       Entrada PullUp      jugador 2
 * PB14   BTN2 Verde (LED)         Salida PP           hacia ULN2003A #2
 * PB15   BTN2 Amarillo (switch)   Entrada PullUp      jugador 2
 * PC0    ADC1_IN10 J2 VRy2        Analogico           joystick 2, eje vertical
 * PC1    ADC1_IN11 J2 VRx2        Analogico           joystick 2, eje horizontal
 * PC4    BTN2 Azul (LED)          Salida PP           hacia ULN2003A #2
 * PC5    BTN1 Amarillo (LED)      Salida PP           hacia ULN2003A #1
 * PC6    BTN1 Azul (switch)       Entrada PullUp      jugador 1
 * PC7    LCD DC                   Salida PP           Data/Command de la pantalla
 * PC8    BTN1 Verde (LED)         Salida PP           hacia ULN2003A #1
 * PC9    BTN1 Amarillo (switch)   Entrada PullUp      jugador 1
 * PC10   BTN2 Rojo (switch)       Entrada PullUp      jugador 2
 * PC13   B1 (boton usuario Nucleo) Entrada             pull-up externo R30=4k7 ya en la placa; ver nota de B1 mas abajo
 * (Los pines SW/click de AMBOS joystick fueron retirados fisicamente del
 * montaje para simplificar el cableado -- ver board_pins.h. Cada boton
 * arcade tiene switch + LED; el LED se maneja siempre a traves de un
 * ULN2003A, un arreglo de transistores Darlington que hace de driver de
 * corriente, porque un pin GPIO del STM32 no puede sostener con seguridad
 * la corriente de un LED de boton arcade en las 8 salidas simultaneas del
 * sistema.)
 *
 * ------------------------------------------------------------------------
 * RELOJES Y PERIFERICOS ACTIVOS
 * ------------------------------------------------------------------------
 * SPI1  - bus de la pantalla ILI9341. Modo maestro, Modo 0 (CPOL=0/CPHA=0,
 *         el que exige el datasheet del ILI9341), NSS por software,
 *         BaudRatePrescaler=2 => APB2 (16 MHz) / 2 = 8 MHz de reloj SPI.
 *         Se eligio 8 MHz por prueba empirica: es la maxima velocidad que
 *         sostuvo una imagen estable sin artefactos en este cableado fisico
 *         concreto (mas rapido = menos margen ante ruido electrico).
 * ADC1  - modo ESCANEO de 4 canales (J1_Y, J1_X, J2_Y, J2_X, en ese orden de
 *         rank), 12 bits de resolucion, disparado por TRGO de TIM3 cada
 *         20 ms, interrupcion de fin de conversion DESPUES DE CADA canal
 *         (no solo al final de los 4), sin DMA -- ver ADC1_Joystick_Init()
 *         y HAL_ADC_ConvCpltCallback() mas abajo.
 * TIM3  - unico proposito: generar el TRGO (trigger interno) que dispara el
 *         ADC1 cada 20 ms. Prescaler=1599 (16 MHz/1600 = tick de 100 us),
 *         Period=199 (200 ticks x 100 us = 20.000 ms exactos). No genera
 *         ninguna interrupcion propia ni señal visible en un pin.
 * TIM4  - genera el tono del buzzer POR SOFTWARE (ver seccion BUZZER mas
 *         abajo), alternando el pin PA6 desde su interrupcion periodica.
 * USART2 - consola de depuracion, 115200 8N1, TX/RX habilitados pero solo se
 *          usa TX (printf retargeteado via __io_putchar). Sale por el mismo
 *          cable USB del ST-Link (VCP, /dev/ttyACM0 en Linux) sin cableado
 *          adicional.
 * SysTick - interrupcion de 1 ms del propio Cortex-M4, alimenta
 *           HAL_IncTick()/HAL_GetTick()/HAL_Delay(); es la base de TODA la
 *           temporizacion no bloqueante de este archivo (ver seccion
 *           siguiente).
 *
 * ------------------------------------------------------------------------
 * POR QUE EL BUZZER SE GENERA POR SOFTWARE (TIM4 + GPIO), NO POR PWM
 * ------------------------------------------------------------------------
 * El unico timer con un canal PWM disponible en el pin PA6 seria TIM3_CH1,
 * pero TIM3 ya esta ocupado como disparador del ADC cada 20 ms. El F411
 * tampoco tiene TIM13/TIM14 (esos si existen, con PWM libre, en la familia
 * F413/F423). La solucion: TIM4 (libre) interrumpe periodicamente y cada
 * interrupcion invierte el nivel logico de PA6 por software
 * (Buzzer_SetSalida()/TIM4_IRQHandler en stm32f4xx_it.c) -- dos inversiones
 * equivalen a un ciclo completo de la onda cuadrada del tono. El ARR
 * (auto-reload) de TIM4 se recalcula en cada cambio de nota:
 *     arr = (BUZZER_TIM_TICK_HZ / (2 * freq_hz)) - 1
 * de forma que el timer desborda 2 veces por cada ciclo de la nota pedida.
 * Buzzer_Actualizar() (llamada 1 vez por vuelta del bucle principal) avanza
 * los patrones de efectos de sonido (SFX) y la musica de fondo comparando
 * HAL_GetTick() contra la duracion de cada paso -- NUNCA usa HAL_Delay,
 * para que el sonido conviva con el render y la lectura de entradas sin
 * congelar nada. El buzzer es un unico pin (mono): un efecto de sonido en
 * primer plano "congela" el cronometro de la musica de fondo mientras
 * suena, y la retoma exactamente donde iba al terminar.
 *
 * ------------------------------------------------------------------------
 * ANTIRREBOTE (DEBOUNCE) Y TEMPORIZACION NO BLOQUEANTE
 * ------------------------------------------------------------------------
 * Ningun temporizador de este archivo usa HAL_Delay() dentro del juego real
 * (la UNICA excepcion es la calibracion de joystick al arrancar, ver mas
 * abajo, que corre antes de que exista partida). El patron que se repite en
 * TODOS los mecanismos de tiempo (antirrebote de botones, combo de salida,
 * cooldowns de reingreso, animaciones, pasos de melodia) es: guardar el
 * tick (HAL_GetTick()) del ultimo evento en una variable, y en cada vuelta
 * del bucle comparar "HAL_GetTick() - tick_guardado >= UMBRAL_MS" sin
 * bloquear nunca la ejecucion. Esto permite tener muchos "relojes" logicos
 * concurrentes sin RTOS.
 * Para los botones arcade especificamente (Botones_LeerColor), el criterio
 * de antirrebote exige que un nivel NUEVO de un pin se mantenga estable
 * durante BTN_DEBOUNCE_MS=30 ms consecutivos antes de aceptarlo como un
 * flanco de presion real -- esto filtra el rebote mecanico del contacto
 * (varias transiciones de nivel en los primeros milisegundos tras
 * presionar/soltar) sin depender de cuanto tarde cada vuelta del bucle.
 *
 * ------------------------------------------------------------------------
 * JOYSTICK: ADC POR TIM3, FILTRO EMA, ZONA MUERTA Y CALIBRACION DE CENTRO
 * ------------------------------------------------------------------------
 * Cada canal leido por HAL_ADC_ConvCpltCallback() se suaviza con un filtro
 * EMA (media movil exponencial, filtro += (crudo-filtro)/ADC_FILTRO_N): un
 * solo acumulador por eje, sin guardar historial de muestras, adecuado para
 * un MCU sin FPU dedicada a esta tarea. Ademas, si la lectura filtrada cae
 * dentro de una banda (JOY_ZONA_MUERTA) alrededor del centro medido, se
 * fuerza al centro EXACTO -- esto elimina el temblor residual de ruido que
 * el filtro por si solo no elimina del todo, evitando direcciones falsas
 * con el joystick en reposo. El potenciometro fisico de este montaje NO
 * descansa exactamente en 2048 (mitad teorica de 12 bits) por tolerancias
 * de fabricacion; por eso, 300 ms despues de arrancar el ADC (HAL_Delay
 * bloqueante, la unica excepcion mencionada arriba, aceptable porque corre
 * antes de que exista juego real), se mide el centro REAL de cada eje y
 * todos los umbrales de direccion (Joy_Umbrales(), JOY_UMBRAL_DESVIO) se
 * calculan relativos a ese centro medido, no a un valor fijo.
 *
 * ------------------------------------------------------------------------
 * LA FSM DE PANTALLAS (DemoScreen_t)
 * ------------------------------------------------------------------------
 *   DEMO_SPLASH -> DEMO_JUGADORES_{1,2} -> DEMO_INICIALES ->
 *   DEMO_MODO_{SIMON,SIMONJOY,GUITAR} -> [DEMO_MENU_DIFICULTAD si el modo
 *   elegido fue Guitar Hero] -> DEMO_CONTEO_{3,2,1,GO} -> [juego real] ->
 *   DEMO_RESULTADO / pantalla de game over propia del modo.
 * El menu de MODO usa seleccion DIRECTA (a diferencia del de JUGADORES, que
 * es cursor+confirmar): presionar cualquier boton arcade arranca BOTONES ya
 * mismo, y mover cualquier joystick arranca SIMONJOY ya mismo -- el metodo
 * de entrada ES la eleccion. Se exige ver el joystick centrado al entrar a
 * esta pantalla antes de aceptar un movimiento, para no heredar el
 * "arrastre" del menu anterior y disparar SIMONJOY sin querer.
 * Demo_Enter(screen) prepara cada pantalla nueva; Renderer_Update*() la
 * redibuja de forma incremental (solo lo que cambia) cuando el cambio es
 * menor (ej. mover un cursor), para no repintar toda la pantalla por SPI en
 * cada frame.
 * Salida de emergencia: mantener Rojo+Amarillo del MISMO jugador durante 3 s
 * (ComboSalir_Detectado(), COMBO_SALIR_MS) resetea duro a DEMO_SPLASH desde
 * CUALQUIER pantalla o modo -- se revisa con maxima prioridad al inicio del
 * bucle principal, antes que cualquier otra logica, porque es la UNICA via
 * de salida ahora que B1 no es alcanzable (ver nota siguiente).
 *
 * ------------------------------------------------------------------------
 * B1 (PC13) Y EL REINTENTO TRAS GAME OVER
 * ------------------------------------------------------------------------
 * B1 fue el unico boton de tipo "click" del sistema mientras el cabinet
 * estaba en construccion (los pines SW de ambos joystick se retiraron
 * fisicamente para simplificar el cableado). Con el cabinet ya armado, B1
 * quedo FISICAMENTE INACCESIBLE (sellado dentro de la caja) -- el codigo
 * que lo maneja (Boton_B1_Flanco()) permanece por compatibilidad pero es
 * codigo muerto inalcanzable en la practica; no confiar en comentarios
 * viejos que digan "B1=confirmar". El reintento tras perder una partida NO
 * depende de ningun boton dedicado: en Simon+Joystick se dispara moviendo
 * el propio stick (estado SJ_GAMEOVER en SimonJoy_Actualizar /
 * SimonJoy2_ActualizarJugador), en Simon con botones presionando
 * cualquiera de los 4 botones propios del jugador (Botones_ActualizarJugador),
 * y en Guitar Hero de la misma forma (GuitarHero_ActualizarJugador) -- este
 * patron de "cualquier entrada del propio jugador reintenta" se aplica a
 * los 3 modos por consistencia de interfaz.
 *
 * ------------------------------------------------------------------------
 * GUITAR HERO: MAQUETA DE DISEÑO vs. MODO REAL
 * ------------------------------------------------------------------------
 * La pantalla DEMO_JUGANDO y GuitarHero_IntentarGolpe() son la maqueta
 * visual ORIGINAL del modo (2 notas fijas, un solo boton de prueba B1 --
 * hoy inalcanzable), anterior al modo jugable actual. El modo REAL y
 * jugable (1 o 2 jugadores) vive en GuitarHero_ActualizarJugador(),
 * GuitarHero_IniciarSolo()/2_IniciarAmbos() y GuitarHero_ProcesarGolpe():
 * carriles de 4 colores con notas que caen a velocidad constante hacia una
 * zona de golpe fija, generadas con el mismo generador congruencial lineal
 * (LCG) de 32 bits usado en el resto del proyecto. En el modo de 1 jugador,
 * las 4 notas caen sobre el MISMO set fisico de 4 botones (pantalla
 * completa); por eso la lectura de botones de ese modo usa
 * Botones_LeerColoresBitmask() (en vez de Botones_LeerColor(), que solo
 * reporta el primer color con flanco de cada tick) -- para no perder un
 * golpe cuando 2 notas de colores distintos exigen un boton cada una
 * dentro del mismo tick de ~33 ms.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"          // trae todo el HAL de ST (GPIO, SPI, ADC, TIM, UART, HAL_Delay/HAL_GetTick, etc.)
#include "board_pins.h"             // todos los #define de pines fisicos (BTN1_*, BTN2_*, pantalla, joystick, buzzer)
#include "game_state.h"             // constantes de juego (tamaños, velocidades, puntajes) y struct GameState_t
#include "ili9341.h"                // prototipos del driver de la pantalla (ILI9341_*)
#include "renderer.h"               // prototipos de la capa de dibujo (Renderer_*)
#include <string.h>                 // strcpy/strcmp/memset usados mas abajo
#include <stdio.h>                  // printf/snprintf (printf sale por USART2, ver __io_putchar mas abajo)

SPI_HandleTypeDef hspi1;             // handle HAL del periferico SPI1, usado por la pantalla (SCK=PA5,MOSI=PA7,CS=PB6,DC=PC7)
ADC_HandleTypeDef hadc1;   /* joystick x/y */                          // handle HAL del ADC1, escanea los 4 canales de los 2 joystick en secuencia
TIM_HandleTypeDef htim3;   /* disparador del adc cada 20 ms */         // TIM3 en modo TRGO: cada 20ms dispara una conversion nueva del ADC1
TIM_HandleTypeDef htim4;   /* alterna pa6 por interrupcion para el tono del buzzer */  // TIM4 genera la interrupcion periodica que alterna PA6 (buzzer)
UART_HandleTypeDef huart2; /* consola de depuracion por el VCP del ST-Link (ver MX_USART2_UART_Init) */  // USART2, 115200 8N1, retargeteado a printf
static GameState_t gs;               // estado "global" de la partida en curso (puntajes, notas, etc.) — se resetea en cada Demo_Enter()

/* === JOYSTICK 1 =========================================================== */
/* Las lecturas se almacenan ya filtradas, en la escala nativa de 12 bits
 * (0-4095) del ADC (ver HAL_ADC_ConvCpltCallback para el filtrado). */
volatile uint16_t joystick_x = 2048;  // ultima lectura filtrada (EMA) del canal VRx de J1 (PA4/ADC1_IN4); volatile porque se escribe desde la rutina de interrupcion del ADC
volatile uint16_t joystick_y = 2048;  // ultima lectura filtrada (EMA) del canal VRy de J1 (PA1/ADC1_IN1); volatile porque se escribe desde la rutina de interrupcion del ADC
static uint8_t    adc_rank_actual = 0;   /* 0=y1(rank1) 1=x1(rank2) 2=y2(rank3) 3=x2(rank4) */   // indice de turno dentro del ciclo de 4 canales del ADC; modificarlo manualmente descuadraria el orden de lectura

/* Orden del filtro EMA (media movil exponencial) aplicado a cada eje del
 * ADC: un valor mayor suaviza mas la lectura mas a costa de mayor latencia
 * de respuesta. Con el centro de cada eje calibrado en el arranque (ver
 * Joystick_Calibrar), no es necesario sacrificar suavidad de la señal a
 * cambio de velocidad de deteccion de direccion. */
#define ADC_FILTRO_N     4   // orden del filtro EMA -- subir este numero suaviza mas la lectura pero la hace mas lenta en reaccionar a un movimiento real del joystick
static int32_t filtro_adc_x = 2048;   // acumulador del filtro EMA para el canal X de J1
static int32_t filtro_adc_y = 2048;   // acumulador del filtro EMA para el canal Y de J1

/* === JOYSTICK 2 — segundo jugador (ver CABLEADO_JOYSTICK2.txt) ============
 * Utiliza el mismo esquema de filtro EMA y zona muerta que el joystick 1,
 * leido en los rangos 3 y 4 del mismo barrido del ADC1 (ver
 * ADC1_Joystick_Init / HAL_ADC_ConvCpltCallback). */
volatile uint16_t joystick2_x = 2048;  // ultima lectura filtrada del canal VRx2 de J2 (PC1/ADC1_IN11)
volatile uint16_t joystick2_y = 2048;  // ultima lectura filtrada del canal VRy2 de J2 (PC0/ADC1_IN10)

static uint8_t btn_modo_1p = 0;     /* 1 = solo el jugador logico 1 esta activo (el otro lado de la pantalla queda en blanco) */
static uint8_t guitar_modo_1p = 0;  /* 1 = solo el jugador logico 1 esta activo (el otro lado de la pantalla queda en blanco) */
static int32_t filtro_adc_x2 = 2048;   // acumulador EMA del canal X de J2
static int32_t filtro_adc_y2 = 2048;   // acumulador EMA del canal Y de J2

#define JOY_ZONA_MUERTA  150   // ancho de la banda "muerta" alrededor del centro donde se ignora el movimiento del filtro EMA (ver HAL_ADC_ConvCpltCallback); aumentar este valor hace el joystick menos sensible cerca del centro, disminuirlo lo hace mas propenso a ruido en reposo

/* CALIBRACION DE CENTRO: el joystick fisico de este montaje no descansa en
 * el valor 2048 (mitad teorica de la escala de 12 bits del ADC), sino en un
 * valor propio de cada unidad y cada eje, determinado por tolerancias
 * mecanicas y de fabricacion del potenciometro. Un centro fijo en 2048
 * haria que un eje casi nunca alcance el umbral de un lado (al tener que
 * recorrer casi toda la escala) mientras el otro lado dispara con
 * cualquier ruido (al estar ya cerca del umbral). Por ello, el centro de
 * cada eje se mide una vez al arrancar (ver Joystick_Calibrar en main(),
 * ejecutada tras HAL_ADC_Start_IT) y todos los umbrales de direccion se
 * calculan en relacion a ese centro medido, no a un valor fijo (ver
 * JOY_UMBRAL_DESVIO y Joy_Umbrales()). */
static uint16_t centro_j1x = 2048, centro_j1y = 2048;   // centro real medido de J1 al arrancar (ver calibracion en main()); inicia en 2048 (mitad teorica) hasta completar la medicion
static uint16_t centro_j2x = 2048, centro_j2y = 2048;   // idem para J2

/* El recorrido fisico de este joystick no es simetrico respecto al centro
 * medido: un lado de cada eje alcanza una distancia considerablemente mayor
 * que el lado opuesto antes de llegar al tope mecanico. Un umbral de
 * deteccion demasiado alto puede volver inalcanzable la direccion
 * correspondiente al lado con menor recorrido; el valor elegido deja
 * margen suficiente bajo el peor caso medido sin ser tan pequeño como para
 * disparar con el ruido normal del ADC en reposo. */
#define JOY_UMBRAL_DESVIO 280U   /* distancia (en cuentas del ADC) al centro medido necesaria para contar como una direccion valida */  // aumentar este valor exige un empuje mas fuerte del joystick para registrar una direccion (menor sensibilidad); disminuirlo dispara con empujes pequeños (mayor sensibilidad, mas riesgo de falsos positivos)

/* Rango de desviacion (en cuentas del ADC, a cada lado del centro medido)
 * que se considera "empuje a fondo" para el cursor visual "X" del modo de 2
 * jugadores (ver Joy_CursorEscala). Un poco mas generoso que
 * JOY_UMBRAL_DESVIO para dejar margen visible incluso antes de que la
 * direccion se registre como valida. */
#define JOY_CURSOR_RANGO 350U   // cuentas de desviacion del ADC que equivalen a "empuje a fondo" para el cursor visual -- bajar este numero hace que el cursor llegue al borde con un empuje mas chico del joystick

/* Convierte una lectura cruda del ADC (0-4095) y el centro calibrado de ESE
 * eje en un valor sintetico tambien 0-4095, pero centrado en 2048 (reposo)
 * y que llega a 0/4095 con un empuje de JOY_CURSOR_RANGO cuentas. Sin esto,
 * el cursor "X" del modo 2 jugadores (que mapea su entrada directamente
 * sobre la escala completa 0-4095 del ADC, ver Renderer_ActualizarCursor-
 * Joystick2P) queda descentrado en reposo -- el centro real de este
 * joystick esta lejos de 2048 (ver la calibracion mas arriba) -- y un
 * empuje real, al ser una fraccion chica de la escala completa, apenas lo
 * mueve unos pocos pixeles dentro de su caja, dando la impresion de que se
 * mueve para cualquier lado en vez de en linea recta hacia la direccion
 * empujada. */
static uint16_t Joy_CursorEscala(uint16_t valor, uint16_t centro) {   // reescala una lectura cruda del ADC (centrada en `centro`) a la escala sintetica 0-4095 centrada en 2048 que usa el cursor del menu de 2 jugadores
    int32_t dev = (int32_t)valor - (int32_t)centro;   // desviacion con signo respecto al centro medido de este eje
    int32_t s = 2048 - (dev * 2048) / (int32_t)JOY_CURSOR_RANGO;   // reescala esa desviacion para que +-JOY_CURSOR_RANGO caiga en 0/4095
    if (s < 0) s = 0;         // clamp por si el empuje supera el rango esperado
    if (s > 4095) s = 4095;   // clamp por el lado alto: nunca deja pasar de el maximo de una escala de 12 bits
    return (uint16_t)s;   // devuelve el valor ya reescalado y recortado a 0-4095
}

/* Calcula el rango [bajo,alto] alrededor de un centro medido, con clamp a
 * los limites reales del ADC de 12 bits (evita underflow/overflow si el
 * centro medido queda muy cerca de 0 o 4095). */
static void Joy_Umbrales(uint16_t centro, uint16_t *bajo, uint16_t *alto) {   // recibe el centro medido de UN eje y devuelve por puntero el rango [bajo,alto] "sin direccion" alrededor de ese centro
    int32_t b = (int32_t)centro - (int32_t)JOY_UMBRAL_DESVIO;   // limite inferior del rango, en 32 bits con signo para poder detectar si se fue negativo
    int32_t a = (int32_t)centro + (int32_t)JOY_UMBRAL_DESVIO;   // limite superior del rango, en 32 bits con signo para poder detectar si se paso de 4095
    *bajo = (uint16_t)(b < 0 ? 0 : b);      // si el centro esta muy cerca de 0, recorta a 0 en vez de dar la vuelta (underflow de un uint16_t)
    *alto = (uint16_t)(a > 4095 ? 4095 : a);   // si el centro esta muy cerca de 4095 (max de un ADC de 12 bits), recorta a 4095
}

/* === BOTONES ARCADE — 2 jugadores x 4 colores ==============================
 * Tablas indexadas por [jugador][color], donde color sigue el orden fijo
 * 0=ROJO 1=VERDE 2=AZUL 3=AMARILLO (ver board_pins.h para el cableado
 * fisico completo de cada pin). */
typedef struct { GPIO_TypeDef *port; uint16_t pin; } PinRef_t;   // par (puerto GPIO, numero de pin), usado para guardar pines en tablas indexables por [jugador][color]

/* Correspondencia entre jugador logico y hardware fisico: el jugador
 * logico 0 (mostrado en pantalla como "jugador 1") se controla con el
 * conjunto de pines fisico BTN2_*, y el jugador logico 1 con BTN1_* -- esta
 * es la unica tabla que requiere modificarse si el cableado fisico de los
 * botones cambia, ya que Botones_LeerColor / Boton_LED / ComboSalir_Detec-
 * tado y el resto de funciones indexan siempre por jugador logico, nunca
 * por el nombre BTN1/BTN2 directamente. Ver tambien JugadorFisico() mas
 * abajo para la correspondencia equivalente en el joystick, que al ser
 * variables globales sueltas (no una tabla) requiere invertir el indice en
 * vez de reordenar los datos. */
static const PinRef_t BTN_SW[2][4] = {              // pines de lectura (switch) de los botones arcade, indexados [jugador logico][color 0-3]
    { { BTN2_ROJO_SW_PORT, BTN2_ROJO_SW_PIN }, { BTN2_VERDE_SW_PORT, BTN2_VERDE_SW_PIN },      // fila [0]: jugador logico 0, hardware fisico BTN2_* (ver comentario de correspondencia logico/fisico arriba)
      { BTN2_AZUL_SW_PORT, BTN2_AZUL_SW_PIN }, { BTN2_AMARILLO_SW_PORT, BTN2_AMARILLO_SW_PIN } },  // columnas 2 y 3 de la fila [0]: azul y amarillo, hardware BTN2
    { { BTN1_ROJO_SW_PORT, BTN1_ROJO_SW_PIN }, { BTN1_VERDE_SW_PORT, BTN1_VERDE_SW_PIN },      // fila [1]: jugador logico 1, hardware fisico BTN1_*
      { BTN1_AZUL_SW_PORT, BTN1_AZUL_SW_PIN }, { BTN1_AMARILLO_SW_PORT, BTN1_AMARILLO_SW_PIN } }   // columnas 2 y 3 de la fila [1]: azul y amarillo, hardware BTN1
};

static const PinRef_t BTN_LED[2][4] = {              // pines de escritura (LED, a traves del driver ULN2003A) de los botones arcade, misma indexacion [jugador logico][color]
    { { BTN2_ROJO_LED_PORT, BTN2_ROJO_LED_PIN }, { BTN2_VERDE_LED_PORT, BTN2_VERDE_LED_PIN },   // fila [0]: LEDs del conjunto fisico BTN2 (jugador logico 0)
      { BTN2_AZUL_LED_PORT, BTN2_AZUL_LED_PIN }, { BTN2_AMARILLO_LED_PORT, BTN2_AMARILLO_LED_PIN } },  // columnas azul y amarillo del conjunto BTN2
    { { BTN1_ROJO_LED_PORT, BTN1_ROJO_LED_PIN }, { BTN1_VERDE_LED_PORT, BTN1_VERDE_LED_PIN },   // fila [1]: LEDs del conjunto fisico BTN1 (jugador logico 1)
      { BTN1_AZUL_LED_PORT, BTN1_AZUL_LED_PIN }, { BTN1_AMARILLO_LED_PORT, BTN1_AMARILLO_LED_PIN } }   // columnas azul y amarillo del conjunto BTN1
};

/* Correspondencia logico/fisico equivalente a la de arriba, aplicada al
 * joystick: como las lecturas joystick_x/y (J1) y joystick2_x/y (J2) son
 * variables globales sueltas y no una tabla indexable, se invierte el
 * indice justo antes de elegir cual leer, en vez de reordenar los datos.
 * Debe usarse unicamente en los puntos que leen hardware crudo del
 * joystick; la capa de renderizado sigue recibiendo el jugador logico sin
 * pasar por esta funcion, de modo que la disposicion en pantalla no se ve
 * afectada. */
static inline uint8_t JugadorFisico(uint8_t jugador_logico) {   // convierte jugador logico (0/1, el que muestra la pantalla) en jugador fisico (que joystick de hardware leer)
    return (uint8_t)(1U - jugador_logico);   // invierte el indice (0->1, 1->0); si el cableado fisico se intercambiara de nuevo, esta es la unica linea a modificar para el joystick
}

static GPIO_PinState btn_prev[2][4] = {   // ultimo nivel leido (sin confirmar aun como estable) de cada boton, [jugador][color], usado por Botones_LeerColor para detectar flancos
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },   // jugador 0: inicia en SET (activo en bajo, es decir, no presionado, con resistencia de pull-up)
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }    // jugador 1: idem
};

/* Antirrebote (debounce) por tiempo: exige que el nuevo nivel de un pin se
 * mantenga estable durante BTN_DEBOUNCE_MS antes de aceptarlo como una
 * pulsacion real, en lugar de comparar unicamente contra el nivel leido en
 * el ciclo anterior del bucle principal. Esto evita que un rebote mecanico
 * del contacto o un pico de ruido electrico de un solo ciclo se interprete
 * como una pulsacion o cambio de pantalla valido. */
#define BTN_DEBOUNCE_MS 30U   // milisegundos que un nivel nuevo debe mantenerse estable antes de contarlo como cambio real; aumentar este valor hace la respuesta de los botones mas lenta pero mas inmune a rebotes, disminuirlo la hace mas rapida pero mas propensa a falsos flancos
static GPIO_PinState btn_estable[2][4] = {   // ultimo nivel ya confirmado (estable) de cada boton, [jugador][color]
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },   // jugador 0, inicia sin ningun boton presionado
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }    // jugador 1, idem
};
static uint32_t btn_tick_cambio[2][4];   // tick (HAL_GetTick()) del ultimo cambio de nivel detectado por cada boton, usado para medir cuanto tiempo lleva estable

static void Boton_LED(uint8_t p, uint8_t c, uint8_t on) {   // enciende/apaga el LED del boton [jugador p][color c]
    if (btn_modo_1p || guitar_modo_1p) p = 0; // en modo de 1 jugador, los LEDs siempre corresponden al conjunto fisico del jugador logico 0, independientemente del jugador logico que se este actualizando
    HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);   // el ULN2003A es un driver en configuracion "sink", pero desde el GPIO se maneja como una salida normal: SET enciende, RESET apaga
}

/* Animacion de encendido secuencial tipo "tira LED": mantiene un unico LED
 * encendido a la vez, recorriendo los 8 botones (los 4 de J1 y luego los 4
 * de J2) en bucle mientras se muestra la pantalla de bienvenida. La
 * animacion no bloquea la ejecucion: avanza un paso por cada vuelta del
 * bucle principal (ver BotonesChase_Actualizar, invocada solo mientras
 * screen == DEMO_SPLASH) y se apaga por completo al salir de la pantalla
 * de bienvenida (ver BotonesChase_Detener). */
#define CHASE_LED_PASO_MS 180U   // duracion en milisegundos de cada LED encendido antes de pasar al siguiente; disminuir este valor acelera la animacion, aumentarlo la hace mas lenta
static uint32_t chase_led_tick   = 0;   // tick del ultimo paso de la animacion
static uint8_t  chase_led_idx    = 0;   // indice 0-7 del LED actualmente prendido (0-3=J1 rojo/verde/azul/amarillo, 4-7=J2)
static uint8_t  chase_led_activo = 0;   // 1 mientras la animacion esta corriendo (solo durante DEMO_SPLASH)

static void BotonesChase_Iniciar(void) {   // arranca la animacion desde cero
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs primero, por si quedo alguno prendido de antes
    chase_led_idx    = 0;              // vuelve a empezar por el primer LED (J1 rojo)
    chase_led_tick   = HAL_GetTick();  // marca el instante de arranque para el primer paso de tiempo
    chase_led_activo = 1;              // habilita BotonesChase_Actualizar() a partir de ahora
    Boton_LED(0, 0, 1);                // prende el primer LED (jugador 0, color 0 = rojo)
}

static void BotonesChase_Actualizar(void) {   // avanza un paso la animacion si ya paso CHASE_LED_PASO_MS; se llama una vez por vuelta del loop principal
    if (!chase_led_activo) return;   // si la animacion esta apagada, no hace nada
    uint32_t ahora = HAL_GetTick();   // tick actual en milisegundos desde el arranque
    if (ahora - chase_led_tick < CHASE_LED_PASO_MS) return;   // todavia no toca avanzar de LED
    chase_led_tick = ahora;   // guarda el tick de este paso para medir el proximo intervalo

    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 0);   // apaga el LED actual (division entera da el jugador, el resto da el color)
    chase_led_idx = (uint8_t)((chase_led_idx + 1) % 8);   // avanza al siguiente indice, dando la vuelta de 7 a 0 (bucle infinito de 8 LEDs)
    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 1);   // prende el nuevo LED actual
}

static void BotonesChase_Detener(void) {   // detiene la animacion y apaga todos los LEDs (se invoca al salir de la pantalla de bienvenida)
    if (!chase_led_activo) return;   // si ya estaba detenida, no hace nada (evita apagar LEDs que otro modo ya este utilizando)
    chase_led_activo = 0;   // deshabilita BotonesChase_Actualizar()
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs
}

/* Registro por consola de cada entrada detectada: permite verificar, sin
 * depender de observar la pantalla fisica, exactamente que boton o
 * direccion de joystick fue reconocida por el sistema. Se ubica en las
 * funciones de mas bajo nivel (Botones_LeerColor / Joystick*_LeerDireccion)
 * para que el registro se genere siempre que se detecta un flanco real,
 * sin importar desde que contexto se invoque la funcion (menu, partida en
 * curso, Guitar Hero), evitando duplicar el registro o interferir con la
 * deteccion del flanco. */
static const char *const COLOR_NOMBRE[4] = { "ROJO", "VERDE", "AZUL", "AMARILLO" };   // nombres para el registro por consola, en el mismo orden que el indice de color 0-3
static const char *const DIR_NOMBRE[4]   = { "ARRIBA", "ABAJO", "IZQUIERDA", "DERECHA" };   // idem para las direcciones de joystick 0-3

/* Devuelve el primer color con flanco de presion (activo en bajo, con
 * resistencia de pull-up interna) del jugador p, o 0xFF si ninguno de sus
 * botones tuvo un flanco de presion estable en este ciclo (ver
 * BTN_DEBOUNCE_MS para el criterio de estabilidad). */
static uint8_t Botones_LeerColor(uint8_t p) {   // p = jugador logico (0 o 1); recorre sus 4 botones y devuelve el primer flanco de presion encontrado
	if (btn_modo_1p || guitar_modo_1p) p = 0; // en modo de 1 jugador, la lectura de hardware siempre corresponde al conjunto fisico del jugador logico 0
	for (uint8_t c = 0; c < 4; c++) {   // recorre los 4 colores en el orden fijo ROJO, VERDE, AZUL, AMARILLO; si dos botones se presionan en el mismo ciclo, tiene prioridad el de menor indice
        GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_SW[p][c].port, BTN_SW[p][c].pin);   // lee el nivel crudo actual del pin de ese boton

        if (cur != btn_estable[p][c]) {   // el nivel cambio respecto al ultimo estado ya confirmado como estable
            btn_estable[p][c]    = cur;          // guarda el nuevo nivel como "candidato" a estable
            btn_tick_cambio[p][c] = HAL_GetTick();   // reinicia el cronometro de estabilidad para este boton
            continue;   /* posible rebote -- todavia no cuenta, seguir con el siguiente color */
        }
        if (HAL_GetTick() - btn_tick_cambio[p][c] < BTN_DEBOUNCE_MS) continue;  /* aun no esta estable */

        uint8_t flanco = (btn_prev[p][c] == GPIO_PIN_SET && cur == GPIO_PIN_RESET);   // flanco de bajada real: antes NO presionado (SET), ahora SI presionado (RESET, activo bajo)
        btn_prev[p][c] = cur;   // actualiza el "anterior" para la proxima llamada, sin importar si hubo flanco o no
        if (flanco) {   // solo entra aca en el instante del flanco de presion, no en cada vuelta que el boton siga presionado
            printf("[INPUT] boton J%u = %s\r\n", (unsigned)(p + 1), COLOR_NOMBRE[c]);   // log por consola: que jugador y que color se detecto (siempre que hay flanco real, sin excepcion)
            return c;   // devuelve el color presionado y CORTA el for -- no sigue buscando otros botones este mismo tick
        }
    }
    return 0xFF;   // ningun boton tuvo flanco de presion estable este tick
}

/* Variante de Botones_LeerColor SOLO para Guitar Hero a 1 jugador: en ese
 * modo las 4 notas caen sobre el mismo set fisico de 4 botones, asi que dos
 * notas de colores distintos pueden necesitar un golpe cada una dentro del
 * mismo tick de ~33ms. Botones_LeerColor corta en el primer flanco que
 * encuentra (return c) y deja SIN LEER los colores restantes ese mismo
 * tick -- ese flanco recien se detecta en el tick siguiente, a veces ya
 * fuera de la ventana de golpe (se sentia como "lag" o como si no marcara
 * el segundo golpe seguido). Esta version recorre los 4 colores completos
 * y devuelve un bitmask (bit c = 1 si el color c tuvo flanco de presion
 * estable este tick), usando el MISMO estado de antirrebote
 * (btn_estable/btn_prev/btn_tick_cambio) que Botones_LeerColor -- no se
 * llaman las dos funciones sobre el mismo jugador en el mismo tick, asi que
 * no hay conflicto por compartir ese estado. */
static uint8_t Botones_LeerColoresBitmask(uint8_t p) {
    if (btn_modo_1p || guitar_modo_1p) p = 0;
    uint8_t bitmask = 0;   // bit c en 1 = hubo flanco de presion estable de ese color este tick
    for (uint8_t c = 0; c < 4; c++) {   // a diferencia de Botones_LeerColor, SIEMPRE recorre los 4 colores (nunca corta antes)
        GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_SW[p][c].port, BTN_SW[p][c].pin);

        if (cur != btn_estable[p][c]) {
            btn_estable[p][c]    = cur;
            btn_tick_cambio[p][c] = HAL_GetTick();
            continue;
        }
        if (HAL_GetTick() - btn_tick_cambio[p][c] < BTN_DEBOUNCE_MS) continue;

        uint8_t flanco = (btn_prev[p][c] == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
        btn_prev[p][c] = cur;
        if (flanco) {
            //printf("[INPUT] boton J%u = %s\r\n", (unsigned)(p + 1), COLOR_NOMBRE[c]);
            bitmask = (uint8_t)(bitmask | (1u << c));   // marca este color y sigue revisando los demas (no corta el for)
        }
    }
    return bitmask;
}

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);     // configura el reloj del micro (HSI/PLL) -- generado, ver definicion mas abajo
static void MX_GPIO_Init(void);           // configura todos los pines GPIO (entradas de botones/joystick, salidas de LED/pantalla/buzzer)
static void MX_SPI1_Init(void);           // configura el periferico SPI1 para la pantalla
static void TIM3_ADCTrigger_Init(void);   // configura TIM3 para disparar el ADC cada 20ms (TRGO)
static void ADC1_Joystick_Init(void);     // configura el ADC1 en modo escaneo de 4 canales (joystick 1 y 2)
static void MX_TIM4_Buzzer_Init(void);    // configura TIM4 para la interrupcion periodica del buzzer
static void MX_USART2_UART_Init(void);    // configura USART2 (consola de depuracion por el VCP del ST-Link)

/* ========================================================================== */
/* === BUZZER — TONO GENERADO POR SOFTWARE (TIM4 + GPIO PA6) ================= */
/* ========================================================================== */
/* El tipo de zumbador (buzzer) montado en el hardware no esta confirmado de
 * antemano: puede ser activo (emite un tono fijo con solo aplicar corriente
 * continua) o pasivo (requiere una onda cuadrada de la frecuencia deseada
 * para sonar; ver prueba_de_sonido/ para la prueba de caracterizacion
 * dedicada). Por ello se maneja siempre con frecuencia variable: si el
 * zumbador es activo, sonara de todas formas (ignora la frecuencia externa
 * y usa su propio tono fijo); si es pasivo, este esquema permite reproducir
 * tonos y melodias reales en lugar de simples clics.
 *
 * El STM32F411 no cuenta con los temporizadores TIM13/TIM14, y el unico
 * temporizador con un canal disponible en el pin PA6 (TIM3_CH1) ya esta
 * ocupado por el disparador del ADC del joystick. Por esta razon el tono no
 * se genera por PWM de hardware: TIM4 (libre) produce una interrupcion
 * periodica que alterna el pin PA6 por software (HAL_GPIO_TogglePin dentro
 * de TIM4_IRQHandler); dos flancos de esa alternancia equivalen a un ciclo
 * completo de la onda cuadrada resultante.
 *
 * La frecuencia del reloj de TIM4 (TIM4CLK) es 16 MHz, ya que el sistema
 * usa el oscilador interno HSI sin multiplicacion por PLL. Con un
 * prescaler de 15, el temporizador cuenta a 1 MHz, de modo que el valor de
 * auto-recarga ARR = 1000000 / (2 * freq_hz) - 1 produce la alternancia del
 * pin a la frecuencia deseada. */
#define BUZZER_TIM_TICK_HZ 1000000U   // frecuencia del tick de TIM4 tras aplicar el prescaler (1 MHz equivale a 1 tick por microsegundo); modificar este valor solo si cambia el prescaler configurado del temporizador
#define BUZZER_TONO_HZ     4000U   /* frecuencia (en Hz) que usa Buzzer_Beep() para los
    efectos de sonido cortos. Se eligio 4000 Hz porque los zumbadores
    piezoelectricos pequeños, como el utilizado en este montaje, suelen
    tener su punto de mayor volumen (resonancia) en el rango aproximado de
    2.7 a 4 kHz, en lugar de frecuencias mas bajas como 2000 Hz. Para medir
    la frecuencia de resonancia real de una unidad especifica, puede
    habilitarse Buzzer_BarridoDiagnostico (ver BUZZER_DIAGNOSTICO_BARRIDO
    mas abajo). */   // cambiar este numero modifica el tono de todos los efectos de sonido cortos a la vez

typedef struct {   // un paso de una melodia/patron de SFX -- el arreglo completo de pasos es lo que reciben Buzzer_Patron/Buzzer_Fondo_Iniciar
    uint16_t freq_hz;   /* 0=silencio, otro=tono en Hz */
    uint16_t dur_ms;    // duracion de este paso en milisegundos
} PasoSonido_t;   // un "paso" de una melodia: una frecuencia sonando (o silencio) durante dur_ms

static void Buzzer_SetSalida(uint16_t freq_hz) {   // arranca/detiene el tono del buzzer a la frecuencia dada (0 = silencio)
    if (freq_hz == 0) {   // caso silencio: no hay que calcular ningun periodo de timer, solo apagar todo
        HAL_TIM_Base_Stop_IT(&htim4);   // detiene la interrupcion periodica de TIM4 -- deja de alternar el pin
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);   // fuerza el pin del buzzer a nivel bajo para que quede en silencio real (no a medio ciclo)
    } else {   // caso tono real: hay que reprogramar TIM4 para que alterne el pin a la frecuencia pedida
        uint32_t arr = (BUZZER_TIM_TICK_HZ / (2U * freq_hz)) - 1U;   // calcula el valor de auto-recarga (ARR) para que TIM4 desborde 2 veces por ciclo de la onda (2 flancos = 1 ciclo completo)
        HAL_TIM_Base_Stop_IT(&htim4);          // detiene el timer antes de reprogramarlo, para no dejarlo en un estado intermedio raro
        __HAL_TIM_SET_COUNTER(&htim4, 0);      // reinicia el contador del timer a 0
        __HAL_TIM_SET_AUTORELOAD(&htim4, arr);   // programa el nuevo periodo (ARR) calculado para la frecuencia pedida
        HAL_TIM_Base_Start_IT(&htim4);         // vuelve a arrancar el timer con interrupcion habilitada (la ISR alterna el pin, ver TIM4_IRQHandler en stm32f4xx_it.c)
    }
}

/* Apunta directamente al arreglo constante de la melodia en curso, sin
 * copiarlo: dado que las canciones pueden tener decenas de notas, resultaria
 * ineficiente mantener un buffer propio de tamaño fijo. */
static const PasoSonido_t *buzzer_pasos     = 0;   // puntero a la melodia/patron de SFX que esta sonando en primer plano ahora mismo (0 = ninguno) -- apuntar esto a otro arreglo (via Buzzer_Patron) es lo unico necesario para sonar un patron distinto
static uint16_t            buzzer_pasos_n   = 0;   // cantidad de pasos de esa melodia
static uint16_t            buzzer_pos       = 0;   // indice del paso actual dentro de buzzer_pasos
static uint32_t            buzzer_tick_paso = 0;   // tick en el que arranco el paso actual, para saber cuando pasar al siguiente

static void Buzzer_Patron(const PasoSonido_t *pasos, uint16_t n) {   // arranca un patron de SFX en primer plano (interrumpe cualquier SFX anterior)
    buzzer_pasos     = pasos;   // guarda el puntero al arreglo de pasos (no copia, debe vivir mientras suena)
    buzzer_pasos_n   = n;       // cuantos pasos tiene
    buzzer_pos       = 0;       // arranca desde el primer paso
    buzzer_tick_paso = HAL_GetTick();   // marca el instante de arranque de este paso
    Buzzer_SetSalida(buzzer_pasos[0].freq_hz);   // suena YA el primer paso (freq_hz==0 => arranca en silencio)
}

static void Buzzer_Beep(uint16_t duracion_ms) {   // sonido corto de un solo tono fijo (BUZZER_TONO_HZ) por duracion_ms
    static PasoSonido_t un_paso;   // static: sobrevive despues de que la funcion retorna, porque Buzzer_Patron guarda el PUNTERO (no una copia)
    un_paso.freq_hz = BUZZER_TONO_HZ;   // siempre el mismo tono para todos los beeps cortos
    un_paso.dur_ms  = duracion_ms;      // duracion pedida por el llamador
    Buzzer_Patron(&un_paso, 1);         // lo reproduce como un patron de un solo paso
}

/* Herramienta de diagnostico de volumen: un zumbador piezoelectrico suena
 * con mayor intensidad cerca de su frecuencia de resonancia propia, que no
 * necesariamente coincide con BUZZER_TONO_HZ. Esta funcion, cuando se
 * habilita, se ejecuta una unica vez al arrancar, antes de mostrar la
 * pantalla de bienvenida; su implementacion es bloqueante (usa HAL_Delay),
 * lo cual es aceptable porque se ejecuta antes de que exista logica de
 * juego en curso. El procedimiento consiste en escuchar el zumbador y
 * registrar, a partir del texto impreso por consola (UART, ver
 * MX_USART2_UART_Init), la frecuencia con mayor volumen percibido, para
 * luego fijar BUZZER_TONO_HZ a ese valor. */
#define BUZZER_DIAGNOSTICO_BARRIDO 0   // establecer en 1 hace que Buzzer_BarridoDiagnostico() se ejecute una vez al arrancar, antes de la pantalla de bienvenida; se mantiene disponible para volver a caracterizar el zumbador si fuera necesario
#if BUZZER_DIAGNOSTICO_BARRIDO   // el bloque completo entre este #if y el #endif de abajo solo se COMPILA si la constante de arriba esta en 1 -- en 0, Buzzer_BarridoDiagnostico ni siquiera existe en el binario final
static void Buzzer_BarridoDiagnostico(void) {   // reproduce un barrido de frecuencias para escuchar cual suena mas fuerte en ESTE buzzer especifico
    printf("\r\n[BUZZER] barrido de frecuencias -- escuchar y anotar cual suena mas fuerte\r\n");   // aviso por consola de que arranca el barrido
    for (uint16_t f = 200; f <= 5000; f = (uint16_t)(f + 200)) {   // recorre 200Hz a 5000Hz en pasos de 200Hz; cambiar estos numeros cambia el rango/resolucion del barrido
        printf("[BUZZER] %u Hz\r\n", f);   // imprime la frecuencia actual para poder anotarla junto a lo que se escucha
        Buzzer_SetSalida(f);   // suena esta frecuencia
        HAL_Delay(400);        // la mantiene sonando 400ms (bloqueante -- se acepta porque corre antes de que haya juego)
        Buzzer_SetSalida(0);   // apaga el tono
        HAL_Delay(120);        // pausa de silencio de 120ms antes de pasar a la siguiente frecuencia, para distinguir un paso del otro
    }
    printf("[BUZZER] fin del barrido\r\n\r\n");   // aviso de que termino todo el barrido
}
#endif   // cierra el bloque condicionado por BUZZER_DIAGNOSTICO_BARRIDO

/* Debe invocarse una vez por cada vuelta del bucle principal, sin importar
 * el estado del sistema. */
/* Musica de fondo: constituye un segundo canal logico independiente del
 * canal de efectos de sonido en primer plano (pitidos de acierto/fallo/fin
 * de ronda, jingle de bienvenida, previsualizacion en DEMO_MENU_CANCIONES).
 * Dado que el zumbador es monofonico (reproduce una unica nota a la vez),
 * mientras un efecto de sonido esta sonando, la musica de fondo permanece
 * "congelada" en su paso actual (su cronometro no avanza), y Buzzer_Actua-
 * lizar retoma esa misma nota apenas finaliza el efecto de sonido, evitando
 * perder un paso o reiniciar la cancion desde el principio. Ver
 * Buzzer_Fondo_Iniciar / Buzzer_Fondo_Detener mas abajo, una vez declaradas
 * las tablas CANCIONES_DATA / CANCIONES_LEN. */
static const PasoSonido_t *buzzer_fondo_pasos     = 0;   // puntero a la melodia de fondo actual (0 = ninguna todavia) -- lo cambia Buzzer_Fondo_Iniciar
static uint16_t            buzzer_fondo_pasos_n   = 0;   // cantidad de pasos de esa melodia
static uint16_t            buzzer_fondo_pos       = 0;   // indice del paso actual dentro de buzzer_fondo_pasos
static uint32_t            buzzer_fondo_tick_paso = 0;   // tick en que arranco el paso actual, para saber cuando avanzar al siguiente
static uint8_t             buzzer_fondo_activo    = 0;   // 1 mientras suena musica de fondo, 0 en silencio (splash, menus)

/* Definida mas abajo, junto a las tablas CANCIONES_DATA/CANCIONES_LEN (las
 * necesita para elegir la proxima cancion) -- se declara aca para que
 * Buzzer_Actualizar pueda invocarla sin reordenar todo el archivo. Rota el
 * repertorio de musica de fondo a una cancion aleatoria distinta cada vez
 * que la que esta sonando completa una vuelta entera, en vez de repetir
 * siempre la misma en loop (pedido explicito del usuario: "solo suena una
 * todo el tiempo"). */
static void Buzzer_Fondo_RotarSiTermino(void);   // prototipo -- la definicion completa esta mas abajo, junto a las tablas de canciones que necesita

static void Buzzer_Actualizar(void) {   // tick no bloqueante del buzzer: avanza el patron de SFX en curso y/o la musica de fondo, sin usar HAL_Delay
    if (buzzer_pos < buzzer_pasos_n) {   // hay un SFX en primer plano todavia sonando (no llego al final del arreglo)
        /* SFX en primer plano en curso */
        if (HAL_GetTick() - buzzer_tick_paso < buzzer_pasos[buzzer_pos].dur_ms) return;   // el paso actual del SFX todavia no cumplio su duracion -- no hace nada este tick
        buzzer_pos++;   // ya paso el tiempo de este paso: avanza al siguiente del SFX
        buzzer_tick_paso = HAL_GetTick();   // marca el instante de arranque del nuevo paso
        if (buzzer_pos < buzzer_pasos_n) {   // todavia quedan pasos del SFX por sonar
            Buzzer_SetSalida(buzzer_pasos[buzzer_pos].freq_hz);   // suena el siguiente paso
            return;   // termina aca este tick, no sigue a la musica de fondo
        }
        /* el SFX termino recien en este mismo tick -- retomar la musica de
         * fondo YA (sin esperar a que termine su paso actual, que quedo
         * congelado mientras sonaba el SFX encima). */
        Buzzer_SetSalida(buzzer_fondo_activo ? buzzer_fondo_pasos[buzzer_fondo_pos].freq_hz : 0);   // retoma la nota de fondo que quedo congelada (o silencio si no habia musica de fondo activa)
        return;   // termina el tick aca -- ya se atendio al SFX que acaba de terminar
    }

    /* sin SFX en curso: avanzar la musica de fondo normalmente (en loop) */
    if (!buzzer_fondo_activo) return;   // no hay musica de fondo activa, nada que hacer
    if (HAL_GetTick() - buzzer_fondo_tick_paso < buzzer_fondo_pasos[buzzer_fondo_pos].dur_ms) return;   // el paso actual todavia no termino su duracion

    buzzer_fondo_pos++;   // avanza al siguiente paso de la cancion
    if (buzzer_fondo_pos >= buzzer_fondo_pasos_n) {   // se paso del ultimo paso: en vez de repetir la misma cancion, rota a otra del repertorio
        Buzzer_Fondo_RotarSiTermino();   // elige una cancion nueva al azar (distinta de la actual) y deja buzzer_fondo_pos en 0
    }
    buzzer_fondo_tick_paso = HAL_GetTick();   // marca el instante de arranque del nuevo paso
    Buzzer_SetSalida(buzzer_fondo_pasos[buzzer_fondo_pos].freq_hz);   // suena la frecuencia de ese paso (0 = silencio, para las notas separadas por GAP_MS)
}

/* Jingle de bienvenida: arpegio ascendente Do-Mi-Sol-Do a lo largo de 3
 * octavas, composicion original para Beat Clash (ver
 * prueba_de_sonido/sonidos/beat_clash_welcome_jingle.ino). */
static const PasoSonido_t BEEP_BIENVENIDA[] = {   // melodia del jingle de bienvenida: pares {frecuencia_Hz, duracion_ms}; los {0,40} intermedios son silencios cortos entre notas para que se escuchen separadas
    { 262, 120 }, { 0, 40 }, { 330, 120 }, { 0, 40 }, { 392, 120 }, { 0, 40 },   // Do-Mi-Sol (arpegio ascendente), cada nota 120ms + 40ms de silencio
    { 523, 180 }, { 0, 40 }, { 659, 180 }, { 0, 40 }, { 784, 250 }, { 0, 40 },   // Do-Mi-Sol una octava arriba, notas mas largas (180-250ms) al acercarse al final
    { 1047, 400 }   // Do final, una octava mas arriba todavia, sostenido 400ms para cerrar el jingle
};
#define BEEP_BIENVENIDA_N (sizeof(BEEP_BIENVENIDA) / sizeof(BEEP_BIENVENIDA[0]))   // cantidad de pasos del arreglo, calculada automaticamente (no hay que actualizarla a mano si se agregan/quitan notas)

/* ========================================================================== */
/* === LISTA DE CANCIONES (pantalla "reproductor", ver DEMO_MENU_CANCIONES) == */
/* ========================================================================== */
/* Todas las melodias corresponden a obras de dominio publico (compositores
 * fallecidos hace mas de 70 años o folclor tradicional), transcritas en
 * prueba_de_sonido/; ver ese directorio para el detalle de fuentes y
 * frecuencias utilizadas. */

#define DO4   262   // frecuencia en Hz de la nota Do de la 4ta octava
#define RE4   294   // Re4
#define MI4   330   // Mi4
#define FA4   349   // Fa4
#define SOL4  392   // Sol4
#define LA4   440   // La4 (referencia estandar de afinacion, 440Hz)

#define DO5   523   // Do5, una octava arriba de DO4 (el doble de frecuencia)
#define RE5   587   // Re5
#define MI5   659   // Mi5
#define FA5   698   // Fa5
#define SOL5  784   // Sol5

#define NEGRA    220   // duracion en ms de una nota "negra" a este tempo fijo; cambiar este numero cambia el tempo de TODAS las canciones que usan NEGRA/CORCHEA/BLANCA
#define CORCHEA  110   // media negra (nota corta)
#define BLANCA   440   // el doble de una negra (nota larga)
#define GAP_MS    20    // silencio corto insertado por la macro N() entre nota y nota, para que no suenen "pegadas"
#define N(f, d)  { (f), (d) }, { 0, GAP_MS }   // macro de conveniencia: expande a DOS pasos, la nota (f,d) seguida de un silencio de GAP_MS -- asi cada llamado a N(...) en las canciones de abajo ya incluye su propio espacio

static const PasoSonido_t CANCION_ESTRELLITA[] = {   /* Estrellita donde estas */
    N(DO4, NEGRA),  N(DO4, NEGRA),  N(SOL4, NEGRA), N(SOL4, NEGRA),
    N(LA4, NEGRA),  N(LA4, NEGRA),  N(SOL4, BLANCA),
    N(FA4, NEGRA),  N(FA4, NEGRA),  N(MI4, NEGRA),  N(MI4, NEGRA),
    N(RE4, NEGRA),  N(RE4, NEGRA),  N(DO4, BLANCA),
    N(SOL4, NEGRA), N(SOL4, NEGRA), N(FA4, NEGRA),  N(FA4, NEGRA),
    N(MI4, NEGRA),  N(MI4, NEGRA),  N(RE4, BLANCA),
    N(SOL4, NEGRA), N(SOL4, NEGRA), N(FA4, NEGRA),  N(FA4, NEGRA),
    N(MI4, NEGRA),  N(MI4, NEGRA),  N(RE4, BLANCA),
    N(DO4, NEGRA),  N(DO4, NEGRA),  N(SOL4, NEGRA), N(SOL4, NEGRA),
    N(LA4, NEGRA),  N(LA4, NEGRA),  N(SOL4, BLANCA),
    N(FA4, NEGRA),  N(FA4, NEGRA),  N(MI4, NEGRA),  N(MI4, NEGRA),
    N(RE4, NEGRA),  N(RE4, NEGRA),  N(DO4, BLANCA),
};

static const PasoSonido_t CANCION_HIMNO[] = {   /* Himno de la Alegria, Beethoven */
    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(MI5, NEGRA), N(0, CORCHEA), N(RE5, CORCHEA), N(RE5, BLANCA),

    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(RE5, NEGRA), N(0, CORCHEA), N(DO5, CORCHEA), N(DO5, BLANCA),

    N(RE5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA), N(DO5, NEGRA),
    N(RE5, NEGRA), N(MI5, CORCHEA), N(FA5, CORCHEA), N(MI5, NEGRA), N(DO5, NEGRA),
    N(RE5, NEGRA), N(MI5, CORCHEA), N(FA5, CORCHEA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(RE5, NEGRA), N(SOL5, BLANCA),

    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(RE5, NEGRA), N(0, CORCHEA), N(DO5, CORCHEA), N(DO5, BLANCA),
};

static const PasoSonido_t CANCION_MARTINILLO[] = {   /* Fray Santiago / Frere Jacques */
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
};

static const PasoSonido_t CANCION_NAVIDAD[] = {   /* Jingle Bells, tradicional 1857 */
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(SOL5, CORCHEA), N(DO5, CORCHEA), N(RE5, CORCHEA), N(MI5, BLANCA),
    N(FA5, CORCHEA), N(FA5, CORCHEA), N(FA5, NEGRA), N(FA5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(RE5, CORCHEA), N(RE5, CORCHEA), N(MI5, CORCHEA), N(RE5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, CORCHEA), N(SOL5, CORCHEA), N(FA5, CORCHEA), N(RE5, CORCHEA), N(DO5, NEGRA),
};

/* Tetris (Korobeiniki, folclor ruso s.XIX), tempo=144 -- codigo de duracion
 * (4=negra,8=corchea,negativo=con puntillo), formula igual a BuzzerMelody.cpp */
#define TD(tempo, div) ((div) > 0 ? (240000 / (tempo)) / (div) \
                                  : ((240000 / (tempo)) / (-(div)) * 3) / 2)
#define NT(freq, tempo, div) N((freq), TD((tempo), (div)))
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880

static const PasoSonido_t CANCION_TETRIS[] = {
    NT(NOTE_E5,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_C5,144,8), NT(NOTE_B4,144,8),
    NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,-4), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_A4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8),

    NT(NOTE_D5,144,-4), NT(NOTE_F5,144,8), NT(NOTE_A5,144,4), NT(NOTE_G5,144,8), NT(NOTE_F5,144,8),
    NT(NOTE_E5,144,-4), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,4), NT(0,144,4),

    NT(NOTE_E5,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_C5,144,8), NT(NOTE_B4,144,8),
    NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,-4), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_A4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8),

    NT(NOTE_D5,144,-4), NT(NOTE_F5,144,8), NT(NOTE_A5,144,4), NT(NOTE_G5,144,8), NT(NOTE_F5,144,8),
    NT(NOTE_E5,144,-4), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,4), NT(0,144,4),

    NT(NOTE_E5,144,2), NT(NOTE_C5,144,2),
    NT(NOTE_D5,144,2), NT(NOTE_B4,144,2),
    NT(NOTE_C5,144,2), NT(NOTE_A4,144,2),
    NT(NOTE_GS4,144,2), NT(NOTE_B4,144,4), NT(0,144,8),
    NT(NOTE_E5,144,2), NT(NOTE_C5,144,2),
    NT(NOTE_D5,144,2), NT(NOTE_B4,144,2),
    NT(NOTE_C5,144,4), NT(NOTE_E5,144,4), NT(NOTE_A5,144,2),
    NT(NOTE_GS5,144,2),
};
#undef NT
#undef TD
#undef N

/* Canciones provistas por el profesor (prueba_de_sonido/sonidos, archivos .ino,
 * convertidas de MIDI a Arduino con https://github.com/ShivamJoker/MIDI-to-Arduino)
 * -- transcritas de forma mecanica y automatica (frecuencia+duracion+silencio de
 * cada nota, tal cual las entrego el profesor) a pares PasoSonido_t, mismo
 * formato que el resto del repertorio de arriba. */

static const PasoSonido_t CANCION_BOHEMIAN[] = {   /* Bohemian-Rhapsody-1.ino -- transcrita de prueba_de_sonido/sonidos/Bohemian-Rhapsody-1.ino */
    {294,394}, {392,402}, {0,9}, {233,394}, {0,17}, {196,86},
    {0,265}, {349,1524}, {0,1729}, {392,394}, {0,17}, {392,325},
    {0,86}, {466,146}, {0,265}, {523,308}, {0,103}, {622,360},
    {0,51}, {622,51}, {0,360}, {466,368}, {0,454}, {466,111},
    {0,300}, {392,420}, {0,402}, {466,522}, {0,711}, {349,771},
    {0,51}, {392,77}, {0,334}, {440,651}, {0,1404}, {494,351},
    {0,471}, {466,171}, {0,651}, {440,137}, {0,685}, {466,154},
    {0,668}, {494,342}, {0,479}, {466,205}, {0,616}, {440,163},
    {0,659}, {466,171}, {0,651}, {392,385}, {0,26}, {311,163},
    {0,248}, {622,283}, {0,128}, {311,77}, {0,334}, {349,368},
    {0,43}, {294,137}, {0,274}, {466,368}, {0,43}, {294,68},
    {0,342}, {466,402}, {0,9}, {349,539}, {0,283}, {311,385},
    {0,26}, {349,77}, {0,317}, {622,111}, {0,257}, {311,411},
    {0,822}, {233,394}, {0,17}, {233,411}, {175,385}, {0,17},
    {349,368}, {0,43}, {175,120}, {0,291}, {392,180}, {0,223},
    {175,248}, {0,163}, {784,146}, {0,265}, {698,154}, {0,257},
    {175,205}, {0,205}, {294,394}, {0,17}, {880,223}, {0,188},
    {196,402}, {0,9}, {784,103}, {0,308}, {196,103}, {0,308},
    {311,368}, {0,43}, {1175,120}, {0,291}, {262,402}, {0,9},
    {1047,86}, {0,325}, {262,163}, {0,248}, {392,231}, {0,180},
    {311,411}, {349,377}, {0,34}, {523,60}, {0,351}, {294,351},
    {0,60}, {784,103}, {0,308}, {698,77}, {0,334}, {175,120},
    {0,291}, {294,385}, {0,26}, {233,411}, {880,86}, {0,325},
    {196,411}, {784,77}, {0,334}, {196,60}, {0,351}, {392,385},
    {0,26}, {311,377}, {0,34}, {392,342}, {0,68}, {392,94},
    {0,317}, {392,111}, {0,300}, {311,402}, {0,9}, {392,240},
    {0,171}, {311,411}, {392,351}, {0,60}, {311,411}, {392,94},
    {0,317}, {311,240}, {0,171}, {622,368}, {0,43}, {392,154},
    {0,257}, {466,368}, {0,43}, {311,68}, {0,342}, {349,368},
    {0,43}, {233,103}, {0,308}, {392,385}, {0,26}, {311,342},
    {0,68}, {1175,77}, {0,334}, {262,394}, {0,17}, {1047,68},
    {0,342}, {262,68}, {0,342}, {415,385}, {0,26}, {349,137},
    {0,274}, {698,103}, {0,308}, {523,94}, {0,317}, {523,351},
    {0,60}, {311,128}, {0,283}, {294,60}, {0,351}, {349,300},
    {0,111}, {117,68}, {0,137}, {117,103}, {0,103}, {466,342},
    {0,68}, {117,94}, {0,111}, {117,94}, {0,111}, {294,317},
    {0,94}, {117,103}, {0,103}, {117,77}, {0,128}, {117,68},
    {0,137}, {117,68}, {0,342}, {392,368}, {0,43}, {311,180},
    {0,231}, {622,265}, {0,146}, {311,103}, {0,308}, {349,377},
    {0,34}, {294,128}, {0,283}, {466,325}, {0,86}, {294,68},
    {0,342}, {392,377}, {0,34}, {392,394}, {0,17}, {262,68},
    {0,342}, {311,873}, {0,745}, {466,385}, {0,9}, {392,325},
    {0,86}, {466,60}, {0,351}, {523,312}, {0,82}, {622,345},
    {0,49}, {466,387}, {0,8}, {622,321}, {0,74}, {415,429},
    {622,207}, {0,226}, {784,238}, {0,156}, {175,395}, {698,82},
    {0,312}, {175,148}, {0,247}, {784,58}, {0,337}, {175,345},
    {0,49}, {698,58}, {0,337}, {175,107}, {0,288}, {233,354},
    {0,41}, {784,164}, {0,230}, {175,345}, {0,49}, {698,345},
    {0,49}, {175,90}, {0,304}, {294,378}, {0,16}, {233,329},
    {0,66}, {880,206}, {0,189}, {196,395}, {784,206}, {0,189},
    {196,66}, {0,329}, {1175,90}, {0,304}, {1047,115}, {0,280},
    {262,395}, {392,387}, {0,8}, {311,329}, {0,66}, {392,107},
    {0,288}, {311,345}, {0,49}, {523,66}, {0,329}, {294,354},
    {0,41}, {233,362}, {0,33}, {294,395}, {784,74}, {0,321},
    {175,329}, {0,66}, {698,115}, {0,280}, {175,173}, {0,222},
    {294,395}, {880,148}, {0,247}, {196,337}, {0,58}, {784,148},
    {0,247}, {196,82}, {0,312}, {392,362}, {0,33}, {392,206},
    {0,189}, {311,387}, {0,8}, {392,132}, {0,263}, {311,387},
    {0,8}, {392,206}, {0,189}, {311,378}, {0,16}, {392,115},
    {0,280}, {392,329}, {0,66}, {392,74}, {0,321}, {311,395},
    {233,395}, {392,395}, {1175,148}, {0,247}, {1047,238}, {0,156},
    {262,395}, {415,378}, {0,16}, {349,123}, {0,271}, {698,66},
    {0,329}, {523,214}, {0,181}, {523,387}, {0,8}, {311,99},
    {0,296}, {294,66}, {0,329}, {294,329}, {0,66}, {117,82},
    {0,115}, {349,82}, {0,115}, {349,378}, {0,16}, {117,66},
    {0,132}, {294,74}, {0,123}, {294,214}, {0,181}, {117,66},
    {0,132}, {415,90}, {0,107}, {415,197}, {117,66}, {0,132},
    {415,66}, {0,329}, {392,354}, {0,41}, {233,173}, {0,222},
    {233,222}, {0,173}, {349,378}, {0,16}, {233,181}, {0,214},
    {233,99}, {0,296}, {262,395}, {415,354}, {0,41}, {349,148},
    {0,247}, {698,107}, {0,288}, {523,197}, {0,197}, {311,140},
    {0,255}, {294,140}, {0,255}, {349,329}, {0,66}, {117,99},
    {0,99}, {466,99}, {0,99}, {117,66}, {0,132}, {349,66},
    {0,132}, {349,321}, {0,74}, {117,66}, {0,132}, {415,90},
    {0,107}, {117,66}, {0,132}, {117,82}, {0,312}, {311,181},
    {0,214}, {466,362}, {0,33}, {311,66}, {0,329}, {294,263},
    {0,132}, {466,387}, {0,8}, {294,82}, {0,312}, {392,354},
    {0,41}, {311,345}, {0,49}, {262,395}, {262,189}, {0,206},
    {415,321}, {0,74}, {349,107}, {0,288}, {698,99}, {0,296},
    {523,189}, {0,206}, {311,123}, {0,271}, {294,82}, {0,312},
    {415,146}, {0,217}, {554,45}, {0,76}, {139,45}, {0,111},
    {131,40}, {0,121}, {554,116}, {0,126}, {415,287}, {0,197},
    {330,111}, {0,373}, {330,96}, {0,388}, {330,106}, {0,378},
    {330,96}, {0,388}, {330,91}, {0,393}, {330,86}, {0,398},
    {330,91}, {0,393}, {330,91}, {0,393}, {370,101}, {0,383},
    {330,86}, {0,156}, {330,86}, {0,156}, {311,106}, {0,378},
    {330,91}, {0,393}, {370,35}, {0,449}, {330,86}, {0,156},
    {330,91}, {0,151}, {311,86}, {0,398}, {330,86}, {0,156},
    {330,66}, {0,176}, {440,76}, {0,408}, {330,86}, {0,156},
    {330,76}, {0,166}, {440,76}, {0,408}, {330,96}, {0,146},
    {330,86}, {0,156}, {311,71}, {0,171}, {311,101}, {0,141},
    {330,111}, {0,373}, {370,101}, {0,383}, {330,106}, {0,378},
    {349,116}, {0,126}, {349,76}, {0,166}, {349,71}, {0,171},
    {349,81}, {0,161}, {311,181}, {0,302}, {311,106}, {0,378},
    {392,116}, {0,126}, {392,86}, {0,156}, {392,91}, {0,151},
    {392,76}, {0,166}, {415,181}, {0,302}, {415,91}, {0,393},
    {440,207}, {0,4148}, {220,86}, {0,156}, {220,186}, {0,55},
    {233,207}, {0,35}, {220,131}, {0,111}, {196,136}, {0,106},
    {175,111}, {0,131}, {165,86}, {0,2818}, {494,81}, {0,403},
    {466,91}, {0,393}, {440,66}, {0,418}, {466,96}, {0,388},
    {494,71}, {0,413}, {466,96}, {0,388}, {440,86}, {0,398},
    {466,96}, {0,388}, {523,192}, {0,292}, {466,116}, {0,126},
    {466,101}, {0,141}, {440,272}, {0,212}, {466,192}, {0,292},
    {523,116}, {0,126}, {523,96}, {0,146}, {466,176}, {0,307},
    {440,116}, {0,126}, {440,101}, {0,141}, {466,186}, {0,297},
    {523,282}, {0,202}, {523,101}, {0,141}, {523,86}, {0,156},
    {466,257}, {0,227}, {466,116}, {0,126}, {466,71}, {0,171},
    {440,297}, {0,186}, {440,86}, {0,156}, {440,81}, {0,161},
    {466,166}, {0,76}, {466,121}, {0,121}, {466,126}, {0,116},
    {466,171}, {0,71}, {523,232}, {0,10}, {622,192}, {0,50},
    {622,207}, {0,35}, {622,242}, {415,232}, {0,10}, {622,106},
    {0,136}, {494,86}, {0,398}, {466,91}, {0,393}, {440,45},
    {0,439}, {466,106}, {0,378}, {494,66}, {0,418}, {466,76},
    {0,408}, {440,55}, {0,428}, {233,116}, {0,368}, {311,96},
    {0,388}, {233,106}, {0,378}, {392,312}, {0,413}, {349,66},
    {0,176}, {349,91}, {0,151}, {392,66}, {0,176}, {415,96},
    {0,146}, {392,71}, {0,171}, {349,176}, {0,1275}, {233,106},
    {0,378}, {311,60}, {0,423}, {233,106}, {0,136}, {349,50},
    {0,192}, {349,96}, {0,146}, {392,71}, {0,171}, {415,81},
    {0,161}, {392,81}, {0,161}, {349,101}, {0,1351}, {233,121},
    {0,363}, {311,86}, {0,398}, {233,106}, {0,136}, {349,60},
    {0,181}, {349,111}, {0,131}, {392,76}, {0,166}, {415,101},
    {0,141}, {392,86}, {0,156}, {349,116}, {0,852}, {349,136},
    {0,106}, {392,96}, {0,146}, {415,126}, {0,116}, {392,60},
    {0,181}, {349,131}, {0,837}, {349,111}, {0,131}, {392,91},
    {0,151}, {415,96}, {0,146}, {392,96}, {0,146}, {349,141},
    {0,2762}, {370,171}, {0,312}, {440,131}, {0,353}, {440,141},
    {0,343}, {494,126}, {0,358}, {466,151}, {0,333}, {466,166},
    {0,318}, {466,166}, {0,2253}, {156,91}, {0,151}, {156,106},
    {0,136}, {208,161}, {0,81}, {156,101}, {0,141}, {147,106},
    {0,136}, {131,106}, {0,136}, {117,156}, {0,811}, {392,640},
    {0,86}, {392,146}, {0,96}, {415,297}, {0,186}, {415,247},
    {0,237}, {440,348}, {0,136}, {440,156}, {0,86}, {440,101},
    {0,141}, {466,348}, {0,136}, {466,282}, {0,202}, {117,71},
    {0,171}, {117,55}, {0,186}, {117,50}, {0,192}, {117,50},
    {0,192}, {117,30}, {0,212}, {117,40}, {0,202}, {117,35},
    {0,207}, {117,91}, {0,151}, {117,50}, {0,192}, {117,50},
    {0,192}, {117,40}, {0,202}, {117,50}, {0,192}, {117,35},
    {0,207}, {117,35}, {0,207}, {117,40}, {0,202}, {117,86},
    {0,156}, {117,50}, {0,192}, {117,45}, {0,197}, {117,30},
    {0,212}, {117,40}, {0,202}, {117,35}, {0,207}, {117,40},
    {0,202}, {117,45}, {0,197}, {117,76}, {0,166}, {117,45},
    {0,197}, {117,50}, {0,192}, {117,45}, {0,197}, {117,50},
    {0,192}, {117,60}, {0,181}, {117,307}, {0,50360}, {117,75},
    {0,96}, {131,75}, {0,96}, {117,71}, {0,99}, {131,82},
    {0,89}, {147,57}, {0,114}, {156,50}, {0,121}, {147,67},
    {0,103}, {156,71}, {0,99}, {175,78}, {0,92}, {196,71},
    {0,99}, {208,75}, {0,96}, {196,78}, {0,92}, {208,75},
    {0,96}, {233,64}, {0,107}, {262,53}, {0,117}, {233,53},
    {0,117}, {294,60}, {0,110}, {233,53}, {0,117}, {349,170},
    {0,259}, {233,357}, {0,71}, {392,384}, {0,45}, {233,304},
    {0,125}, {466,393}, {0,36}, {233,152}, {0,277}, {349,411},
    {0,18}, {233,161}, {0,268}, {466,330}, {0,98}, {233,89},
    {0,339}, {392,375}, {0,54}, {262,205}, {0,223}, {523,214},
    {0,214}, {262,89}, {0,339}, {392,491}, {0,366}, {392,348},
    {0,509}, {392,268}, {0,375}, {415,107}, {0,107}, {392,330},
    {0,527}, {466,348}, {0,509}, {466,277}, {0,580}, {587,357},
    {0,71}, {294,45}, {0,384}, {880,429}, {440,54}, {0,375},
    {466,420}, {0,9}, {294,125}, {0,304}, {587,250}, {0,179},
    {294,62}, {0,366}, {415,411}, {0,18}, {262,420}, {0,9},
    {415,384}, {0,45}, {311,62}, {0,366}, {466,393}, {0,36},
    {622,429}, {311,80}, {0,348}, {392,420}, {0,9}, {392,402},
    {0,27}, {262,71}, {0,357}, {294,402}, {0,27}, {392,321},
    {0,107}, {196,205}, {0,223}, {392,429}, {392,62}, {0,366},
    {294,366}, {0,62}, {233,366}, {0,62}, {294,62}, {0,312},
    {392,366}, {0,27}, {311,429}, {392,98}, {0,250}, {311,607},
    {0,1009}, {208,1598}, {0,116}, {392,420}, {0,9}, {392,420},
    {0,9}, {466,330}, {0,98}, {587,357}, {0,71}, {587,205},
    {0,223}, {392,411}, {0,18}, {466,80}, {0,348}, {523,420},
    {0,9}, {587,375}, {0,54}, {523,339}, {0,89}, {587,98},
    {0,286}, {466,223}, {0,134}, {466,429}, {523,411}, {0,18},
    {466,420}, {0,9}, {523,134}, {0,259}, {330,411}, {0,18},
    {392,107}, {0,321}, {415,196}, {0,232}, {415,223}, {0,205},
    {392,250}, {0,179}, {330,429}, {233,98}, {0,277}, {698,429},
    {0,71}, {87,65535},   /* la fuente trae 77205ms en esta ultima nota (F2, cola de silencio del conversor MIDI-to-Arduino) -- no entra en el uint16_t de PasoSonido_t.dur_ms (max 65535), se deja en el maximo representable en vez de truncarse silenciosamente a un valor incorrecto */
};

static const PasoSonido_t CANCION_STILLDRE[] = {   /* Dr Dre - Still Dre.ino -- transcrita de prueba_de_sonido/sonidos/Dr Dre - Still Dre.ino */
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {784,313},
    {784,313}, {784,313}, {784,313}, {784,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {784,313}, {784,313}, {784,313},
    {784,313}, {784,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {880,313}, {880,313}, {880,313}, {880,313}, {880,313},
    {880,313}, {784,313}, {784,313}, {784,313}, {784,313}, {784,2500},
};

static const PasoSonido_t CANCION_NARUTO[] = {   /* Naruto Shippuden - Naruto Shpippuuden Opening 9.ino -- transcrita de prueba_de_sonido/sonidos/Naruto Shippuden - Naruto Shpippuuden Opening 9.ino */
    {440,188}, {0,375}, {415,188}, {0,375}, {392,188}, {0,188},
    {311,563}, {311,563}, {988,94}, {988,94}, {740,94}, {740,94},
    {831,563}, {831,188}, {740,188}, {740,188}, {740,188}, {622,375},
    {740,938}, {988,94}, {988,94}, {740,94}, {740,94}, {831,563},
    {831,188}, {740,188}, {831,188}, {740,188}, {622,188}, {494,188},
    {0,188}, {466,188}, {0,188}, {370,188}, {0,188}, {988,94},
    {988,94}, {740,94}, {740,94}, {831,375}, {831,188}, {831,188},
    {932,375}, {932,188}, {784,188}, {1109,188}, {988,188}, {932,188},
    {988,188}, {370,188}, {415,188}, {622,188}, {1109,1500}, {932,375},
    {1480,94}, {1109,94}, {932,94}, {740,94}, {1109,94}, {932,94},
    {740,94}, {554,94}, {932,94}, {740,94}, {554,94}, {466,94},
    {740,375}, {494,375}, {466,188}, {466,188}, {494,188}, {740,375},
    {988,188}, {988,188}, {988,188}, {932,188}, {988,188}, {932,188},
    {988,188}, {494,188}, {415,188}, {415,188}, {622,188}, {554,375},
    {494,375}, {494,188}, {466,188}, {494,188}, {466,188}, {370,938},
    {415,188}, {415,188}, {622,188}, {554,375}, {494,375}, {494,188},
    {587,188}, {554,188}, {494,188}, {622,375}, {740,375}, {0,563},
    {415,188}, {415,188}, {415,375}, {415,188}, {494,375}, {466,188},
    {415,188}, {415,375}, {415,188}, {415,188}, {466,188}, {494,375},
    {466,375}, {415,188}, {370,375}, {415,375}, {0,1688}, {415,375},
    {415,375}, {370,188}, {415,375}, {370,188}, {415,563}, {311,188},
    {277,188}, {247,188}, {554,563}, {659,563}, {1109,375}, {370,188},
    {277,188}, {370,188}, {466,188}, {1109,188}, {932,188}, {740,188},
    {622,188}, {208,188}, {208,188}, {415,375}, {415,375}, {415,188},
    {494,375}, {466,188}, {415,188}, {494,750}, {0,188}, {208,188},
    {208,188}, {415,188}, {415,188}, {415,375}, {415,324}, {466,51},
    {494,375}, {466,188}, {415,750}, {0,188}, {208,188}, {208,188},
    {415,375}, {415,188}, {415,375}, {370,188}, {415,375}, {370,188},
    {415,375}, {622,188}, {554,188}, {494,188}, {554,563}, {659,563},
    {554,375}, {370,563}, {740,563}, {370,563}, {0,938}, {415,188},
    {415,188}, {370,188}, {370,375}, {311,375}, {277,375}, {247,375},
    {277,188}, {311,375}, {0,375}, {415,188}, {415,188}, {370,188},
    {370,375}, {370,375}, {311,188}, {370,188}, {494,188}, {415,750},
    {0,375}, {415,188}, {415,188}, {392,188}, {392,375}, {415,375},
    {311,188}, {277,188}, {247,375}, {277,188}, {311,188}, {311,188},
    {330,563}, {330,375}, {311,188}, {277,188}, {247,188}, {247,563},
    {659,375}, {208,188}, {208,188}, {311,188}, {277,281}, {0,281},
    {277,281}, {0,281}, {277,188}, {0,188}, {311,563}, {311,563},
    {988,94}, {988,94}, {740,94}, {740,94}, {831,563}, {831,188},
    {740,188}, {740,188}, {740,188}, {622,375}, {740,938}, {988,94},
    {988,94}, {740,94}, {740,94}, {831,563}, {831,188}, {740,188},
    {831,188}, {740,188}, {622,938}, {494,188}, {622,188}, {988,94},
    {988,94}, {740,94}, {740,94}, {831,375}, {831,188}, {831,188},
    {932,375}, {932,188}, {784,188}, {1109,188}, {988,188}, {932,188},
    {988,188}, {831,188}, {831,188}, {1245,188}, {1109,1500}, {932,750},
    {932,188}, {0,188}, {1109,188}, {0,188}, {740,375}, {494,188},
    {494,188}, {466,188}, {494,375}, {740,94}, {0,281}, {494,188},
    {494,188}, {494,188}, {466,188}, {494,188}, {466,188}, {494,188},
    {415,188}, {415,188}, {622,188}, {554,375}, {494,375}, {494,188},
    {466,188}, {494,188}, {466,188}, {370,938}, {415,188}, {415,188},
    {622,188}, {554,375}, {494,375}, {494,188}, {587,188}, {554,188},
    {494,188}, {622,375}, {392,188}, {740,188}, {392,188}, {740,375},
    {494,188}, {494,188}, {466,188}, {494,375}, {740,375}, {494,188},
    {494,188}, {494,188}, {466,188}, {494,188}, {466,188}, {494,188},
    {415,188}, {415,188}, {622,188}, {554,375}, {494,375}, {494,188},
    {587,188}, {554,188}, {494,188}, {622,375}, {740,563}, {415,1500},
    {622,750}, {415,375}, {494,375}, {622,375}, {831,375}, {988,375},
    {831,375}, {932,375}, {740,1500}, {554,375}, {494,375}, {466,375},
    {370,375}, {311,563}, {208,5763},
};

static const PasoSonido_t CANCION_ONEPIECE[] = {   /* One Piece - Bink's Sake.ino -- transcrita de prueba_de_sonido/sonidos/One Piece - Bink's Sake.ino */
    {233,156}, {0,10}, {262,150}, {0,10}, {294,168}, {294,119},
    {0,8}, {233,502}, {0,2}, {466,156}, {0,10}, {523,150},
    {0,10}, {587,168}, {587,495}, {698,494}, {0,2}, {932,512},
    {1865,989}, {0,2002}, {466,162}, {466,167}, {523,372}, {466,121},
    {0,10}, {466,495}, {233,1000}, {0,2}, {466,162}, {466,167},
    {523,372}, {466,121}, {0,10}, {233,512}, {233,495}, {233,500},
    {0,1}, {233,510}, {0,3}, {466,162}, {466,167}, {523,367},
    {466,126}, {0,2}, {466,495}, {233,495}, {165,494}, {0,2},
    {466,162}, {466,167}, {523,367}, {466,119}, {0,10}, {156,512},
    {233,495}, {233,1000}, {0,2}, {392,119}, {0,8}, {349,367},
    {311,120}, {0,8}, {294,119}, {0,8}, {208,502}, {0,2},
    {392,119}, {0,8}, {311,120}, {0,8}, {311,118}, {0,8},
    {196,494}, {0,2}, {311,119}, {0,8}, {196,495}, {294,118},
    {0,8}, {415,119}, {0,10}, {392,119}, {0,8}, {311,120},
    {0,8}, {117,495}, {175,494}, {0,2}, {392,119}, {0,8},
    {349,367}, {311,120}, {0,8}, {294,119}, {0,8}, {208,502},
    {0,2}, {392,119}, {0,8}, {311,120}, {0,8}, {311,118},
    {0,8}, {196,494}, {0,2}, {311,119}, {0,8}, {196,495},
    {294,118}, {0,8}, {415,119}, {0,10}, {311,119}, {0,8},
    {349,120}, {0,8}, {156,498}, {233,502}, {0,2}, {784,119},
    {0,8}, {698,367}, {622,120}, {0,8}, {587,119}, {0,8},
    {208,502}, {0,2}, {784,119}, {0,8}, {622,120}, {0,8},
    {622,118}, {0,8}, {196,494}, {0,2}, {622,119}, {0,8},
    {196,495}, {587,118}, {0,8}, {831,119}, {0,10}, {784,119},
    {0,8}, {622,120}, {0,8}, {117,495}, {175,494}, {0,2},
    {784,119}, {0,8}, {698,367}, {622,120}, {0,8}, {587,119},
    {0,8}, {208,502}, {0,2}, {784,119}, {0,8}, {622,120},
    {0,8}, {622,118}, {0,8}, {196,494}, {0,2}, {622,119},
    {0,8}, {196,495}, {587,118}, {0,8}, {831,119}, {0,10},
    {622,119}, {0,8}, {698,120}, {0,8}, {156,498}, {233,502},
    {0,2}, {466,162}, {466,167}, {523,372}, {466,129}, {0,2},
    {466,495}, {233,1000}, {0,2}, {466,162}, {466,167}, {523,372},
    {466,121}, {0,10}, {233,512}, {233,495}, {233,500}, {0,1},
    {233,510}, {0,3}, {466,162}, {466,167}, {523,367}, {466,126},
    {0,2}, {466,495}, {131,495}, {165,494}, {0,2}, {466,162},
    {466,167}, {523,367}, {466,119}, {0,10}, {156,512}, {233,495},
    {1245,495},
};

static const PasoSonido_t CANCION_PIRATES[] = {   /* Pirates Of The Caribbean - Davy Jones.ino -- transcrita de prueba_de_sonido/sonidos/Pirates Of The Caribbean - Davy Jones.ino */
    {147,1492}, {0,8}, {165,742}, {0,8}, {147,1492}, {0,8},
    {165,742}, {0,8}, {147,1492}, {0,8}, {392,742}, {0,8},
    {349,1492}, {0,758}, {349,1492}, {0,8}, {392,742}, {0,8},
    {466,367}, {0,8}, {392,367}, {0,8}, {349,1492}, {0,8},
    {330,742}, {0,8}, {294,1680}, {0,570}, {392,1492}, {0,8},
    {440,742}, {0,8}, {349,1492}, {0,8}, {294,742}, {0,8},
    {330,1492}, {0,8}, {196,742}, {0,8}, {147,1492}, {0,758},
    {131,742}, {0,8}, {110,1492}, {0,758}, {294,1492}, {0,758},
    {349,1492},
};

static const PasoSonido_t CANCION_TOKYOGHOUL[] = {   /* Tokyo Ghoul - Unravel.ino -- transcrita de prueba_de_sonido/sonidos/Tokyo Ghoul - Unravel.ino */
    {932,416}, {0,1}, {1047,416}, {0,1}, {932,416}, {0,1},
    {932,207}, {0,1}, {784,207}, {0,209}, {1047,416}, {0,1},
    {932,416}, {0,1}, {880,416}, {0,1}, {784,416}, {0,1},
    {784,207}, {0,1}, {698,207}, {0,418}, {698,207}, {0,1},
    {622,416}, {0,1}, {698,207}, {0,1}, {587,1041}, {0,626},
    {587,207}, {0,1}, {587,416}, {0,1}, {587,207}, {0,1},
    {587,416}, {0,1}, {1047,207}, {0,1}, {1047,416}, {0,1459},
    {932,207}, {0,1}, {880,416}, {0,1}, {880,207}, {0,1},
    {880,416}, {0,1}, {932,416}, {0,1}, {932,416}, {0,1251},
    {932,207}, {0,1}, {1047,416}, {0,1}, {932,416}, {0,1},
    {880,207}, {0,1}, {784,207}, {0,209}, {1047,416}, {0,1},
    {932,416}, {0,1}, {880,416}, {0,1}, {784,416}, {0,1},
    {784,207}, {0,1}, {698,416}, {0,209}, {698,207}, {0,1},
    {622,416}, {0,1}, {698,207}, {0,1}, {587,832}, {0,834},
    {587,207}, {0,1}, {587,416}, {0,1}, {587,207}, {0,1},
    {587,416}, {0,1}, {1047,207}, {0,1}, {1047,416}, {0,1459},
    {932,207}, {0,1}, {880,416}, {0,1}, {880,207}, {0,1},
    {880,416}, {0,1}, {932,416}, {0,1}, {932,207}, {0,1},
    {587,103}, {0,1}, {587,103}, {0,105}, {587,103}, {0,105},
    {587,103}, {0,1}, {587,103}, {0,105}, {587,103}, {0,1},
    {587,103}, {0,105}, {587,103}, {0,105}, {587,103}, {0,1},
    {587,103}, {0,105}, {587,103}, {0,1}, {392,103}, {0,1},
    {392,103}, {0,1}, {440,103}, {0,1}, {392,103}, {0,1},
    {392,103}, {0,1}, {466,103}, {0,1}, {440,103}, {0,1},
    {587,103}, {0,1}, {392,103}, {0,1}, {392,103}, {0,1},
    {440,103}, {0,1}, {392,103}, {0,1}, {392,103}, {0,1},
    {440,103}, {0,1}, {392,103}, {0,1}, {587,103}, {0,1},
    {587,103}, {0,105}, {587,103}, {0,105}, {587,103}, {0,1},
    {587,103}, {0,105}, {587,103}, {0,1}, {587,103}, {0,105},
    {587,103}, {0,105}, {587,103}, {0,1}, {587,103}, {0,105},
    {587,103}, {0,1}, {932,103}, {0,105}, {587,103}, {0,1},
    {784,207}, {0,1}, {466,103}, {0,1}, {466,103}, {0,1},
    {523,103}, {0,1}, {466,103}, {0,1}, {784,103}, {0,1},
    {466,103}, {0,1}, {466,103}, {0,1}, {698,103}, {0,1},
    {466,207}, {0,1}, {466,103}, {0,1}, {932,103}, {0,105},
    {587,103}, {0,1}, {784,207}, {0,1}, {466,103}, {0,1},
    {466,103}, {0,1}, {523,103}, {0,1}, {466,103}, {0,1},
    {784,103}, {0,1}, {466,103}, {0,1}, {466,103}, {0,1},
    {784,103}, {0,1}, {466,207}, {0,1}, {466,103}, {0,1},
    {932,103}, {0,105}, {587,103}, {0,1}, {784,207}, {0,1},
    {466,103}, {0,1}, {466,103}, {0,1}, {523,103}, {0,1},
    {466,103}, {0,1}, {784,103}, {0,1}, {466,103}, {0,1},
    {466,103}, {0,1}, {784,103}, {0,1}, {466,207}, {0,1},
    {466,103}, {0,1}, {932,103}, {0,105}, {587,103}, {0,1},
    {784,207}, {0,1}, {466,103}, {0,1}, {466,103}, {0,1},
    {523,103}, {0,1}, {466,103}, {0,1}, {784,103}, {0,1},
    {466,103}, {0,1}, {466,103}, {0,1}, {784,103}, {0,1},
    {466,207}, {0,1}, {698,103}, {0,1}, {784,103}, {0,105},
    {622,103}, {0,209}, {880,103}, {0,105}, {698,103}, {0,1},
    {784,103}, {0,105}, {622,103}, {0,209}, {880,207}, {0,1},
    {932,312}, {0,1}, {932,312}, {0,1}, {932,207}, {0,418},
    {932,207}, {0,1}, {1175,207}, {0,1}, {1175,312}, {0,1},
    {1047,312}, {0,1}, {1047,207}, {0,626}, {932,207}, {0,1},
    {1047,312}, {0,1}, {932,312}, {0,1}, {880,416}, {0,1},
    {698,416}, {0,1}, {587,207}, {0,1459}, {880,207}, {0,1},
    {932,207}, {0,1}, {932,103}, {0,1}, {932,416}, {0,313},
    {932,207}, {0,209}, {1175,207}, {0,1}, {1175,312}, {0,1},
    {1047,312}, {0,1}, {1047,416}, {0,1}, {932,207}, {0,418},
    {1175,416}, {0,1}, {1047,207}, {0,1}, {1047,624}, {0,1},
    {880,207}, {0,1}, {932,207}, {0,1459}, {1397,207}, {0,1},
    {1397,207}, {0,1}, {1175,103}, {0,1}, {1175,103}, {0,209},
    {1175,207}, {0,1}, {1047,207}, {0,1}, {1175,103}, {0,1},
    {1175,103}, {0,209}, {1397,207}, {0,1}, {1397,207}, {0,1},
    {1175,103}, {0,1}, {1175,103}, {0,209}, {1175,207}, {0,1},
    {1047,207}, {0,1}, {1175,103}, {0,1}, {1175,103}, {0,209},
    {1397,207}, {0,1}, {1397,207}, {0,1}, {1175,103}, {0,1},
    {1175,103}, {0,209}, {1175,207}, {0,1}, {1047,312}, {0,1},
    {1047,312}, {0,1}, {1047,207}, {0,1}, {1175,624}, {0,1},
    {1175,207}, {0,1}, {1175,312}, {0,1}, {1047,312}, {0,1},
    {1047,207}, {0,1}, {1175,312}, {0,1}, {1047,312}, {0,1},
    {1047,207}, {0,1}, {1047,312}, {0,1}, {932,312}, {0,1},
    {932,207}, {0,1}, {880,312}, {0,1}, {932,312}, {0,1},
    {880,416}, {0,1}, {698,416}, {0,1}, {698,207}, {0,1},
    {1175,312}, {0,1}, {1047,312}, {0,1}, {1047,207}, {0,1},
    {1047,312}, {0,1}, {932,312}, {0,1}, {932,207}, {0,1},
    {880,312}, {0,1}, {932,312}, {0,1}, {1397,416}, {0,1},
    {880,416}, {0,1}, {880,207}, {0,1}, {1568,312}, {0,1},
    {1397,312}, {0,1}, {1397,207}, {0,1}, {1397,312}, {0,1},
    {1175,312}, {0,1}, {932,207}, {0,1}, {932,312}, {0,1},
    {880,312}, {0,1}, {784,416}, {0,1}, {880,416}, {0,1},
    {932,832}, {0,834}, {932,207}, {0,1}, {880,312}, {0,1},
    {932,312}, {0,1}, {880,416}, {0,1}, {698,416}, {0,209},
    {1175,312}, {0,1}, {1047,312}, {0,1}, {1047,207}, {0,1},
    {1047,312}, {0,1}, {932,312}, {0,1}, {932,207}, {0,1},
    {880,312}, {0,1}, {932,312}, {0,1}, {880,416}, {0,1},
    {698,416}, {0,1}, {698,207}, {0,1}, {1175,312}, {0,1},
    {1047,312}, {0,1}, {1047,207}, {0,1}, {1047,312}, {0,1},
    {932,312}, {0,1}, {932,207}, {0,1}, {880,312}, {0,1},
    {932,312}, {0,1}, {1397,416}, {0,1}, {880,416}, {0,1},
    {880,207}, {0,1}, {1568,312}, {0,1}, {1397,312}, {0,1},
    {1397,207}, {0,1}, {1397,312}, {0,1}, {1175,312}, {0,1},
    {932,207}, {0,1}, {932,312}, {0,1}, {880,312}, {0,1},
    {784,416}, {0,1}, {880,416}, {0,1}, {932,1874},
};

typedef enum { CANCION_BIENVENIDA_IDX = 0, CANCION_ESTRELLITA_IDX, CANCION_HIMNO_IDX,   // indices del repertorio completo de canciones -- el orden debe coincidir con CANCIONES_DATA/LEN mas abajo y con RLC_NOMBRE en renderer.c
               CANCION_MARTINILLO_IDX, CANCION_NAVIDAD_IDX, CANCION_TETRIS_IDX,   // repertorio original (temas de dominio publico)
               CANCION_BOHEMIAN_IDX, CANCION_STILLDRE_IDX, CANCION_NARUTO_IDX,   // temas provistos por el profesor (ver prueba_de_sonido/sonidos/*.ino)
               CANCION_ONEPIECE_IDX, CANCION_PIRATES_IDX, CANCION_TOKYOGHOUL_IDX,   // resto de los temas del profesor
               CANCIONES_N } CancionIdx_t;   // CANCIONES_N = cantidad total, calculada sola por el compilador (siempre el ultimo valor del enum) -- agregar una cancion nueva no requiere actualizar este numero a mano

/* Los nombres para mostrar en pantalla viven en renderer.c (RLC_NOMBRE) --
 * el ORDEN debe coincidir exactamente con este arreglo. */
static const PasoSonido_t *const CANCIONES_DATA[CANCIONES_N] = {   // tabla de punteros a cada arreglo de notas, indexada por CancionIdx_t -- agregar una cancion nueva implica sumarla aca EN EL MISMO ORDEN que el enum
    BEEP_BIENVENIDA, CANCION_ESTRELLITA, CANCION_HIMNO, CANCION_MARTINILLO, CANCION_NAVIDAD, CANCION_TETRIS,   // repertorio original
    CANCION_BOHEMIAN, CANCION_STILLDRE, CANCION_NARUTO, CANCION_ONEPIECE, CANCION_PIRATES, CANCION_TOKYOGHOUL   // temas del profesor
};
static const uint16_t CANCIONES_LEN[CANCIONES_N] = {   // cantidad de pasos de cada cancion, calculada automaticamente con sizeof -- no hay que contar notas a mano ni actualizar esto si una cancion cambia de largo
    BEEP_BIENVENIDA_N,   // el jingle ya tiene su propia macro N, calculada mas arriba junto al arreglo
    (uint16_t)(sizeof(CANCION_ESTRELLITA)  / sizeof(CANCION_ESTRELLITA[0])),   // bytes totales del arreglo / bytes de 1 elemento = cantidad de elementos
    (uint16_t)(sizeof(CANCION_HIMNO)       / sizeof(CANCION_HIMNO[0])),
    (uint16_t)(sizeof(CANCION_MARTINILLO)  / sizeof(CANCION_MARTINILLO[0])),
    (uint16_t)(sizeof(CANCION_NAVIDAD)     / sizeof(CANCION_NAVIDAD[0])),
    (uint16_t)(sizeof(CANCION_TETRIS)      / sizeof(CANCION_TETRIS[0])),
    (uint16_t)(sizeof(CANCION_BOHEMIAN)    / sizeof(CANCION_BOHEMIAN[0])),
    (uint16_t)(sizeof(CANCION_STILLDRE)    / sizeof(CANCION_STILLDRE[0])),
    (uint16_t)(sizeof(CANCION_NARUTO)      / sizeof(CANCION_NARUTO[0])),
    (uint16_t)(sizeof(CANCION_ONEPIECE)    / sizeof(CANCION_ONEPIECE[0])),
    (uint16_t)(sizeof(CANCION_PIRATES)     / sizeof(CANCION_PIRATES[0])),
    (uint16_t)(sizeof(CANCION_TOKYOGHOUL)  / sizeof(CANCION_TOKYOGHOUL[0])),
};

static uint8_t  buzzer_fondo_idx  = CANCION_TETRIS_IDX;   // indice (CancionIdx_t) de la cancion de fondo sonando ahora mismo -- se usa solo para elegir la siguiente al rotar el repertorio, ver Buzzer_Fondo_RotarSiTermino
static uint32_t buzzer_repertorio_seed = 2463534242u;      // semilla propia del LCG que elige la proxima cancion de fondo al azar (independiente de las semillas de secuencias de juego)

/* Arranca la musica de fondo con la cancion `cancion_idx` (loop continuo,
 * ver Buzzer_Actualizar para como convive con los SFX en primer plano). */
static void Buzzer_Fondo_Iniciar(uint8_t cancion_idx) {   // arranca la musica de fondo en loop con la cancion `cancion_idx` del repertorio (ver CANCIONES_DATA)
    buzzer_fondo_idx       = cancion_idx;   // recuerda que cancion es la actual, para que Buzzer_Fondo_RotarSiTermino sepa cual NO repetir
    buzzer_fondo_pasos     = CANCIONES_DATA[cancion_idx];   // apunta al arreglo de notas de la cancion elegida
    buzzer_fondo_pasos_n   = CANCIONES_LEN[cancion_idx];   // cuantos pasos tiene esa cancion
    buzzer_fondo_pos       = 0;   // arranca desde el primer paso
    buzzer_fondo_tick_paso = HAL_GetTick();   // marca el instante de arranque de ese primer paso
    buzzer_fondo_activo    = 1;   // habilita a Buzzer_Actualizar a avanzarla en cada tick
    if (buzzer_pos >= buzzer_pasos_n) Buzzer_SetSalida(buzzer_fondo_pasos[0].freq_hz);   // si no hay un SFX en curso, suena YA la primera nota (si hay un SFX sonando, Buzzer_Actualizar la retomara sola al terminar)
}

static void Buzzer_Fondo_Detener(void) {   // corta la musica de fondo y silencia el buzzer si no hay un SFX corto sonando encima
    buzzer_fondo_activo = 0;   // apaga la bandera -- Buzzer_Actualizar deja de avanzarla
    if (buzzer_pos >= buzzer_pasos_n) Buzzer_SetSalida(0);   // si no hay un SFX en curso que la tape, silencia el buzzer ya mismo
}

/* Rota el repertorio de musica de fondo: elige al azar una cancion distinta
 * de la que acaba de terminar su vuelta (excluyendo BEEP_BIENVENIDA, indice
 * 0, que es el jingle corto de la pantalla de bienvenida y no una cancion de
 * fondo) y la deja lista desde su primer paso. Se invoca UNICAMENTE desde
 * Buzzer_Actualizar cuando buzzer_fondo_pos se pasa del final del arreglo, de
 * modo que la rotacion ocurre sola con el tiempo (cada vez que una cancion
 * completa su vuelta) sin tocar ningun punto de arranque de partida --
 * TODAS las partidas siguen empezando en CANCION_TETRIS_IDX como hasta
 * ahora, y a partir de ahi el repertorio se mezcla solo. */
static void Buzzer_Fondo_RotarSiTermino(void) {   // ver comentario de arriba: cambia de cancion sola cuando la actual completa una vuelta
    buzzer_repertorio_seed = buzzer_repertorio_seed * 1103515245u + 12345u;   // mismo LCG que el resto del proyecto, semilla propia
    uint8_t nuevo = (uint8_t)(1u + ((buzzer_repertorio_seed >> 16) % (CANCIONES_N - 1)));   // 1..CANCIONES_N-1: salta el indice 0 (jingle de bienvenida)
    if (nuevo == buzzer_fondo_idx) nuevo = (uint8_t)(1u + (nuevo % (CANCIONES_N - 1)));   // evita repetir la misma cancion 2 veces seguidas
    buzzer_fondo_idx      = nuevo;   // recuerda la nueva cancion actual
    buzzer_fondo_pasos    = CANCIONES_DATA[nuevo];   // apunta al arreglo de notas de la cancion nueva
    buzzer_fondo_pasos_n  = CANCIONES_LEN[nuevo];   // cuantos pasos tiene
    buzzer_fondo_pos      = 0;   // arranca desde el primer paso de la cancion nueva
}

/* ========================================================================== */
/* === RECORRIDO DE PANTALLAS DE DISEÑO ====================================== */
/* ========================================================================== */

typedef enum {   // cada valor es una pantalla del recorrido -- agregar una pantalla nueva implica sumarla aca Y en DEMO_NOMBRE (mismo orden) mas abajo
    DEMO_SPLASH = 0,
    DEMO_JUGADORES_1,       /* seleccion de jugadores, cursor en "1 JUGADOR"   */
    DEMO_JUGADORES_2,       /* seleccion de jugadores, cursor en "2 JUGADORES" */
    DEMO_INICIALES,         /* jugador(es) eligen su nombre de 3 letras        */
    DEMO_MODO_SIMON,        /* seleccion de modo, cursor en "SIMON"            */
    DEMO_MODO_SIMONJOY,     /* seleccion de modo, cursor en "SIM+JOY"          */
    DEMO_MODO_GUITAR,       /* seleccion de modo, cursor en "GT HERO"          */
    DEMO_PREVIEW_SIMON,     /* vista previa del modo Simon Clasico             */
    DEMO_PREVIEW_SIMONJOY,  /* vista previa del modo Simon + Joystick          */
    DEMO_CONTEO_3,          // primer numero del conteo regresivo 3-2-1-GO antes de empezar a jugar de verdad
    DEMO_CONTEO_2,          // segundo numero del conteo
    DEMO_CONTEO_1,          // tercer numero del conteo
    DEMO_CONTEO_GO,         // ultimo paso del conteo ("GO"), justo antes de arrancar el modo elegido
    DEMO_JUGANDO,           /* modo Guitar Hero (ya construido)                */
    DEMO_RESULTADO,         // pantalla de resultado/game over del recorrido de diseño (no la real de cada modo, ver *_DibujarGameOver)
    DEMO_MENU_DIFICULTAD,   /* seleccion de dificultad (FACIL/MEDIO/PRO), SOLO para Guitar Hero, justo despues de confirmar el modo */
    DEMO_MENU_CANCIONES,    /* lista de canciones tipo "reproductor"           */
    DEMO_COUNT              // cantidad total de pantallas -- SIEMPRE debe quedar ultima en el enum, se usa para dimensionar DEMO_NOMBRE[]
} DemoScreen_t;   // tipo del estado de pantalla actual, usado por la variable `screen` del bucle principal en main()

/* Nombres para el log por consola (ver MX_USART2_UART_Init) -- el orden
 * debe coincidir exactamente con DemoScreen_t. */
static const char *const DEMO_NOMBRE[DEMO_COUNT] = {   // nombres de texto de cada pantalla, en el MISMO orden que DemoScreen_t -- usados solo para el log "[SCREEN] -> %s" por consola
    "SPLASH", "JUGADORES_1", "JUGADORES_2", "INICIALES", "MODO_SIMON", "MODO_SIMONJOY",
    "MODO_GUITAR", "PREVIEW_SIMON", "PREVIEW_SIMONJOY", "CONTEO_3", "CONTEO_2",   // continua la lista de nombres, mismo orden que el enum de arriba
    "CONTEO_1", "CONTEO_GO", "JUGANDO", "RESULTADO", "MENU_DIFICULTAD", "MENU_CANCIONES"   // ultimos nombres de la lista
};

/* Indice seleccionado en DEMO_MENU_CANCIONES (persiste entre visitas) */
static uint8_t cancion_cursor = CANCION_TETRIS_IDX;   // cancion actualmente resaltada en la lista; se almacena aca (y no en gs) para que el valor se conserve al entrar y salir del menu -- arranca en Tetris (la cancion de fondo historica) para que la primera visita a la lista no empiece resaltando el jingle de bienvenida

/* Indice seleccionado en DEMO_MENU_DIFICULTAD (0=FACIL, 1=MEDIO, 2=PRO) --
 * persiste entre visitas igual que cancion_cursor. Solo se usa para Guitar
 * Hero (ver GuitarHero_IniciarSolo/GuitarHero2_IniciarAmbos); arranca en
 * MEDIO para no forzar al primer jugador a decidir entre 2 extremos. */
static uint8_t dificultad_cursor = 1;   // 0=FACIL 1=MEDIO 2=PRO -- cambiar el valor inicial aca cambia con que dificultad arranca la PRIMERA vez que se abre el menu (despues, el cursor recuerda la ultima eleccion)

/* Iniciales de 3 letras por jugador: se solicitan en la pantalla
 * DEMO_INICIALES, entre la seleccion de cantidad de jugadores y la
 * seleccion de modo de juego. Se muestran en lugar de las etiquetas
 * genericas "J1"/"J2" en cada modo (ver Nombre_Jugador(), utilizada desde
 * renderer.c). */
static char    nombre_jugadores[2][4] = { "AAA", "AAA" };   // nombre de 3 letras + terminador '\0' de cada jugador; "AAA" es el valor por defecto antes de escribir el nombre real
static uint8_t nombre_jugador_actual  = 0;   /* 0 o 1 -- jugador que esta escribiendo su nombre actualmente */
static uint8_t nombre_pos_actual      = 0;   /* 0,1,2 -- posicion de la letra que se esta editando */

/* Indica si el jugador ya completo la escritura de su nombre en la sesion
 * actual. Es necesario un indicador explicito porque el valor por defecto
 * de nombre_jugadores ("AAA") nunca esta vacio: comprobar unicamente si el
 * primer caracter es distinto de '\0' resultaria en verdadero incluso
 * antes de que el jugador haya escrito nada, dejando la pantalla de
 * iniciales inalcanzable. */
static uint8_t nombre_confirmado[2] = { 0, 0 };

/* Medida preventiva frente a un caso observado de corrupcion de memoria: en
 * determinadas condiciones, el contenido de nombre_jugadores[1] puede
 * contener bytes fuera del rango esperado entre el momento en que se
 * confirma en DEMO_INICIALES y el uso posterior durante la partida del
 * jugador 2. Todos los accesos a este arreglo dentro del codigo estan
 * correctamente delimitados, por lo que identificar la causa exacta
 * requeriria depuracion asistida por hardware (punto de observacion de
 * memoria via SWD/OpenOCD). Como medida preventiva, esta funcion sustituye
 * cualquier byte fuera del rango imprimible soportado por la fuente
 * tipografica (32-90) por el caracter '-', evitando que la corrupcion se
 * propague visualmente a la pantalla. */
const char *Nombre_Jugador(uint8_t jugador) {   // devuelve el nombre "seguro" (sin bytes corruptos) del jugador logico 0/1, para mostrar en pantalla
    static char seguro[2][4];   // buffer de salida por jugador; static para que el puntero devuelto siga valido despues de retornar (el renderer lo usa enseguida)
    const char *n = nombre_jugadores[jugador];   // apunta al nombre "crudo" (posiblemente corrupto) de ese jugador
    for (uint8_t i = 0; i < 3; i++) {   // recorre las 3 letras del nombre
        char c = n[i];   // letra cruda actual
        seguro[jugador][i] = (c >= 32 && c <= 90) ? c : '-';   // si esta fuera del rango imprimible que soporta la fuente (32-90), la reemplaza por '-'; cambiar el rango cambiaria que caracteres se consideran "validos"
    }
    seguro[jugador][3] = '\0';   // cierra el string de 3 letras
    return seguro[jugador];   // devuelve el puntero al buffer seguro de ESE jugador
}

/* Paso "encendido" (0-3, o 0xFF=ninguno) para animar las vistas previas de
 * Simon Clasico / Simon+Joystick, cambia cada 500ms mientras se muestran. */
static uint8_t Demo_PasoSimon(void) {   // calcula, a partir del reloj del sistema, cual de los 4 pasos animar ahora en las vistas previas (sin estado propio -- se deriva del tiempo)
    uint32_t t = (HAL_GetTick() / 500) % 5;   // ciclo de 5 posiciones (0-4) que cambia cada 500ms; cambiar el 500 cambia la velocidad del parpadeo, cambiar el 5 cambia cuanto dura el "hueco" sin nada encendido
    return (t < 4) ? (uint8_t)t : 0xFF;   // valores 0-3 son un paso real encendido; el 5to valor (t==4) devuelve 0xFF (nada encendido), da una pausa visible antes de repetir
}

/* Entra a la pantalla `screen`: prepara el GameState_t de ejemplo y dibuja
 * el primer cuadro. Renderer_Update() se encarga de animar lo que siga. */
static void Demo_Enter(uint8_t screen) {   // punto unico de entrada a cualquier pantalla: loguea, resetea el estado de ejemplo, fija orientacion y dibuja el primer cuadro
    printf("[SCREEN] -> %s\r\n", DEMO_NOMBRE[screen]);   // log de consola de cada cambio de pantalla, para poder seguir el flujo sin ver la pantalla fisica
    memset(&gs, 0, sizeof(gs));   // limpia TODO el estado de juego de ejemplo antes de preparar la nueva pantalla, para no arrastrar datos viejos de la anterior

    /* Simon Clasico usa layout "cocktail" en retrato (240x320); el resto
     * del recorrido sigue en paisaje (320x240) como hasta ahora. */
    ILI9341_SetPortrait(screen == DEMO_PREVIEW_SIMON);   // solo la vista previa de Simon Clasico usa retrato aca; el juego REAL cambia a retrato aparte (ver Botones_IniciarAmbos/Solo)

    switch (screen) {   // arma el contenido especifico de cada pantalla segun cual sea
    case DEMO_SPLASH:
        gs.estado = ESTADO_SPLASH;   // marca el estado de ejemplo como splash (usado por el renderer de fondo, si lo consulta)
        Buzzer_Patron(BEEP_BIENVENIDA, (uint8_t)BEEP_BIENVENIDA_N);   // arranca el jingle de bienvenida cada vez que se entra al splash
        break;

    case DEMO_JUGADORES_1:
    case DEMO_JUGADORES_2:
        Renderer_DrawSeleccionJugadores((uint8_t)(screen - DEMO_JUGADORES_1));   // dibuja el menu de 1/2 jugadores con el cursor en la opcion correspondiente (resta el indice base para obtener 0 o 1)
        break;

    case DEMO_INICIALES:
        /* Unicamente se reinicia a "AAA" y se solicita letra por letra el
         * nombre de los jugadores que aun no lo hayan confirmado en la
         * sesion actual; si el jugador 1 ya confirmo su nombre previamente,
         * no se le vuelve a solicitar ni se borra su valor al entrar a esta
         * pantalla para pedirle el nombre al jugador 2. */
        nombre_jugador_actual = nombre_confirmado[0] ? 1 : 0;   // si el jugador 1 ya confirmo su nombre, se solicita directamente el del jugador 2; de lo contrario, se comienza por el jugador 1
        nombre_pos_actual     = 0;   // siempre se comienza por la primera letra del jugador correspondiente
        if (!nombre_confirmado[0]) strcpy(nombre_jugadores[0], "AAA");   // solo se reinicia a "AAA" si el jugador 1 aun no confirmo su nombre (de lo contrario, se conserva el nombre ya confirmado)
        if (!nombre_confirmado[1]) strcpy(nombre_jugadores[1], "AAA");   // idem para el jugador 2
        Renderer_DrawNombre(nombre_jugador_actual, nombre_jugadores[nombre_jugador_actual], 0);   // dibuja la pantalla de iniciales arrancando en la letra 0 (ultimo parametro) del jugador que corresponda
        break;

    case DEMO_MODO_SIMON:
    case DEMO_MODO_SIMONJOY:
    case DEMO_MODO_GUITAR:
        Renderer_DrawSeleccionModo((uint8_t)(screen - DEMO_MODO_SIMON));   // dibuja el menu de modos con el cursor en la opcion correspondiente (0=Simon,1=Sim+Joy,2=Guitar)
        break;

    case DEMO_PREVIEW_SIMON:
        Renderer_DrawModoSimonClasico(0xFF, 0xFF);   // dibuja el layout completo de Simon Clasico sin ningun boton "encendido" (0xFF=ninguno) para ambos jugadores, solo de adorno
        break;

    case DEMO_PREVIEW_SIMONJOY:
        /* misma pantalla de solo-flechas que usa el juego real
         * (Renderer_DrawModoSimonJoystick1P) -- antes se dibujaba con la
         * version de 2 jugadores con texto (ARRIBA/ABAJO/IZQ/DER), que no
         * coincidia con lo que se ve al arrancar la partida de verdad. */
        Renderer_DrawModoSimonJoystick1P(0xFF);   // D-pad completo sin ningun paso encendido, solo de adorno
        break;

    case DEMO_CONTEO_3:
    case DEMO_CONTEO_2:
    case DEMO_CONTEO_1:
    case DEMO_CONTEO_GO:
        Renderer_DrawConteo((uint8_t)(DEMO_CONTEO_GO - screen));   // convierte la pantalla actual en el numero a mostrar: DEMO_CONTEO_3->3, ..., DEMO_CONTEO_GO->0 (interpretado como "GO" por el renderer)
        break;

    case DEMO_JUGANDO:
        /* El puntaje y el combo inician en 0 porque esta pantalla permite
         * jugar de verdad presionando B1 (ver GuitarHero_IntentarGolpe). */
        gs.estado       = ESTADO_JUGANDO;   // marca el estado de ejemplo como "jugando" (lo consulta el renderer de Guitar Hero) -- cambiar esto a otro ESTADO_* haria que el renderer de fondo dibuje otra pantalla
        gs.nota_speed   = NOTE_SPEED_L2;   // velocidad de caida de las notas del recorrido de diseño de Guitar Hero; cambiar esta constante (en game_state.h) cambia que tan rapido caen -- NO es la misma variable que usa el Guitar Hero real (ver gh_speed_base mas abajo)
        gs.j[0].puntaje = 0;   // puntaje inicial de jugador 0 en este recorrido de prueba -- cambiar este numero solo afecta el arranque de este recorrido de adorno, no el juego real
        gs.j[0].combo   = 0;   // combo inicial de jugador 0
        gs.j[1].puntaje = 0;   // puntaje inicial de jugador 1
        gs.j[1].combo   = 0;   // combo inicial de jugador 1
        gs.notas[0] = (Nota_t){ .x_rel = -NOTE_W, .carril = 1, .jugador = 0, .activa = 1 };   // primera nota de prueba: arranca justo fuera de pantalla a la izquierda (-NOTE_W), carril 1, del jugador 0, activa
        gs.notas[1] = (Nota_t){ .x_rel = -NOTE_W, .carril = 2, .jugador = 1, .activa = 1 };   // segunda nota de prueba, carril 2, del jugador 1
        break;

    case DEMO_RESULTADO:
        gs.estado       = ESTADO_RESULTADO;   // marca el estado de ejemplo como resultado (pantalla de adorno del recorrido, no la real de cada modo)
        gs.j[0].puntaje = 1250;   // puntaje de ADORNO fijo para mostrar como se veria la pantalla de resultado con datos
        gs.j[0].combo   = 8;      // combo de adorno de jugador 0
        gs.j[1].puntaje = 980;    // puntaje de adorno de jugador 1
        gs.j[1].combo   = 5;      // combo de adorno de jugador 1
        break;

    case DEMO_MENU_DIFICULTAD:
        Renderer_DrawMenu(dificultad_cursor);   // reusa la pantalla legada de FACIL/MEDIO/PRO, ahora conectada de verdad (ver MenuDificultad_Procesar)
        break;

    case DEMO_MENU_CANCIONES:
        Renderer_DrawListaCanciones(cancion_cursor);   // dibuja la lista de canciones con el cursor en la posicion recordada de la ultima visita
        break;
    }
}

/* ========================================================================== */
/* === GOLPEO DE NOTAS (GUITAR HERO) — B1 COMO BOTON UNICO =================== */
/* ========================================================================== */
/* Corresponde a la maqueta visual original del modo Guitar Hero, previa a
 * la existencia de los botones arcade cableados. El boton B1 actua como
 * "boton unico": golpea la nota activa mas cercana al centro de la zona de
 * presion, sin distinguir carril ni jugador, ya que un solo boton no
 * permite seleccionar un carril especifico. Esta logica solo se ejecuta
 * dentro del recorrido de diseño (pantalla DEMO_JUGANDO, con 2 notas fijas
 * que reaparecen automaticamente); el modo completo, con sincronizacion
 * real respecto a una melodia, constituye el siguiente objetivo de
 * desarrollo del proyecto (ver guitar_hero/ANALISIS_REFERENCIA.md para el
 * diseño propuesto: un mapa de notas ligado al tiempo real de una melodia,
 * en lugar de la generacion periodica actual). */
static void GuitarHero_IntentarGolpe(void) {   // busca la nota activa mas cercana al centro de la zona de golpe y la puntua (o falla si ninguna esta cerca)
    int16_t pz_centro = (int16_t)(PRESS_ZONE_W / 2);   // posicion X del centro de la zona de presion, en coordenadas locales del carril
    int16_t mejor_dist = 0x7FFF;   // arranca en el maximo posible de un int16_t, para que la primera nota comparada siempre sea "mejor" que este valor inicial
    int8_t  mejor_i    = -1;   // indice de la mejor nota encontrada hasta ahora; -1 = ninguna todavia

    for (uint8_t i = 0; i < MAX_NOTES; i++) {   // recorre TODAS las notas posibles del arreglo (activas o no)
        Nota_t *n = &gs.notas[i];   // puntero a la nota actual, para no repetir gs.notas[i] varias veces
        if (!n->activa) continue;   // ignora las notas que no estan en juego ahora mismo
        int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);   // posicion X del centro de la nota (su borde izquierdo x_rel mas la mitad de su ancho)
        int16_t dist = (int16_t)((centro_nota > pz_centro) ? (centro_nota - pz_centro) : (pz_centro - centro_nota));   // distancia absoluta entre el centro de la nota y el centro de la zona de presion (valor siempre positivo)
        if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }   // si esta nota esta mas cerca que la mejor encontrada hasta ahora, la reemplaza
    }

    if (mejor_i < 0 || mejor_dist > (int16_t)HIT_OK) {   // no habia ninguna nota activa, o la mas cercana esta mas lejos que la ventana de puntuacion "OK" (la mas ancha de las 3)
        printf("[GUITARHERO] MISS (nada en rango)\r\n");   // log de fallo por consola
        return;   /* nada cerca: fallo silencioso */
    }

    Nota_t          *n = &gs.notas[mejor_i];   // puntero a la nota que SI se va a puntuar
    EstadoJugador_t *j = &gs.j[n->jugador];   // puntero al estado (puntaje/combo) del jugador dueño de esa nota
    const char      *calidad;   // texto de la calidad del golpe, solo para el log por consola

    if      (mejor_dist <= (int16_t)HIT_PERFECT) { j->puntaje = (uint16_t)(j->puntaje + SCORE_PERFECT); calidad = "PERFECT"; }   // golpe dentro de la ventana mas angosta -> maximo puntaje
    else if (mejor_dist <= (int16_t)HIT_GOOD)    { j->puntaje = (uint16_t)(j->puntaje + SCORE_GOOD);    calidad = "GOOD"; }      // dentro de la ventana intermedia -> puntaje medio
    else                                          { j->puntaje = (uint16_t)(j->puntaje + SCORE_OK);     calidad = "OK"; }        // dentro de la ventana mas ancha (HIT_OK) -> puntaje minimo
    j->combo++;   // cualquier golpe (aunque sea "OK") suma combo -- solo un MISS total lo cortaria (no implementado en este recorrido de prueba)
    printf("[GUITARHERO] jugador=%u dist=%d %s puntaje=%u combo=%u\r\n",
           n->jugador, mejor_dist, calidad, j->puntaje, j->combo);   // log completo del golpe: jugador, distancia real, calidad, puntaje y combo resultantes

    n->x_rel = -NOTE_W;   /* respawn inmediato para poder seguir probando */
}

/* Detecta el flanco de presion de B1 (activo en bajo, con resistencia de
 * pull-up externa en la placa Nucleo), aplicando el mismo criterio de
 * antirrebote por tiempo (BTN_DEBOUNCE_MS). Dado que B1 confirma menus y
 * permite salir de una partida en curso, un flanco falso en esta deteccion
 * se traduciria en un cambio de pantalla no solicitado por el usuario. */
static uint8_t Boton_B1_Flanco(void) {   // detecta un flanco de presion (SET->RESET) de B1 con debounce real; devuelve 1 solo el tick en que se confirma el flanco
    static GPIO_PinState prev        = GPIO_PIN_SET;   // ultimo nivel YA CONFIRMADO en la llamada anterior (para comparar flanco); arranca en SET = no presionado
    static GPIO_PinState estable      = GPIO_PIN_SET;   // nivel "candidato" que se esta esperando confirmar como estable
    static uint32_t       tick_cambio = 0;   // tick del ultimo cambio de nivel detectado, para medir cuanto lleva estable
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);   // lee el nivel crudo actual de B1 (PC13)

    if (cur != estable) {   // el nivel leido cambio respecto al ultimo candidato
        estable     = cur;   // este nuevo nivel pasa a ser el candidato a estable
        tick_cambio = HAL_GetTick();   // reinicia el cronometro de estabilidad
        return 0;   /* posible rebote -- todavia no cuenta */
    }
    if (HAL_GetTick() - tick_cambio < BTN_DEBOUNCE_MS) return 0;  /* aun no esta estable */

    uint8_t flanco = (prev == GPIO_PIN_SET && cur == GPIO_PIN_RESET);   // flanco de bajada real: antes no presionado, ahora si (activo bajo)
    prev = cur;   // actualiza el "anterior" confirmado para la proxima llamada
    return flanco;
}

/* Navegacion del recorrido de pantallas (menus, lista de canciones, etc.):
 * el avance entre pantallas se realiza presionando cualquiera de los 8
 * botones arcade, manteniendo el joystick reservado exclusivamente para
 * jugar (deteccion de direccion en los modos JOYS). Esta funcion revisa los
 * 8 botones (de ambos jugadores) en busca de un flanco de presion;
 * cualquiera de ellos cuenta como una accion de "avanzar" (no existe un
 * boton dedicado a retroceder, ya que el recorrido es ciclico y presionar
 * varias veces permite alcanzar cualquier pantalla). Debe invocarse
 * unicamente fuera de una partida real (ver la condicion correspondiente
 * en el bucle principal) para no interceptar el flanco de presion que le
 * corresponde a Botones_ActualizarJugador durante una partida del modo de
 * botones. */
static uint8_t BotonesNavegacion_Presionado(void) {   // devuelve 1 si CUALQUIERA de los 8 botones arcade (ambos jugadores) tuvo un flanco de presion este tick
    uint8_t presionado = 0;   // acumulador de resultado; arranca en "no se presiono nada"
    for (uint8_t p = 0; p < 2; p++) {   // revisa ambos jugadores
        if (Botones_LeerColor(p) != 0xFF) presionado = 1;   // si ese jugador tuvo un flanco en cualquiera de sus 4 colores, cuenta como "se presiono algo" (nota: SIEMPRE llama a Botones_LeerColor de los 2, aunque el primero ya haya dado positivo, para no dejar flancos sin consumir)
    }
    return presionado;
}

/* Gesto de salida de emergencia: mantener presionados simultaneamente los
 * botones ROJO y AMARILLO del mismo jugador durante 3 segundos ofrece una
 * alternativa a B1 para abandonar una partida en curso sin que el jugador
 * deba desplazarse hasta la placa Nucleo. Esta funcion lee el nivel crudo
 * de los pines en lugar del flanco que entrega Botones_LeerColor (la cual
 * detecta un solo color a la vez y consume el flanco en el primer color
 * detectado), de modo que no interfiere con la deteccion normal de colores
 * del juego. */
#define COMBO_SALIR_MS 3000U   // tiempo (en milisegundos) que deben mantenerse presionados ROJO+AMARILLO para que se dispare el reinicio; disminuir este valor acelera la respuesta pero facilita activarlo sin intencion, aumentarlo lo hace mas seguro pero mas lento
static uint32_t combo_salir_tick[2] = { 0, 0 };   // tick en que comenzo el combo para cada jugador (0 = combo no esta en curso); se mantiene por separado para que cada jugador pueda salir sin depender del otro

static uint8_t ComboSalir_Detectado(void) {   // revisa si algun jugador lleva 3 segundos manteniendo presionados ROJO+AMARILLO; devuelve 1 en el ciclo en que se cumple la condicion
    uint8_t disparado = 0;   // resultado acumulado (contempla el caso, poco probable, de que ambos jugadores completen el gesto en el mismo ciclo)
    for (uint8_t p = 0; p < 2; p++) {   // revisa cada jugador por separado
        uint8_t target_p = p;
        if (btn_modo_1p || guitar_modo_1p) target_p = 0;   // en modo de 1 jugador, ambos jugadores logicos leen los botones fisicos del conjunto 0

        uint8_t rojo_amarillo = (HAL_GPIO_ReadPin(BTN_SW[target_p][0].port, BTN_SW[target_p][0].pin) == GPIO_PIN_RESET) &&
                                 (HAL_GPIO_ReadPin(BTN_SW[target_p][3].port, BTN_SW[target_p][3].pin) == GPIO_PIN_RESET);   // lee el NIVEL crudo (no el flanco) de rojo (color 0) y amarillo (color 3) de este jugador; 1 solo si AMBOS estan presionados ahora mismo
        if (rojo_amarillo) {   // los 2 botones estan presionados en este instante
            if (combo_salir_tick[p] == 0) {
                combo_salir_tick[p] = HAL_GetTick();   // primera vez que se detecta el combo: arranca el cronometro
            } else if (HAL_GetTick() - combo_salir_tick[p] >= COMBO_SALIR_MS) {
                printf("[COMBO] P%u rojo+amarillo 3s -> salir\r\n", p);   // log del disparo, con que jugador lo activo
                disparado = 1;   // marca que hay que resetear
                combo_salir_tick[p] = 0;   /* rearma para la proxima vez */
            }
        } else {
            combo_salir_tick[p] = 0;   // se solto alguno de los 2 botones antes de completar los 3s: cancela el cronometro de este jugador
        }
    }
    return disparado;
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — MODO DE 1 JUGADOR ================================== */
/* ========================================================================== */
/* Variante del juego "Simon Dice" que utiliza direcciones del joystick en
 * lugar de colores: la secuencia se reproduce parpadeando sobre el D-pad
 * dibujado por Renderer_DrawModoSimonJoystick1P(), el jugador la repite
 * inclinando el joystick, y cada acierto agrega un paso adicional a la
 * secuencia (dificultad adaptativa: comienza lenta y se acelera de forma
 * automatica, sin niveles fijos predefinidos). Las direcciones se codifican
 * como 0=ARRIBA, 1=ABAJO, 2=IZQUIERDA, 3=DERECHA. */

typedef enum {   // fases de la maquina de estados de Simon+Joystick (1P y 2P comparten estos mismos 4 nombres, ver sj2_fase)
    SJ_MOSTRANDO,   /* reproduciendo la secuencia (parpadeo on/off)          */
    SJ_ESPERANDO,   /* esperando que el jugador repita paso por paso         */
    SJ_ACIERTO,     /* pausa corta de "bien" antes de mostrar la siguiente   */
    SJ_GAMEOVER     /* fallo: pantalla de resultado, espera mover el stick
                       (reintentar) o B1 (salir) -- ver comentario arriba   */
} SimonJoyFase_t;   // este mismo tipo lo reusan Botones (btn_fase) y Guitar Hero para sus propias maquinas de estado, ya que las 4 fases son identicas en concepto

#define SJ_MAX_LONGITUD      64   // tamaño maximo del arreglo sj_secuencia -- si una partida llegara a superar 64 rondas (dificilmente, con velocidad creciente) dejaria de agregar pasos nuevos, no se rompe nada
#define SJ_PAUSA_ACIERTO_MS  550U  // duracion de la pausa de "acierto" antes de mostrar el siguiente paso; un valor demasiado bajo hace que la siguiente ronda comience de forma abrupta, sin dar tiempo de reaccion al jugador

/* La velocidad se modela como un multiplicador: en la ronda 1 vale x0.10
 * (lento) y aumenta +0.02 por cada ronda superada, hasta un tope de x0.40
 * (alcanzado alrededor de la ronda 16), donde se estabiliza. El intervalo
 * de tiempo real se calcula como REF_MS / velocidad, de modo que a mayor
 * multiplicador corresponde un intervalo menor (mas rapido). Con
 * REF_MS=100, el multiplicador inicial produce un intervalo de 1000 ms y
 * el tope produce 250 ms (cada mitad del parpadeo on/off dura 125 ms, lo
 * cual resulta suficiente para distinguir 2 repeticiones consecutivas de la
 * misma direccion; topes de velocidad mayores probados previamente
 * resultaron demasiado rapidos para esa distincion). */
#define SJ_VELOCIDAD_INICIAL  0.10f   // multiplicador de velocidad en la ronda 1 (mas chico = mas lento); ver formula en SimonJoy_IntervaloActual
#define SJ_VELOCIDAD_PASO     0.02f   // cuanto sube el multiplicador por cada ronda superada; subirlo hace que el juego se acelere mas rapido ronda a ronda
#define SJ_VELOCIDAD_MAX      0.40f   // tope maximo del multiplicador (se deja de acelerar despues de esto); subirlo sube el techo de velocidad maxima del juego
#define SJ_INTERVALO_REF_MS   100U   // milisegundos de referencia usados junto al multiplicador para calcular el intervalo real (ver SimonJoy_IntervaloActual)

static uint8_t        sj_secuencia[SJ_MAX_LONGITUD];   // secuencia completa de direcciones (0-3) que el jugador debe repetir, se va alargando de a 1
static uint8_t        sj_longitud;       /* pasos en la secuencia actual      */
static uint8_t        sj_paso_mostrar;   /* indice que se esta parpadeando    */
static uint8_t        sj_paso_esperado;  /* indice que se espera del jugador  */
static uint8_t        sj_mostrando_on;   /* sub-fase del parpadeo (on/off)    */
static uint8_t        sj_paso_dibujado = 0xFF; /* ultimo paso pintado en el D-pad
    (0xFF=ninguno) -- para actualizar solo el boton que cambio en vez de
    redibujar toda la pantalla (evita el parpadeo de un FillScreen completo) */   // arranca en 0xFF ("nada pintado") para que el primer paso real se dibuje como un cambio
static uint8_t        sj_mejor_racha;    /* mejor racha de esta sesion        */
static SimonJoyFase_t sj_fase;   // fase actual de la maquina de estados (MOSTRANDO/ESPERANDO/ACIERTO/GAMEOVER)
static uint32_t       sj_tick_fase;   // tick en que arranco la fase actual, para medir cuanto lleva en ella
static uint32_t       sj_seed = 1;   // semilla del generador pseudoaleatorio LCG (ver SJ_Random4); nunca debe quedar en 0 o el LCG se "traba" repitiendo 0
static uint8_t        en_juego_real = 0; /* 1 = SimonJoy real, no el recorrido*/
static uint8_t         jugadores_seleccionados = 1; /* 1 o 2, elegido en el menu */
static uint8_t         joy_listo_dir = 1;   // 1 = el stick ya volvio al centro y esta listo para contar un nuevo movimiento (evita que un empuje sostenido cuente varias veces)

/* Conteo regresivo automatico (3, 2, 1, GO) al confirmar un modo jugable
 * desde el menu real: a diferencia del recorrido de diseño (que espera la
 * pulsacion de B1 en cada pantalla), en este caso cada numero avanza de
 * forma automatica, como en una partida real. La variable modo_confirmado
 * indica que modo de juego iniciar al llegar a "GO" entre las 3 opciones
 * disponibles (BOTONES, SIMONJOY, GUITAR); ver GuitarHero2_IniciarAmbos y
 * GuitarHero_IniciarSolo mas abajo. */
typedef enum { MODO_SEL_BOTONES = 0, MODO_SEL_SIMONJOY, MODO_SEL_GUITAR } ModoSeleccion_t;   // que modo de juego se eligio en DEMO_MODO_* -- el ORDEN debe coincidir con el orden de las 3 tarjetas del menu (Renderer_DrawSeleccionModo)
static ModoSeleccion_t modo_confirmado = MODO_SEL_BOTONES;   // que modo real arrancar cuando el conteo automatico llegue a GO
static uint8_t  conteo_auto = 0;   // 1 mientras el conteo 3-2-1-GO esta avanzando solo (sin esperar B1)
static uint32_t conteo_tick = 0;   // tick en que se mostro el numero actual del conteo
#define CONTEO_PASO_MS 700U   // milisegundos que se muestra cada numero del conteo antes de pasar al siguiente; cambiar esto cambia el ritmo del "3, 2, 1, GO"

/* generador pseudoaleatorio simple (LCG), suficiente para elegir 1 de 4 direcciones */
static uint8_t SJ_Random4(void) {   // genera un numero pseudoaleatorio 0-3, avanzando el generador LCG (congruencial lineal) un paso
    sj_seed = sj_seed * 1103515245u + 12345u;   // formula estandar de un LCG de 32 bits (mismas constantes que usa glibc rand()); cambiar estos numeros cambia por completo la secuencia generada
    return (uint8_t)((sj_seed >> 16) & 0x3u);   // toma 2 bits "del medio" de la semilla (mas aleatorios que los bits bajos de un LCG) y los recorta a 0-3
}

/* siguiente direccion de la secuencia, sin permitir una 3ra repeticion
 * seguida (verificado: el LCG de arriba solo puede repetir 3-6 veces
 * seguidas cada ~20 tiradas, se siente injusto en un juego de memoria).
 * Si las 2 anteriores ya son iguales entre si, se fuerza que la nueva sea
 * distinta -- eligiendo uniforme entre las otras 3 direcciones. */
static uint8_t SJ_SiguienteDireccion(void) {   // elige la proxima direccion a agregar a la secuencia, evitando una 3ra repeticion seguida
    uint8_t nuevo = SJ_Random4();   // primer candidato aleatorio
    if (sj_longitud >= 2 &&   // solo puede haber una 3ra repeticion si ya hay al menos 2 pasos previos en la secuencia
        sj_secuencia[sj_longitud - 1] == sj_secuencia[sj_longitud - 2] &&   // los 2 ultimos pasos ya son iguales entre si
        nuevo == sj_secuencia[sj_longitud - 1]) {   // las 2 direcciones anteriores ya son iguales entre si Y el candidato nuevo tambien coincide (seria una 3ra repeticion)
        nuevo = (uint8_t)((nuevo + 1u + (SJ_Random4() % 3u)) & 0x3u);   // fuerza un valor DISTINTO al repetido, eligiendo uniforme entre las otras 3 direcciones posibles
    }
    return nuevo;
}

static uint16_t SimonJoy_IntervaloActual(void) {   // calcula cuantos ms dura cada mitad (on u off) del parpadeo de la secuencia, segun la ronda actual
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj_longitud - 1) * SJ_VELOCIDAD_PASO;   // velocidad sube linealmente con la ronda (sj_longitud), arrancando en SJ_VELOCIDAD_INICIAL
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;   // no deja que la velocidad supere el tope maximo configurado
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);   // a mayor velocidad, menor intervalo (mas rapido) -- relacion inversa
}

/* Deteccion de direccion del joystick por flanco: exige que ambos ejes
 * salgan de la zona muerta y regresen a ella antes de contar el siguiente
 * movimiento, sin requerir que el valor vuelva al centro exacto, ya que un
 * eje con un desplazamiento propio de fabricacion podria no retornar nunca
 * a una banda demasiado angosta, dejando el juego sin responder despues
 * del primer acierto. */
static uint8_t Joystick_LeerDireccion(void) {   // devuelve la direccion (0-3) del joystick fisico usado en el modo de 1 jugador, detectada por flanco
    uint16_t bajo_y, alto_y, bajo_x, alto_x;   // rango "sin direccion" (zona muerta) de cada eje, calculado por Joy_Umbrales segun el centro medido
    Joy_Umbrales(centro_j2y, &bajo_y, &alto_y);   // rango de la zona muerta del eje Y de J2, relativo a su centro medido
    Joy_Umbrales(centro_j2x, &bajo_x, &alto_x);   // rango de la zona muerta del eje X de J2, relativo a su centro medido

    if (!joy_listo_dir) {   // el stick todavia no volvio al centro desde el ultimo movimiento contado -- no se puede contar otro todavia
        if (joystick2_y < alto_y && joystick2_y > bajo_y && joystick2_x < alto_x && joystick2_x > bajo_x) {   // los 2 ejes ya volvieron dentro de la zona muerta (centrado)
            joy_listo_dir = 1;   // habilita la deteccion del proximo movimiento
        }
        return 0xFF;   // este tick no cuenta como movimiento nuevo, sin importar si recien se centro o seguia afuera
    }

    /* Los ejes electricos del joystick fisico usado en el modo de 1 jugador
     * quedan invertidos respecto a la orientacion visual del montaje: el
     * canal que el ADC identifica como "Y" (joystick2_y, PC0) responde al
     * movimiento lateral del joystick, mientras que el canal "X"
     * (joystick2_x, PC1) responde al movimiento vertical. Por esta razon,
     * la direccion ARRIBA/ABAJO se calcula a partir del canal X, y la
     * direccion IZQUIERDA/DERECHA a partir del canal Y. */
    int32_t dev_vert  = (int32_t)joystick2_x - (int32_t)centro_j2x;  /*  + = ABAJO   */
    int32_t dev_horiz = (int32_t)joystick2_y - (int32_t)centro_j2y;  /*  + = IZQUIERDA */
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;   // magnitud de la desviacion vertical, sin signo
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;   // magnitud de la desviacion horizontal, sin signo

    uint8_t dir = 0xFF;   // 0xFF = todavia ninguna direccion valida detectada este tick
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {   // el eje vertical supera el umbral Y es el mas inclinado de los 2 (evita diagonales ambiguas)
        dir = (dev_vert > 0) ? 1 : 0;    /* ABAJO : ARRIBA  */
        joy_listo_dir = 0;   // consume la deteccion: hay que volver al centro antes de contar el proximo movimiento
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {   // el eje horizontal supera el umbral (y ya se descarto que el vertical fuera el dominante)
        dir = (dev_horiz < 0) ? 3 : 2;   /* DERECHA cuando joystick2_y cae por debajo de su centro : IZQUIERDA */
        joy_listo_dir = 0;   // idem, consume la deteccion
    }
    if (dir != 0xFF) printf("[INPUT] joystick J2 Solo = %s\r\n", DIR_NOMBRE[dir]);   // log solo cuando SI se detecto una direccion real
    return dir;
}

/* Misma mitigacion que Nombre_Jugador() de mas arriba (ver su comentario),
 * aplicada aca a los buffers "linea"/"buf" armados con snprintf antes de
 * dibujarlos en el modo de 1 jugador: reemplaza en el lugar cualquier byte
 * fuera del rango imprimible que soporta la fuente (32-90, mas minusculas
 * 97-122 -- ILI9341_DrawChar ya las normaliza a mayuscula antes de validar
 * el rango, asi que SON validas y no deben tocarse; una version anterior
 * las trataba como corruptas, rompiendo literales como "Racha: " en
 * pantalla) por '-'. Es un cambio puramente de visualizacion (no toca
 * timing ni logica de juego del modo de 1 jugador). */
static void Texto_Sanear(char *s) {   // reemplaza bytes fuera del rango imprimible de la fuente por '-', para no mostrar simbolos corruptos en pantalla (copia local de la misma mitigacion que renderer.c)
    for (; *s; s++) {   // recorre la cadena caracter por caracter hasta el '\0' final, modificandola IN-PLACE
        char c = *s;   // caracter actual
        if (c >= 'a' && c <= 'z') {   // minuscula legitima (la fuente las soporta, normalizandolas a mayuscula al dibujar)
            *s = (char)(c - 32);  // Convierte minusculas a MAYUSCULAS
            continue;   // ya se resolvio este caracter, sigue con el siguiente sin evaluar el rango de abajo
        }
        if (c < 32 || c > 90) *s = '-';   // fuera del rango imprimible que soporta la fuente (32-90) -- lo reemplaza por '-' en vez de dejar un byte corrupto en pantalla
    }
}

static void SimonJoy_DibujarGameOver(void) {   // dibuja la pantalla completa de "GAME OVER" del modo 1 jugador
    char linea[32];   // buffer temporal para armar cada linea de texto con snprintf antes de dibujarla

    /* NO tocar la orientacion aca: SimonJoy 1 jugador se mantiene en
     * paisaje (320x240, heredado del recorrido de menus, ver SimonJoy_
     * Iniciar) durante TODA la partida -- esta pantalla es solo el final,
     * no un cambio de modo. */
    ILI9341_FillScreen(COLOR_BLACK);   // borra toda la pantalla a negro antes de dibujar el resultado

    ILI9341_DrawString(70, 80, "GAME OVER", COLOR_RED, COLOR_BLACK, 3);   // titulo grande (escala 3) en rojo -- cambiar el 3 cambia el tamaño del titulo

    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)(sj_longitud - 1));   // sj_longitud-1 porque el ultimo paso agregado fue el que fallo (no cuenta como acertado)
    Texto_Sanear(linea);
    ILI9341_DrawString(100, 140, linea, COLOR_WHITE, COLOR_BLACK, 2);   // racha de ESTA partida

    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)sj_mejor_racha);   // mejor racha de TODA la sesion (no se resetea entre partidas)
    Texto_Sanear(linea);
    ILI9341_DrawString(100, 165, linea, COLOR_YELLOW, COLOR_BLACK, 2);

    ILI9341_DrawString(60, 205, "mueve=reintentar", COLOR_GRAY, COLOR_BLACK, 1);   // instruccion: mover el joystick (no hay boton dedicado) reinicia la partida
    ILI9341_DrawString(60, 218, "ROJO 2s=menu de modos", COLOR_GRAY, COLOR_BLACK, 1);   // atajo de salida, ver SalirGameOver1P_Detectado
}

/* Numero de ronda mostrado en tiempo real en la esquina superior derecha
 * del encabezado, para que el jugador pueda ver su progreso mientras juega
 * y no solo al perder la partida. Redibuja unicamente esa esquina, sin
 * afectar el resto del encabezado. */
static void SimonJoy_MostrarRacha(uint8_t racha) {   // redibuja solo el numero de ronda en la esquina superior derecha, sin tocar el resto de la pantalla
    char buf[12];   // buffer para el texto "RONDA:NN"
    snprintf(buf, sizeof(buf), "RONDA:%2u", racha);   // arma el texto con el numero de ronda actual (ancho fijo de 2 digitos)
    Texto_Sanear(buf);   // por si algun caracter quedo fuera de rango (defensivo, aca siempre son digitos)
    ILI9341_FillRect(LCD_W - 80, 0, 80, 26, COLOR_DARKGRAY);   // borra solo el recuadro de la esquina (80px de ancho, pegado al borde derecho) antes de escribir encima
    ILI9341_DrawString(LCD_W - 74, 9, buf, COLOR_YELLOW, COLOR_DARKGRAY, 1);   // dibuja el texto en amarillo sobre fondo gris oscuro
}

static void SimonJoy_Iniciar(void) {   // arranca una partida nueva de SimonJoy 1 jugador desde cero
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // arranca la musica de fondo (Tetris) en loop mientras se juega
    sj_seed ^= HAL_GetTick();   // mezcla la semilla con el tick actual para que cada partida tenga una secuencia distinta (no siempre la misma)
    if (sj_seed == 0) sj_seed = 1;   // evita que la semilla quede en 0 (el LCG se trabaria devolviendo siempre 0)

    sj_longitud        = 1;   // arranca con una secuencia de un solo paso
    sj_secuencia[0]     = SJ_Random4();   // primer paso aleatorio de la secuencia
    sj_paso_mostrar     = 0;   // arranca mostrando desde el paso 0
    sj_mostrando_on     = 1;   // arranca en la mitad "encendida" del parpadeo
    sj_fase             = SJ_MOSTRANDO;   // arranca en la fase de reproducir la secuencia
    sj_tick_fase        = HAL_GetTick();   // marca el instante de arranque de esta fase
    joy_listo_dir       = 1;   // habilita de entrada la deteccion de movimiento (no hace falta esperar a que "vuelva" al centro la primera vez)

    /* pantalla completa para 1 jugador (sin dividir), girada 180 hacia el
     * lado de "jugador 2" -- ya en landscape (heredado del recorrido de
     * menus), asi que solo hace falta el flip, sin tocar SetPortrait. */
    ILI9341_SetFlip180(0);   // gira la pantalla 180 grados en hardware para este modo
    Renderer_DrawModoSimonJoystick1P(sj_secuencia[0]);   // dibuja el D-pad completo con el primer paso ya "encendido"
    sj_paso_dibujado    = sj_secuencia[0];   // sincroniza que paso quedo pintado en pantalla, para que las actualizaciones incrementales sepan de donde partir
    Renderer_ResetCursorJoystick();   // vuelve el cursor en forma de "X" del centro del D-pad a su posicion neutral
    SimonJoy_MostrarRacha(sj_longitud);   // dibuja el numero de ronda inicial (1)
}

/* Cambia cual boton del D-pad esta encendido (0xFF=ninguno). Invoca
 * unicamente la actualizacion incremental (sin borrar toda la pantalla) y
 * solo cuando el estado realmente cambio, evitando repintar cuando el
 * joystick permanece quieto en la misma direccion, lo cual produciria un
 * parpadeo visual innecesario. */
static void SimonJoy_MostrarPaso(uint8_t nuevo) {   // cambia cual flecha del D-pad esta "encendida" (nuevo=0-3) o las apaga todas (nuevo=0xFF)
    if (nuevo == sj_paso_dibujado) return;   // ya esta pintado asi, no redibuja nada (evita parpadeo innecesario)
    Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, nuevo);   // redibuja solo las 2 flechas que cambiaron (la que se apaga y la que se prende), no toda la pantalla
    sj_paso_dibujado = nuevo;   // guarda el nuevo estado pintado
    if (nuevo < 4) Buzzer_Beep(90);  /* beep corto cada vez que se prende una flecha */
}

/* Confirmacion visual de una entrada del jugador durante la fase
 * SJ_ESPERANDO, mediante un ciclo real de apagado y encendido (no
 * instantaneo). Esto es necesario porque, si dos entradas consecutivas
 * corresponden a la misma direccion, SimonJoy_MostrarPaso no vuelve a
 * redibujar la flecha al no cambiar su estado, lo cual haria parecer que la
 * segunda entrada no fue registrada. Esta funcion apaga la flecha de
 * inmediato y programa su reencendido SJ_FLASH_INPUT_MS despues (verificado
 * en SimonJoy_Actualizar), garantizando que el apagon sea visible incluso
 * cuando la direccion se repite. */
#define SJ_FLASH_INPUT_MS 70U   // milisegundos que la flecha queda apagada antes de volver a prenderse al confirmar una entrada; subirlo hace el parpadeo mas notorio pero mas lento
static uint8_t  sj_flash_pendiente = 0xFF;  /* 0xFF = nada pendiente */
static uint32_t sj_flash_tick      = 0;   // tick en que se apago la flecha, para saber cuando volver a prenderla

static void SimonJoy_ConfirmarInput(uint8_t dir) {   // apaga YA la flecha actual y programa que se vuelva a prender en SJ_FLASH_INPUT_MS (para que el parpadeo se note aunque se repita la misma direccion)
    if (sj_paso_dibujado != 0xFF) {   // si habia algo pintado
        Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, 0xFF);   // lo apaga ya mismo
        sj_paso_dibujado = 0xFF;   // marca que no hay nada pintado ahora
    }
    sj_flash_pendiente = dir;   // guarda que direccion hay que prender despues del apagon
    sj_flash_tick      = HAL_GetTick();   // marca el instante del apagon, para medir cuando cumplir SJ_FLASH_INPUT_MS
    Buzzer_Beep(90);   // beep corto de confirmacion de entrada, sin importar si fue acierto o fallo (el fallo agrega un beep largo aparte)
}


/* tick no bloqueante del minijuego: se llama una vez por vuelta del loop
 * principal mientras en_juego_real este activo. Nunca usa HAL_Delay largo,
 * asi que el filtro/deadzone del joystick sigue actualizandose por interrupcion. */
static void SimonJoy_Actualizar(void) {   // tick no bloqueante de la maquina de estados de SimonJoy 1 jugador; se llama una vez por vuelta del loop principal
    uint32_t ahora = HAL_GetTick();   // tick actual, usado para medir tiempos en todas las fases

    if (sj_fase != SJ_GAMEOVER) {   // en game over se muestra otra pantalla completa, no tiene sentido seguir moviendo el cursor del D-pad
        Renderer_ActualizarCursorJoystick(joystick2_x, joystick2_y, joy_listo_dir);   // mueve el cursor "X" del centro del D-pad segun la posicion cruda actual del stick (menos en game over, que muestra otra pantalla)
    }

    /* completa el parpadeo de SimonJoy_ConfirmarInput -- prende la flecha
     * ya pasado el tiempo minimo de "apagada" */
    if (sj_flash_pendiente != 0xFF && (ahora - sj_flash_tick) >= SJ_FLASH_INPUT_MS) {   // hay un prendido pendiente Y ya paso el tiempo minimo de apagado
        SimonJoy_MostrarPaso(sj_flash_pendiente);   // prende la flecha correspondiente a la entrada confirmada
        sj_flash_pendiente = 0xFF;   // consume el pendiente, para no repetir el prendido en el proximo tick
    }

    switch (sj_fase) {   // logica especifica de cada fase del juego
    case SJ_MOSTRANDO: {   // reproduciendo la secuencia completa, un paso a la vez, parpadeando
        uint16_t medio = (uint16_t)(SimonJoy_IntervaloActual() / 2U);   // mitad del intervalo actual = duracion de cada mitad (on/off) del parpadeo
        if ((ahora - sj_tick_fase) < medio) break;   // todavia no paso suficiente tiempo para cambiar de sub-fase
        sj_tick_fase = ahora;   // marca el inicio de la nueva sub-fase

        if (sj_mostrando_on) {   // estaba en la mitad "encendida" del parpadeo -- toca apagar
            /* mitad "apagada" del parpadeo, mismo paso */
            SimonJoy_MostrarPaso(0xFF);   // apaga la flecha actual (sigue siendo el mismo paso, solo cambia a "apagado")
            sj_mostrando_on = 0;   // pasa a la sub-fase "apagada"
        } else {   // estaba apagado -- toca avanzar al siguiente paso (o terminar de mostrar la secuencia)
            sj_paso_mostrar++;   // avanza al siguiente paso de la secuencia a mostrar
            if (sj_paso_mostrar >= sj_longitud) {   // ya se mostraron TODOS los pasos de la secuencia
                sj_fase          = SJ_ESPERANDO;   // pasa a esperar la respuesta del jugador
                sj_paso_esperado = 0;   // el jugador debe repetir empezando desde el primer paso
                joy_listo_dir    = 1;   // habilita la deteccion de movimiento para la primera entrada del jugador
            } else {   // todavia quedan pasos de la secuencia por mostrar
                SimonJoy_MostrarPaso(sj_secuencia[sj_paso_mostrar]);   // prende la flecha del siguiente paso de la secuencia
                sj_mostrando_on = 1;   // vuelve a la sub-fase "encendida"
            }
        }
        break;   // cierra el case SJ_MOSTRANDO
    }

    case SJ_ESPERANDO: {   // esperando que el jugador repita la secuencia paso a paso con el joystick
        uint8_t dir = Joystick_LeerDireccion();   // intenta leer una direccion nueva del joystick este tick
        if (dir == 0xFF) break;   // todavia no hay una entrada nueva, sigue esperando

        SimonJoy_ConfirmarInput(dir);  /* parpadeo real, aunque se repita la misma direccion */
        printf("[SIMONJOY 1P] dir=%u esperado=%u %s\r\n", dir, sj_secuencia[sj_paso_esperado],   // log de la entrada: que se leyo, que se esperaba, y si fue acierto o fallo
               (dir == sj_secuencia[sj_paso_esperado]) ? "OK" : "FALLO");

        if (dir != sj_secuencia[sj_paso_esperado]) {   // la direccion leida NO coincide con la esperada -- fallo
            if ((uint8_t)(sj_longitud - 1) > sj_mejor_racha) sj_mejor_racha = (uint8_t)(sj_longitud - 1);   // si la racha de esta partida (pasos acertados antes de fallar) supera la mejor de la sesion, la actualiza
            printf("[SIMONJOY 1P] GAME OVER racha=%u mejor=%u\r\n", (unsigned)(sj_longitud - 1), sj_mejor_racha);   // log del game over con la racha final y la mejor historica
            sj_fase      = SJ_GAMEOVER;   // pasa a la fase de game over
            sj_tick_fase = ahora;   // marca el instante de entrada a game over
            sj_flash_pendiente = 0xFF;  /* cancela: game over dibuja otra pantalla encima */
            Buzzer_Beep(350);  /* beep largo de error (extiende el corto que ya sonaba) */
            SimonJoy_DibujarGameOver();   // dibuja la pantalla de resultado
            break;   // corta el case SJ_ESPERANDO aca, no sigue evaluando el resto de este bloque (el fallo ya se manejo por completo)
        }

        sj_paso_esperado++;   // acerto este paso: avanza al siguiente paso esperado
        if (sj_paso_esperado >= sj_longitud) {   // el jugador ya repitio TODA la secuencia correctamente
            if (sj_longitud > sj_mejor_racha) sj_mejor_racha = sj_longitud;   // actualiza la mejor racha si corresponde
            if (sj_longitud < SJ_MAX_LONGITUD) {   // todavia hay espacio en el arreglo para un paso mas (limite de seguridad, dificilmente se alcanza jugando)
                sj_secuencia[sj_longitud] = SJ_SiguienteDireccion();   // agrega un paso nuevo aleatorio al final de la secuencia
                sj_longitud++;   // la secuencia crece en 1
                SimonJoy_MostrarRacha(sj_longitud);   // actualiza el numero de ronda en pantalla
            }
            sj_fase      = SJ_ACIERTO;   // pasa a la pausa corta de "bien hecho" antes de repetir la secuencia mas larga
            sj_tick_fase = ahora;   // marca el inicio de esa pausa
            sj_flash_pendiente = 0xFF;   /* cancela cualquier prendido pendiente */
            SimonJoy_MostrarPaso(0xFF);  /* apaga el ultimo boton -- si no, se
                queda encendido durante la pausa y el primer paso de la
                siguiente repeticion no se ve como un flanco nuevo */
            {
                static const PasoSonido_t BEEP_RONDA[3] = {   // jingle corto de "ronda superada": tono-silencio-tono; cambiar estos numeros cambia el ritmo/tono del jingle
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };  /* 2 pitidos cortos */
                Buzzer_Patron(BEEP_RONDA, 3);   // suena el jingle corto de "ronda superada"
            }
        }
        break;   // cierra el case SJ_ESPERANDO
    }

    case SJ_ACIERTO:   // pausa corta despues de completar una ronda, antes de repetir la secuencia (ahora un paso mas larga)
        if ((ahora - sj_tick_fase) < SJ_PAUSA_ACIERTO_MS) break;   // todavia no paso el tiempo de pausa configurado
        sj_paso_mostrar = 0;   // reinicia el indice de reproduccion al primer paso
        sj_mostrando_on = 1;   // arranca de nuevo en la sub-fase "encendida"
        sj_tick_fase    = ahora;   // marca el inicio de la nueva reproduccion
        sj_fase         = SJ_MOSTRANDO;   // vuelve a la fase de mostrar la secuencia (ahora mas larga)
        SimonJoy_MostrarPaso(sj_secuencia[0]);   // prende de una vez la primera flecha de la nueva repeticion
        break;   // cierra el case SJ_ACIERTO

    case SJ_GAMEOVER:   // esperando que el jugador mueva el stick para reintentar (o el atajo de ROJO 2s para salir, manejado aparte en el bucle principal)
        /* Al no existir un pulsador dedicado en el joystick, mover el stick
         * en cualquier direccion reinicia la partida (mismo patron de
         * "cualquier entrada reintenta" utilizado en el modo de botones);
         * la salida mediante B1 se maneja por separado en el bucle
         * principal. */
        if (Joystick_LeerDireccion() != 0xFF) { printf("[SIMONJOY 1P] retry\r\n"); SimonJoy_Iniciar(); }   // cualquier movimiento del stick reinicia una partida nueva
        break;   // cierra el case SJ_GAMEOVER
    }
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — 2 JUGADORES CARA A CARA ============================ */
/* ========================================================================== */
/* Aplica la misma mecanica que la version de 1 jugador descrita arriba,
 * pero duplicada por jugador mediante arreglos de tamaño 2 (indice 0 = J1,
 * en la mitad inferior de la pantalla; indice 1 = J2, en la mitad superior,
 * rotada 180 grados). Cada jugador juega su propia secuencia de forma
 * independiente en su mitad de la pantalla en orientacion retrato (ver
 * Renderer_DrawModoSimonJoystick2P y las funciones Cockpit_* en
 * renderer.c). Este modo se activa al confirmar el modo "JOYS" con la
 * opcion "2 JUGADORES" seleccionada.
 *
 * Nota sobre el cableado: dado que cada joystick fisico puede presentar un
 * desplazamiento de ejes propio de su montaje individual (ver el comentario
 * de Joystick_LeerDireccion para el caso del joystick usado en el modo de 1
 * jugador), los signos de deteccion de direccion de cada joystick en este
 * modo se calibran de forma independiente en Joystick2_LeerDireccion. */

static uint8_t        sj2_secuencia[2][SJ_MAX_LONGITUD];   // secuencia de direcciones de cada jugador, independiente entre si (primer indice = jugador logico)
static uint8_t        sj2_longitud[2];   // cuantos pasos tiene la secuencia de cada jugador
static uint8_t        sj2_paso_mostrar[2];   // indice del paso que se esta parpadeando, por jugador
static uint8_t        sj2_paso_esperado[2];   // indice del paso que se espera del jugador, por jugador
static uint8_t        sj2_mostrando_on[2];   // sub-fase del parpadeo (on/off), por jugador
static uint8_t        sj2_paso_dibujado[2] = { 0xFF, 0xFF };   // ultimo paso pintado en el D-pad de cada jugador (0xFF=ninguno)
static uint8_t        sj2_mejor_racha[2];   // mejor racha de la sesion, por jugador
static SimonJoyFase_t sj2_fase[2];   // fase actual de la maquina de estados de cada jugador (independientes entre si)
static uint32_t       sj2_tick_fase[2];   // tick de inicio de la fase actual, por jugador
static uint32_t       sj2_seed[2] = { 1, 7 };   /* semillas distintas -- secuencias distintas entre jugadores */
static uint8_t        sj2_joy_listo_dir[2];   // 1 = el stick de ese jugador ya volvio al centro y puede contar un nuevo movimiento
static uint8_t        sj2_flash_pendiente[2] = { 0xFF, 0xFF };   // direccion pendiente de prender tras el apagon de confirmacion, por jugador
static uint32_t       sj2_flash_tick[2];   // tick del apagon de confirmacion, por jugador
static uint8_t        en_juego_real_2p = 0;   /* 1 = modo 2 jugadores real, no el recorrido */

static uint8_t SJ2_Random4(uint8_t p) {   // igual que SJ_Random4 pero con la semilla PROPIA del jugador p, para que sus secuencias no coincidan con las del otro
    sj2_seed[p] = sj2_seed[p] * 1103515245u + 12345u;   // mismo LCG que la version 1 jugador, aplicado a la semilla de este jugador
    return (uint8_t)((sj2_seed[p] >> 16) & 0x3u);   // mismos bits "del medio" recortados a 0-3
}

/* mismo anti-repeticion que SJ_SiguienteDireccion, por jugador */
static uint8_t SJ2_SiguienteDireccion(uint8_t p) {   // elige el proximo paso de la secuencia del jugador p, evitando una 3ra repeticion seguida
    uint8_t nuevo = SJ2_Random4(p);   // candidato aleatorio inicial
    if (sj2_longitud[p] >= 2 &&   // solo puede haber 3ra repeticion si ya hay al menos 2 pasos previos de ESTE jugador
        sj2_secuencia[p][sj2_longitud[p] - 1] == sj2_secuencia[p][sj2_longitud[p] - 2] &&   // sus 2 ultimos pasos ya son iguales entre si
        nuevo == sj2_secuencia[p][sj2_longitud[p] - 1]) {   // los 2 ultimos pasos de ESTE jugador ya son iguales y el candidato tambien coincide
        nuevo = (uint8_t)((nuevo + 1u + (SJ2_Random4(p) % 3u)) & 0x3u);   // fuerza un valor distinto, elegido uniforme entre las otras 3 direcciones
    }
    return nuevo;   // direccion final (original o forzada) para el siguiente paso de la secuencia de este jugador
}

static uint16_t SimonJoy2_IntervaloActual(uint8_t p) {   // misma formula que SimonJoy_IntervaloActual pero con la longitud de secuencia PROPIA del jugador p
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj2_longitud[p] - 1) * SJ_VELOCIDAD_PASO;   // velocidad de ESTE jugador, segun su propia ronda
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;   // mismo tope maximo compartido entre los 2 jugadores
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);   // intervalo resultante para ESTE jugador
}

/* Aplica la misma logica de deteccion de flanco por zona de disparo que
 * Joystick_LeerDireccion, pero recibe el jugador logico p (0 o 1) y lo
 * traduce al joystick fisico correspondiente mediante JugadorFisico(). */
static uint8_t Joystick2_LeerDireccion(uint8_t p) {   // igual que Joystick_LeerDireccion pero para el jugador logico p (0 o 1), traduciendo a que joystick fisico leer
    /* Unicamente la lectura de hardware pasa por la variable "hw" (ver
     * JugadorFisico() y el comentario de la tabla BTN_SW mas arriba); el
     * arreglo sj2_joy_listo_dir[] permanece indexado por el jugador logico
     * "p", ya que representa estado de juego y no de hardware. */
    uint8_t hw = JugadorFisico(p);   // traduce jugador logico a joystick fisico a leer (0=hardware J1, 1=hardware J2)
    uint16_t jx = (hw == 0) ? joystick_x : joystick2_x;   // lectura cruda del eje X del joystick fisico que le toca a este jugador
    uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;   // idem para el eje Y
    uint16_t bajo_y, alto_y, bajo_x, alto_x;   // rango "sin direccion" de cada eje
    Joy_Umbrales((hw == 0) ? centro_j1y : centro_j2y, &bajo_y, &alto_y);   // calcula el rango del eje Y usando el centro medido del joystick fisico correcto
    Joy_Umbrales((hw == 0) ? centro_j1x : centro_j2x, &bajo_x, &alto_x);   // idem para el eje X

    if (!sj2_joy_listo_dir[p]) {   // este jugador todavia no volvio al centro tras su ultimo movimiento
        if (jy < alto_y && jy > bajo_y &&   // eje Y de este jugador dentro de su banda muerta
            jx < alto_x && jx > bajo_x) {   // Y el eje X tambien dentro de su banda muerta
            sj2_joy_listo_dir[p] = 1;   // habilita el proximo movimiento de ESTE jugador
        }
        return 0xFF;   // este tick no cuenta como movimiento nuevo para este jugador
    }
    /* Se utiliza el eje dominante (no el primero evaluado): un empuje
     * impreciso del joystick puede superar el umbral de ambos ejes
     * simultaneamente, por lo que debe prevalecer el eje que se encuentre
     * mas alejado de su centro medido. */
    uint16_t centro_x = (hw == 0) ? centro_j1x : centro_j2x;   // centro medido del eje X del joystick fisico de este jugador
    uint16_t centro_y = (hw == 0) ? centro_j1y : centro_j2y;   // idem eje Y
    int32_t dev_x = (int32_t)jx - (int32_t)centro_x;   // desviacion del canal "X" del ADC respecto a su centro medido
    int32_t dev_y = (int32_t)jy - (int32_t)centro_y;   // desviacion del canal "Y" del ADC respecto a su centro medido

    /* El joystick fisico J2 (hw==1) tiene los ejes electricos cruzados
     * respecto a la orientacion visual del montaje -- la MISMA
     * caracteristica ya documentada y usada en Joystick_LeerDireccion (modo
     * de 1 jugador, que SIEMPRE lee este mismo joystick fisico, ver su
     * comentario mas arriba): el canal "X" controla el movimiento VERTICAL y
     * el canal "Y" el LATERAL. El fisico J1 (hw==0), confirmado correcto tal
     * cual esta, no tiene ese cruce: cada canal controla su eje "natural". */
    int32_t dev_vert  = (hw == 1) ? dev_y : dev_x;   // vertical: canal X si es el fisico J2 (cruzado), canal Y si es el fisico J1
    int32_t dev_horiz = (hw == 1) ? dev_x : dev_y;   // lateral: el canal que no se uso arriba
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;   // magnitud vertical
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;   // magnitud horizontal

    /* Una version anterior invertia tambien el signo de ARRIBA/ABAJO a
     * proposito respecto a Joystick_LeerDireccion (1 jugador) -- se
     * confirmo con el usuario en hardware real que esa inversion vertical
     * estaba mal (empujar hacia ARRIBA se registraba como ABAJO en los 2
     * fisicos, ver el signo ya corregido mas abajo). IZQUIERDA/DERECHA
     * (dev_horiz) tambien comparte la misma polaridad entre los 2 fisicos
     * una vez identificado el canal correcto -- una version anterior asumia
     * que el fisico J1 necesitaba el signo opuesto al J2 por una supuesta
     * diferencia de cableado real, pero esa asuncion resulto falsa (ver el
     * comentario junto al signo mas abajo). */
    uint8_t dir = 0xFF;   // sin direccion detectada todavia
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {   // vertical supero el umbral y es el eje dominante
        /* CORREGIDO (confirmado con el usuario en hardware real, los 2
         * fisicos): la inversion vertical de abajo estaba al reves --
         * empujar el stick fisico hacia ARRIBA se registraba como ABAJO y
         * viceversa, en ambos jugadores, causando fallos/game over con el
         * stick visualmente apuntando a la direccion correcta. Misma
         * polaridad (sin invertir) que Joystick_LeerDireccion, el modo de 1
         * jugador que ya esta confirmado correcto. */
    	dir = (dev_vert > 0) ? 0 : 1;    /* ABAJO cuando el eje vertical crece, ARRIBA cuando decrece */
    	sj2_joy_listo_dir[p] = 0;   // bloquea nuevas detecciones para este jugador hasta que vuelva al centro
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {   // horizontal supero su umbral
        /* CORREGIDO (2026-08-02, confirmado con el usuario en hardware real
         * con una prueba de las 4 direcciones una por una, comparando contra
         * el cursor "X" que ya se sabe correcto): la version anterior de
         * esta linea distinguia por "hw" con signos opuestos entre los 2
         * fisicos (comentario viejo: "el canal Y de J2 y el canal X de J1 no
         * comparten el mismo sentido fisico"), asumiendo un cableado
         * distinto entre ambos joystick. Esa asuncion resulto ser falsa en
         * el hardware real ya armado: para el fisico J1 (hw==0), empujar a
         * la DERECHA se registraba como IZQUIERDA y viceversa (verificado
         * con el cursor llegando al borde derecho de su caja mientras la
         * consola decia IZQUIERDA). Los 2 fisicos comparten la MISMA
         * polaridad una vez que dev_horiz ya identifica el canal correcto
         * (ver dev_horiz arriba) -- no hace falta distinguir por hw aca. */
    	dir = (dev_horiz > 0) ? 3 : 2;   /* IZQUIERDA cuando crece, DERECHA cuando decrece -- misma polaridad para los 2 fisicos */
    	sj2_joy_listo_dir[p] = 0;   // bloquea nuevas detecciones para este jugador hasta que vuelva al centro
    }
    if (dir != 0xFF) printf("[INPUT] joystick J%u = %s\r\n", (unsigned)(hw + 1), DIR_NOMBRE[dir]);   // log con el numero de joystick FISICO (hw+1), no el jugador logico
    return dir;   // direccion detectada (0-3) o 0xFF si ninguna
}

static void SimonJoy2_MostrarPaso(uint8_t p, uint8_t nuevo) {   // igual que SimonJoy_MostrarPaso pero para la mitad de pantalla del jugador p
    if (nuevo == sj2_paso_dibujado[p]) return;   // ya esta pintado asi, no redibuja
    Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], nuevo);   // redibuja solo las 2 flechas que cambiaron, en la mitad de este jugador
    sj2_paso_dibujado[p] = nuevo;   // guarda el nuevo estado pintado de este jugador
    if (nuevo < 4) Buzzer_Beep(90);   // beep corto al prender una flecha (compartido entre los 2 jugadores, el buzzer es mono)
}

static void SimonJoy2_ConfirmarInput(uint8_t p, uint8_t dir) {   // igual que SimonJoy_ConfirmarInput pero para el jugador p
    if (sj2_paso_dibujado[p] != 0xFF) {   // si este jugador tenia algo pintado
        Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], 0xFF);   // lo apaga ya
        sj2_paso_dibujado[p] = 0xFF;   // marca que este jugador no tiene nada pintado ahora
    }
    sj2_flash_pendiente[p] = dir;   // guarda que direccion prender despues del apagon, para este jugador
    sj2_flash_tick[p]      = HAL_GetTick();   // marca el instante del apagon de este jugador
    Buzzer_Beep(90);   // beep corto de confirmacion, compartido entre los 2 jugadores (buzzer mono)
}

/* Inicia (o reinicia) unicamente al jugador p, sin afectar la pantalla del otro jugador. */
static void SimonJoy2_ReiniciarJugador(uint8_t p) {   // reinicia SOLO al jugador p (tras perder) sin tocar la partida en curso del otro
    sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 3u);   // remezcla la semilla de este jugador con el tick actual (mas un offset por jugador, para que no coincidan si arrancan en el mismo tick)
    if (sj2_seed[p] == 0) sj2_seed[p] = 1;   // evita semilla en 0

    sj2_longitud[p]        = 1;   // secuencia nueva de 1 paso
    sj2_secuencia[p][0]    = SJ2_Random4(p);   // primer paso aleatorio
    sj2_paso_mostrar[p]    = 0;   // arranca mostrando desde el paso 0
    sj2_mostrando_on[p]    = 1;   // arranca en la mitad "encendida"
    sj2_fase[p]            = SJ_MOSTRANDO;   // arranca reproduciendo la secuencia
    sj2_tick_fase[p]       = HAL_GetTick();   // marca el inicio de esta fase
    sj2_joy_listo_dir[p]   = 1;   // habilita deteccion de movimiento de entrada
    sj2_flash_pendiente[p] = 0xFF;   // sin flash pendiente

    Renderer_DrawModoSimonJoystick2PJugador(p, sj2_secuencia[p][0]);   // redibuja SOLO la mitad de pantalla de este jugador
    sj2_paso_dibujado[p] = sj2_secuencia[p][0];   // sincroniza el paso pintado
    Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);   // actualiza el numero de ronda mostrado de este jugador
    Renderer_ResetCursorJoystick2P(p);   // vuelve el cursor del centro del D-pad de este jugador a neutral
}

/* Dibuja ambas mitades de la pantalla desde cero (arranque de una partida nueva). */
static void SimonJoy2_IniciarAmbos(void) {   // arranca una partida nueva de SimonJoy 2 jugadores, ambos lados desde cero
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // arranca la musica de fondo compartida
    ILI9341_SetPortrait(1);   // pasa la pantalla a orientacion retrato (layout cara a cara)
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    for (uint8_t p = 0; p < 2; p++) {   // inicializa el estado de ambos jugadores
        sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 1u);   // remezcla la semilla de cada jugador (offset distinto al de ReiniciarJugador, para variar aun mas)
        if (sj2_seed[p] == 0) sj2_seed[p] = 1;   // evita semilla en 0

        sj2_longitud[p]        = 1;   // secuencia nueva de 1 paso
        sj2_secuencia[p][0]    = SJ2_Random4(p);   // primer paso aleatorio de este jugador
        sj2_paso_mostrar[p]    = 0;   // arranca mostrando desde el paso 0
        sj2_mostrando_on[p]    = 1;   // arranca en la mitad "encendida" del parpadeo
        sj2_fase[p]            = SJ_MOSTRANDO;   // arranca reproduciendo la secuencia
        sj2_tick_fase[p]       = HAL_GetTick();   // marca el inicio de esta fase
        sj2_joy_listo_dir[p]   = 1;   // habilita deteccion de movimiento de entrada
        sj2_flash_pendiente[p] = 0xFF;   // sin flash pendiente
        sj2_paso_dibujado[p]   = sj2_secuencia[p][0];   // sincroniza el paso pintado con el primero de la secuencia
    }
    Renderer_DrawModoSimonJoystick2P(sj2_secuencia[0][0], sj2_secuencia[1][0]);   // dibuja las 2 mitades completas de una vez, cada una con su primer paso encendido
    Renderer_ActualizarRachaJoystick2P(0, sj2_longitud[0]);   // numero de ronda inicial de jugador 0
    Renderer_ActualizarRachaJoystick2P(1, sj2_longitud[1]);   // numero de ronda inicial de jugador 1
    Renderer_ResetCursorJoystick2P(0);   // cursor neutral del D-pad de jugador 0
    Renderer_ResetCursorJoystick2P(1);   // cursor neutral del D-pad de jugador 1
}

/* Tick no bloqueante de un jugador; se invoca 2 veces por cada vuelta del
 * bucle principal (una por jugador) mientras en_juego_real_2p este activo. */
static void SimonJoy2_ActualizarJugador(uint8_t p) {   // igual que SimonJoy_Actualizar pero para un jugador (p); se llama 2 veces por ciclo, una por jugador (ver SimonJoy2_Actualizar)
    uint32_t ahora = HAL_GetTick();   // tick actual, usado en todas las fases

    if (sj2_fase[p] != SJ_GAMEOVER) {   // en game over de este jugador se muestra otra pantalla, no tiene sentido mover su cursor
        /* El primer parametro de Renderer_ActualizarCursorJoystick2P
         * corresponde siempre al jugador logico "p" (determina en que
         * mitad de la pantalla se dibuja); unicamente el joystick fisico
         * de origen (jx/jy) cambia segun la correspondencia logico/fisico
         * (ver JugadorFisico() mas arriba). */
        uint8_t hw = JugadorFisico(p);   // traduce a joystick fisico correspondiente
        uint16_t jx = (hw == 0) ? joystick_x : joystick2_x;   // lectura cruda del canal "X" de ese joystick fisico
        uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;   // lectura cruda del canal "Y"
        /* CORREGIDO (2026-08-02): Renderer_ActualizarCursorJoystick2P ya
         * aplica POR SI SOLA el mismo cruce de canales que usa el cursor del
         * modo de 1 jugador para el fisico J2 (px sale de joy_y, py sale de
         * joy_x -- ver su implementacion en renderer.c). La version anterior
         * de este bloque volvia a cruzar jx/jy ANTES de llamarla, aplicando
         * el cruce 2 veces: para hw==1 eso anulaba el cruce (dejaba el
         * cursor como si fuera un joystick sin cruzar) y para hw==0 metia un
         * cruce que ese fisico no tiene, dejando los ejes horizontal/vertical
         * de la "X" en pantalla intercambiados entre si en los 2 jugadores
         * (mover el stick arriba/abajo desplazaba la X a los lados y
         * viceversa) -- la logica real de acierto/fallo (Joystick2_LeerDir-
         * eccion) no pasa por aca y no se vio afectada, pero al guiarse
         * visualmente por esta X el jugador terminaba empujando el stick en
         * una direccion fisica distinta a la que el juego registraba. Ahora
         * se le pasan los canales crudos en el mismo orden que usa el modo
         * de 1 jugador para el fisico J2 (hw==1: jx,jy tal cual, igual que
         * Renderer_ActualizarCursorJoystick(joystick2_x, joystick2_y, ...));
         * para el fisico J1 (hw==0, sin cruce electrico real) se le pasan
         * intercambiados (jy,jx) para cancelar el cruce que la funcion ya
         * aplica internamente y dejarlo sin cruzar.
         *
         * Ademas (mismo dia, mismo problema visual reportado por el
         * usuario), jx/jy se reescalan con Joy_CursorEscala usando el
         * centro medido de CADA eje antes de pasarlos: sin esto el cursor
         * quedaba descentrado en reposo y un empuje real apenas lo corria
         * unos pixeles dentro de su caja (ver comentario de
         * Joy_CursorEscala), lo cual se percibia como que se movia para
         * cualquier lado en vez de en linea recta hacia donde se empujaba
         * el stick, aun con el cruce de ejes ya corregido. */
        uint16_t centro_x = (hw == 0) ? centro_j1x : centro_j2x;   // centro medido del canal "X" de ese fisico
        uint16_t centro_y = (hw == 0) ? centro_j1y : centro_j2y;   // idem canal "Y"
        uint16_t sx = Joy_CursorEscala(jx, centro_x);   // canal "X" reescalado y centrado
        uint16_t sy = Joy_CursorEscala(jy, centro_y);   // canal "Y" reescalado y centrado
        if (hw == 1) {   // fisico J2: tiene el cruce electrico real (ver comentario largo arriba) -- se le pasan sx/sy intercambiados para cancelarlo
            Renderer_ActualizarCursorJoystick2P(p, sy, sx, sj2_joy_listo_dir[p]); // Intercambiado sx <-> sy
        } else {   // fisico J1: sin cruce electrico real -- se le pasan sx/sy tal cual
            Renderer_ActualizarCursorJoystick2P(p, sx, sy, sj2_joy_listo_dir[p]); // Intercambiado sy <-> sx
        }
    }

    if (sj2_flash_pendiente[p] != 0xFF && (ahora - sj2_flash_tick[p]) >= SJ_FLASH_INPUT_MS) {   // hay un prendido pendiente de este jugador y ya paso el tiempo minimo
        SimonJoy2_MostrarPaso(p, sj2_flash_pendiente[p]);   // prende la flecha confirmada de este jugador
        sj2_flash_pendiente[p] = 0xFF;   // consume el pendiente, para no repetirlo el proximo tick
    }

    switch (sj2_fase[p]) {   // logica de la fase actual de ESTE jugador (independiente del otro)
    case SJ_MOSTRANDO: {   // reproduciendo la secuencia de ESTE jugador
        uint16_t medio = (uint16_t)(SimonJoy2_IntervaloActual(p) / 2U);   // mitad del intervalo actual de este jugador
        if ((ahora - sj2_tick_fase[p]) < medio) break;   // todavia no toca cambiar de sub-fase para este jugador
        sj2_tick_fase[p] = ahora;   // marca el inicio de la nueva sub-fase

        if (sj2_mostrando_on[p]) {   // estaba encendido -- toca apagar
            SimonJoy2_MostrarPaso(p, 0xFF);   // apaga la flecha actual de este jugador
            sj2_mostrando_on[p] = 0;   // pasa a la sub-fase apagada
        } else {   // estaba apagado -- toca avanzar de paso
            sj2_paso_mostrar[p]++;   // avanza al siguiente paso de la secuencia de ESTE jugador
            if (sj2_paso_mostrar[p] >= sj2_longitud[p]) {   // ya mostro toda su secuencia
                sj2_fase[p]          = SJ_ESPERANDO;   // pasa a esperar la respuesta de este jugador
                sj2_paso_esperado[p] = 0;   // debe repetir desde el primer paso
                sj2_joy_listo_dir[p] = 1;   // habilita la deteccion de su primera entrada
            } else {   // todavia quedan pasos de su secuencia por mostrar
                SimonJoy2_MostrarPaso(p, sj2_secuencia[p][sj2_paso_mostrar[p]]);   // prende el siguiente paso de SU secuencia
                sj2_mostrando_on[p] = 1;   // vuelve a la sub-fase encendida
            }
        }
        break;   // cierra el case SJ_MOSTRANDO
    }

    case SJ_ESPERANDO: {   // esperando la respuesta de ESTE jugador
        uint8_t dir = Joystick2_LeerDireccion(p);   // intenta leer una direccion nueva del joystick fisico de ESTE jugador
        if (dir == 0xFF) break;   // todavia nada nuevo

        SimonJoy2_ConfirmarInput(p, dir);   // parpadeo real de confirmacion, aunque se repita la misma direccion
        printf("[SIMONJOY 2P] P%u dir=%u esperado=%u %s\r\n", p, dir, sj2_secuencia[p][sj2_paso_esperado[p]],   // log con el jugador logico, la direccion leida, la esperada y si acerto
               (dir == sj2_secuencia[p][sj2_paso_esperado[p]]) ? "OK" : "FALLO");

        if (dir != sj2_secuencia[p][sj2_paso_esperado[p]]) {   // fallo de ESTE jugador (no afecta al otro)
            if ((uint8_t)(sj2_longitud[p] - 1) > sj2_mejor_racha[p]) sj2_mejor_racha[p] = (uint8_t)(sj2_longitud[p] - 1);   // actualiza la mejor racha de este jugador si corresponde
            printf("[SIMONJOY 2P] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);   // log del game over con la racha final y la mejor historica de este jugador
            sj2_fase[p]            = SJ_GAMEOVER;   // SOLO este jugador pasa a game over, el otro sigue jugando su propia partida
            sj2_tick_fase[p]       = ahora;   // marca el instante de entrada a game over de este jugador
            sj2_flash_pendiente[p] = 0xFF;   // cancela cualquier prendido pendiente, game over dibuja otra pantalla encima
            Buzzer_Beep(350);   // beep largo de error
            Renderer_DibujarGameOverJoystick2P(p, (uint16_t)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);   // dibuja el resultado SOLO en la mitad de este jugador
            break;   // corta el case SJ_ESPERANDO aca, el fallo ya se manejo por completo
        }

        sj2_paso_esperado[p]++;   // acerto: avanza al siguiente paso esperado de este jugador
        if (sj2_paso_esperado[p] >= sj2_longitud[p]) {   // este jugador repitio TODA su secuencia
            if (sj2_longitud[p] > sj2_mejor_racha[p]) sj2_mejor_racha[p] = sj2_longitud[p];   // actualiza la mejor racha de este jugador si corresponde
            if (sj2_longitud[p] < SJ_MAX_LONGITUD) {   // todavia hay espacio en el arreglo para un paso mas
                sj2_secuencia[p][sj2_longitud[p]] = SJ2_SiguienteDireccion(p);   // agrega un paso nuevo a la secuencia de ESTE jugador
                sj2_longitud[p]++;   // la secuencia de este jugador crece en 1
                Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);   // actualiza el numero de ronda en la mitad de este jugador
            }
            sj2_fase[p]            = SJ_ACIERTO;   // pausa corta antes de repetir, solo para este jugador
            sj2_tick_fase[p]       = ahora;   // marca el inicio de esa pausa
            sj2_flash_pendiente[p] = 0xFF;   // cancela cualquier prendido pendiente
            SimonJoy2_MostrarPaso(p, 0xFF);   // apaga el ultimo boton de este jugador antes de la pausa
            {
                static const PasoSonido_t BEEP_RONDA[3] = {   // jingle corto de ronda superada; cambiar estos numeros cambia el ritmo/tono
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };
                Buzzer_Patron(BEEP_RONDA, 3);   // jingle corto de ronda superada (compartido, el buzzer es mono)
            }
        }
        break;   // cierra el case SJ_ESPERANDO
    }

    case SJ_ACIERTO:   // pausa corta de este jugador despues de completar una ronda
        if ((ahora - sj2_tick_fase[p]) < SJ_PAUSA_ACIERTO_MS) break;   // todavia no paso la pausa de este jugador
        sj2_paso_mostrar[p] = 0;   // reinicia el indice de reproduccion al primer paso
        sj2_mostrando_on[p] = 1;   // arranca de nuevo en la sub-fase encendida
        sj2_tick_fase[p]    = ahora;   // marca el inicio de la nueva reproduccion
        sj2_fase[p]         = SJ_MOSTRANDO;   // vuelve a la fase de mostrar la secuencia (ahora mas larga)
        SimonJoy2_MostrarPaso(p, sj2_secuencia[p][0]);   // prende de una vez el primer paso de la nueva repeticion de ESTE jugador
        break;   // cierra el case SJ_ACIERTO

    case SJ_GAMEOVER:   // esperando que ESTE jugador mueva su stick para reintentar (no afecta al otro)
        /* Al no existir un pulsador dedicado en el joystick, mover el
         * propio stick en cualquier direccion reinicia unicamente la
         * partida de ese jugador; la salida mediante B1 sigue disponible
         * para ambos lados y se maneja en el bucle principal. */
        if (Joystick2_LeerDireccion(p) != 0xFF) { printf("[SIMONJOY 2P] P%u retry\r\n", p); SimonJoy2_ReiniciarJugador(p); }   // solo el propio stick de este jugador lo reintenta, no afecta al otro
        break;   // cierra el case SJ_GAMEOVER
    }
}

static void SimonJoy2_Actualizar(void) {   // tick de la partida 2 jugadores: actualiza a cada jugador por separado, uno tras otro
    SimonJoy2_ActualizarJugador(0);   // procesa un tick de la logica del jugador 0
    SimonJoy2_ActualizarJugador(1);   // procesa un tick de la logica del jugador 1
}

/* ========================================================================== */
/* === BOTONES ARCADE — 2 JUGADORES, CADA UNO CON SUS 4 BOTONES ============== */
/* ========================================================================== */
/* Aplica la misma mecanica y el mismo layout cara a cara (retrato, cockpit)
 * que Simon + Joystick a 2 jugadores descrito arriba, con la diferencia de
 * que el color lo entrega un boton fisico real, con su propio LED (ver
 * Boton_LED y Botones_LeerColor), en lugar del joystick. Cada jugador juega
 * su propia secuencia de forma independiente en su propia mitad de la
 * pantalla. */

static uint8_t        btn_secuencia[2][SJ_MAX_LONGITUD];   // secuencia de colores (0-3) de cada jugador de Botones, independiente entre si
static uint8_t        btn_longitud[2];   // cuantos pasos tiene la secuencia de cada jugador
static uint8_t        btn_paso_mostrar[2];   // indice del paso que se esta parpadeando, por jugador
static uint8_t        btn_paso_esperado[2];   // indice del paso que se espera del jugador, por jugador
static uint8_t        btn_mostrando_on[2];   // sub-fase del parpadeo (on/off), por jugador
static uint8_t        btn_paso_dibujado[2] = { 0xFF, 0xFF };   // ultimo boton pintado/encendido de cada jugador (0xFF=ninguno)
static uint8_t        btn_mejor_racha[2];   // mejor racha de la sesion, por jugador
static SimonJoyFase_t btn_fase[2];   // fase actual de la maquina de estados de cada jugador
static uint32_t       btn_tick_fase[2];   // tick de inicio de la fase actual, por jugador
static uint32_t       btn_seed[2] = { 3, 11 };   /* semillas distintas de las de SJ2 */
static uint8_t        btn_flash_pendiente[2] = { 0xFF, 0xFF };   // color pendiente de prender tras el apagon de confirmacion, por jugador
static uint32_t       btn_flash_tick[2];   // tick del apagon de confirmacion, por jugador
static uint8_t        en_juego_real_botones = 0;   /* 1 = modo BOTONES real (1 o 2 jugadores) */
static uint8_t        en_juego_real_guitar = 0;    /* 1 = modo GUITAR HERO real (1 o 2 jugadores) */

/* El joystick limita de forma natural el ritmo de entrada, ya que
 * Joystick_LeerDireccion() exige que el stick salga de la zona muerta en
 * ambos ejes antes de contar el siguiente movimiento; el jugador no puede
 * encadenar aciertos mas rapido que el tiempo que tarda el stick en volver
 * al centro. El boton arcade no posee ese freno fisico (soltar y volver a
 * presionar es casi instantaneo), por lo que sin una limitacion equivalente
 * las rondas se completarian mucho mas rapido, alcanzando longitudes altas
 * (donde Botones_IntervaloActual ya esta en su tope de velocidad) de forma
 * prematura, y el ritmo de juego percibido diferiria notablemente entre
 * ambos modos. BTN_REARME_MIN_MS establece un tiempo de espera minimo entre
 * 2 entradas aceptadas de un mismo jugador, emulando artificialmente ese
 * freno. */
#define BTN_REARME_MIN_MS 250U   // ms minimos entre 2 entradas aceptadas de un mismo jugador de Botones; bajarlo permite golpear mas rapido pero acerca el ritmo al de un boton "instantaneo" sin freno
static uint32_t       btn_ultimo_input_tick[2];   // tick de la ultima entrada aceptada, por jugador

/* Cooldown equivalente al de arriba, pero propio de Guitar Hero (arreglo
 * separado de btn_ultimo_input_tick para no compartir estado con Simon con
 * botones, aunque los 2 modos nunca corran al mismo tiempo). En Guitar Hero
 * el timing importa al milimetro -- hay que golpear notas que cruzan la
 * zona de golpe a velocidad constante, muchas veces una detras de otra
 * rapido -- asi que el freno es mucho mas corto que BTN_REARME_MIN_MS: solo
 * lo suficiente para no contar un rebote electrico del switch como 2
 * golpes distintos, sin notarse como demora para los dedos. */
#define GH_REARME_MIN_MS 20U   // 20ms: Ultra rapido para detectar toques seguidos sin congelar los botones -- OJO: el loop principal corre a RENDER_TICK_MS=33ms, asi que en la practica el muestreo real nunca es mas fino que eso, bajar este numero por debajo de 33 no acelera mas alla del tick del loop
static uint32_t       gh_ultimo_input_tick[2];   // tick de la ultima entrada aceptada de Guitar Hero, por jugador

/* Tiempo de espera minimo en la pantalla de GAME OVER antes de aceptar un
 * reintento: sin este margen, un boton que rebota justo al perder la
 * partida (o que el jugador aun mantiene presionado) podria reiniciar la
 * ronda antes de que la pantalla de resultado llegue a mostrarse. */
#define BTN_GAMEOVER_COOLDOWN_MS 1000U   // milisegundos de espera obligatoria en game over antes de aceptar un boton como reintento; bajarlo permite reintentar mas rapido pero mas riesgo de reiniciar sin querer

static uint8_t Botones_Random4(uint8_t p) {   // mismo LCG que SJ_Random4/SJ2_Random4 pero con la semilla propia de BOTONES del jugador p
    btn_seed[p] = btn_seed[p] * 1103515245u + 12345u;   // mismo LCG que el resto del proyecto, semilla propia de este jugador
    return (uint8_t)((btn_seed[p] >> 16) & 0x3u);   // bits "del medio" recortados a 0-3
}

static uint8_t Botones_SiguienteColor(uint8_t p) {   // elige el proximo color de la secuencia del jugador p, evitando una 3ra repeticion seguida (mismo criterio que SJ_SiguienteDireccion)
    uint8_t nuevo = Botones_Random4(p);   // candidato aleatorio inicial
    if (btn_longitud[p] >= 2 &&   // solo puede haber 3ra repeticion si ya hay al menos 2 pasos previos
        btn_secuencia[p][btn_longitud[p] - 1] == btn_secuencia[p][btn_longitud[p] - 2] &&   // los 2 ultimos colores ya son iguales entre si
        nuevo == btn_secuencia[p][btn_longitud[p] - 1]) {   // Y el candidato tambien coincide (seria una 3ra repeticion)
        nuevo = (uint8_t)((nuevo + 1u + (Botones_Random4(p) % 3u)) & 0x3u);   // fuerza un color distinto, elegido uniforme entre los otros 3
    }
    return nuevo;   // color final para el siguiente paso de la secuencia
}

static uint16_t Botones_IntervaloActual(uint8_t p) {   // misma formula de velocidad que SimonJoy, aplicada a la longitud de secuencia de BOTONES del jugador p
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(btn_longitud[p] - 1) * SJ_VELOCIDAD_PASO;   // sube linealmente con la ronda, igual formula que SimonJoy_IntervaloActual
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;   // mismo tope maximo compartido con SimonJoy
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);   // a mayor velocidad, menor intervalo
}

static void Botones_MostrarColor(uint8_t p, uint8_t nuevo) {   // cambia cual boton esta "encendido" en pantalla Y en el LED fisico real (nuevo=0-3, o 0xFF=ninguno)
    if (nuevo == btn_paso_dibujado[p]) return;   // ya esta asi, no hace nada
    if (btn_paso_dibujado[p] < 4) Boton_LED(p, btn_paso_dibujado[p], 0);   // apaga el LED fisico del boton que estaba prendido antes (si habia uno)
    if (nuevo < 4) Boton_LED(p, nuevo, 1);   // prende el LED fisico del nuevo boton (si corresponde prender alguno)
    if (btn_modo_1p) Renderer_UpdateModoSimonClasico1PPaso(btn_paso_dibujado[p], nuevo);   // layout de pantalla completa (1 jugador)
    else             Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], nuevo);   // layout Cockpit dividido (2 jugadores)
    btn_paso_dibujado[p] = nuevo;   // guarda el nuevo estado pintado
    if (nuevo < 4) Buzzer_Beep(90);   // beep corto al prender un boton
}

/* Ciclo real de apagado y encendido (no instantaneo) para que 2 presiones
 * consecutivas del mismo color se perciban como 2 confirmaciones distintas;
 * aplica el mismo criterio que SimonJoy2_ConfirmarInput, extendido aqui al
 * LED fisico ademas de la pantalla. */
static void Botones_ConfirmarInput(uint8_t p, uint8_t color) {   // apaga YA el boton actual (LED + pantalla) y programa el prendido del nuevo color tras el apagon, igual criterio que SimonJoy2_ConfirmarInput
    if (btn_paso_dibujado[p] != 0xFF) {   // si habia algo prendido
        Boton_LED(p, btn_paso_dibujado[p], 0);   // apaga el LED fisico
        if (btn_modo_1p) Renderer_UpdateModoSimonClasico1PPaso(btn_paso_dibujado[p], 0xFF);   // apaga el domo en el layout de pantalla completa
        else             Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], 0xFF);   // apaga el badge en el layout Cockpit dividido
        btn_paso_dibujado[p] = 0xFF;   // marca que este jugador no tiene nada pintado ahora
    }
    btn_flash_pendiente[p] = color;   // guarda que color prender despues del apagon
    btn_flash_tick[p]      = HAL_GetTick();   // marca el instante del apagon
    Buzzer_Beep(90);   // beep corto de confirmacion
}

/* Numero de ronda a pantalla completa (1 jugador) -- misma idea que
 * SimonJoy_MostrarRacha, pero en RETRATO (240 de ancho, no LCD_W=320 que es
 * el ancho de paisaje) y con el estilo de color de BOTONES (amarillo sobre
 * gris oscuro, igual que Renderer_ActualizarRachaBotones). Redibuja solo la
 * esquina, sin tocar el resto de la pantalla. */
static void Botones_MostrarRacha1P(uint16_t racha) {   // redibuja solo el numero de ronda en la esquina, pantalla completa 1 jugador
    char buf[12];   // buffer para el texto "RONDA:NN"
    snprintf(buf, sizeof(buf), "RONDA:%2u", racha);   // ancho fijo de 2 digitos
    Texto_Sanear(buf);   // defensivo, aca siempre son digitos
    ILI9341_FillRect(240 - 80, 0, 80, 26, COLOR_DARKGRAY);   // borra solo el recuadro de la esquina superior derecha (ancho de RETRATO = 240)
    ILI9341_DrawString(240 - 74, 9, buf, COLOR_YELLOW, COLOR_DARKGRAY, 1);   // dibuja el texto nuevo en amarillo sobre fondo gris oscuro
}

/* Pantalla de GAME OVER a pantalla completa (1 jugador) de BOTONES -- mismo
 * patron que SimonJoy_DibujarGameOver, adaptado a retrato 240x320. */
static void Botones_DibujarGameOver1P(uint16_t racha, uint16_t mejor) {   // dibuja la pantalla de GAME OVER a pantalla completa, 1 jugador
    char linea[32];   // buffer temporal para armar cada linea con snprintf

    /* NO tocar la orientacion aca: el modo 1 jugador de Botones se mantiene
     * en retrato (240x320) durante TODA la partida (ver Botones_IniciarSolo),
     * esta pantalla es solo el final, no un cambio de modo. Forzar paisaje
     * giraba la pantalla fisica a mitad de partida sin motivo. */
    ILI9341_FillScreen(COLOR_BLACK);   // borra toda la pantalla antes de dibujar el resultado

    ILI9341_DrawString(30, 80, "GAME OVER", COLOR_RED, COLOR_BLACK, 3);   // titulo grande en rojo

    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)racha);   // racha de ESTA partida
    Texto_Sanear(linea);
    ILI9341_DrawString(50, 140, linea, COLOR_WHITE, COLOR_BLACK, 2);

    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)mejor);   // mejor racha de TODA la sesion
    Texto_Sanear(linea);
    ILI9341_DrawString(50, 165, linea, COLOR_YELLOW, COLOR_BLACK, 2);

    ILI9341_DrawString(10, 210, "V/A/AM=reintentar", COLOR_GRAY, COLOR_BLACK, 1);   // ROJO queda reservado para el atajo de salida (ver rojo_reservado en Botones_ActualizarJugador)
    ILI9341_DrawString(10, 223, "ROJO 2s=menu de modos", COLOR_GRAY, COLOR_BLACK, 1);   // atajo de salida, ver SalirGameOver1P/2P_Detectado
}
static void Botones_ReiniciarJugador(uint8_t p) {   // reinicia SOLO al jugador p tras perder, sin tocar al otro
    btn_seed[p] ^= (HAL_GetTick() + p * 977u + 5u);   // remezcla la semilla de este jugador (offset 5, distinto al resto de las funciones "reiniciar/iniciar" para variar mas)
    if (btn_seed[p] == 0) btn_seed[p] = 1;   // evita semilla en 0

    btn_longitud[p]        = 1;   // secuencia nueva de 1 paso
    btn_secuencia[p][0]    = Botones_Random4(p);   // primer color aleatorio
    btn_paso_mostrar[p]    = 0;   // arranca mostrando desde el paso 0
    btn_mostrando_on[p]    = 1;   // arranca en la mitad encendida del parpadeo
    btn_fase[p]            = SJ_MOSTRANDO;   // arranca reproduciendo la secuencia
    btn_tick_fase[p]       = HAL_GetTick();   // marca el inicio de esta fase
    btn_flash_pendiente[p] = 0xFF;   // sin flash pendiente
    btn_ultimo_input_tick[p] = HAL_GetTick();   // reinicia el cooldown de entrada para este jugador

    if (btn_modo_1p) {   // pantalla completa (1 jugador)
        Renderer_DrawModoSimonClasico1P(btn_secuencia[p][0]);   // redibuja la pantalla completa (1 jugador)
    } else {   // layout Cockpit dividido (2 jugadores)
        Renderer_DrawModoSimonClasicoJugador(p, btn_secuencia[p][0]);   // redibuja SOLO la mitad de este jugador (Cockpit, 2 jugadores)
    }
    Boton_LED(p, btn_secuencia[p][0], 1);   // prende el LED fisico del primer color de la nueva secuencia
    btn_paso_dibujado[p] = btn_secuencia[p][0];   // sincroniza el paso pintado
    if (btn_modo_1p) Botones_MostrarRacha1P(btn_longitud[p]);
    else             Renderer_ActualizarRachaBotones(p, btn_longitud[p]);   // actualiza el numero de ronda de este jugador
}

static void Botones_IniciarAmbos(void) {   // arranca una partida nueva de BOTONES a 2 jugadores, ambos lados desde cero
    Renderer_SetSimonClasicoInvertido(1);   // en 2 jugadores, ROJO<->AMARILLO y VERDE<->AZUL cambian de posicion en pantalla para coincidir con la disposicion fisica real de los botones (pedido explicito del usuario)
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // musica de fondo compartida entre los 2 jugadores
    ILI9341_SetPortrait(1);   // orientacion retrato (layout cara a cara)
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    for (uint8_t p = 0; p < 2; p++) {   // inicializa el estado de ambos jugadores
        btn_seed[p] ^= (HAL_GetTick() + p * 977u + 2u);   // remezcla semilla (offset 2, distinto de ReiniciarJugador)
        if (btn_seed[p] == 0) btn_seed[p] = 1;   // evita semilla en 0

        btn_longitud[p]        = 1;   // secuencia nueva de 1 paso
        btn_secuencia[p][0]    = Botones_Random4(p);   // primer color aleatorio
        btn_paso_mostrar[p]    = 0;   // arranca mostrando desde el paso 0
        btn_mostrando_on[p]    = 1;   // arranca en la mitad encendida del parpadeo
        btn_fase[p]            = SJ_MOSTRANDO;   // arranca reproduciendo la secuencia
        btn_tick_fase[p]       = HAL_GetTick();   // marca el inicio de esta fase
        btn_flash_pendiente[p] = 0xFF;   // sin flash pendiente
        btn_paso_dibujado[p]   = 0xFF;   // nada pintado todavia
        btn_ultimo_input_tick[p] = HAL_GetTick();   // reinicia el cooldown de entrada de este jugador
        for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 4 LEDs de este jugador antes de arrancar (por si quedo alguno prendido de una partida anterior)
    }
    Renderer_DrawModoSimonClasico(btn_secuencia[0][0], btn_secuencia[1][0]);   // dibuja las 2 mitades completas de una vez
    Boton_LED(0, btn_secuencia[0][0], 1);   // prende el LED del primer color de jugador 0
    Boton_LED(1, btn_secuencia[1][0], 1);   // prende el LED del primer color de jugador 1
    btn_paso_dibujado[0] = btn_secuencia[0][0];   // sincroniza el paso pintado de jugador 0
    btn_paso_dibujado[1] = btn_secuencia[1][0];   // idem jugador 1
    Renderer_ActualizarRachaBotones(0, btn_longitud[0]);   // numero de ronda inicial de jugador 0
    Renderer_ActualizarRachaBotones(1, btn_longitud[1]);   // numero de ronda inicial de jugador 1
}

/* Arranca el modo de botones a 1 solo jugador (jugador logico 1) a PANTALLA
 * COMPLETA (ver Renderer_DrawModoSimonClasico1P) -- btn_modo_1p ya debe
 * estar en 1 antes de llamar a esta funcion (lo pone main() al confirmar
 * "1 JUGADOR" en el menu), asi que Botones_ReiniciarJugador(1) mas abajo ya
 * toma la rama de pantalla completa por si sola. Botones_Actualizar()
 * nunca invoca Botones_ActualizarJugador(0) mientras btn_modo_1p este
 * activo (ver mas abajo). Reutiliza Botones_ReiniciarJugador(1), que ya
 * implementa exactamente esta inicializacion para el reintento tras un
 * game over. */
static void Botones_IniciarSolo(void) {   // arranca el modo de botones a 1 solo jugador, a pantalla completa
    Renderer_SetSimonClasicoInvertido(0);   // el modo de 1 jugador NO debe modificarse: siempre el layout historico, sin importar que haya quedado activo en una partida de 2 jugadores anterior
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // musica de fondo
    ILI9341_SetPortrait(1);   // orientacion retrato, se mantiene toda la partida
    /* No se aplica el giro de hardware en este modo: la pantalla completa
     * de 1 jugador se dibuja siempre "al derecho" (sin rotar), a diferencia
     * del modo de 2 jugadores donde el giro orienta la mitad del jugador 2. */
    for (uint8_t c = 0; c < 4; c++) { Boton_LED(0, c, 0); Boton_LED(1, c, 0); }   // apaga los 8 LEDs (de ambos jugadores) antes de arrancar
    Botones_ReiniciarJugador(1);   // reusa la logica de "reiniciar tras perder" para arrancar tambien la primera partida -- ya dibuja la pantalla completa nueva (ver mas arriba), no hace falta un FillScreen previo
}

static void Botones_ActualizarJugador(uint8_t p) {   // tick no bloqueante de UN jugador de BOTONES; misma estructura de maquina de estados que SimonJoy2_ActualizarJugador
    uint32_t ahora = HAL_GetTick();   // tick actual, usado en todas las fases

    if (btn_flash_pendiente[p] != 0xFF && (ahora - btn_flash_tick[p]) >= SJ_FLASH_INPUT_MS) {   // hay un prendido pendiente de este jugador y ya paso el tiempo minimo
        Botones_MostrarColor(p, btn_flash_pendiente[p]);   // prende el color confirmado
        btn_flash_pendiente[p] = 0xFF;   // consume el pendiente
    }

    switch (btn_fase[p]) {   // logica de la fase actual de ESTE jugador (independiente del otro)
    case SJ_MOSTRANDO: {   // reproduciendo la secuencia de ESTE jugador
        uint16_t medio = (uint16_t)(Botones_IntervaloActual(p) / 2U);   // mitad del intervalo actual de este jugador
        if ((ahora - btn_tick_fase[p]) < medio) break;   // todavia no toca cambiar de sub-fase
        btn_tick_fase[p] = ahora;   // marca el inicio de la nueva sub-fase

        if (btn_mostrando_on[p]) {   // estaba encendido -- toca apagar
            Botones_MostrarColor(p, 0xFF);
            btn_mostrando_on[p] = 0;
        } else {   // estaba apagado -- toca avanzar de paso
            btn_paso_mostrar[p]++;
            if (btn_paso_mostrar[p] >= btn_longitud[p]) {   // ya mostro toda su secuencia
                btn_fase[p]          = SJ_ESPERANDO;
                btn_paso_esperado[p] = 0;
            } else {   // todavia quedan pasos por mostrar
                Botones_MostrarColor(p, btn_secuencia[p][btn_paso_mostrar[p]]);
                btn_mostrando_on[p] = 1;
            }
        }
        break;   // cierra el case SJ_MOSTRANDO
    }

    case SJ_ESPERANDO: {   // esperando la respuesta de ESTE jugador
        uint8_t color = Botones_LeerColor(p);   // intenta leer un flanco de boton de este jugador
        if (color == 0xFF) break;   // nada presionado este tick
        if ((ahora - btn_ultimo_input_tick[p]) < BTN_REARME_MIN_MS) break;  /* cooldown: ver BTN_REARME_MIN_MS */
        btn_ultimo_input_tick[p] = ahora;   // marca el instante de esta entrada aceptada, para el cooldown de la proxima

        Botones_ConfirmarInput(p, color);   // parpadeo real de confirmacion, aunque se repita el mismo color
        printf("[BOTONES] P%u color=%u esperado=%u %s\r\n", p, color, btn_secuencia[p][btn_paso_esperado[p]],   // log de la entrada: que se leyo, que se esperaba, acierto o fallo
               (color == btn_secuencia[p][btn_paso_esperado[p]]) ? "OK" : "FALLO");

        if (color != btn_secuencia[p][btn_paso_esperado[p]]) {   // fallo de este jugador
            if ((uint8_t)(btn_longitud[p] - 1) > btn_mejor_racha[p]) btn_mejor_racha[p] = (uint8_t)(btn_longitud[p] - 1);   // actualiza la mejor racha si corresponde
            printf("[BOTONES] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            btn_fase[p]            = SJ_GAMEOVER;   // pasa a la fase de game over
            btn_tick_fase[p]       = ahora;   // marca el instante de entrada a game over
            btn_flash_pendiente[p] = 0xFF;   // cancela cualquier prendido pendiente
            for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 4 LEDs de este jugador al perder
            Buzzer_Beep(350);   // beep largo de error
            if (btn_modo_1p) Botones_DibujarGameOver1P((uint16_t)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            else             Renderer_DibujarGameOverBotones(p, (uint16_t)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            break;   // corta el case aca, el fallo ya se manejo por completo
        }

        btn_paso_esperado[p]++;   // acerto: avanza al siguiente paso esperado
        if (btn_paso_esperado[p] >= btn_longitud[p]) {   // este jugador completo toda su secuencia
            if (btn_longitud[p] > btn_mejor_racha[p]) btn_mejor_racha[p] = btn_longitud[p];   // actualiza la mejor racha si corresponde
            if (btn_longitud[p] < SJ_MAX_LONGITUD) {   // todavia hay espacio en el arreglo
                btn_secuencia[p][btn_longitud[p]] = Botones_SiguienteColor(p);   // agrega un color nuevo a la secuencia
                btn_longitud[p]++;   // la secuencia crece en 1
                if (btn_modo_1p) Botones_MostrarRacha1P(btn_longitud[p]);
                else             Renderer_ActualizarRachaBotones(p, btn_longitud[p]);
            }
            btn_fase[p]            = SJ_ACIERTO;   // pausa corta antes de repetir
            btn_tick_fase[p]       = ahora;   // marca el inicio de esa pausa
            btn_flash_pendiente[p] = 0xFF;   // cancela cualquier prendido pendiente
            Botones_MostrarColor(p, 0xFF);   // apaga el ultimo boton antes de la pausa
            {
                static const PasoSonido_t BEEP_RONDA[3] = {   // jingle corto de ronda superada
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };
                Buzzer_Patron(BEEP_RONDA, 3);   // suena el jingle
            }
        }
        break;   // cierra el case SJ_ESPERANDO
    }

    case SJ_ACIERTO:   // pausa corta de este jugador despues de completar una ronda
        if ((ahora - btn_tick_fase[p]) < SJ_PAUSA_ACIERTO_MS) break;   // todavia no paso la pausa
        btn_paso_mostrar[p] = 0;   // reinicia el indice de reproduccion
        btn_mostrando_on[p] = 1;   // arranca de nuevo encendido
        btn_tick_fase[p]    = ahora;   // marca el inicio de la nueva reproduccion
        btn_fase[p]         = SJ_MOSTRANDO;   // vuelve a la fase de mostrar (ahora mas larga)
        Botones_MostrarColor(p, btn_secuencia[p][0]);   // prende de una vez el primer paso de la nueva repeticion
        break;   // cierra el case SJ_ACIERTO

    case SJ_GAMEOVER: {   // esperando que ESTE jugador reintente (o el atajo de ROJO 2s)
        /* No existe un boton de confirmacion separado: cualquiera de los
         * botones propios del jugador reinicia la partida, siempre que ya
         * haya transcurrido BTN_GAMEOVER_COOLDOWN_MS. Botones_LeerColor
         * debe invocarse en cada ciclo, independientemente de si el
         * cooldown ya se cumplio, para que su logica interna de antirrebote
         * no pierda sincronizacion.
         *
         * ROJO queda RESERVADO para el atajo de salida por sostenido de 2s
         * (ver SalirGameOver1P_Detectado/SalirGameOver2P_Detectado) siempre
         * que ese atajo pueda estar corriendo AHORA: en 1 jugador, todo el
         * tiempo; en 2 jugadores, solo una vez que el OTRO jugador tambien
         * termino (si el otro sigue jugando, el atajo 2P no aplica todavia,
         * asi que ROJO debe seguir reiniciando normal para no dejar a este
         * jugador sin forma de reintentar). Si no se reservara en el
         * momento correcto, la propia pulsacion de ROJO cambiaria
         * btn_fase[p] fuera de SJ_GAMEOVER antes de llegar a los 2s, y el
         * atajo nunca se alcanzaria a disparar. Los otros 3 colores siguen
         * reiniciando al toque como siempre. */
        uint8_t color = Botones_LeerColor(p);   // se invoca siempre, haya pasado o no el cooldown (ver el comentario anterior)
        uint8_t rojo_reservado = (color == 0) && (btn_modo_1p || btn_fase[p ^ 1] == SJ_GAMEOVER);   // ROJO reservado si es 1 jugador, o si en 2 jugadores el otro ya termino tambien
        if (color != 0xFF && !rojo_reservado && (ahora - btn_tick_fase[p]) >= BTN_GAMEOVER_COOLDOWN_MS) {   // hubo un boton (valido) Y ya paso el cooldown minimo de game over
            printf("[BOTONES] P%u retry\r\n", p);
            Botones_ReiniciarJugador(p);
        }
        break;   // cierra el case SJ_GAMEOVER
    }
    }   // cierra el switch(btn_fase[p])
}

static void Botones_Actualizar(void) {   // tick de BOTONES: en 1 jugador actualiza solo el jugador logico 1, en 2 jugadores actualiza a ambos
    if (btn_modo_1p) {   // 1 jugador: solo existe estado real para el jugador logico 1
        // En modo de 1 Jugador, actualiza SOLO al jugador de arriba (lógico 1)
        Botones_ActualizarJugador(1);
    } else {   // 2 jugadores: cada uno se actualiza por separado, sin depender del otro
        // En modo de 2 Jugadores, actualiza a ambos lados de forma independiente
        Botones_ActualizarJugador(0);
        Botones_ActualizarJugador(1);
    }
}


/* ========================================================================== */
/* === GUITAR HERO — CARA A CARA (RETRATO, COCKPIT) ========================== */
/* ========================================================================== */
/* Implementa notas reales generadas mediante un spawn periodico
 * independiente por jugador, a diferencia de la maqueta original del
 * recorrido de diseño (DEMO_JUGANDO / GuitarHero_IntentarGolpe), que
 * emplea 2 notas fijas y B1 como boton unico sin distinguir color ni
 * carril. En este modo, cada jugador dispone de su propia mitad del
 * arreglo gs.notas[MAX_NOTES] (posiciones 0-7 para el jugador 1 y 8-15
 * para el jugador 2; ver el comentario de MAX_NOTES en game_state.h), y su
 * color debe coincidir con el carril de la nota para que el golpe sea
 * valido; utiliza los mismos 4 botones arcade que el modo Simon Clasico
 * (ver Botones_LeerColor). El diseño visual (carril angosto y zona de
 * golpe circular) se implementa en las funciones Renderer_DrawModoGuitar-
 * Hero... y Renderer_GH_... de renderer.c (ver guitar_hero/ANALISIS_REFE-
 * RENCIA.md para el diseño de referencia consultado). Cada jugador
 * finaliza su ronda al alcanzar NOTES_PER_GAME notas; en ese momento se
 * muestra "RONDA COMPLETA" unicamente en su mitad de pantalla, y
 * cualquiera de sus propios 4 botones inicia una ronda nueva sin afectar
 * la partida en curso del otro jugador (mismo patron de reintento aplicado
 * en Simon Clasico y Simon + Joystick). */

#define GH_NOTA_BASE(p)     ((uint8_t)((p) * (MAX_NOTES / 2)))   // indice base del arreglo gs.notas donde arranca la mitad de un jugador (0 para p=0, MAX_NOTES/2 para p=1); depende de MAX_NOTES en game_state.h
#define GH_NOTAS_POR_JUG    (MAX_NOTES / 2)   // cuantos slots de nota tiene cada jugador -- subir MAX_NOTES en game_state.h permite mas notas simultaneas en pantalla por jugador

static uint32_t gh_seed[2]      = { 5, 13 };   // semillas del LCG de Guitar Hero, independientes de SimonJoy/Botones
static uint8_t  gh_terminado[2] = { 0, 0 };   // 1 = este jugador ya completo su ronda de NOTES_PER_GAME notas y esta esperando reintentar

/* Dificultad elegida en DEMO_MENU_DIFICULTAD (dificultad_cursor, 0-2): fija
 * la velocidad de caida y el intervalo de spawn de TODA la sesion de
 * Guitar Hero -- ninguno de los 2 cambia mientras se juega ni entre
 * reintentos individuales (pedido explicito del usuario: si se elige
 * FACIL, se queda en FACIL toda la partida, sin subir sola a mitad de
 * ronda). Para cambiar de dificultad hay que salir de la partida (ver
 * atajo AZUL 2s, GH_BotonSostenidoDetectado mas abajo) y volver a elegir. */
static const uint8_t  GH_SPEED_POR_NIVEL[3] = { NOTE_SPEED_L1, NOTE_SPEED_L2, NOTE_SPEED_L3 };   // velocidad de caida por nivel (indice = dificultad_cursor); agregar un 4to nivel implica agrandar este arreglo Y el menu de dificultad
static const uint16_t GH_SPAWN_POR_NIVEL[3] = { SPAWN_INTERVAL_L1, SPAWN_INTERVAL_L2, SPAWN_INTERVAL_L3 };   // intervalo de spawn por nivel, mismo indice
static uint8_t  gh_speed_base = NOTE_SPEED_L2;   // velocidad fija de la sesion actual, segun la dificultad elegida -- no crece
static uint16_t gh_spawn_ms   = SPAWN_INTERVAL_L2;   // intervalo de spawn fijo de la sesion actual, segun la dificultad elegida -- no crece

static uint8_t GH_Random4(uint8_t p) {   // mismo LCG que los otros modos, aplicado a la semilla de Guitar Hero del jugador p
    gh_seed[p] = gh_seed[p] * 1103515245u + 12345u;   // mismo LCG que el resto del proyecto
    return (uint8_t)((gh_seed[p] >> 16) & 0x3u);   // bits del medio recortados a 0-3
}

/* Inicia (o reinicia) unicamente al jugador p, sin afectar la mitad del otro. */
static void GuitarHero_ReiniciarJugador(uint8_t p) {   // resetea el puntaje, combo y notas de un jugador, y redibuja unicamente su mitad
    gh_seed[p] ^= (HAL_GetTick() + p * 977u + 9u);   // remezcla la semilla de este jugador
    if (gh_seed[p] == 0) gh_seed[p] = 1;   // evita semilla en 0

    gs.j[p].puntaje           = 0;   // puntaje en 0 al arrancar/reiniciar
    gs.j[p].combo             = 0;   // combo en 0
    gs.j[p].notas_spawneadas  = 0;   // contador de notas ya generadas en esta ronda, en 0
    gs.j[p].tick_ultimo_spawn = HAL_GetTick();   // marca el instante base para el proximo spawn de nota
    gh_ultimo_input_tick[p]  = HAL_GetTick();   // reinicia el cooldown de entrada de este jugador (ver GH_REARME_MIN_MS)
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) gs.notas[GH_NOTA_BASE(p) + i].activa = 0;   // desactiva todas las notas de la mitad de este jugador (limpia notas viejas de la partida anterior)
    gh_terminado[p]    = 0;   // ya no esta "terminado", arranca una ronda nueva

    if (guitar_modo_1p) {   // pantalla completa (1 jugador)
        Renderer_DrawModoGuitarHero1P();       // pantalla completa (1 jugador)
        Renderer_GH1P_ActualizarPuntaje(0, 0);   // muestra puntaje/combo en 0
    } else {   // layout Cockpit dividido (2 jugadores)
        Renderer_DrawModoGuitarHeroJugador(p);   // redibuja SOLO la mitad de pantalla de este jugador (carril, zona de golpe, encabezado)
        Renderer_GH_ActualizarPuntaje(p, 0, 0);   // muestra puntaje/combo en 0
    }
}

/* Dibuja ambas mitades de la pantalla desde cero (arranque de una partida nueva a 2 jugadores). */
static void GuitarHero2_IniciarAmbos(void) {   // arranca una partida nueva de Guitar Hero a 2 jugadores, ambos lados desde cero
    Buzzer_Fondo_Iniciar(cancion_cursor);   // cancion elegida por los jugadores en DEMO_MENU_CANCIONES (unico paso previo a Guitar Hero, ver MenuCanciones_Procesar); a partir de que esa cancion complete una vuelta, Buzzer_Fondo_RotarSiTermino sigue variando sola
    ILI9341_SetPortrait(1);   // orientacion retrato, se mantiene toda la partida
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    gh_speed_base = GH_SPEED_POR_NIVEL[dificultad_cursor];   // velocidad y spawn FIJOS de toda la sesion, segun la dificultad elegida en DEMO_MENU_DIFICULTAD
    gh_spawn_ms   = GH_SPAWN_POR_NIVEL[dificultad_cursor];   // idem para el intervalo de spawn
    gs.nota_speed = gh_speed_base;   // velocidad de caida de notas de este modo real (no confundir con el recorrido de diseño DEMO_JUGANDO)
    memset(gs.notas, 0, sizeof(gs.notas));   // limpia TODO el arreglo de notas (ambos jugadores) antes de arrancar
    Renderer_DrawModoGuitarHero2P();   // dibuja el layout completo de las 2 mitades (carriles, zonas de golpe, encabezados)
    GuitarHero_ReiniciarJugador(0);   // inicializa el estado de jugador 0
    GuitarHero_ReiniciarJugador(1);   // inicializa el estado de jugador 1
}

/* Arranca Guitar Hero a 1 solo jugador (jugador logico 1) a PANTALLA
 * COMPLETA (ver Renderer_DrawModoGuitarHero1P), siguiendo el mismo patron
 * que Botones_IniciarSolo -- guitar_modo_1p ya debe estar en 1 antes de
 * llamar a esta funcion (lo pone main() al confirmar "1 JUGADOR"), asi que
 * GuitarHero_ReiniciarJugador(1) mas abajo ya toma la rama de pantalla
 * completa por si sola. Tampoco se aplica el giro de hardware en este modo:
 * la pantalla completa de 1 jugador se dibuja siempre "al derecho" (sin
 * rotar), a diferencia del modo de 2 jugadores donde el giro orienta la
 * mitad del jugador 2. */
static void GuitarHero_IniciarSolo(void) {   // arranca Guitar Hero a 1 solo jugador, a pantalla completa
    Buzzer_Fondo_Iniciar(cancion_cursor);   // misma cancion elegida en DEMO_MENU_CANCIONES, ver GuitarHero2_IniciarAmbos
    ILI9341_SetPortrait(1);   // orientacion retrato, se mantiene toda la partida
    gh_speed_base = GH_SPEED_POR_NIVEL[dificultad_cursor];   // velocidad y spawn FIJOS de toda la sesion, segun la dificultad elegida en DEMO_MENU_DIFICULTAD
    gh_spawn_ms   = GH_SPAWN_POR_NIVEL[dificultad_cursor];   // idem para el intervalo de spawn
    gs.nota_speed = gh_speed_base;   // velocidad de caida de notas de este modo real
    memset(gs.notas, 0, sizeof(gs.notas));   // limpia todas las notas antes de arrancar
    GuitarHero_ReiniciarJugador(1);   // ya dibuja la pantalla completa nueva (ver mas arriba), no hace falta un FillScreen previo
}

/* Busca la nota mas cercana del carril `color` para el jugador p y la
 * puntua (o registra el fallo). Extraida de GuitarHero_ActualizarJugador
 * sin cambiar su logica, para poder invocarla una vez por color golpeado
 * -- en 2 jugadores sigue siendo, como antes, como maximo una vez por tick
 * (un solo `color` via Botones_LeerColor); en 1 jugador puede invocarse
 * varias veces en el mismo tick (una por cada bit de
 * Botones_LeerColoresBitmask), para no perder golpes de colores distintos
 * que caen en el mismo tick de ~33ms. */
static void GuitarHero_ProcesarGolpe(uint8_t p, uint8_t base, uint8_t color) {
    int16_t mejor_dist = 0x7FFF;   // arranca en el maximo posible
    int8_t  mejor_i    = -1;   // indice de la mejor nota encontrada del carril del color presionado, -1 = ninguna
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // busca entre las notas de ESTE jugador
        Nota_t *n = &gs.notas[base + i];
        if (!n->activa || n->carril != color) continue;   // ignora notas apagadas o de OTRO carril
        int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);   // centro X de la nota
        int16_t dist = (int16_t)((centro_nota > (int16_t)GH_ZONA_CX) ? (centro_nota - (int16_t)GH_ZONA_CX) : ((int16_t)GH_ZONA_CX - centro_nota));
        if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }   // se queda con la mas cercana
    }
    uint16_t hit_ok = guitar_modo_1p ? GH_HIT_OK_1P : GH_HIT_OK_2P;   // ventana "OK"
    if (mejor_i >= 0 && mejor_dist <= (int16_t)hit_ok) {   // encontro una nota dentro de la ventana de golpe
        Nota_t *n = &gs.notas[base + mejor_i];
        if      (mejor_dist <= (int16_t)HIT_PERFECT) { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_PERFECT); }
        else if (mejor_dist <= (int16_t)HIT_GOOD)    { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_GOOD); }
        else                                          { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_OK); }
        gs.j[p].combo++;   // suma combo por acierto
        n->activa = 0;   // desactiva la nota golpeada
        Buzzer_Beep(80);   // beep corto de golpe
        if (guitar_modo_1p) Renderer_GH1P_FlashZona(color);   // flash de impacto
        else                 Renderer_GH_FlashZona(p, color);
    } else {   // fallo
        gs.j[p].combo = 0;   // corta combo
    }
}

static void GuitarHero_ActualizarJugador(uint8_t p) {   // tick no bloqueante de UN jugador de Guitar Hero: spawnea notas, lee golpes, mueve/dibuja notas y detecta fin de ronda
    /* revierte el flash blanco de impacto del tick anterior (si hubo uno) --
     * ver Renderer_GH_FlashZona/ActualizarFlashes en renderer.c, esto le da
     * exactamente 1 frame (~33ms) de blanco antes de volver a su estilo
     * normal de "blanco/diana". */
    if (guitar_modo_1p) Renderer_GH1P_ActualizarFlashes();   // pantalla completa
    else                 Renderer_GH_ActualizarFlashes(p);   // Cockpit dividido

    if (gh_terminado[p]) {   // este jugador ya termino su ronda de NOTES_PER_GAME notas, esta esperando a que reintente
        uint8_t color = Botones_LeerColor(p);   // cualquiera de sus 4 botones arranca una ronda nueva...
        uint8_t rojo_reservado = (color == 0) && (guitar_modo_1p || gh_terminado[p ^ 1]);   // ...salvo ROJO, reservado para el atajo de salida por sostenido de 2s (ver Botones_ActualizarJugador para el mismo criterio en detalle)
        if (color != 0xFF && !rojo_reservado) {   // hubo un boton (valido) que arranca una ronda nueva
            printf("[GUITARHERO] P%u nueva ronda\r\n", p);
            GuitarHero_ReiniciarJugador(p);
        }
        return;   // mientras esta terminado, no corre el resto de la logica (spawns/movimiento) de este jugador
    }

    uint32_t ahora = HAL_GetTick();
    uint8_t  base  = GH_NOTA_BASE(p);   // indice base del arreglo gs.notas donde arranca la mitad de este jugador (0 para p=0, MAX_NOTES/2 para p=1)

    /* Spawn periodico -- una nota nueva cada gh_spawn_ms (fijado por la
     * dificultad elegida, ver GuitarHero_IniciarSolo/2_IniciarAmbos)
     * mientras queden cupos en la ronda (NOTES_PER_GAME) y un slot libre en
     * la mitad de p. */
    if (gs.j[p].notas_spawneadas < NOTES_PER_GAME &&   // todavia quedan notas por generar en esta ronda
        (ahora - gs.j[p].tick_ultimo_spawn) >= gh_spawn_ms) {   // Y ya paso el intervalo de spawn
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // busca el primer slot LIBRE (no activo) en la mitad de este jugador
            Nota_t *n = &gs.notas[base + i];
            if (n->activa) continue;   // este slot ya esta ocupado por otra nota, prueba el siguiente
            n->carril    = GH_Random4(p);   // color/carril aleatorio de la nota nueva
            n->jugador   = p;   // marca a que jugador pertenece
            n->x_rel     = COCKPIT_ZONE_W;   // nace justo en el borde derecho de la zona de juego de este jugador
            n->x_prev    = n->x_rel;   // posicion previa igual a la actual, para que el primer borrado de "estela" no borre nada de mas
            n->activa    = 1;   // la marca como activa/en juego
            gs.j[p].notas_spawneadas++;   // cuenta una nota mas generada en esta ronda
            gs.j[p].tick_ultimo_spawn = ahora;   // reinicia el cronometro del proximo spawn
            break;   // ya encontro un slot libre, no sigue buscando otro
        }
    }

    /* Input: el color propio del jugador caza la nota mas cercana de ESE
     * carril (no la mas cercana de cualquier color, a diferencia de la
     * maqueta original de 1 solo boton). Todas las notas se golpean al
     * toque (sin mantener presionado).
     *
     * En 1 jugador las 4 notas caen sobre el mismo set fisico de botones,
     * asi que puede hacer falta procesar mas de un color en el mismo tick
     * (ver Botones_LeerColoresBitmask); en 2 jugadores se deja EXACTAMENTE
     * el mismo comportamiento de siempre (un solo color por tick, via
     * Botones_LeerColor). */
    if (guitar_modo_1p) {
        uint8_t bitmask = Botones_LeerColoresBitmask(p);   // SIEMPRE se llama (cooldown o no), mismo criterio que antes con Botones_LeerColor
        if (bitmask && (ahora - gh_ultimo_input_tick[p]) >= GH_REARME_MIN_MS) {
            gh_ultimo_input_tick[p] = ahora;
            for (uint8_t color = 0; color < 4; color++) {
                if (bitmask & (uint8_t)(1u << color)) GuitarHero_ProcesarGolpe(p, base, color);
            }
        }
    } else {
        uint8_t color = Botones_LeerColor(p);   // intenta leer un flanco de boton de este jugador (SIEMPRE se llama, cooldown o no, para no desincronizar su antirrebote interno -- mismo criterio que el game over de Botones_ActualizarJugador)
        if (color != 0xFF && (ahora - gh_ultimo_input_tick[p]) >= GH_REARME_MIN_MS) {   // presiono algun boton este tick Y ya paso el cooldown corto de Guitar Hero (ver GH_REARME_MIN_MS)
            gh_ultimo_input_tick[p] = ahora;   // marca el instante de esta entrada aceptada, para el cooldown de la proxima
            GuitarHero_ProcesarGolpe(p, base, color);
        }
    }

    /* Movimiento + render delta de las notas activas de este jugador --
     * viajan de derecha a izquierda (nacen lejos, se acercan a la zona de
     * golpe), al reves de la maqueta original DEMO_JUGANDO. */
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // recorre todas las notas (activas o no) de este jugador
        Nota_t *n = &gs.notas[base + i];
        if (!n->activa) continue;   // ignora las apagadas
        n->x_prev = n->x_rel;   // guarda la posicion de este frame como "anterior", para poder borrar solo la estela recorrida
        n->x_rel  = (int16_t)(n->x_rel - gs.nota_speed);   // avanza la nota hacia la izquierda segun la velocidad configurada (fija toda la sesion, ver gh_speed_base)
        if (guitar_modo_1p) Renderer_GH1P_EraseNotaTrail(n, gs.nota_speed);   // borra solo el tramo de pantalla que la nota acaba de dejar atras (no toda la pantalla)
        else                 Renderer_GH_EraseNotaTrail(p, n, gs.nota_speed);
        if (n->x_rel < -(int16_t)NOTE_W) {   // la nota ya salio completamente por la izquierda sin ser golpeada
            n->activa     = 0;   // se desactiva (se perdio)
            gs.j[p].combo = 0;   /* nota perdida sin presionar -- corta combo */
        } else if (guitar_modo_1p) {   // todavia visible, dibujarla en su nueva posicion (pantalla completa)
            Renderer_GH1P_DrawNota(n);   // todavia visible: la dibuja en su nueva posicion
        } else {   // todavia visible (layout Cockpit dividido)
            Renderer_GH_DrawNota(p, n);
        }
    }

    if (guitar_modo_1p) Renderer_GH1P_ActualizarPuntaje(gs.j[p].puntaje, gs.j[p].combo);   // refresca el puntaje/combo mostrados en pantalla
    else                 Renderer_GH_ActualizarPuntaje(p, gs.j[p].puntaje, gs.j[p].combo);

    /* Fin de ronda: se agotaron los spawns y no queda ninguna nota viva */
    if (gs.j[p].notas_spawneadas >= NOTES_PER_GAME) {   // ya se generaron todas las notas posibles de esta ronda
        uint8_t queda_activa = 0;
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) if (gs.notas[base + i].activa) { queda_activa = 1; break; }   // busca si queda alguna nota todavia viva en pantalla
        if (!queda_activa) {   // ninguna nota viva: la ronda termino de verdad
            gh_terminado[p] = 1;   // marca a este jugador como terminado, esperando reintento
            printf("[GUITARHERO] P%u ronda completa puntaje=%u\r\n", p, gs.j[p].puntaje);
            if (guitar_modo_1p) Renderer_GH1P_DibujarFin(gs.j[p].puntaje);   // dibuja "RONDA COMPLETA" con el puntaje final, a pantalla completa
            else                 Renderer_GH_DibujarFin(p, gs.j[p].puntaje);   // idem, solo en la mitad de este jugador
        }
    }
}

static void GuitarHero_Actualizar(void) {   // tick de Guitar Hero: en modo Solo actualiza SOLO al jugador 1 (el que GuitarHero_IniciarSolo dibujo e inicializo); en 2 jugadores actualiza ambos
    if (guitar_modo_1p) {   // 1 jugador: solo existe estado real para el jugador logico 1
        GuitarHero_ActualizarJugador(1);
    } else {   // 2 jugadores: cada uno se actualiza por separado
        GuitarHero_ActualizarJugador(0);
        GuitarHero_ActualizarJugador(1);
    }
}

/* Atajo de salida rapida SOLO disponible en pantallas de FIN de partida de 1
 * jugador: GAME OVER de Botones (btn_fase[1]==SJ_GAMEOVER con btn_modo_1p) o
 * de SimonJoy (sj_fase==SJ_GAMEOVER con en_juego_real, que es exclusivo de
 * SimonJoy 1 jugador -- no confundir con en_juego_real_2p), o RONDA COMPLETA
 * de Guitar Hero (gh_terminado[1] con guitar_modo_1p). Mantener sostenido
 * SOLO el boton ROJO (sin AMARILLO) del jugador fisico 0 -- el mismo conjunto
 * que btn_modo_1p/guitar_modo_1p ya usan siempre en modo 1 jugador -- durante
 * SALIR_GAMEOVER_1P_MS dispara la salida. Fuera de esas pantallas (por
 * ejemplo, sosteniendo ROJO como parte normal del juego) el cronometro no
 * corre, para no interferir con la partida en curso. Mismo patron de lectura
 * de NIVEL crudo por GPIO (no flanco) que ComboSalir_Detectado, pero con un
 * solo boton y un umbral mas corto (pensado para salir de una pantalla ya
 * terminada, no para interrumpir una partida en curso). */
#define SALIR_GAMEOVER_1P_MS 2000U   // ms que hay que sostener ROJO en fin de partida 1P antes de disparar la salida; bajarlo hace el atajo mas sensible (mas riesgo de dispararlo sin querer)
static uint32_t salir_gameover_1p_tick = 0;   // tick en que se detecto ROJO presionado (0 = no hay cronometro corriendo)

static uint8_t SalirGameOver1P_Detectado(void) {   // revisa si, en una pantalla de fin de partida de 1 jugador, se lleva 2s manteniendo presionado solo ROJO; devuelve 1 en el ciclo en que se cumple
    uint8_t en_fin_1p = (btn_modo_1p    && btn_fase[1] == SJ_GAMEOVER) ||
                        (guitar_modo_1p && gh_terminado[1]) ||
                        (en_juego_real  && sj_fase == SJ_GAMEOVER);
    if (!en_fin_1p) {
        salir_gameover_1p_tick = 0;   // fuera de estas pantallas, el cronometro no corre
        return 0;
    }

    uint8_t rojo = (HAL_GPIO_ReadPin(BTN_SW[0][0].port, BTN_SW[0][0].pin) == GPIO_PIN_RESET);   // nivel crudo de ROJO del jugador fisico 0
    if (!rojo) {
        salir_gameover_1p_tick = 0;   // se solto antes de completar el tiempo: cancela el cronometro
        return 0;
    }
    if (salir_gameover_1p_tick == 0) {
        salir_gameover_1p_tick = HAL_GetTick();   // primera vez que se detecta ROJO presionado: arranca el cronometro
        return 0;
    }
    if (HAL_GetTick() - salir_gameover_1p_tick >= SALIR_GAMEOVER_1P_MS) {
        printf("[COMBO] ROJO 2s en fin de partida 1P -> menu de modos\r\n");
        salir_gameover_1p_tick = 0;   /* rearma para la proxima vez */
        return 1;
    }
    return 0;
}

/* Mismo atajo que SalirGameOver1P_Detectado pero para 2 jugadores: solo
 * corre cuando AMBOS jugadores ya terminaron su partida (GAME OVER de los 2
 * en Botones/SimonJoy, o RONDA COMPLETA de los 2 en Guitar Hero) -- si uno
 * de los 2 sigue jugando, el atajo no aplica todavia. Sostener 2s el ROJO
 * de CUALQUIERA de los 2 jugadores fisicos dispara la salida (cada uno se
 * cronometra por separado, salir_gameover_2p_tick[p]). A diferencia del
 * atajo de 1 jugador, este NO corre durante una partida 2P EN CURSO -- para
 * salir a mitad de partida sigue estando el combo ROJO+AMARILLO de 3s
 * (ComboSalir_Detectado), que exige los 2 botones a la vez y no se confunde
 * con presionar ROJO solo como parte normal del juego. */
#define SALIR_GAMEOVER_2P_MS 2000U   // ms que hay que sostener ROJO en fin de partida 2P antes de disparar la salida (una vez que AMBOS jugadores ya terminaron)
static uint32_t salir_gameover_2p_tick[2] = { 0, 0 };   // tick en que cada jugador empezo a sostener ROJO (0 = sin cronometro corriendo)

static uint8_t SalirGameOver2P_Detectado(void) {   // revisa si, con AMBOS jugadores ya terminados, alguno lleva 2s sosteniendo su propio ROJO; devuelve 1 en el ciclo en que se cumple
    uint8_t ambos_terminaron =   // true solo si el modo real actual tiene a SUS 2 jugadores en estado de fin de partida
        (en_juego_real_botones && !btn_modo_1p    && btn_fase[0]  == SJ_GAMEOVER && btn_fase[1]  == SJ_GAMEOVER) ||   // Botones 2P: ambos en GAME OVER
        (en_juego_real_2p                                                        && sj2_fase[0]  == SJ_GAMEOVER && sj2_fase[1] == SJ_GAMEOVER) ||   // SimonJoy 2P: ambos en GAME OVER
        (en_juego_real_guitar  && !guitar_modo_1p && gh_terminado[0] && gh_terminado[1]);   // Guitar Hero 2P: ambos en RONDA COMPLETA
    if (!ambos_terminaron) {   // todavia no aplica el atajo (al menos uno sigue jugando)
        salir_gameover_2p_tick[0] = salir_gameover_2p_tick[1] = 0;   // fuera de esta condicion, ningun cronometro corre
        return 0;
    }

    uint8_t disparado = 0;   // resultado acumulado (contempla el caso de que los 2 jugadores completen el gesto en el mismo tick)
    for (uint8_t p = 0; p < 2; p++) {   // revisa el ROJO de cada jugador fisico por separado
        uint8_t rojo = (HAL_GPIO_ReadPin(BTN_SW[p][0].port, BTN_SW[p][0].pin) == GPIO_PIN_RESET);   // nivel crudo de ROJO de este jugador
        if (!rojo) {   // no esta presionado
            salir_gameover_2p_tick[p] = 0;   // se solto antes de completar el tiempo: cancela el cronometro de este jugador
            continue;
        }
        if (salir_gameover_2p_tick[p] == 0) {   // primera vez que se lo ve presionado
            salir_gameover_2p_tick[p] = HAL_GetTick();   // primera vez que se detecta ROJO presionado: arranca el cronometro de este jugador
            continue;
        }
        if (HAL_GetTick() - salir_gameover_2p_tick[p] >= SALIR_GAMEOVER_2P_MS) {   // ya paso el tiempo minimo sostenido
            printf("[COMBO] P%u ROJO 2s en fin de partida 2P -> menu de modos\r\n", p);
            salir_gameover_2p_tick[0] = salir_gameover_2p_tick[1] = 0;   /* rearma para la proxima vez */
            disparado = 1;
        }
    }
    return disparado;   // 1 si algun jugador completo el gesto este tick
}

/* Sostener VERDE (color 1) o AZUL (color 2) 2s durante una partida real de
 * Guitar Hero (jugando o en la pantalla de RONDA COMPLETA, no importa cual
 * de las 2) salta directo a elegir otra cancion o dificultad, sin tener que
 * salir primero al menu de modos. Funciona en 1 y 2 jugadores: en 1
 * jugador solo se revisa el conjunto fisico 0 (el unico que se lee en ese
 * modo, ver Botones_LeerColor); en 2 jugadores, CUALQUIERA de los 2
 * jugadores puede disparar el cambio, porque cancion y dificultad son
 * compartidas por toda la partida (gs.nota_speed, buzzer_fondo_*), no hay
 * una version "por jugador". No choca con la logica de juego: las notas
 * sostenidas ya no existen (ver conversacion anterior), asi que sostener un
 * color mientras se juega no interfiere con ningun puntaje. */
#define GH_CAMBIAR_MS 2000U   // ms que hay que sostener VERDE/AZUL para saltar a elegir cancion/dificultad
static uint32_t gh_cambiar_cancion_tick[2]    = { 0, 0 };   // tick en que cada jugador empezo a sostener VERDE (0 = sin cronometro)
static uint32_t gh_cambiar_dificultad_tick[2] = { 0, 0 };   // idem para AZUL

static uint8_t GH_BotonSostenidoDetectado(uint8_t color, uint32_t *tick_arr) {   // nivel crudo de `color` sostenido 2s -- mismo patron que SalirGameOver2P_Detectado, con un solo boton en vez de ROJO
    uint8_t max_p     = (uint8_t)(guitar_modo_1p ? 1 : 2);   // en 1 jugador solo hay un fisico relevante (el 0)
    uint8_t disparado = 0;
    for (uint8_t p = 0; p < max_p; p++) {   // revisa cada jugador fisico relevante
        uint8_t hw = (uint8_t)(guitar_modo_1p ? 0 : p);   // en 1 jugador siempre lee el conjunto fisico 0, igual que Botones_LeerColor
        uint8_t presionado = (HAL_GPIO_ReadPin(BTN_SW[hw][color].port, BTN_SW[hw][color].pin) == GPIO_PIN_RESET);   // nivel crudo del color pedido
        if (!presionado) {   // no esta presionado
            tick_arr[p] = 0;   // se solto antes de completar el tiempo: cancela el cronometro de este jugador
            continue;
        }
        if (tick_arr[p] == 0) {   // primera vez que se lo ve presionado
            tick_arr[p] = HAL_GetTick();   // primera vez que se detecta presionado: arranca el cronometro de este jugador
            continue;
        }
        if (HAL_GetTick() - tick_arr[p] >= GH_CAMBIAR_MS) {   // ya paso el tiempo minimo sostenido
            tick_arr[0] = tick_arr[1] = 0;   /* rearma para la proxima vez */
            disparado = 1;
        }
    }
    return disparado;   // 1 si algun jugador completo el gesto este tick
}

/* ========================================================================== */
/* === FUNCION PRINCIPAL Y NAVEGACION DE MENUS =============================== */
/* ========================================================================== */

/* --------------------------------------------------------------------------
 * NAVEGACION CON JOYSTICK SIN EXIGIR CENTRADO EXACTO
 * -------------------------------------------------------------------------- */
#define MENU_JOY_COOLDOWN_MS 300U   // tiempo minimo (en milisegundos) entre 2 movimientos de cursor aceptados en el menu de seleccion de jugadores; disminuir este valor hace el menu mas sensible y rapido
#define MENU_MODO_COOLDOWN_MS 250U   // idem, para el menu de seleccion de modo

/* Mueve el cursor unicamente mediante el joystick (cualquiera de los 2),
 * dejando el boton arcade reservado exclusivamente para la accion de
 * confirmar (ver la variable "confirmar_boton" en main()). Esta separacion
 * es necesaria porque, si el mismo boton se utilizara tanto para mover el
 * cursor como para confirmar la seleccion, el menu nunca completaria la
 * transicion a la siguiente pantalla: el cursor se moveria, pero la accion
 * de avanzar nunca se ejecutaria. */
static void MenuJugadores_Procesar(uint8_t *screen) {   // mueve el cursor del menu 1/2 JUGADORES cuando cualquiera de los 2 joystick sale de su zona muerta
    static uint32_t ultimo_mov_joy = 0;   // tick del ultimo movimiento aceptado, para aplicar el cooldown
    uint32_t ahora = HAL_GetTick();   // tick actual

    uint16_t j1x_b, j1x_a, j1y_b, j1y_a, j2x_b, j2x_a, j2y_b, j2y_a;   // rangos "sin movimiento" de los 4 ejes (X/Y de J1 y J2)
    Joy_Umbrales(centro_j1x, &j1x_b, &j1x_a);   // rango del eje X de J1
    Joy_Umbrales(centro_j1y, &j1y_b, &j1y_a);   // rango del eje Y de J1
    Joy_Umbrales(centro_j2x, &j2x_b, &j2x_a);   // rango del eje X de J2
    Joy_Umbrales(centro_j2y, &j2y_b, &j2y_a);   // rango del eje Y de J2

    uint8_t joy_movido = 0;   // 1 si corresponde alternar el cursor este tick
    if ((ahora - ultimo_mov_joy) >= MENU_JOY_COOLDOWN_MS) {   // solo revisa si ya paso el cooldown desde el ultimo movimiento
        if (joystick_x < j1x_b || joystick_x > j1x_a || joystick_y < j1y_b || joystick_y > j1y_a ||
            joystick2_x < j2x_b || joystick2_x > j2x_a || joystick2_y < j2y_b || joystick2_y > j2y_a) {   // CUALQUIERA de los 4 ejes (de cualquiera de los 2 joystick) esta fuera de su banda muerta
            joy_movido = 1;   // algun eje esta fuera de su banda muerta
            ultimo_mov_joy = ahora;   // reinicia el cooldown
        }
    }

    if (joy_movido) {   // corresponde alternar el cursor
        uint8_t screen_ant = *screen;   // guarda la pantalla (cursor) anterior, para saber que borrar
        *screen = (*screen == DEMO_JUGADORES_1) ? DEMO_JUGADORES_2 : DEMO_JUGADORES_1;   // alterna entre las 2 unicas opciones (no hay mas de 2, asi que "mover" siempre es alternar)
        Renderer_UpdateSeleccionJugadores((uint8_t)(screen_ant - DEMO_JUGADORES_1),
                                          (uint8_t)(*screen - DEMO_JUGADORES_1));   // redibuja solo el cambio de cursor (de que opcion a que opcion), no todo el menu
        printf("[MENU] jugadores -> %u\r\n", (unsigned)(*screen - DEMO_JUGADORES_1));
    }
}

static void MenuModos_Procesar(uint8_t *screen) {   // avanza el cursor del menu de MODO (Simon/Sim+Joy/Guitar) cuando cualquier joystick se mueve
    static uint32_t ultimo_mov_modo = 0;   // tick del ultimo movimiento aceptado
    uint32_t ahora = HAL_GetTick();   // tick actual

    uint16_t j1x_b, j1x_a, j1y_b, j1y_a, j2x_b, j2x_a, j2y_b, j2y_a;   // rangos sin movimiento de los 4 ejes
    Joy_Umbrales(centro_j1x, &j1x_b, &j1x_a);   // eje X de J1
    Joy_Umbrales(centro_j1y, &j1y_b, &j1y_a);   // eje Y de J1
    Joy_Umbrales(centro_j2x, &j2x_b, &j2x_a);   // eje X de J2
    Joy_Umbrales(centro_j2y, &j2y_b, &j2y_a);   // eje Y de J2

    uint8_t joy_movido = 0;   // 1 si corresponde avanzar el cursor este tick
    if ((ahora - ultimo_mov_modo) >= MENU_MODO_COOLDOWN_MS) {   // solo revisa si ya paso el cooldown
        if (joystick_x < j1x_b || joystick_x > j1x_a || joystick_y < j1y_b || joystick_y > j1y_a ||
            joystick2_x < j2x_b || joystick2_x > j2x_a || joystick2_y < j2y_b || joystick2_y > j2y_a) {   // CUALQUIERA de los 4 ejes fuera de banda muerta
            joy_movido = 1;
            ultimo_mov_modo = ahora;
        }
    }

    if (joy_movido) {   // corresponde avanzar el cursor
        uint8_t screen_ant = *screen;   // pantalla (opcion) anterior, para saber que borrar
        if (*screen == DEMO_MODO_GUITAR)
            *screen = DEMO_MODO_SIMON;   // desde la ultima opcion, vuelve a la primera (ciclico)
        else
            (*screen)++;   // avanza a la siguiente opcion del enum
        Renderer_UpdateSeleccionModo((uint8_t)(screen_ant - DEMO_MODO_SIMON),
                                     (uint8_t)(*screen - DEMO_MODO_SIMON));   // redibuja solo el cambio de cursor
        printf("[MENU] modo -> %s\r\n", DEMO_NOMBRE[*screen]);
    }
}

/* Pantalla de canciones (DEMO_MENU_CANCIONES) SOLO como paso previo a Guitar
 * Hero (ver el bloque de confirmacion de modo mas abajo, unico lugar que
 * lleva a esta pantalla) -- pedido explicito del usuario para que los
 * jugadores elijan con que cancion arranca la musica de fondo antes de
 * jugar. CUALQUIER joystick (el de cualquiera de los 2 jugadores) sube/baja
 * el cursor por la lista, igual que un reproductor de musica; CUALQUIER
 * boton arcade confirma la cancion resaltada y arranca el conteo 3-2-1-GO
 * (via "confirmar_boton", calculado en main() igual que en el resto de las
 * pantallas DEMO_MODO_x y DEMO_JUGADORES_x). */

/* Se llama SIEMPRE justo antes de mostrar DEMO_MENU_CANCIONES (ver el
 * bloque de confirmacion de modo mas abajo). Fuerza a Joystick2_LeerDireccion
 * a exigir ver el stick centrado antes de aceptar el primer movimiento,
 * para no heredar como "subir/bajar" el mismo arrastre de joystick con el
 * que el jugador recien llego a la pantalla de MODO anterior (mover el
 * stick es como se navega entre DEMO_MODO_SIMON/SIMONJOY/GUITAR, ver
 * MenuModos_Procesar) -- mismo riesgo de "arrastre" que ya se evita en el
 * resto de este recorrido. */
static void MenuCanciones_Armar(void) {   // llamar SIEMPRE justo antes de mostrar DEMO_MENU_CANCIONES
    sj2_joy_listo_dir[0] = 0;   // exige ver el stick de J1 centrado antes de aceptar el primer movimiento
    sj2_joy_listo_dir[1] = 0;   // idem para J2
}

static void MenuCanciones_Procesar(uint8_t *screen) {   // sube/baja el cursor de la lista con cualquiera de los 2 joystick; confirmar corre en main() via confirmar_boton
    (void)screen;   // esta pantalla no cambia sola de pantalla: la confirmacion (boton) se maneja en main()
    for (uint8_t p = 0; p < 2; p++) {   // revisa el joystick de cada jugador por separado, cualquiera de los 2 puede navegar
        uint8_t dir = Joystick2_LeerDireccion(p);   // 0=ARRIBA, 1=ABAJO, 2/3=IZQUIERDA/DERECHA (ignoradas aca), 0xFF=sin movimiento nuevo
        if (dir != 0 && dir != 1) continue;   // solo arriba/abajo mueven el cursor de esta lista

        uint8_t ant = cancion_cursor;   // cursor anterior, para el redibujo incremental
        cancion_cursor = (dir == 0)
            ? (uint8_t)((cancion_cursor == 0) ? (CANCIONES_N - 1) : (cancion_cursor - 1))   /* ARRIBA: cancion anterior, con vuelta ciclica */
            : (uint8_t)((cancion_cursor + 1) % CANCIONES_N);                                 /* ABAJO: cancion siguiente, con vuelta ciclica */
        Renderer_UpdateListaCanciones(ant, cancion_cursor);   // redibuja solo las 2 filas que cambiaron
        Buzzer_Patron(CANCIONES_DATA[cancion_cursor], CANCIONES_LEN[cancion_cursor]);   // previsualiza el sonido de la cancion resaltada
    }
}

/* ========================================================================== */
/* === MENU DE DIFICULTAD (SOLO GUITAR HERO) ================================= */
/* ========================================================================== */
/* Pantalla legada (Renderer_DrawMenu, ver renderer.c) reconectada al
 * recorrido real: aparece SOLO para Guitar Hero, justo despues de confirmar
 * el modo y antes de elegir cancion (ver el bloque de confirmacion de modo
 * mas abajo). IZQUIERDA/DERECHA de cualquiera de los 2 joystick mueve el
 * cursor entre las 3 tarjetas; CUALQUIER boton confirma (via
 * "confirmar_boton", igual que el resto de los menus de este recorrido). */

static void MenuDificultad_Armar(void) {   // llamar SIEMPRE justo antes de mostrar DEMO_MENU_DIFICULTAD -- mismo motivo que MenuCanciones_Armar
    sj2_joy_listo_dir[0] = 0;   // exige ver el stick de J1 centrado antes de aceptar el primer movimiento
    sj2_joy_listo_dir[1] = 0;   // idem para J2
}

static void MenuDificultad_Procesar(uint8_t *screen) {   // mueve el cursor entre las 3 tarjetas con cualquiera de los 2 joystick; confirmar corre en main() via confirmar_boton
    (void)screen;   // esta pantalla no cambia sola de pantalla: la confirmacion (boton) se maneja en main()
    for (uint8_t p = 0; p < 2; p++) {   // revisa el joystick de cada jugador por separado, cualquiera de los 2 puede navegar
        uint8_t dir = Joystick2_LeerDireccion(p);   // 0=ARRIBA, 1=ABAJO (ignoradas aca), 2=IZQUIERDA, 3=DERECHA
        if (dir != 2 && dir != 3) continue;   // solo izquierda/derecha mueven el cursor de esta lista (las tarjetas estan en fila horizontal)

        uint8_t ant = dificultad_cursor;   // cursor anterior (sin usar, se mantiene por simetria con MenuCanciones_Procesar)
        dificultad_cursor = (dir == 2)
            ? (uint8_t)((dificultad_cursor == 0) ? 2 : (dificultad_cursor - 1))   /* IZQUIERDA: tarjeta anterior, con vuelta ciclica */
            : (uint8_t)((dificultad_cursor + 1) % 3);                              /* DERECHA: tarjeta siguiente, con vuelta ciclica */
        (void)ant;   // Renderer_DrawMenu redibuja TODO, no necesita el cursor anterior (a diferencia de la lista de canciones)
        Renderer_DrawMenu(dificultad_cursor);   // redibuja completo -- solo 3 tarjetas, mucho mas barato que la lista de canciones, no hace falta una version incremental
    }
}

/* Recorre ciclicamente el alfabeto A-Z para la letra actual del nombre en
 * edicion (nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]),
 * usando el joystick del jugador que esta escribiendo (arriba = siguiente
 * letra, abajo = letra anterior). La confirmacion, ya sea para avanzar de
 * posicion o de jugador, se gestiona en la logica de confirmacion de menus
 * de main(), no en esta funcion. */
#define INICIALES_JOY_COOLDOWN_MS 180U   // tiempo minimo (en milisegundos) entre 2 cambios de letra aceptados; disminuir este valor permite recorrer las letras mas rapido
static void MenuIniciales_Procesar(void) {   // recorre ciclicamente A-Z la letra actual del nombre en edicion, segun el joystick del jugador correspondiente
    static uint32_t ultimo_mov = 0;   // tick del ultimo cambio de letra aceptado
    uint32_t ahora = HAL_GetTick();   // tick actual
    if ((ahora - ultimo_mov) < INICIALES_JOY_COOLDOWN_MS) return;   // todavia no paso el cooldown desde el ultimo cambio de letra

    /* El mismo joystick fisico que un jugador utilizara durante la partida
     * (determinado por JugadorFisico(), ver mas arriba) es el que emplea
     * para escribir su nombre en esta pantalla. */
    uint8_t hw = JugadorFisico(nombre_jugador_actual);   // joystick fisico que le corresponde a este jugador
    uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;   // lectura cruda del eje Y de ese joystick fisico
    uint16_t bajo_y, alto_y;   // rango sin movimiento del eje Y
    Joy_Umbrales((hw == 0) ? centro_j1y : centro_j2y, &bajo_y, &alto_y);   // calcula ese rango segun el centro medido correcto

    char *c = &nombre_jugadores[nombre_jugador_actual][nombre_pos_actual];   // puntero directo a la letra que se esta editando ahora mismo
    if (jy >= alto_y) {   // stick empujado hacia abajo del rango (arriba, segun la convencion ya corregida)
        *c = (char)((*c >= 'Z') ? 'A' : (char)(*c + 1));   // siguiente letra, con vuelta ciclica de Z a A
        ultimo_mov = ahora;   // reinicia el cooldown
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);   // redibuja SOLO esa letra en pantalla
    } else if (jy <= bajo_y) {   // stick empujado hacia el otro lado (abajo)
        *c = (char)((*c <= 'A') ? 'Z' : (char)(*c - 1));   // letra anterior, con vuelta ciclica de A a Z
        ultimo_mov = ahora;   // reinicia el cooldown
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);
    }
}

int main(void) {   // punto de entrada del programa: inicializa todo el hardware, calibra los joysticks y entra al bucle principal a aproximadamente 30 cuadros por segundo
    HAL_Init();   // inicializa el HAL de ST (SysTick, flash latency, etc.), requerido antes de cualquier otro periferico
    SystemClock_Config();   // configura el reloj del sistema (generado, ver definicion mas abajo)

    MX_GPIO_Init();   // configura todos los pines GPIO (entradas de botones/joystick, salidas de LED/pantalla/buzzer)
    MX_SPI1_Init();   // configura SPI1 para la pantalla
    TIM3_ADCTrigger_Init();   // configura TIM3 (disparador del ADC cada 20ms), pero todavia sin arrancarlo
    ADC1_Joystick_Init();   // configura el ADC1 en modo escaneo de 4 canales (joystick 1 y 2), pero todavia sin arrancarlo
    MX_TIM4_Buzzer_Init();   // configura TIM4 (interrupcion periodica del buzzer)
    MX_USART2_UART_Init();   // configura USART2 (consola de depuracion)

    BotonesChase_Iniciar();   // arranca la animacion de LEDs tipo "tira" que corre durante el splash
    ILI9341_Init();   // inicializa el controlador de la pantalla (secuencia de comandos de arranque del ILI9341)

    HAL_TIM_Base_Start(&htim3);   // arranca TIM3 -- desde aca en adelante, el ADC se dispara cada 20ms
    HAL_ADC_Start_IT(&hadc1);   // arranca el ADC en modo interrupcion -- desde aca, HAL_ADC_ConvCpltCallback empieza a recibir conversiones

    printf("\r\n=== Final Box boot OK (PA2/PA3 @ 115200 8N1) ===\r\n");   // primer mensaje de arranque por consola, confirma que el UART esta vivo

    /* Indicador visual de calibración */
    ILI9341_FillScreen(COLOR_BLACK);   // pantalla negra mientras se calibra (todavia no hay nada mas que mostrar)
    ILI9341_DrawString(20, 100, "Calibrando joysticks...", COLOR_WHITE, COLOR_BLACK, 2);   // aviso principal
    ILI9341_DrawString(20, 130, "No tocar los sticks", COLOR_YELLOW, COLOR_BLACK, 1);   // instruccion critica: si se toca el stick aca, la calibracion sale mal
    HAL_Delay(300);   // espera bloqueante de 300ms para dejar que el filtro EMA se asiente en el valor real de reposo antes de medir el centro; SOLO se usa HAL_Delay largo aca, antes de que exista juego real

    centro_j1x = joystick_x;   // toma la lectura filtrada actual como centro "real" medido del eje X de J1
    centro_j1y = joystick_y;   // idem eje Y de J1
    centro_j2x = joystick2_x;   // idem eje X de J2
    centro_j2y = joystick2_y;   // idem eje Y de J2
    printf("[CALIB] centro j1=(%u,%u) j2=(%u,%u)\r\n", centro_j1x, centro_j1y, centro_j2x, centro_j2y);   // log de los 4 centros medidos, para poder verificarlos por consola

#if BUZZER_DIAGNOSTICO_BARRIDO   // este bloque completo solo se compila si la constante esta en 1
    Buzzer_BarridoDiagnostico();   // solo compila/corre si BUZZER_DIAGNOSTICO_BARRIDO esta en 1 (ver su #define mas arriba)
#endif   // cierra el bloque condicionado

    uint8_t  screen        = DEMO_SPLASH;   // pantalla inicial: splash de bienvenida
    uint8_t  last_paso_sim = 0xFF;   /* fuerza el primer dibujo de las vistas previas */
    Demo_Enter(screen);   // dibuja el primer cuadro (el splash)

    uint32_t last_frame_tick = 0;   /* para sincronización no bloqueante */

    while (1) {   // loop principal: corre para siempre, ritmo fijo de RENDER_TICK_MS por vuelta
        /* ---- sincronización de frame (no bloqueante) ---- */
        if (HAL_GetTick() - last_frame_tick < RENDER_TICK_MS) continue;   // todavia no paso el tiempo de un frame -- vuelve a evaluar sin hacer nada (no bloqueante: el ADC/UART siguen funcionando por interrupcion mientras tanto)
        last_frame_tick = HAL_GetTick();   // marca el inicio de este nuevo frame

        /* ---- diagnóstico ADC (solo cuando no hay partida) ---- */
        if (!en_juego_real && !en_juego_real_2p && !en_juego_real_botones && !en_juego_real_guitar) {   // ningun modo real esta corriendo ahora mismo
            static uint32_t dbg_tick = 0;   // tick del ultimo log de diagnostico impreso
            if (HAL_GetTick() - dbg_tick >= 500) {   // cada 500ms (no cada frame, para no inundar la consola)
                dbg_tick = HAL_GetTick();   // marca este log como el ultimo impreso
                printf("[ADC] j1=(%u,%u) j2=(%u,%u)\r\n", joystick_x, joystick_y, joystick2_x, joystick2_y);   // log periodico de diagnostico con las 4 lecturas crudas filtradas
            }
        }

        /* Recuperacion automatica de la pantalla tras ruido electrico
         * persistente en el bus SPI (ver ILI9341_FalloComunicacionDetectado
         * en ili9341.c, y el comentario de LCD_SPI_Send en ese mismo
         * archivo) -- maxima prioridad, igual que el combo de salida de
         * abajo: el controlador de la pantalla puede haber quedado en
         * blanco o desincronizado (p. ej. un glitch en RST por el mismo
         * ruido de los LEDs de los botones via ULN2003A), y no hay forma de
         * leer su estado de vuelta (SPI1 es solo-escritura) para confirmarlo
         * antes de actuar. Se reinicializa el controlador por completo y se
         * reconstruye desde el splash -- perder la partida en curso es
         * preferible a una pantalla en blanco permanente hasta reiniciar la
         * placa a mano. Esto es un parche sobre el sintoma, NO arregla la
         * causa electrica de fondo. */
        if (ILI9341_FalloComunicacionDetectado()) {   // se detecto una racha de fallas de SPI consecutivas (ver LCD_SPI_FALLAS_UMBRAL en ili9341.c)
            printf("[LCD] fallas de SPI persistentes -> reinicializando pantalla\r\n");
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // apaga todos los flags de partida real en curso
            btn_modo_1p    = 0;   // limpia el flag de 1 jugador de Botones
            guitar_modo_1p = 0;   // idem Guitar Hero
            conteo_auto    = 0;   // cancela cualquier conteo 3-2-1-GO en curso
            Buzzer_Fondo_Detener();   // corta la musica de fondo
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs
            ILI9341_Init();          // reinicializa el controlador de la pantalla por completo (recupera de un posible glitch en RST)
            ILI9341_SetPortrait(1);   // vuelve a retrato (orientacion del splash/menus)
            ILI9341_SetFlip180(0);   // sin flip
            screen = DEMO_SPLASH;   // vuelve al splash
            Demo_Enter(screen);   // dibuja el splash
            BotonesChase_Iniciar();   // reactiva la animacion de LEDs
            continue;   // salta el resto del loop este tick
        }

        /* Combo de salida ROJO+AMARILLO (máxima prioridad) */
        if (ComboSalir_Detectado()) {   // se revisa ANTES que cualquier otra logica del loop, para poder salir desde cualquier pantalla/modo
            en_juego_real         = 0;   // apaga todos los flags de "partida real" en curso
            en_juego_real_2p      = 0;   // apaga tambien el flag de SimonJoy 2 jugadores
            en_juego_real_botones = 0;   // idem Botones
            en_juego_real_guitar  = 0;   // idem Guitar Hero
            btn_modo_1p           = 0;   // limpia el flag de "modo 1 jugador" de BOTONES -- si quedara en 1, la proxima partida de 2 jugadores leeria solo el hardware fisico del jugador 0 para ambos lados (ver Botones_LeerColor/Boton_LED/ComboSalir_Detectado)
            guitar_modo_1p        = 0;   // idem para Guitar Hero -- ambos flags se recalculan igual al confirmar el proximo modo, esto es solo limpieza defensiva al salir
            conteo_auto           = 0;   // cancela cualquier conteo 3-2-1-GO en curso
            Buzzer_Fondo_Detener();   // corta la musica de fondo si estaba sonando
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs de botones arcade
            ILI9341_SetPortrait(1);   // fuerza retrato (algunos modos reales quedan en retrato)
            ILI9341_SetFlip180(0);   // pantalla SIN flip -- el splash/menus deben verse normales, no girados
            screen = DEMO_SPLASH;   // vuelve al splash
            Demo_Enter(screen);   // dibuja el splash
            BotonesChase_Iniciar();   // reactiva la animacion de LEDs del splash
            continue;   // salta el resto del loop este tick (ya se manejo la prioridad maxima)
        }

        /* Atajo de salida rapida en pantallas de FIN de partida de 1 jugador
         * (GAME OVER de Botones/SimonJoy, RONDA COMPLETA de Guitar Hero):
         * mantener ROJO 2s vuelve directo al menu de SELECCION DE MODO (no
         * al de jugadores, ni al splash), con el cursor en el modo recien
         * jugado -- ver SalirGameOver1P_Detectado para el detalle de en que
         * pantallas aplica. */
        if (SalirGameOver1P_Detectado()) {   // ROJO sostenido 2s en fin de partida 1 jugador
            screen = guitar_modo_1p ? DEMO_MODO_GUITAR   // vuelve al mismo modo que se estaba jugando
                    : btn_modo_1p    ? DEMO_MODO_SIMON
                                      : DEMO_MODO_SIMONJOY;   // ninguno de los 2 flags activo -> fue SimonJoy 1 jugador
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // apaga todos los flags de partida real
            btn_modo_1p    = 0;   // limpia el flag de 1 jugador de Botones
            guitar_modo_1p = 0;   // idem Guitar Hero
            Buzzer_Fondo_Detener();   // corta la musica de fondo
            for (uint8_t c = 0; c < 4; c++) Boton_LED(0, c, 0);   // apaga los 4 LEDs del jugador fisico 0 (el unico usado en modo 1 jugador)
            ILI9341_SetFlip180(0);   // sin flip, orientacion normal del menu
            Demo_Enter(screen);   // dibuja el menu destino
            continue;   // salta el resto del loop este tick
        }

        /* Mismo atajo, para 2 jugadores: solo cuando AMBOS ya terminaron su
         * partida (ver SalirGameOver2P_Detectado) -- mantiene "2 JUGADORES"
         * elegido, solo vuelve al menu de SELECCION DE MODO. */
        if (SalirGameOver2P_Detectado()) {   // ROJO sostenido 2s en fin de partida 2 jugadores (ambos ya terminaron)
            screen = en_juego_real_guitar   ? DEMO_MODO_GUITAR   // vuelve al mismo modo que se estaba jugando
                    : en_juego_real_botones ? DEMO_MODO_SIMON
                                              : DEMO_MODO_SIMONJOY;   // ninguno de los 2 -> fue SimonJoy 2 jugadores
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // apaga todos los flags de partida real
            btn_modo_1p    = 0;   // limpia el flag de 1 jugador de Botones (defensivo, no deberia estar en 1 en 2P)
            guitar_modo_1p = 0;   // idem Guitar Hero
            Buzzer_Fondo_Detener();   // corta la musica de fondo
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs, de ambos jugadores
            ILI9341_SetFlip180(0);   // sin flip, orientacion normal del menu
            Demo_Enter(screen);   // dibuja el menu destino
            continue;   // salta el resto del loop este tick
        }

        /* Sostener VERDE 2s durante una partida real de Guitar Hero (jugando
         * o en RONDA COMPLETA) salta a elegir otra cancion, sin pasar por el
         * menu de modos ni tocar la dificultad actual (ver
         * GH_BotonSostenidoDetectado). guitar_modo_1p/jugadores_seleccionados
         * quedan intactos: al confirmar la cancion nueva, el conteo
         * automatico vuelve a calcular guitar_modo_1p igual que siempre. */
        if (en_juego_real_guitar && GH_BotonSostenidoDetectado(1, gh_cambiar_cancion_tick)) {   // VERDE sostenido 2s durante Guitar Hero (jugando o en fin de ronda)
            printf("[COMBO] VERDE 2s en Guitar Hero -> elegir cancion\r\n");
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // corta la partida actual
            Buzzer_Fondo_Detener();   // corta la musica de fondo
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs
            ILI9341_SetFlip180(0);   // sin flip
            screen = DEMO_MENU_CANCIONES;   // salta directo al menu de canciones
            Demo_Enter(screen);   // lo dibuja
            MenuCanciones_Armar();   // exige stick centrado antes del primer movimiento
            continue;   // salta el resto del loop este tick
        }

        /* Mismo atajo con AZUL, para elegir otra dificultad -- desde ahi el
         * flujo normal sigue a DEMO_MENU_CANCIONES (ver el bloque de
         * confirmacion de DEMO_MENU_DIFICULTAD), asi que tambien deja
         * elegir cancion de paso. */
        if (en_juego_real_guitar && GH_BotonSostenidoDetectado(2, gh_cambiar_dificultad_tick)) {   // AZUL sostenido 2s durante Guitar Hero
            printf("[COMBO] AZUL 2s en Guitar Hero -> elegir dificultad\r\n");
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // corta la partida actual
            Buzzer_Fondo_Detener();   // corta la musica de fondo
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs
            ILI9341_SetFlip180(0);   // sin flip
            screen = DEMO_MENU_DIFICULTAD;   // salta directo al menu de dificultad
            Demo_Enter(screen);   // lo dibuja
            MenuDificultad_Armar();   // exige stick centrado antes del primer movimiento
            continue;   // salta el resto del loop este tick
        }

        Buzzer_Actualizar();   // tick no bloqueante del buzzer (SFX + musica de fondo), siempre corre sin importar la pantalla
        if (screen == DEMO_SPLASH) BotonesChase_Actualizar();   // la animacion de LEDs solo avanza mientras se ve el splash
        uint8_t avanzar = Boton_B1_Flanco();   // flanco de B1 este tick (aunque en la practica el boton fisico esta inalcanzable, la logica sigue viva)

        /* Confirmacion de los menus de JUGADORES/MODO con CUALQUIER boton
         * arcade (B1 fisicamente inaccesible, ver comentario de main.c
         * arriba) -- calculado ANTES de mover el cursor (MenuXxx_Procesar,
         * abajo) para que un boton confirme lo que se estaba mostrando este
         * tick, no lo que el cursor recien paso a mostrar. Se computa una
         * sola vez (BotonesNavegacion_Presionado consume el flanco) y solo
         * aplica en estas 2 pantallas -- el resto de las confirmaciones de
         * abajo (salir de un juego real, arrancar preview) siguen atadas
         * solo a B1 por ahora, sin cambios de comportamiento ahi. */
        uint8_t en_pantalla_modo = (screen == DEMO_MODO_SIMON || screen == DEMO_MODO_SIMONJOY || screen == DEMO_MODO_GUITAR);   // true en cualquiera de las 3 tarjetas del menu de modo
        uint8_t confirmar_boton = (screen == DEMO_SPLASH || screen == DEMO_JUGADORES_1 ||   // pantallas donde CUALQUIER boton arcade confirma (no solo B1)
                                    screen == DEMO_JUGADORES_2 || screen == DEMO_INICIALES || en_pantalla_modo ||
                                    screen == DEMO_MENU_DIFICULTAD || screen == DEMO_MENU_CANCIONES)
                                 ? BotonesNavegacion_Presionado() : 0;   // fuera de esas pantallas, confirmar_boton siempre es 0 (solo B1 confirma ahi)

        /* ------------------------------------------------------------------
         * MENÚS DE SELECCIÓN (con joystick y botones, sin B1 para mover)
         * ------------------------------------------------------------------ */
        if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {   // menu de cantidad de jugadores
            MenuJugadores_Procesar(&screen);
        }
        else if (screen == DEMO_INICIALES) {   // pantalla de captura de nombre
            MenuIniciales_Procesar();
        }
        else if (en_pantalla_modo) {   // menu de modo de juego
            MenuModos_Procesar(&screen);
        }
        else if (screen == DEMO_MENU_DIFICULTAD) {   // menu de dificultad (solo Guitar Hero)
            MenuDificultad_Procesar(&screen);
        }
        else if (screen == DEMO_MENU_CANCIONES) {   // menu de seleccion de cancion (solo Guitar Hero)
            MenuCanciones_Procesar(&screen);
        }

        /* ------------------------------------------------------------------
         * CONFIRMACIÓN CON B1 (o navegación en lista de canciones)
         * ------------------------------------------------------------------ */
        if (avanzar || confirmar_boton) {   // hubo una confirmacion este tick (B1 o, donde aplica, cualquier boton arcade)
            if (en_juego_real || en_juego_real_2p || en_juego_real_botones || en_juego_real_guitar) {   // habia una partida real en curso
                /* B1 durante un juego real: salir al recorrido */
                en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;   // corta la partida
                Buzzer_Fondo_Detener();   // corta la musica de fondo
                for (uint8_t p = 0; p < 2; p++)
                    for (uint8_t c = 0; c < 4; c++)
                        Boton_LED(p, c, 0);   // apaga los 8 LEDs
                ILI9341_SetFlip180(0);   // sin flip
                Demo_Enter(screen);   // redibuja la pantalla actual (sin cambiar `screen`, vuelve al recorrido de diseño)
                avanzar = 0;   // consume el flanco
            }
            else if (screen == DEMO_SPLASH) {   // confirmacion en el splash
                /* Transicion de la pantalla de bienvenida al menu de
                 * seleccion de jugadores. */
                BotonesChase_Detener();   // apaga la animacion de LEDs del splash
                screen = DEMO_JUGADORES_1;   // avanza al menu de jugadores
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {   // confirmacion en el menu de jugadores
                jugadores_seleccionados = (screen == DEMO_JUGADORES_2) ? 2 : 1;   // fija la cantidad elegida segun en que tarjeta estaba el cursor
                printf("[MENU] jugadores = %u\r\n", jugadores_seleccionados);
                /* Reutiliza nombres ingresados en una partida anterior dentro
                 * de la misma sesion de encendido: los arreglos
                 * nombre_confirmado[] y nombre_jugadores[] son variables
                 * estaticas que no se reinician entre partidas, de modo que,
                 * si los jugadores requeridos para este modo ya confirmaron
                 * su nombre previamente, la pantalla DEMO_INICIALES se omite
                 * y se avanza directamente a la seleccion de modo (este
                 * estado se pierde unicamente al apagar la placa, ya que no
                 * se persiste en memoria flash). Se utiliza el indicador
                 * nombre_confirmado[] en lugar de verificar si el buffer esta
                 * vacio, porque el valor por defecto del buffer ("AAA") nunca
                 * lo esta. */
                uint8_t nombres_listos = nombre_confirmado[0] &&   // jugador 1 ya confirmo su nombre en esta sesion de encendido
                                          (jugadores_seleccionados < 2 || nombre_confirmado[1]);   // Y (es 1 jugador, o el jugador 2 tambien ya confirmo)
                if (nombres_listos) {   // ya hay nombres validos de una partida anterior -- se saltea la captura
                    printf("[INICIALES] reutilizando J1=%s J2=%s\r\n", nombre_jugadores[0], nombre_jugadores[1]);
                    screen = DEMO_MODO_SIMON;   // va directo al menu de modo
                } else {   // hace falta pedir el nombre de al menos un jugador
                    screen = DEMO_INICIALES;
                }
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_INICIALES) {   // confirmacion de una letra en la captura de nombre
                printf("[INICIALES] J%u letra %u = %c confirmada\r\n",
                       (unsigned)(nombre_jugador_actual + 1), (unsigned)(nombre_pos_actual + 1),
                       nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);
                Renderer_ConfirmarNombreLetra(nombre_pos_actual, nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);   // deja de resaltar esa letra

                if (nombre_pos_actual < 2) {   // todavia quedan letras del nombre (3 letras, indices 0-2) por confirmar
                    nombre_pos_actual++;   // avanza a la siguiente posicion
                    Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual,
                                                nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);   // resalta la nueva letra en edicion
                } else if (jugadores_seleccionados == 2 && nombre_jugador_actual == 0) {   // jugador 1 termino su nombre Y falta el jugador 2
                    nombre_confirmado[0]  = 1;   // marca al jugador 1 como confirmado
                    nombre_jugador_actual = 1;   // pasa a capturar el nombre del jugador 2
                    nombre_pos_actual     = 0;   // arranca desde la primera letra
                    Renderer_DrawNombre(1, nombre_jugadores[1], 0);   // dibuja la pantalla de captura para el jugador 2
                } else {   // ya se termino de capturar el/los nombre(s) necesario(s)
                    nombre_confirmado[nombre_jugador_actual] = 1;   // marca a este jugador como confirmado
                    printf("[INICIALES] J1=%s J2=%s\r\n", nombre_jugadores[0], nombre_jugadores[1]);
                    screen = DEMO_MODO_SIMON;   // avanza al menu de modo
                    Demo_Enter(screen);
                }
                avanzar = 0;
            }
            else if (en_pantalla_modo) {   // confirmacion en el menu de modo de juego
                modo_confirmado = (screen == DEMO_MODO_SIMONJOY) ? MODO_SEL_SIMONJOY :   // guarda que modo quedo elegido, segun en que tarjeta estaba el cursor
                                   (screen == DEMO_MODO_GUITAR)  ? MODO_SEL_GUITAR  : MODO_SEL_BOTONES;
                printf("[MENU] modo = %s\r\n",
                       modo_confirmado == MODO_SEL_SIMONJOY ? "SIMONJOY" :
                       modo_confirmado == MODO_SEL_GUITAR   ? "GUITAR"   : "BOTONES");
                if (modo_confirmado == MODO_SEL_GUITAR) {   // Guitar Hero: pasa primero por dificultad y cancion
                    /* Unicamente Guitar Hero pasa primero por dificultad y
                     * lista de canciones (pedido explicito del usuario) --
                     * SimonJoy y Botones siguen yendo derecho al conteo, sin
                     * tocar su flujo. */
                    screen = DEMO_MENU_DIFICULTAD;   // salta a elegir dificultad
                    Demo_Enter(screen);
                    MenuDificultad_Armar();   // exige ver el stick centrado antes de aceptar el primer izquierda/derecha (evita heredar el "arrastre" de haber llegado hasta GT HERO en el menu anterior)
                    Buzzer_Beep(100);   // beep de confirmacion
                } else {   // Simon o Simon+Joystick: van derecho al conteo
                    screen = DEMO_CONTEO_3;
                    Demo_Enter(screen);
                    Buzzer_Beep(100);   // beep de confirmacion
                    conteo_auto = 1;   // arranca el conteo automatico
                    conteo_tick = HAL_GetTick();   // marca el instante de arranque del primer numero
                }
                avanzar = 0;
            }
            else if (screen == DEMO_MENU_DIFICULTAD) {   // confirmacion en el menu de dificultad
                /* Cualquier boton confirma la tarjeta resaltada por el
                 * cursor (movido con el joystick, ver MenuDificultad_
                 * Procesar) y avanza a elegir la cancion inicial. */
                printf("[MENU] dificultad de Guitar Hero = %u\r\n", dificultad_cursor);
                screen = DEMO_MENU_CANCIONES;   // avanza a elegir cancion
                Demo_Enter(screen);
                MenuCanciones_Armar();   // exige ver el stick centrado antes de aceptar el primer arriba/abajo
                Buzzer_Beep(100);   // beep de confirmacion
                avanzar = 0;
            }
            else if (screen == DEMO_MENU_CANCIONES) {   // confirmacion en el menu de canciones
                /* Cualquier boton confirma la cancion resaltada por el
                 * cursor (movido con el joystick, ver MenuCanciones_Procesar)
                 * y arranca el conteo 3-2-1-GO de Guitar Hero. */
                printf("[MENU] cancion inicial de Guitar Hero = idx %u\r\n", cancion_cursor);
                screen = DEMO_CONTEO_3;   // arranca el conteo
                Demo_Enter(screen);
                Buzzer_Beep(100);   // beep de confirmacion
                conteo_auto = 1;   // arranca el conteo automatico
                conteo_tick = HAL_GetTick();   // marca el instante de arranque del primer numero
                avanzar = 0;
            }
            else if (screen == DEMO_PREVIEW_SIMONJOY) {   // confirmacion en la vista previa (recorrido de diseño) -- arranca la partida REAL
                en_juego_real = 1;
                SimonJoy_Iniciar();
                avanzar = 0;
            }
            else if (screen == DEMO_PREVIEW_SIMON) {   // idem para Simon Clasico
                en_juego_real_botones = 1;
                btn_modo_1p    = (jugadores_seleccionados == 1);   // 1 jugador si asi se eligio
                guitar_modo_1p = 0;   // limpia el flag de Guitar Hero por si quedo en 1 de una sesion anterior (ver el mismo reset en el conteo automatico, mas abajo)
                if (jugadores_seleccionados == 2) Botones_IniciarAmbos();
                else Botones_IniciarSolo();
                avanzar = 0;
            }
            else if (screen == DEMO_JUGANDO) {   // confirmacion en el recorrido de diseño de Guitar Hero (maqueta, no el modo real)
                GuitarHero_IntentarGolpe();
                avanzar = 0;
            }
        }

        /* ------------------------------------------------------------------
         * CONTEO AUTOMÁTICO 3-2-1-GO
         * ------------------------------------------------------------------ */
        if (conteo_auto) {   // el conteo 3-2-1-GO esta avanzando solo
            if (HAL_GetTick() - conteo_tick >= CONTEO_PASO_MS) {   // ya paso el tiempo de mostrar este numero
                conteo_tick = HAL_GetTick();   // marca el instante de este nuevo numero
                if (screen == DEMO_CONTEO_GO) {   // ya se llego al final del conteo ("GO") -- arranca el modo elegido de verdad
                    conteo_auto = 0;   // el conteo termino, no sigue avanzando
                    printf("[GO] modo=%s jugadores=%u\r\n",
                           modo_confirmado == MODO_SEL_SIMONJOY ? "SIMONJOY" :
                           modo_confirmado == MODO_SEL_GUITAR   ? "GUITAR"   : "BOTONES",
                           jugadores_seleccionados);
                    /* Limpia SIEMPRE los 2 flags de "modo 1 jugador" antes de
                     * arrancar el modo elegido, sin importar cual haya
                     * quedado activo en una partida anterior de OTRO modo:
                     * btn_modo_1p y guitar_modo_1p comparten la logica de
                     * lectura de hardware de Botones_LeerColor/Boton_LED/
                     * ComboSalir_Detectado ("if (btn_modo_1p ||
                     * guitar_modo_1p) p = 0"), asi que si uno de los 2
                     * quedaba en 1 de una sesion 1-jugador anterior, una
                     * partida de 2 jugadores del OTRO modo forzaba p=0 para
                     * ambos lados -- exactamente el bug reportado de
                     * "arranca en 2 jugadores pero solo responde un lado". */
                    btn_modo_1p    = 0;   // limpia el flag de 1 jugador de Botones, se recalcula abajo solo si corresponde
                    guitar_modo_1p = 0;   // idem Guitar Hero
                    switch (modo_confirmado) {   // arranca el modo que se eligio
                    case MODO_SEL_SIMONJOY:   // Simon + Joystick
                        screen = DEMO_PREVIEW_SIMONJOY;
                        if (jugadores_seleccionados == 2) {
                            en_juego_real_2p = 1;
                            SimonJoy2_IniciarAmbos();
                        } else {
                            en_juego_real = 1;
                            SimonJoy_Iniciar();
                        }
                        break;
                    case MODO_SEL_GUITAR:   // Guitar Hero
                        screen = DEMO_JUGANDO;
                        en_juego_real_guitar = 1;
                        guitar_modo_1p = (jugadores_seleccionados != 2);   // 1 jugador si no se eligieron 2
                        if (jugadores_seleccionados == 2) {
                            GuitarHero2_IniciarAmbos();
                        } else {
                            GuitarHero_IniciarSolo();
                        }
                        break;
                    default: /* MODO_SEL_BOTONES */   // Simon con botones arcade
                        screen = DEMO_PREVIEW_SIMON;
                        en_juego_real_botones = 1;
                        if (jugadores_seleccionados == 2) {
                            btn_modo_1p = 0;   // 2 jugadores: layout Cockpit dividido
                            Botones_IniciarAmbos();
                        } else {
                            btn_modo_1p = 1;   // 1 jugador: pantalla completa
                            Botones_IniciarSolo();
                        }
                        break;
                    }
                } else {   // todavia no se llego a DEMO_CONTEO_GO -- sigue avanzando el conteo numero por numero
                    screen++;   // avanza al siguiente numero del conteo (3->2->1->GO, orden del enum)
                    Demo_Enter(screen);   // dibuja el numero nuevo
                    if (screen == DEMO_CONTEO_GO) {   // ultimo paso: jingle distinto para "GO"
                        static const PasoSonido_t BEEP_GO[3] = {   // jingle de "GO": 2 notas ascendentes
                            { SOL4, 90 }, { 0, 20 }, { DO5, 220 }
                        };
                        Buzzer_Patron(BEEP_GO, 3);   // suena el jingle de GO
                    } else {   // 3, 2 o 1: beep corto simple
                        Buzzer_Beep(100);
                    }
                }
            }
            continue;   // salta el resto del loop este tick (el conteo ya se atendio)
        }

        /* ------------------------------------------------------------------
         * JUEGOS ACTIVOS
         * ------------------------------------------------------------------ */
        if (en_juego_real) {   // SimonJoy 1 jugador en curso
            SimonJoy_Actualizar();
            continue;   // salta el resto del loop, ya se atendio esta partida
        }
        if (en_juego_real_2p) {   // SimonJoy 2 jugadores en curso
            SimonJoy2_Actualizar();
            continue;
        }
        if (en_juego_real_botones) {   // Botones (1 o 2 jugadores) en curso
            Botones_Actualizar();
            continue;
        }
        if (en_juego_real_guitar) {   // Guitar Hero (1 o 2 jugadores) en curso
            GuitarHero_Actualizar();
            continue;
        }

        /* ------------------------------------------------------------------
         * RECORRIDO DE DISEÑO (PANTALLAS ESTÁTICAS)
         * ------------------------------------------------------------------ */
        switch (screen) {   // ninguna partida real en curso -- solo queda animar las pantallas estaticas del recorrido de diseño

        case DEMO_SPLASH:   // splash: Renderer_Update anima el parpadeo del texto
        case DEMO_JUGANDO:   // recorrido de diseño de Guitar Hero: las 2 notas fijas siguen cayendo
        case DEMO_RESULTADO:   // pantalla de resultado de adorno: nada que animar, solo la dibuja Demo_Enter
            if (screen == DEMO_JUGANDO) {   // solo en el recorrido de diseño de Guitar Hero se mueven las notas de prueba
                for (uint8_t i = 0; i < 2; i++) {   // las 2 notas fijas del recorrido de diseño
                    Nota_t *n = &gs.notas[i];
                    n->x_prev = n->x_rel;   // guarda posicion anterior para el borrado delta
                    n->x_rel  = (int16_t)(n->x_rel + gs.nota_speed);   // avanza la nota (aca suma, al reves del modo real, ver comentario de la seccion)
                    if (n->x_rel > (int16_t)PLAYER_W) n->x_rel = -NOTE_W;   // reaparece del otro lado al salir de pantalla (loop infinito de la maqueta)
                }
            }
            Renderer_Update(&gs);   // anima/dibuja el frame actual del recorrido de diseño
            break;

        case DEMO_PREVIEW_SIMON:   // vista previa de Simon Clasico (antes del menu de jugadores confirma)
        case DEMO_PREVIEW_SIMONJOY: {   // vista previa de Simon+Joystick
            uint8_t paso = Demo_PasoSimon();   // que paso animar ahora, derivado del reloj
            if (paso != last_paso_sim) {   // solo redibuja si el paso a animar cambio
                if (screen == DEMO_PREVIEW_SIMON)
                    Renderer_DrawModoSimonClasico(paso, paso);   // redibuja las 2 mitades con el mismo paso de adorno
                else
                    Renderer_UpdateModoSimonJoystick1P(last_paso_sim, paso);   // redibuja solo la flecha que cambio
                last_paso_sim = paso;   // recuerda el paso ya animado
            }
            break;
        }

        default:   // resto de pantallas (menus): no necesitan animacion por tick, ya quedaron dibujadas por Demo_Enter/MenuXxx_Procesar
            break;
        }
    }
}

/* ========================================================================== */
/* === RELOJ DEL SISTEMA ===================================================== */
/* ========================================================================== */

static void SystemClock_Config(void) {   // configura el reloj del microcontrolador (HSI + PLL) -- generado por CubeMX/CubeIDE, no tocar a mano
    RCC_OscInitTypeDef osc = {0};   // configuracion del oscilador (fuente de reloj)
    RCC_ClkInitTypeDef clk = {0};   // configuracion de los buses de reloj derivados

    /* HSI 16MHz sin PLL — suficiente para SPI@8MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;   // usa el oscilador interno HSI (16MHz), no un cristal externo
    osc.HSIState            = RCC_HSI_ON;   // habilita el HSI
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;   // calibracion de fabrica del HSI, sin ajuste manual
    osc.PLL.PLLState        = RCC_PLL_NONE;   // sin PLL: el sistema corre directo a 16MHz (HSI), no se multiplica el reloj
    HAL_RCC_OscConfig(&osc);   // aplica la configuracion del oscilador

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK   // que buses de reloj configurar: sistema, AHB y los 2 APB
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;   // el reloj de sistema viene directo del HSI (sin PLL)
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16MHz */   // AHB sin dividir
    clk.APB1CLKDivider = RCC_HCLK_DIV1;      /* APB1  = 16MHz */   // APB1 sin dividir
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* APB2  = 16MHz (SPI1) */   // APB2 sin dividir -- de aca sale el reloj que alimenta a SPI1
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);   // aplica la configuracion; FLASH_LATENCY_0 alcanza a 16MHz (no hace falta mas espera de wait-states)
}

/* ========================================================================== */
/* === GPIO — PANTALLA, JOYSTICKS, BOTONES ARCADE Y BUZZER =================== */
/* ========================================================================== */

static void MX_GPIO_Init(void) {   // configura todos los pines GPIO: entradas de botones/joystick, salidas de LEDs/pantalla/buzzer
    GPIO_InitTypeDef g = {0};   // struct de configuracion reutilizada para cada grupo de pines (se pisa entre usos)

    __HAL_RCC_GPIOA_CLK_ENABLE();   // habilita el reloj del puerto A (sin esto, sus pines no responden)
    __HAL_RCC_GPIOB_CLK_ENABLE();   // idem puerto B
    __HAL_RCC_GPIOC_CLK_ENABLE();   // idem puerto C

    g.Mode  = GPIO_MODE_OUTPUT_PP;   // salida push-pull (puede tanto poner en alto como en bajo activamente)
    g.Pull  = GPIO_NOPULL;   // sin resistencia de pull interna (no hace falta en una salida)
    g.Speed = GPIO_SPEED_FREQ_HIGH;   // velocidad de conmutacion alta (necesaria para SPI)

    g.Pin = LCD_RST_PIN;   // pin de RESET de la pantalla
    HAL_GPIO_Init(LCD_RST_PORT, &g);
    LCD_RST_HIGH();   // arranca en alto (inactivo, sin resetear)

    g.Pin = LCD_CS_PIN;   // pin de Chip Select de la pantalla
    HAL_GPIO_Init(LCD_CS_PORT, &g);
    LCD_CS_HIGH();   // arranca en alto (chip no seleccionado)

    g.Pin = LCD_DC_PIN;   // pin de Data/Command de la pantalla
    HAL_GPIO_Init(LCD_DC_PORT, &g);

    /* B1 (PC13) — pull-up externo R30=4k7 en la Nucleo, no usar PULLUP sw */
    g.Mode = GPIO_MODE_INPUT;   // B1 es una entrada
    g.Pull = GPIO_NOPULL;   // sin pull-up interno: ya hay uno externo en la placa Nucleo
    g.Pin  = BTN_USER_PIN;
    HAL_GPIO_Init(BTN_USER_PORT, &g);

    /* pa1/pa4: entradas analogicas del joystick (pa1=vry, pa4=vrx) */
    g.Mode = GPIO_MODE_ANALOG;   // modo analogico, requerido para que el ADC pueda leer estos pines
    g.Pull = GPIO_NOPULL;   // sin pull en una entrada analogica
    g.Pin  = GPIO_PIN_1 | GPIO_PIN_4;   // VRy (PA1) y VRx (PA4) del joystick 1
    HAL_GPIO_Init(GPIOA, &g);

    /* pc0/pc1: entradas analogicas del joystick 2 (pc0=vry2, pc1=vrx2) */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = GPIO_PIN_0 | GPIO_PIN_1;   // VRy2 (PC0) y VRx2 (PC1) del joystick 2
    HAL_GPIO_Init(GPIOC, &g);

    /* botones arcade: 8 switches (entrada, pull-up interno) + 8 LED (salida
     * hacia ULN2003A) */
    g.Mode = GPIO_MODE_INPUT;   // los switches de los botones son entradas
    g.Pull = GPIO_PULLUP;   // pull-up interno: el boton conecta a tierra al presionar (activo en bajo)
    for (uint8_t p = 0; p < 2; p++) {   // recorre los 2 jugadores
        for (uint8_t c = 0; c < 4; c++) {   // y los 4 colores de cada uno
            g.Pin = BTN_SW[p][c].pin;   // toma el pin de ESTE switch de la tabla BTN_SW
            HAL_GPIO_Init(BTN_SW[p][c].port, &g);
        }
    }

    g.Mode  = GPIO_MODE_OUTPUT_PP;   // los LEDs (via ULN2003A) son salidas
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;   // no hace falta velocidad alta para encender/apagar un LED
    for (uint8_t p = 0; p < 2; p++) {   // recorre los 2 jugadores
        for (uint8_t c = 0; c < 4; c++) {   // y los 4 colores de cada uno
            g.Pin = BTN_LED[p][c].pin;   // toma el pin de ESTE LED de la tabla BTN_LED
            HAL_GPIO_Init(BTN_LED[p][c].port, &g);
            HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, GPIO_PIN_RESET);   // apaga el LED de entrada (RESET = LED apagado via el driver ULN2003A)
        }
    }

    /* buzzer en pa6 */
    g.Mode  = GPIO_MODE_OUTPUT_PP;   // el buzzer se maneja como salida digital normal (alternada por software, no PWM de hardware)
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &g);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);   // arranca en silencio (bajo)
}

/* -----------------------------------------------------------------------
 * TIM4: INTERRUPCION PERIODICA QUE ALTERNA PA6 POR SOFTWARE
 * ----------------------------------------------------------------------- */
static void MX_TIM4_Buzzer_Init(void) {   // configura TIM4 para generar la interrupcion periodica que alterna PA6 (tono del buzzer)
    __HAL_RCC_TIM4_CLK_ENABLE();   // habilita el reloj de TIM4

    htim4.Instance           = TIM4;
    htim4.Init.Prescaler     = 15;    /* 16MHz/16 = 1MHz -> tick de 1us */   // divide el reloj de 16MHz entre 16 (prescaler+1) para que el timer cuente a 1MHz
    htim4.Init.CounterMode   = TIM_COUNTERMODE_UP;   // cuenta ascendente (0 hasta el periodo, despues desborda)
    htim4.Init.Period        = 999;   /* arranca detenido, se ajusta en runtime */   // valor inicial de ARR, sin importancia real porque Buzzer_SetSalida lo reprograma antes de arrancar el timer con interrupcion
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;   // sin division adicional del reloj interno del timer
    HAL_TIM_Base_Init(&htim4);   // aplica toda la configuracion

    HAL_NVIC_SetPriority(TIM4_IRQn, 3, 0);   // prioridad de interrupcion 3 (relativamente baja, no es tiempo-critico)
    HAL_NVIC_EnableIRQ(TIM4_IRQn);   // habilita la interrupcion de TIM4 en el NVIC (sin esto, TIM4_IRQHandler nunca se ejecutaria)
}

/* -----------------------------------------------------------------------
 * TIM3: DISPARADOR (TRGO) DEL ADC1 CADA 20 MS
 * ----------------------------------------------------------------------- */
static void TIM3_ADCTrigger_Init(void) {   // configura TIM3 en modo TRGO para disparar una conversion del ADC1 cada 20ms
    __HAL_RCC_TIM3_CLK_ENABLE();   // habilita el reloj de TIM3

    htim3.Instance           = TIM3;
    htim3.Init.Prescaler     = 1599;  /* 16mhz/1600 = 10khz -> tick de 100us */   // divide el reloj de 16MHz entre 1600 para que el timer cuente a 10kHz
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;   // cuenta ascendente
    htim3.Init.Period        = 199;   /* 200 ticks x 100us = 20 ms exactos  */   // 200 cuentas de 100us cada una = 20ms exactos entre disparos
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;   // sin division adicional
    HAL_TIM_Base_Init(&htim3);   // aplica la configuracion base del timer

    TIM_MasterConfigTypeDef sMasterConfig = {0};   // configuracion del modo "master" del timer (para generar el TRGO)
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;   // el TRGO se dispara en cada evento de actualizacion (cada vez que el contador completa su periodo)
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;   // no encadenado a otro timer como esclavo
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
}

/* -----------------------------------------------------------------------
 * ADC1: JOYSTICK 1 (CH1=PA1=Y1, CH4=PA4=X1) + JOYSTICK 2 (CH10=PC0=Y2,
 * CH11=PC1=X2) EN MODO SCAN, 4 CANALES
 * ----------------------------------------------------------------------- */
static void ADC1_Joystick_Init(void) {   // configura el ADC1 en modo escaneo de 4 canales (ejes X/Y de los 2 joystick)
    ADC_ChannelConfTypeDef sConfig = {0};   // configuracion de CADA canal individual (se reusa y se pisa 4 veces, una por rank)
    __HAL_RCC_ADC1_CLK_ENABLE();   // habilita el reloj del ADC1

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;   // divide el reloj de APB2 entre 4 para el reloj del ADC
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;   // 12 bits de resolucion (0-4095), la maxima del ADC del F411
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;   // el resultado queda alineado a la derecha del registro (uso directo, sin desplazar)
    hadc1.Init.ScanConvMode          = ENABLE;   // modo escaneo: recorre los 4 canales configurados en orden de rank
    hadc1.Init.ContinuousConvMode    = DISABLE;   // no conversion continua -- cada disparo de TIM3 arranca una conversion nueva
    hadc1.Init.DiscontinuousConvMode = DISABLE;   // no modo discontinuo (los 4 ranks se completan seguidos, no de a uno por disparo)
    hadc1.Init.NbrOfConversion       = 4;   // 4 canales en la secuencia de escaneo (Y1, X1, Y2, X2)
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;   // disparado externamente por el TRGO de TIM3, no por software
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;   // dispara en el flanco de subida del TRGO
    hadc1.Init.DMAContinuousRequests = DISABLE;   // no se usa DMA (la lectura es por interrupcion, ver HAL_ADC_ConvCpltCallback)
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;   // genera la interrupcion de fin de conversion despues de CADA canal (no solo al final de la secuencia completa)
    HAL_ADC_Init(&hadc1);   // aplica toda la configuracion

    HAL_NVIC_SetPriority(ADC_IRQn, 3, 0);   // prioridad de interrupcion 3
    HAL_NVIC_EnableIRQ(ADC_IRQn);   // habilita la interrupcion del ADC en el NVIC

    sConfig.Channel      = ADC_CHANNEL_1;   /* rank 1: joy1 eje y (pa1) */   // primer canal del escaneo: VRy de J1
    sConfig.Rank         = 1;   // orden 1 de 4 en la secuencia
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;   // tiempo de muestreo largo (480 ciclos), mas preciso a costa de ser mas lento -- el joystick no necesita velocidad extrema
    sConfig.Offset       = 0;   // sin offset de calibracion adicional
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);   // aplica la config de este canal

    sConfig.Channel = ADC_CHANNEL_4;        /* rank 2: joy1 eje x (pa4) */   // segundo canal: VRx de J1
    sConfig.Rank    = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_10;       /* rank 3: joy2 eje y (pc0) */   // tercer canal: VRy2 de J2
    sConfig.Rank    = 3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_11;       /* rank 4: joy2 eje x (pc1) */   // cuarto canal: VRx2 de J2
    sConfig.Rank    = 4;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* ========================================================================== */
/* === SPI1 — BUS HACIA EL ILI9341 =========================================== */
/* ========================================================================== */

static void MX_SPI1_Init(void) {   // configura el periferico SPI1 (solo-escritura) usado por la pantalla ILI9341
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;   // el STM32 es el maestro del bus (la pantalla no tiene forma de serlo)
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;   // full-duplex a nivel de configuracion (aunque MISO no esta cableado, la pantalla es solo-escritura)
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;   // transmite de a bytes (8 bits)
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL=0 */   // el reloj esta en bajo en reposo
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA=0 → SPI Mode 0 */   // los datos se capturan en el primer flanco -- Modo 0, el que espera el ILI9341
    hspi1.Init.NSS               = SPI_NSS_SOFT;   // Chip Select manejado por software (LCD_CS_LOW/HIGH), no por hardware del periferico
    /* Bajado de /2 (8MHz) a /8 (2MHz): hipotesis de diagnostico para la
     * pantalla que ocasionalmente se pone blanca durante partidas largas.
     * No hay evidencia de un bug de software que explique un llenado blanco
     * (no se encontro overflow de buffers ni comandos SPI mal armados); el
     * cableado de protoboard, a 8MHz y cerca de los ULN2003A que conmutan
     * los LEDs de los botones, es candidato tipico a ruido/glitches en la
     * linea SPI que se acumulan con el tiempo. Si el problema persiste con
     * este cambio, la causa esta en otro lado y esto puede revertirse. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  /* 8MHz - Velocidad Maxima Fluida */   // divide el reloj de APB2 (16MHz) entre 2 -> 8MHz de reloj SPI; bajarlo mas (prescaler mayor) reduce la velocidad de dibujo pero da mas margen ante ruido electrico (ver el bug de pantalla en blanco ya resuelto por otra via)
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;   // transmite el bit mas significativo primero (el orden que espera el ILI9341)
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;   // sin el modo especial TI, SPI estandar
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;   // sin verificacion CRC (no la usa el protocolo del ILI9341)
    hspi1.Init.CRCPolynomial     = 10;   // sin efecto real (CRC deshabilitado), queda con el valor por defecto
    HAL_SPI_Init(&hspi1);   // aplica toda la configuracion
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {   // inicializacion de bajo nivel del SPI1 (clocks + pines) -- llamada automaticamente por HAL_SPI_Init()
    if (hspi->Instance == SPI1) {   // verifica que sea justo el periferico SPI1 (esta funcion podria compartirse entre varios SPI si el proyecto tuviera mas)
        __HAL_RCC_SPI1_CLK_ENABLE();   // habilita el reloj del periferico SPI1
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_5 | GPIO_PIN_7;  /* PA5=SCK, PA7=MOSI */   // los 2 pines que usa este SPI (no hay MISO, la pantalla es solo-escritura)
        g.Mode      = GPIO_MODE_AF_PP;   // funcion alternativa (el periferico SPI controla el pin, no GPIO normal)
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;   // velocidad alta, necesaria para 8MHz de SPI
        g.Alternate = GPIO_AF5_SPI1;   // numero de funcion alternativa especifico de SPI1 en estos pines (ver datasheet del STM32F411)
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* ========================================================================== */
/* === CONSOLA DE DEPURACION — USART2 POR EL VCP DEL ST-LINK ================= */
/* ========================================================================== */

static void MX_USART2_UART_Init(void) {   // configura USART2 (115200 8N1) -- consola de depuracion por el VCP del ST-Link
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;   // velocidad estandar de consola -- cambiar esto exige cambiar tambien la velocidad configurada en el programa terminal (screen, PuTTY, etc.)
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;   // 8 bits de datos por caracter
    huart2.Init.StopBits     = UART_STOPBITS_1;   // 1 bit de parada (formato "8N1")
    huart2.Init.Parity       = UART_PARITY_NONE;   // sin bit de paridad (la "N" de "8N1")
    huart2.Init.Mode         = UART_MODE_TX_RX;   // habilita transmision Y recepcion (aunque este proyecto solo usa TX para printf)
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;   // sin control de flujo por hardware (RTS/CTS)
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;   // sobremuestreo estandar de 16x, mas preciso para detectar el bit de inicio
    HAL_UART_Init(&huart2);   // aplica toda la configuracion
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {   // inicializacion de bajo nivel del USART2 (clocks + pines PA2/PA3) -- llamada automaticamente por HAL_UART_Init()
    if (huart->Instance == USART2) {   // verifica que sea justo USART2
        __HAL_RCC_USART2_CLK_ENABLE();   // habilita el reloj del periferico USART2
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;  /* PA2=TX, PA3=RX -- VCP ST-Link */   // mismos pines que ya trae cableados el ST-Link integrado de la Nucleo, sin cableado adicional
        g.Mode      = GPIO_MODE_AF_PP;   // funcion alternativa (el periferico USART controla el pin)
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF7_USART2;   // numero de funcion alternativa especifico de USART2 en estos pines
        HAL_GPIO_Init(GPIOA, &g);
    }
}

int __io_putchar(int ch) {   // retarget de printf(): manda cada caracter por USART2 -- esto es lo que hace que printf() salga por el cable del ST-Link
    uint8_t c = (uint8_t)ch;   // printf entrega un int, pero HAL_UART_Transmit necesita un puntero a byte
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);   // transmite ese unico byte, bloqueante (aceptable: es solo 1 byte a la vez, printf ya es lento por naturaleza)
    return ch;   // printf espera que se le devuelva el mismo caracter recibido
}

/* ========================================================================== */
/* === CALLBACKS DEL JOYSTICK ================================================ */
/* ========================================================================== */

/* tick de tim4: alterna pa6 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {   // callback periodico de TIM4: alterna el pin del buzzer (PA6) para generar el tono
    if (htim->Instance == TIM4) {   // verifica que sea justo TIM4 (este callback es compartido por TODOS los timers del proyecto)
        HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);   // invierte el nivel del pin del buzzer -- 2 toggles seguidos = 1 ciclo completo de la onda cuadrada
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {   // callback de fin de conversion del ADC1: filtra (EMA) el canal recien leido y encadena el siguiente de los 4 (Y1,X1,Y2,X2)
    if (hadc->Instance == ADC1) {   // verifica que sea justo ADC1
        uint16_t valor = (uint16_t)HAL_ADC_GetValue(&hadc1);   // lee el resultado de la conversion que se acaba de completar

        switch (adc_rank_actual) {   // segun de que canal (rank) era esta conversion, filtra y guarda en la variable correspondiente
        case 0:   // rank 1 recien completado: eje Y de J1
            filtro_adc_y += ((int32_t)valor - filtro_adc_y) / ADC_FILTRO_N;   // filtro EMA: se acerca al valor nuevo en una fraccion 1/ADC_FILTRO_N
            joystick_y = ((int32_t)filtro_adc_y > (int32_t)centro_j1y - (int32_t)JOY_ZONA_MUERTA &&
                          (int32_t)filtro_adc_y < (int32_t)centro_j1y + (int32_t)JOY_ZONA_MUERTA)
                       ? centro_j1y : (uint16_t)filtro_adc_y;   // si esta dentro de la zona muerta, se fuerza al centro exacto (evita temblor en reposo); si no, se usa el valor filtrado real
            adc_rank_actual = 1;   // el proximo resultado que llegue sera del rank 2
            break;
        case 1:   // rank 2 recien completado: eje X de J1
            filtro_adc_x += ((int32_t)valor - filtro_adc_x) / ADC_FILTRO_N;
            joystick_x = ((int32_t)filtro_adc_x > (int32_t)centro_j1x - (int32_t)JOY_ZONA_MUERTA &&
                          (int32_t)filtro_adc_x < (int32_t)centro_j1x + (int32_t)JOY_ZONA_MUERTA)
                       ? centro_j1x : (uint16_t)filtro_adc_x;
            adc_rank_actual = 2;   // el proximo sera rank 3
            break;
        case 2:   // rank 3 recien completado: eje Y de J2
            filtro_adc_y2 += ((int32_t)valor - filtro_adc_y2) / ADC_FILTRO_N;
            joystick2_y = ((int32_t)filtro_adc_y2 > (int32_t)centro_j2y - (int32_t)JOY_ZONA_MUERTA &&
                           (int32_t)filtro_adc_y2 < (int32_t)centro_j2y + (int32_t)JOY_ZONA_MUERTA)
                        ? centro_j2y : (uint16_t)filtro_adc_y2;
            adc_rank_actual = 3;   // el proximo sera rank 4, el ultimo
            break;
        default:   // rank 4 recien completado: eje X de J2 -- ultimo de la secuencia, hay que reiniciar el ciclo
            filtro_adc_x2 += ((int32_t)valor - filtro_adc_x2) / ADC_FILTRO_N;
            joystick2_x = ((int32_t)filtro_adc_x2 > (int32_t)centro_j2x - (int32_t)JOY_ZONA_MUERTA &&
                           (int32_t)filtro_adc_x2 < (int32_t)centro_j2x + (int32_t)JOY_ZONA_MUERTA)
                        ? centro_j2x : (uint16_t)filtro_adc_x2;
            adc_rank_actual = 0;   // vuelve al rank 1 para el proximo ciclo de 20ms
            HAL_ADC_Start_IT(&hadc1);   // rearma el ADC para la proxima secuencia de 4 conversiones (el modo escaneo se detiene solo al completar los 4 ranks)
            break;
        }
    }
}

/* ========================================================================== */
/* === MANEJO DE ERRORES ===================================================== */
/* ========================================================================== */

void Error_Handler(void) {   // trampa de error generica del HAL (init de perifericos fallido, etc.) -- se queda con las interrupciones apagadas para depurar
    __disable_irq();   // apaga todas las interrupciones -- congela el sistema en un estado conocido para inspeccionar con el debugger
    while (1) {}   // bucle infinito: nunca vuelve, hay que resetear la placa o depurar con SWD para salir de aca
}
