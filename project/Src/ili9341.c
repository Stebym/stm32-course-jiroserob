/**
 ******************************************************************************
 * @file    ili9341.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver ILI9341 — inicializacion y funciones de dibujo basico.
 ******************************************************************************
 */

#include "ili9341.h"
#include "board_pins.h"

extern SPI_HandleTypeDef hspi1;
static uint8_t lcd_row_buf[ILI9341_W * 2];

/* ========================================================================== */
/* === FUNCIONES INTERNAS =================================================== */
/* ========================================================================== */

static void LCD_WriteCmd(uint8_t cmd) {
    LCD_CS_LOW();
    LCD_DC_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    LCD_CS_HIGH();
}

static void LCD_CmdData(uint8_t cmd, const uint8_t *data, uint8_t n) {
    LCD_CS_LOW();
    LCD_DC_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);

    LCD_DC_HIGH();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, n, 50);
    LCD_CS_HIGH();
}

/* ========================================================================== */
/* === INICIALIZACION ======================================================= */
/* ========================================================================== */

void ILI9341_Init(void) {
    /* Reset estricto por Hardware */
    LCD_RST_HIGH();
    HAL_Delay(5);
    LCD_RST_LOW();
    HAL_Delay(20);
    LCD_RST_HIGH();
    HAL_Delay(150);

    /* Software Reset */
    LCD_WriteCmd(0x01);
    HAL_Delay(150);

    /* Display OFF durante la configuracion */
    LCD_WriteCmd(0x28);

    /* Power Control A */
    { uint8_t d[] = {0x39,0x2C,0x00,0x34,0x02}; LCD_CmdData(0xCB, d, 5); }

    /* Power Control B */
    { uint8_t d[] = {0x00,0xC1,0x30}; LCD_CmdData(0xCF, d, 3); }

    /* Driver Timing Control A */
    { uint8_t d[] = {0x85,0x00,0x78}; LCD_CmdData(0xE8, d, 3); }

    /* Driver Timing Control B */
    { uint8_t d[] = {0x00,0x00}; LCD_CmdData(0xEA, d, 2); }

    /* Power on Sequence Control */
    { uint8_t d[] = {0x64,0x03,0x12,0x81}; LCD_CmdData(0xED, d, 4); }

    /* Pump Ratio Control */
    { uint8_t d[] = {0x20}; LCD_CmdData(0xF7, d, 1); }

    /* Power Control 1 — VRH */
    { uint8_t d[] = {0x23}; LCD_CmdData(0xC0, d, 1); }

    /* Power Control 2 */
    { uint8_t d[] = {0x10}; LCD_CmdData(0xC1, d, 1); }

    /* VCOM Control 1 */
    { uint8_t d[] = {0x3E,0x28}; LCD_CmdData(0xC5, d, 2); }

    /* VCOM Control 2 */
    { uint8_t d[] = {0x86}; LCD_CmdData(0xC7, d, 1); }

    /* Memory Access Control (MADCTL) — Modo Paisaje 320x240 BGR */
    { uint8_t d[] = {ILI9341_MADCTL_LANDSCAPE}; LCD_CmdData(0x36, d, 1); }

    /* Pixel Format — 16 bits RGB565 */
    { uint8_t d[] = {0x55}; LCD_CmdData(0x3A, d, 1); }

    /* Frame Rate Control ~70Hz */
    { uint8_t d[] = {0x00,0x18}; LCD_CmdData(0xB1, d, 2); }

    /* Display Function Control */
    { uint8_t d[] = {0x08,0x82,0x27}; LCD_CmdData(0xB6, d, 3); }

    /* Gamma Enable */
    { uint8_t d[] = {0x00}; LCD_CmdData(0xF2, d, 1); }
    { uint8_t d[] = {0x01}; LCD_CmdData(0x26, d, 1); }

    /* Positive Gamma */
    { uint8_t d[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                     0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
      LCD_CmdData(0xE0, d, 15); }

    /* Negative Gamma */
    { uint8_t d[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                     0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
      LCD_CmdData(0xE1, d, 15); }

    /* Sleep Out */
    LCD_WriteCmd(0x11);
    HAL_Delay(120);

    /* Display ON */
    LCD_WriteCmd(0x29);
    HAL_Delay(20);

    /* Limpiar pantalla a negro */
    ILI9341_FillScreen(0x0000);
}

/* ========================================================================== */
/* === VENTANA DE DIRECCION ================================================= */
/* ========================================================================== */

void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t d[4];

    /* CASET (0x2A) — Columnas (Eje X) */
    d[0] = x0 >> 8; d[1] = x0 & 0xFF;
    d[2] = x1 >> 8; d[3] = x1 & 0xFF;
    LCD_CmdData(0x2A, d, 4);

    /* PASET (0x2B) — Filas (Eje Y) */
    d[0] = y0 >> 8; d[1] = y0 & 0xFF;
    d[2] = y1 >> 8; d[3] = y1 & 0xFF;
    LCD_CmdData(0x2B, d, 4);

    /* RAMWR (0x2C) — Preparar bus para recibir píxeles */
    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t c = 0x2C;
    HAL_SPI_Transmit(&hspi1, &c, 1, 10);
    LCD_DC_HIGH();
}

void ILI9341_EndWrite(void) {
    LCD_CS_HIGH();
}

/* ========================================================================== */
/* === DIBUJO Y ESCRITURA =================================================== */
/* ========================================================================== */

void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes) {
    LCD_DC_HIGH();
    /* HAL_SPI_Transmit tiene Size uint16_t (max 65535) — enviar en trozos */
    while (len_bytes > 0) {
        uint16_t chunk = (len_bytes > 65534u) ? 65534u : (uint16_t)len_bytes;
        HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, chunk, 2000);
        buf       += chunk;
        len_bytes -= chunk;
    }
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!w || !h) return;
    if (x >= ILI9341_W || y >= ILI9341_H) return;

    if ((uint32_t)x + w > ILI9341_W) w = ILI9341_W - x;
    if ((uint32_t)y + h > ILI9341_H) h = ILI9341_H - y;

    ILI9341_SetWindow(x, y, x + w - 1, y + h - 1);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint16_t fill = (w <= ILI9341_W) ? w : ILI9341_W;
    for (uint16_t i = 0; i < fill; i++) {
        lcd_row_buf[i * 2]     = hi;
        lcd_row_buf[i * 2 + 1] = lo;
    }

    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(&hspi1, lcd_row_buf, fill * 2, 500);
    }

    ILI9341_EndWrite();
}

void ILI9341_FillScreen(uint16_t color) {
    ILI9341_FillRect(0, 0, ILI9341_W, ILI9341_H, color);
}
