/**
 ******************************************************************************
 * @file    ili9341.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver ILI9341 — inicializacion y primitivas de dibujo basico
 *          para practicas de pantalla (relleno, figuras, imagenes, texto).
 ******************************************************************************
 */

#include "ili9341.h"
#include "board_pins.h"

extern SPI_HandleTypeDef hspi1;
static uint8_t lcd_row_buf[ILI9341_MAXDIM * 2];

/* Dimensiones activas segun orientacion — Init() arranca en paisaje 320x240 */
uint16_t ILI9341_W = 320;
uint16_t ILI9341_H = 240;

/* ========================================================================== */
/* === FUNCIONES INTERNAS DE BUS ============================================ */
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
    ILI9341_FillScreen(COLOR_BLACK);
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
/* === ORIENTACION EN TIEMPO DE EJECUCION ==================================== */
/* ========================================================================== */
/* Reconfigura MADCTL y las dimensiones activas. Usar antes de dibujar una    */
/* pantalla que necesite la otra orientacion (ej. modo cocktail Simon).      */

void ILI9341_SetPortrait(uint8_t portrait) {
    uint8_t madctl = portrait ? ILI9341_MADCTL_PORTRAIT : ILI9341_MADCTL_LANDSCAPE;
    LCD_CmdData(0x36, &madctl, 1);

    if (portrait) { ILI9341_W = 240; ILI9341_H = 320; }
    else          { ILI9341_W = 320; ILI9341_H = 240; }
}

/* ========================================================================== */
/* === ESCRITURA DE PIXELES ================================================= */
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

/* ========================================================================== */
/* === FIGURAS BASICAS ======================================================= */
/* ========================================================================== */

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

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    ILI9341_FillRect(x, y, 1, 1, color);
}

void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!w || !h) return;
    ILI9341_FillRect(x,         y,         w, 1, color);
    ILI9341_FillRect(x,         y + h - 1, w, 1, color);
    ILI9341_FillRect(x,         y,         1, h, color);
    ILI9341_FillRect(x + w - 1, y,         1, h, color);
}

/* Bresenham clasico. Los tramos horizontales/verticales se despachan como
 * FillRect de una fila/columna para aprovechar la rafaga SPI. */
