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

/* Boton de usuario B1 de la Nucleo — util para avanzar de prueba a mano */
#define BTN_USER_PORT   GPIOC
#define BTN_USER_PIN    GPIO_PIN_13

#endif /* __BOARD_PINS_H */
