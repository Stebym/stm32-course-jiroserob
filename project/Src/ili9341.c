/**
 ******************************************************************************
 * @file    ili9341.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver ILI9341 — inicializacion y funciones de dibujo basico.
 *          Modo paisaje 320x240, RGB565, SPI1 polling a 8MHz.
 *
 * Aqui puedo cambiar MADCTL (0x28) si la pantalla aparece rotada/invertida.
 * Secuencia de init basada en la hoja de datos ILI9341 secciones 7.1.8 y 8.
 ******************************************************************************
 */

#include "ili9341.h"
#include "board_pins.h"

/* Handle SPI1 definido en main.c */
extern SPI_HandleTypeDef hspi1;

/* Buffer de fila para envio eficiente de rectangulos grandes */
static uint8_t lcd_row_buf[ILI9341_W * 2];  /* 640 bytes en BSS             */

/* ========================================================================== */
/* === FUNCIONES INTERNAS =================================================== */
/* ========================================================================== */

static void LCD_WriteCmd(uint8_t cmd) {
    LCD_CS_LOW();
    LCD_DC_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    /* CS queda LOW si va seguido de datos — el llamador cierra con CS_HIGH */
}

static void LCD_WriteCmdClose(uint8_t cmd) {
    LCD_CS_LOW();
    LCD_DC_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    LCD_CS_HIGH();
}

static void LCD_CmdData(uint8_t cmd, const uint8_t *data, uint8_t n) {
    LCD_WriteCmd(cmd);
    LCD_DC_HIGH();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, n, 100);
    LCD_CS_HIGH();
}

/* ========================================================================== */
/* === INICIALIZACION ======================================================= */
/* ========================================================================== */

