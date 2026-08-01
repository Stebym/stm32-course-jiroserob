/**
 ******************************************************************************
 * @file    board_pins.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mapa de pines de la pantalla ILI9341 en Nucleo-F411RE.
 ******************************************************************************
 */

#ifndef __BOARD_PINS_H
#define __BOARD_PINS_H

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* === PANTALLA ILI9341 — SPI1 ============================================== */
/* ========================================================================== */
/* SPI1: SCK=PA5 (D13), MOSI=PA7 (D11). Sin MISO -- el driver ILI9341 en este */
/* proyecto es de solo escritura (no se lee el estado de la pantalla), por   */
/* eso PA6 (MISO fisico de SPI1) queda libre para reutilizarse como salida   */
/* del buzzer (ver seccion BUZZER mas abajo). Pines CS/DC/RST heredados de   */
/* la configuracion ya validada en examen_parcial, mismo proyecto base del   */
/* que parte practica_pantalla/.                                            */

#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_PIN_6      /* PB6 (D10) — Chip Select          */

#define LCD_DC_PORT     GPIOC
#define LCD_DC_PIN      GPIO_PIN_7      /* PC7 (D9)  — Data/Command         */

#define LCD_RST_PORT    GPIOA
#define LCD_RST_PIN     GPIO_PIN_9      /* PA9 (D8)  — Reset Hardware       */

/* Macros de control GPIO para el display */
#define LCD_CS_LOW()    HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_SET)
#define LCD_DC_LOW()    HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_RESET)
#define LCD_DC_HIGH()   HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_SET)
#define LCD_RST_LOW()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

/* Boton de usuario B1 de la Nucleo (integrado en la placa, PC13, pull-up   */
/* externo R30=4k7 ya presente en el hardware de la Nucleo) -- en un inicio */
/* solo servia para avanzar manualmente el recorrido de pantallas de prueba.*/
/* Desde que se retiraron los pines SW de ambos joystick (2026-07-30, ver   */
/* seccion JOYSTICK mas abajo) es el UNICO boton de "click" que le queda a  */
/* todo el sistema, y paso a ser tambien el que confirma menus y lanza el   */
/* conteo 3-2-1-GO (ver el bloque "if (avanzar)" en main.c).                */
#define BTN_USER_PORT   GPIOC
#define BTN_USER_PIN    GPIO_PIN_13

/* ========================================================================== */
/* === JOYSTICK — misma configuracion probada en examen_parcial ============= */
/* ========================================================================== */
/* PA1=ADC1_IN1 (VRy), PA4=ADC1_IN4 (VRx) — separados a proposito para evitar */
/* continuidad electrica entre ejes.                                         */
/* SW (click) RETIRADO fisicamente (2026-07-30) para ahorrar espacio/cables  */
/* -- solo quedan los 2 ejes analogicos + VCC(3V3) + GND, 4 hilos en vez de  */
/* 5. El reintento tras un game over ahora se dispara moviendo el stick (ver */
/* SJ_GAMEOVER en SimonJoy_Actualizar, main.c) en vez de con un click.       */

/* ========================================================================== */
/* === JOYSTICK 2 — segundo jugador (cableado 2026-07-29) ==================== */
/* ========================================================================== */
/* PC0=ADC1_IN10 (VRy2), PC1=ADC1_IN11 (VRx2) -- elegidos porque la board */
/* auxiliar del usuario no trae PA2/PA3 (los candidatos originales del CN10) */
/* SW2 RETIRADO fisicamente (2026-07-30), igual que el del joystick 1 --    */
/* mismo motivo (ahorrar espacio) y mismo reemplazo (reintento por          */
/* movimiento, ver SJ_GAMEOVER en SimonJoy2_ActualizarJugador, main.c).     */

/* ========================================================================== */
/* === BOTONES ARCADE — 2 JUGADORES x 4 COLORES (cableado 2026-07-29) ======== */
/* ========================================================================== */
/* Cada boton: switch (entrada, pull-up interno, sin resistencia externa) +
 * LED (salida digital hacia un canal de ULN2003A que hace de driver de
 * corriente -- GPIO=HIGH prende el LED). Jugador 1 usa el primer ULN2003A
 * (set original); Jugador 2 usa un 2do ULN2003A completo. Orden de color
 * 0=ROJO 1=VERDE 2=AZUL 3=AMARILLO (igual que NOTE_COLOR/LANE_COLOR y que
 * la cuadricula 2x2 de Renderer_DrawModoSimonClasico). Ver
 * CABLEADO_BOTONES.txt para el detalle fisico completo.
 *
 * REORGANIZADO (2026-07-30): en el header fisico de la board auxiliar, PA8
 * queda al medio de un solo lado. El usuario cableo el jugador 1 en el
 * tramo "PA8 hacia PB12" y el jugador 2 en el tramo "PA8 hacia PB2" -- por
 * eso los 8 pines de cada jugador se reasignaron para quedar TODOS del
 * mismo lado de PA8 (nada de cruzar de un tramo al otro), evitando cables
 * desordenados en la board. Sobra PB9 (jugador1) y PB2 (jugador2) libres
 * para uso futuro (ej. boton de confirmar por jugador).
 *
 * AJUSTE (2026-07-30): el orden de los 4 LED (pines IN1..IN4 del ULN2003A)
 * se re-emparejo con el color para que los cables de control queden rectos
 * en la board fisica -- ver CABLEADO_BOTONES.txt para el detalle. */

