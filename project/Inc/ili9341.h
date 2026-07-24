/**
 ******************************************************************************
 * @file    ili9341.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Cabecera para el driver ILI9341 240x320 via SPI1 en modo paisaje.
 ******************************************************************************
 */

#ifndef __ILI9341_H
#define __ILI9341_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ========================================================================== */
/* === CONFIGURACION DEL DISPLAY ============================================ */
/* ========================================================================== */

/* Orientacion paisaje (320 ancho x 240 alto) */
#define ILI9341_MADCTL_LANDSCAPE    0x28

/* Dimensiones en modo paisaje */
#define ILI9341_W   320
#define ILI9341_H   240

/* ========================================================================== */
/* === API PUBLICA (Solo prototipos) ======================================= */
/* ========================================================================== */

void ILI9341_Init(void);
void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ILI9341_EndWrite(void);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes);

#endif /* __ILI9341_H */
