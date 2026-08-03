/**
 ******************************************************************************
 * @file    ili9341.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver ILI9341 240x320 via SPI1 en modo paisaje (320x240), mas
 *          primitivas de dibujo basicas para practicas de pantalla.
 ******************************************************************************
 */

#ifndef __ILI9341_H
#define __ILI9341_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ========================================================================== */
/* === CONFIGURACION DEL DISPLAY ============================================ */
/* ========================================================================== */

/* Orientacion paisaje (320 ancho x 240 alto) — la que usa Init() por defecto */
#define ILI9341_MADCTL_LANDSCAPE    0x28
/* Orientacion retrato (240 ancho x 320 alto) — para el modo cocktail        */
#define ILI9341_MADCTL_PORTRAIT     0x08

/* Tamaño maximo de fila usado para el buffer interno (el mayor de ambas
 * orientaciones); ILI9341_W/H cambian en tiempo real segun orientacion. */
#define ILI9341_MAXDIM  320

extern uint16_t ILI9341_W;
extern uint16_t ILI9341_H;

/* Cambia la orientacion fisica del panel (MADCTL) y actualiza ILI9341_W/H. */
void ILI9341_SetPortrait(uint8_t portrait);

/* Rotacion adicional de 180 grados sobre la orientacion actual (landscape o
 * portrait, la que este activa via ILI9341_SetPortrait) -- para el modo de
 * 1 solo jugador, que debe verse "desde el lado del jugador 2" del cabinet.
 * Es una rotacion real de hardware (bits MY/MX de MADCTL), no cambia
 * ILI9341_W/H ni ninguna coordenada de dibujo -- todo el codigo de
 * renderizado existente sigue funcionando igual, solo cambia fisicamente
 * hacia donde apunta cada pixel en el panel. Llamar SIEMPRE despues de
 * ILI9341_SetPortrait() en la misma transicion de pantalla (SetPortrait
 * reprograma MADCTL sin flip). */
void ILI9341_SetFlip180(uint8_t flip);

/* ========================================================================== */
/* === COLORES RGB565 ======================================================= */
/* ========================================================================== */

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* Paleta por defecto — se salta si game_state.h (u otro header) ya definio
 * estos mismos nombres, para poder incluir ambos sin choques de macro.    */
#ifndef COLOR_BLACK
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_GRAY      0x8410
#define COLOR_DARKGRAY  0x2104
#endif

/* ========================================================================== */
/* === API — INICIALIZACION Y BAJO NIVEL ==================================== */
/* ========================================================================== */

void ILI9341_Init(void);

/* Devuelve 1 (una sola vez, se rearma solo) cuando se detectaron varias
 * fallas de transmision SPI seguidas -- señal de ruido electrico en el bus
 * que puede haber dejado al controlador desincronizado (ver el comentario
 * de LCD_SPI_Send en ili9341.c). El llamador (main.c) debe reaccionar
 * reinicializando la pantalla por completo. */
uint8_t ILI9341_FalloComunicacionDetectado(void);

void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ILI9341_EndWrite(void);
void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes);

/* ========================================================================== */
/* === API — FIGURAS BASICAS ================================================ */
/* ========================================================================== */

void ILI9341_FillScreen(uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void ILI9341_DrawCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color);
void ILI9341_FillCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color);
/* Circulo de 2 colores concentricos (anillo/cuerpo + nucleo) en un solo
 * pase de SPI -- usar en vez de 2 llamadas a ILI9341_FillCircle siempre que
 * el circulo interior comparta centro con el exterior (domos, notas, zonas
 * de golpe). r_in<=0 equivale a un ILI9341_FillCircle normal. */
void ILI9341_FillCircle2(int16_t xc, int16_t yc, int16_t r_out, uint16_t color_out,
                          int16_t r_in, uint16_t color_in);

/* ========================================================================== */
/* === API — IMAGENES ======================================================= */
/* ========================================================================== */

/* data: buffer RGB565 big-endian (2 bytes/pixel), w*h pixeles, fila por fila. */
void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data);

/* ========================================================================== */
/* === API — TEXTO (fuente 5x7, ASCII 32-90) ================================ */
/* ========================================================================== */

void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void ILI9341_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

#endif /* __ILI9341_H */
