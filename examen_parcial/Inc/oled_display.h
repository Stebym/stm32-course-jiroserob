/**
 ******************************************************************************
 * @file    oled_display.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver OLED GME12864-41 (controlador SSD1315, compatible con el
 *          set de comandos SSD1306) sobre I2C1 (PB8=SCL, PB9=SDA), polling,
 *          sin interrupciones (segun lo indicado en la rubrica del examen).
 *          Basado en el codigo de referencia enviado por el profesor Nerio
 *          Montoya por correo (oled_display.c, GME12864-41.pdf confirma
 *          SSD1315 = mismo command table que SSD1306).
 ******************************************************************************
 */

#ifndef __OLED_DISPLAY_H
#define __OLED_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void OLED_I2C1_Init(void);
void OLED_ScanAddress(UART_HandleTypeDef *huart);
void SSD1306_Init(void);
void SSD1306_Fill(uint8_t color);
void SSD1306_UpdateScreen(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_WriteString(uint8_t x, uint8_t y, char *str);
void SSD1306_DrawEmptyRect(uint8_t x_zero, uint8_t y_zero, uint8_t x_wide, uint8_t y_height);
void SSD1306_DrawCross(uint8_t x, uint8_t y);
long Map(long x, long in_min, long in_max, long out_min, long out_max);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_DISPLAY_H */