#define BTN1_ROJO_SW_PORT       GPIOB
#define BTN1_ROJO_SW_PIN        GPIO_PIN_12
#define BTN1_VERDE_SW_PORT      GPIOA
#define BTN1_VERDE_SW_PIN       GPIO_PIN_12
#define BTN1_AZUL_SW_PORT       GPIOC
#define BTN1_AZUL_SW_PIN        GPIO_PIN_6
#define BTN1_AMARILLO_SW_PORT   GPIOC
#define BTN1_AMARILLO_SW_PIN    GPIO_PIN_9

#define BTN1_ROJO_LED_PORT      GPIOB
#define BTN1_ROJO_LED_PIN       GPIO_PIN_8
#define BTN1_VERDE_LED_PORT     GPIOC
#define BTN1_VERDE_LED_PIN      GPIO_PIN_8
/* AZUL/AMARILLO cruzados en el ULN2003A #1 (2026-07-31, confirmado por el
 * usuario: pines 13/14 del ULN cambiados, dificil de resoldar) -- se
 * compensa aca intercambiando a que pin del micro apunta cada nombre, en
 * vez de tocar el cableado fisico. Los botones (SW) NO estan cruzados,
 * solo el driver de LED -- por eso este intercambio va SOLO en los _LED_,
 * no en los _SW_ de arriba. */
#define BTN1_AZUL_LED_PORT      GPIOA
#define BTN1_AZUL_LED_PIN       GPIO_PIN_11
#define BTN1_AMARILLO_LED_PORT  GPIOC
#define BTN1_AMARILLO_LED_PIN   GPIO_PIN_5

#define BTN2_ROJO_SW_PORT       GPIOC
#define BTN2_ROJO_SW_PIN        GPIO_PIN_10
#define BTN2_VERDE_SW_PORT      GPIOA
#define BTN2_VERDE_SW_PIN       GPIO_PIN_10
#define BTN2_AZUL_SW_PORT       GPIOB
#define BTN2_AZUL_SW_PIN        GPIO_PIN_13
#define BTN2_AMARILLO_SW_PORT   GPIOB
#define BTN2_AMARILLO_SW_PIN    GPIO_PIN_15

#define BTN2_ROJO_LED_PORT      GPIOB
#define BTN2_ROJO_LED_PIN       GPIO_PIN_1
#define BTN2_VERDE_LED_PORT     GPIOB
#define BTN2_VERDE_LED_PIN      GPIO_PIN_14
#define BTN2_AZUL_LED_PORT      GPIOC
#define BTN2_AZUL_LED_PIN       GPIO_PIN_4
/* Sigue en diagnostico (2026-07-31): canal original IN4/OUT4(pin13) del
 * ULN2003A #2 confirmado malo (LED bueno con multimetro, anodo+resistencia
 * confirmados bien). Se probo IN5/OUT5(pin12) con el control en PB2, sin
 * exito -- ahora se prueba esa MISMA reubicacion de canal pero con el
 * control de vuelta en PB5 (combinacion todavia no probada). Si tampoco
 * prende, el canal IN5/OUT5 o su cableado nuevo puede estar mal, no el pin
 * del micro -- revisar continuidad ahi antes de seguir cambiando pines del
 * STM32. */
#define BTN2_AMARILLO_LED_PORT  GPIOB
#define BTN2_AMARILLO_LED_PIN   GPIO_PIN_5

/* ========================================================================== */
/* === BUZZER — TIPO SIN CONFIRMAR (activo u pasivo) ========================= */
/* ========================================================================== */
/* PA6: libre porque el SPI1 de la pantalla no usa MISO (driver de solo      */
/* escritura). El STM32F411 NO tiene TIM13/TIM14 (solo existen en la familia */
/* F413/423) y el UNICO timer con canal disponible en PA6 es TIM3_CH1, que ya */
/* esta ocupado como disparador del ADC del joystick cada 20ms -- por eso el */
/* tono no se genera con PWM de hardware, sino alternando este pin por      */
/* software desde la interrupcion de TIM4 (libre), ver MX_TIM4_Buzzer_Init  */
/* en main.c. PA6 sigue siendo GPIO de salida normal, igual que antes.       */

#define BUZZER_PORT     GPIOA
#define BUZZER_PIN      GPIO_PIN_6

#endif /* __BOARD_PINS_H */