void ILI9341_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (y0 == y1) {
        int16_t x = (x0 < x1) ? x0 : x1;
        uint16_t w = (uint16_t)((x0 < x1) ? (x1 - x0) : (x0 - x1)) + 1;
        if (x >= 0 && y0 >= 0) ILI9341_FillRect((uint16_t)x, (uint16_t)y0, w, 1, color);
        return;
    }
    if (x0 == x1) {
        int16_t y = (y0 < y1) ? y0 : y1;
        uint16_t h = (uint16_t)((y0 < y1) ? (y1 - y0) : (y0 - y1)) + 1;
        if (x0 >= 0 && y >= 0) ILI9341_FillRect((uint16_t)x0, (uint16_t)y, 1, h, color);
        return;
    }

    int16_t dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (int16_t)-((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (1) {
        if (x0 >= 0 && y0 >= 0) ILI9341_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/* Circulo de punto medio (Bresenham) — solo contorno, 8 octantes por simetria. */
void ILI9341_DrawCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {
    int16_t x = r, y = 0, err = 0;

    while (x >= y) {
        if (xc + x >= 0 && yc + y >= 0) ILI9341_DrawPixel((uint16_t)(xc + x), (uint16_t)(yc + y), color);
        if (xc + y >= 0 && yc + x >= 0) ILI9341_DrawPixel((uint16_t)(xc + y), (uint16_t)(yc + x), color);
        if (xc - y >= 0 && yc + x >= 0) ILI9341_DrawPixel((uint16_t)(xc - y), (uint16_t)(yc + x), color);
        if (xc - x >= 0 && yc + y >= 0) ILI9341_DrawPixel((uint16_t)(xc - x), (uint16_t)(yc + y), color);
        if (xc - x >= 0 && yc - y >= 0) ILI9341_DrawPixel((uint16_t)(xc - x), (uint16_t)(yc - y), color);
        if (xc - y >= 0 && yc - x >= 0) ILI9341_DrawPixel((uint16_t)(xc - y), (uint16_t)(yc - x), color);
        if (xc + y >= 0 && yc - x >= 0) ILI9341_DrawPixel((uint16_t)(xc + y), (uint16_t)(yc - x), color);
        if (xc + x >= 0 && yc - y >= 0) ILI9341_DrawPixel((uint16_t)(xc + x), (uint16_t)(yc - y), color);

        y++;
        if (err <= 0) { err += 2 * y + 1; }
        if (err > 0)  { x--; err -= 2 * x + 1; }
    }
}

/* Circulo relleno — barrido de lineas horizontales entre los bordes de cada fila. */
void ILI9341_FillCircle(int16_t xc, int16_t yc, int16_t r, uint16_t color) {
    for (int16_t y = -r; y <= r; y++) {
        int16_t dx = (int16_t)((int32_t)r * r - (int32_t)y * y);
        /* raiz entera aproximada por busqueda lineal (r es pequeño en pantalla) */
        int16_t half = 0;
        while ((half + 1) * (half + 1) <= dx) half++;
        int16_t yy = (int16_t)(yc + y);
        if (yy < 0) continue;
        int16_t xx0 = (int16_t)(xc - half);
        if (xx0 < 0) xx0 = 0;
        uint16_t w = (uint16_t)(2 * half + 1);
        ILI9341_FillRect((uint16_t)xx0, (uint16_t)yy, w, 1, color);
    }
}

/* ========================================================================== */
/* === IMAGENES ============================================================== */
/* ========================================================================== */

void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data) {
    if (!w || !h) return;
    if (x >= ILI9341_W || y >= ILI9341_H) return;
    if ((uint32_t)x + w > ILI9341_W) w = ILI9341_W - x;
    if ((uint32_t)y + h > ILI9341_H) h = ILI9341_H - y;

    ILI9341_SetWindow(x, y, x + w - 1, y + h - 1);
    ILI9341_WritePixels(data, (uint32_t)w * h * 2);
    ILI9341_EndWrite();
}

/* ========================================================================== */
/* === FUENTE BITMAP 5x7 (ASCII 32-90) ======================================= */
/* ========================================================================== */
/* Cada char: 5 bytes (columnas izq→der). Cada byte: bit0=fila top, bit6=bot. */

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' 32 */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' 33 */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' 34 */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' 35 */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' 36 */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' 37 */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' 38 */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' 39 */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' 40 */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' 41 */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' 42 */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' 43 */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' 44 */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' 45 */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' 46 */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' 47 */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' 48 */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' 49 */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' 50 */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' 51 */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' 52 */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' 53 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' 54 */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' 55 */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' 56 */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' 57 */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' 58 */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' 59 */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' 60 */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' 61 */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' 62 */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' 63 */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' 64 */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' 65 */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' 66 */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' 67 */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' 68 */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' 69 */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' 70 */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' 71 */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' 72 */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' 73 */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' 74 */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' 75 */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' 76 */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' 77 */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' 78 */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' 79 */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' 80 */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' 81 */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' 82 */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' 83 */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' 84 */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' 85 */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' 86 */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' 87 */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' 88 */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' 89 */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' 90 */
};

void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);   /* minusculas -> mayusculas */
    if (c < 32 || c > 90) c = '?';
    const uint8_t *g = font5x7[(uint8_t)c - 32];

    uint16_t adv = (uint16_t)(6u * scale);
    uint16_t hgt = (uint16_t)(7u * scale);
    ILI9341_FillRect(x, y, adv, hgt, bg);

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1u << row)) {
                ILI9341_FillRect((uint16_t)(x + col * scale),
                                 (uint16_t)(y + row * scale),
                                 scale, scale, fg);
            }
        }
    }
}

void ILI9341_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        ILI9341_DrawChar(x, y, *s, fg, bg, scale);
        x = (uint16_t)(x + 6u * scale);
        s++;
    }
}
