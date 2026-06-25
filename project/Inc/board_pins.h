/**
 ******************************************************************************
 * @file    board_pins.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mapa de pines confirmados para Beat Clash en Nucleo-F411RE.
 *          Aqui puedo ajustar la asignacion de pines sin tocar la logica.
 *
 * SEGURIDAD ELECTRICA:
 *  - PA0 es pin TC (Standard 3.3V), los joysticks van a 3.3V OBLIGATORIO
 *  - PA13/PA14 son SWDIO/SWCLK — nunca reasignar
 *  - LEDs: solo a traves de ULN2003A, nunca directo desde GPIO
 *  - Buzzer: se recomienda transistor driver (BCxxx o canal ULN2003A)
 *    si el buzzer es magnetico (bobina < 200 ohms); los piezo van directo
 ******************************************************************************
 */

#ifndef __BOARD_PINS_H
#define __BOARD_PINS_H

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* === PANTALLA ILI9341 — SPI1 ============================================== */
/* ========================================================================== */
/* SPI1: SCK=PA5 (AF5), MOSI=PA7 (AF5). Sin MISO (display write-only).      */

#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_PIN_6      /* PB6 — chip select, activo BAJO    */

#define LCD_DC_PORT     GPIOC
#define LCD_DC_PIN      GPIO_PIN_7      /* PC7 — data/cmd: HIGH=dato, LOW=cmd */

#define LCD_RST_PORT    GPIOA
#define LCD_RST_PIN     GPIO_PIN_9      /* PA9 — reset, activo BAJO          */

/* Macros de conveniencia para el driver ili9341 */
#define LCD_CS_LOW()    HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_SET)
#define LCD_DC_LOW()    HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_RESET)
#define LCD_DC_HIGH()   HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_SET)
#define LCD_RST_LOW()   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

/* ========================================================================== */
/* === JOYSTICKS KY-023 — ADC1 DMA ========================================== */
/* ========================================================================== */
/* IMPORTANTE: Todos los joysticks deben alimentarse a 3.3V, nunca 5V.       */
/* PA0 es pin TC (Standard Voltage), maximo 3.3V absoluto en señal.          */

#define J1_JOY_X_PIN    GPIO_PIN_0      /* PA0 — ADC1 IN0  (VRX jugador 1)  */
#define J1_JOY_Y_PIN    GPIO_PIN_1      /* PA1 — ADC1 IN1  (VRY jugador 1)  */
#define J1_JOY_PORT     GPIOA

#define J2_JOY_X_PIN    GPIO_PIN_4      /* PA4 — ADC1 IN4  (VRX jugador 2)  */
#define J2_JOY_X_PORT   GPIOA
#define J2_JOY_Y_PIN    GPIO_PIN_0      /* PB0 — ADC1 IN8  (VRY jugador 2)  */
#define J2_JOY_Y_PORT   GPIOB

/* Indices en el buffer adc_raw[4] (orden de conversion) */
#define ADC_IDX_J1_X    0
#define ADC_IDX_J1_Y    1
#define ADC_IDX_J2_X    2
#define ADC_IDX_J2_Y    3

/* ========================================================================== */
/* === BOTONES ARCADE JUGADOR 1 — GPIO PULLUP, activo BAJO ================== */
/* ========================================================================== */

#define J1_BTN_R_PORT   GPIOC
#define J1_BTN_R_PIN    GPIO_PIN_0      /* PC0 — boton Rojo J1   */
#define J1_BTN_G_PIN    GPIO_PIN_1      /* PC1 — boton Verde J1  */
#define J1_BTN_B_PIN    GPIO_PIN_2      /* PC2 — boton Azul J1   */
#define J1_BTN_Y_PIN    GPIO_PIN_3      /* PC3 — boton Amarillo J1 */

/* Mascara para leer los 4 botones J1 en un solo acceso al puerto */
#define J1_BTN_MASK     (GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3)

/* ========================================================================== */
/* === BOTONES ARCADE JUGADOR 2 — GPIO PULLUP, activo BAJO ================== */
/* ========================================================================== */
/* J2_BTN_R/G: Puerto C. J2_BTN_B: PB1. J2_BTN_Y: PB3 (libre en modo SWD). */

