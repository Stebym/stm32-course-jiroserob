/**
 ******************************************************************************
 * @file    board_pins.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mapa de pines confirmados para Beat Clash en Nucleo-F411RE.
 ******************************************************************************
 */

#ifndef __BOARD_PINS_H
#define __BOARD_PINS_H

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* === PANTALLA ILI9341 — SPI1 ============================================== */
/* ========================================================================== */
/* SPI1: SCK=PA5 (D13), MOSI=PA7 (D11). Sin MISO.                            */

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

/* ========================================================================== */
/* === JOYSTICKS KY-023 — ADC1 DMA ========================================== */
/* ========================================================================== */
#define J1_JOY_X_PIN    GPIO_PIN_0      /* PA0 — ADC1 IN0  */
#define J1_JOY_Y_PIN    GPIO_PIN_1      /* PA1 — ADC1 IN1  */
#define J1_JOY_PORT     GPIOA

#define J2_JOY_X_PIN    GPIO_PIN_4      /* PA4 — ADC1 IN4  */
#define J2_JOY_X_PORT   GPIOA
#define J2_JOY_Y_PIN    GPIO_PIN_0      /* PB0 — ADC1 IN8  */
#define J2_JOY_Y_PORT   GPIOB

#define ADC_IDX_J1_X    0
#define ADC_IDX_J1_Y    1
#define ADC_IDX_J2_X    2
#define ADC_IDX_J2_Y    3

/* ========================================================================== */
/* === BOTONES ARCADE JUGADOR 1 ============================================= */
/* ========================================================================== */
#define J1_BTN_R_PORT   GPIOC
#define J1_BTN_R_PIN    GPIO_PIN_0      /* PC0 */
#define J1_BTN_G_PIN    GPIO_PIN_1      /* PC1 */
#define J1_BTN_B_PIN    GPIO_PIN_2      /* PC2 */
#define J1_BTN_Y_PIN    GPIO_PIN_3      /* PC3 */
#define J1_BTN_MASK     (GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3)

/* ========================================================================== */
/* === BOTONES ARCADE JUGADOR 2 ============================================= */
/* ========================================================================== */
#define J2_BTN_R_PORT   GPIOC
#define J2_BTN_R_PIN    GPIO_PIN_4      /* PC4 */
#define J2_BTN_G_PIN    GPIO_PIN_5      /* PC5 */

#define J2_BTN_B_PORT   GPIOB
#define J2_BTN_B_PIN    GPIO_PIN_1      /* PB1 */
#define J2_BTN_Y_PIN    GPIO_PIN_3      /* PB3 */

/* ========================================================================== */
/* === LEDs JUGADOR 1 & 2 =================================================== */
/* ========================================================================== */
#define J1_LED_PORT     GPIOB
#define J1_LED_R_PIN    GPIO_PIN_12
#define J1_LED_G_PIN    GPIO_PIN_13
#define J1_LED_B_PIN    GPIO_PIN_14
#define J1_LED_Y_PIN    GPIO_PIN_15
#define J1_LED_ALL_PINS (GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15)

#define J2_LED_PORT     GPIOC
#define J2_LED_R_PIN    GPIO_PIN_8
#define J2_LED_G_PIN    GPIO_PIN_9
#define J2_LED_B_PIN    GPIO_PIN_10
#define J2_LED_Y_PIN    GPIO_PIN_11
#define J2_LED_ALL_PINS (GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11)

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
/* === BUZZERS & START ====================================================== */
/* ========================================================================== */
#define J1_BUZ_PORT     GPIOA
#define J1_BUZ_PIN      GPIO_PIN_15

#define J2_BUZ_PORT     GPIOB
#define J2_BUZ_PIN      GPIO_PIN_4

#define BTN_START_PORT  GPIOC
#define BTN_START_PIN   GPIO_PIN_13

#endif /* __BOARD_PINS_H */
