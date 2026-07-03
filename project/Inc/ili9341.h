/**
 ******************************************************************************
 * @file    ili9341.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver para la pantalla ILI9341 240x320 via SPI1 en modo paisaje.
 *          Aqui puedo ajustar el MADCTL si la pantalla aparece invertida.
 ******************************************************************************
 */

#ifndef __ILI9341_H
#define __ILI9341_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ========================================================================== */
/* === CONFIGURACION DEL DISPLAY ============================================ */
/* ========================================================================== */

/* Orientacion paisaje (320 ancho x 240 alto):                               */
/* MADCTL = 0x28: MV=1, BGR=1. Si la imagen sale invertida, probar 0x48.    */
#define ILI9341_MADCTL_LANDSCAPE    0x28

/* Dimensiones en modo paisaje */
#define ILI9341_W   320
#define ILI9341_H   240

/* ========================================================================== */
/* === API PUBLICA ========================================================== */
/* ========================================================================== */

/* Inicializacion — llamar una vez al arranque despues de MX_SPI1_Init()     */
void ILI9341_Init(void);

/* Definir ventana de escritura y dejar la pantalla lista para datos         */
/* Nota: deja CS=LOW y DC=HIGH. El caller llama ILI9341_EndWrite() al final.*/
void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/* Cerrar la transmision (CS HIGH) */
void ILI9341_EndWrite(void);

/* Rellenar un rectangulo con un color solido (RGB565) */
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* Borrar toda la pantalla con un color */
void ILI9341_FillScreen(uint16_t color);

/* Enviar un bloque de pixels (buffer ya en formato RGB565 big-endian)       */
void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes);

#endif /* __ILI9341_H */