#define J2_BTN_R_PORT   GPIOC
#define J2_BTN_R_PIN    GPIO_PIN_4      /* PC4 — boton Rojo J2   */
#define J2_BTN_G_PIN    GPIO_PIN_5      /* PC5 — boton Verde J2  */

#define J2_BTN_B_PORT   GPIOB
#define J2_BTN_B_PIN    GPIO_PIN_1      /* PB1 — boton Azul J2   */
#define J2_BTN_Y_PIN    GPIO_PIN_3      /* PB3 — boton Amarillo J2 (JTDO libre en SWD) */

/* ========================================================================== */
/* === LEDs JUGADOR 1 — via ULN2003A, activo ALTO =========================== */
/* ========================================================================== */
/* HIGH en GPIO → ULN2003A conduce → corriente de LED desde supply externa.  */

#define J1_LED_PORT     GPIOB
#define J1_LED_R_PIN    GPIO_PIN_12     /* PB12 — LED Rojo J1    */
#define J1_LED_G_PIN    GPIO_PIN_13     /* PB13 — LED Verde J1   */
#define J1_LED_B_PIN    GPIO_PIN_14     /* PB14 — LED Azul J1    */
#define J1_LED_Y_PIN    GPIO_PIN_15     /* PB15 — LED Amarillo J1 */
#define J1_LED_ALL_PINS (GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15)

/* ========================================================================== */
/* === LEDs JUGADOR 2 — via ULN2003A, activo ALTO =========================== */
/* ========================================================================== */

#define J2_LED_PORT     GPIOC
#define J2_LED_R_PIN    GPIO_PIN_8      /* PC8  — LED Rojo J2    */
#define J2_LED_G_PIN    GPIO_PIN_9      /* PC9  — LED Verde J2   */
#define J2_LED_B_PIN    GPIO_PIN_10     /* PC10 — LED Azul J2    */
#define J2_LED_Y_PIN    GPIO_PIN_11     /* PC11 — LED Amarillo J2 */
#define J2_LED_ALL_PINS (GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11)

/* Lookup de pines LED por jugador y carril — aqui puedo reasignar si cambio cableado */
static inline void LED_Set(uint8_t jugador, uint8_t carril, uint8_t on) {
    static const uint16_t pins_j1[4] = {J1_LED_R_PIN, J1_LED_G_PIN, J1_LED_B_PIN, J1_LED_Y_PIN};
    static const uint16_t pins_j2[4] = {J2_LED_R_PIN, J2_LED_G_PIN, J2_LED_B_PIN, J2_LED_Y_PIN};
    if (carril > 3) return;
    if (jugador == 0)
        HAL_GPIO_WritePin(J1_LED_PORT, pins_j1[carril], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(J2_LED_PORT, pins_j2[carril], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ========================================================================== */
/* === BUZZERS PASIVOS — PWM via TIMx ======================================= */
/* ========================================================================== */
/* PA15 = TIM2_CH1 AF1 (libre en modo SWD). PB4 = TIM3_CH1 AF2 (libre SWD). */
/* Si el buzzer es magnetico (bobina < 200 ohms), usar transistor driver.    */

#define J1_BUZ_PORT     GPIOA
#define J1_BUZ_PIN      GPIO_PIN_15     /* PA15 — TIM2_CH1 AF1   */

#define J2_BUZ_PORT     GPIOB
#define J2_BUZ_PIN      GPIO_PIN_4      /* PB4  — TIM3_CH1 AF2   */

/* ========================================================================== */
/* === BOTON INICIO (B1 Nucleo) ============================================== */
/* ========================================================================== */
/* PC13 tiene pull-up externo R30=4.7k en la Nucleo. No configurar PULLUP sw.*/
/* Activo BAJO (presionado = 0).                                              */

#define BTN_START_PORT  GPIOC
#define BTN_START_PIN   GPIO_PIN_13

#endif /* __BOARD_PINS_H */