void ILI9341_Init(void) {
    /* Reset por hardware */
    LCD_RST_LOW();
    HAL_Delay(10);
    LCD_RST_HIGH();
    HAL_Delay(120);

    /* Software Reset */
    LCD_WriteCmdClose(0x01);
    HAL_Delay(120);

    /* Power Control A */
    { uint8_t d[] = {0x39,0x2C,0x00,0x34,0x02};
      LCD_CmdData(0xCB, d, 5); }

    /* Power Control B */
    { uint8_t d[] = {0x00,0xC1,0x30};
      LCD_CmdData(0xCF, d, 3); }

    /* Driver Timing Control A */
    { uint8_t d[] = {0x85,0x00,0x78};
      LCD_CmdData(0xE8, d, 3); }

    /* Driver Timing Control B */
    { uint8_t d[] = {0x00,0x00};
      LCD_CmdData(0xEA, d, 2); }

    /* Power on Sequence Control */
    { uint8_t d[] = {0x64,0x03,0x12,0x81};
      LCD_CmdData(0xED, d, 4); }

    /* Pump Ratio Control */
    { uint8_t d[] = {0x20};
      LCD_CmdData(0xF7, d, 1); }

    /* Power Control 1 — VRH */
    { uint8_t d[] = {0x23};
      LCD_CmdData(0xC0, d, 1); }

    /* Power Control 2 */
    { uint8_t d[] = {0x10};
      LCD_CmdData(0xC1, d, 1); }

    /* VCOM Control 1 */
    { uint8_t d[] = {0x3E,0x28};
      LCD_CmdData(0xC5, d, 2); }

    /* VCOM Control 2 */
    { uint8_t d[] = {0x86};
      LCD_CmdData(0xC7, d, 1); }

    /* Memory Access Control (MADCTL) — modo paisaje 320x240, BGR            */
    /* Aqui puedo cambiar a 0x48 si la pantalla sale invertida horizontalmente */
    { uint8_t d[] = {ILI9341_MADCTL_LANDSCAPE};
      LCD_CmdData(0x36, d, 1); }

    /* Pixel Format — 16 bits RGB565 */
    { uint8_t d[] = {0x55};
      LCD_CmdData(0x3A, d, 1); }

    /* Frame Rate Control ~70Hz */
    { uint8_t d[] = {0x00,0x18};
      LCD_CmdData(0xB1, d, 2); }

    /* Display Function Control */
    { uint8_t d[] = {0x08,0x82,0x27};
      LCD_CmdData(0xB6, d, 3); }

    /* Gamma Enable */
    { uint8_t d[] = {0x00};
      LCD_CmdData(0xF2, d, 1); }
    { uint8_t d[] = {0x01};
      LCD_CmdData(0x26, d, 1); }

    /* Positive Gamma */
    { uint8_t d[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                     0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
      LCD_CmdData(0xE0, d, 15); }

    /* Negative Gamma */
    { uint8_t d[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                     0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
      LCD_CmdData(0xE1, d, 15); }

    /* Sleep Out */
    LCD_WriteCmdClose(0x11);
    HAL_Delay(120);

    /* Display ON */
    LCD_WriteCmdClose(0x29);
    HAL_Delay(20);

    /* Limpiar pantalla a negro */
    ILI9341_FillScreen(0x0000);
}

/* ========================================================================== */
/* === VENTANA DE DIRECCION ================================================= */
/* ========================================================================== */

/* Abre la ventana de escritura y deja el bus listo para enviar pixels       */
/* (CS=LOW, DC=HIGH). El caller debe llamar ILI9341_EndWrite() al terminar.  */
void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t d[4];

    LCD_CS_LOW();

    /* CASET — rango de columnas */
    LCD_DC_LOW();
    uint8_t c = 0x2A;
    HAL_SPI_Transmit(&hspi1, &c, 1, 10);
    LCD_DC_HIGH();
    d[0] = x0 >> 8; d[1] = x0 & 0xFF;
    d[2] = x1 >> 8; d[3] = x1 & 0xFF;
    HAL_SPI_Transmit(&hspi1, d, 4, 10);

    /* PASET — rango de paginas (filas) */
    LCD_DC_LOW();
    c = 0x2B;
    HAL_SPI_Transmit(&hspi1, &c, 1, 10);
    LCD_DC_HIGH();
    d[0] = y0 >> 8; d[1] = y0 & 0xFF;
    d[2] = y1 >> 8; d[3] = y1 & 0xFF;
    HAL_SPI_Transmit(&hspi1, d, 4, 10);

    /* RAMWR — iniciar escritura de pixels */
    LCD_DC_LOW();
    c = 0x2C;
    HAL_SPI_Transmit(&hspi1, &c, 1, 10);
    LCD_DC_HIGH();
    /* CS queda LOW, DC queda HIGH — listo para pixel data */
}

void ILI9341_EndWrite(void) {
    LCD_CS_HIGH();
}

/* ========================================================================== */
/* === ESCRITURA DE PIXELS ================================================== */
/* ========================================================================== */

void ILI9341_WritePixels(const uint8_t *buf, uint32_t len_bytes) {
    LCD_DC_HIGH();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len_bytes, 2000);
}

/* ========================================================================== */
/* === RELLENO DE RECTANGULOS =============================================== */
/* ========================================================================== */

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!w || !h) return;
    if (x >= ILI9341_W || y >= ILI9341_H) return;

    /* Clip al borde de la pantalla */
    if ((uint32_t)x + w > ILI9341_W) w = ILI9341_W - x;
    if ((uint32_t)y + h > ILI9341_H) h = ILI9341_H - y;

    ILI9341_SetWindow(x, y, x + w - 1, y + h - 1);

    /* Preparar fila con el color (big-endian RGB565) */
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint16_t fill = (w <= ILI9341_W) ? w : ILI9341_W;
    for (uint16_t i = 0; i < fill; i++) {
        lcd_row_buf[i * 2]     = hi;
        lcd_row_buf[i * 2 + 1] = lo;
    }

    /* Enviar fila por fila */
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(&hspi1, lcd_row_buf, fill * 2, 500);
    }

    ILI9341_EndWrite();
}

void ILI9341_FillScreen(uint16_t color) {
    ILI9341_FillRect(0, 0, ILI9341_W, ILI9341_H, color);
}
