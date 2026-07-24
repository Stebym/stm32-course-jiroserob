/**
 ******************************************************************************
 * @file    oled_display.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Driver OLED GME12864-41 (controlador SSD1315) sobre I2C1, polling.
 *          Adaptado del codigo de referencia que envio el profesor Nerio
 *          Montoya por correo para el Examen Parcial Taller V. El
 *          GME12864-41.pdf confirma que el SSD1315 comparte el mismo
 *          command table que el SSD1306 (paginas 20-28), por eso la
 *          secuencia de inicializacion y los comandos son identicos a los
 *          de un modulo SSD1306 128x64 generico.
 ******************************************************************************
 */

#include "oled_display.h"
#include <string.h>
#include <stdio.h>

/* ------ Handle I2C propio de este modulo ------ */
static I2C_HandleTypeDef hi2c1 = {0};

/* Direccion de 8 bits (con R/W en bit0) del modulo, 0x3C<<1 = 0x78 por
 * defecto. El modulo fisico trae un jumper de soldadura "IIC ADDRESS
 * SELECT" en la parte trasera que elige entre 0x78 y 0x7A (ver datasheet
 * "OLED 4 Pin 128x64...") -- en vez de adivinar cual lado quedo soldado,
 * OLED_ScanAddress() la detecta sola al arrancar y actualiza esta variable. */
static uint8_t ssd1306_addr = 0x78;

static uint8_t SSD1306_Buffer[1024]; // 128 columnas x 8 paginas (8 filas de pixeles c/u)

/* --- Fuente 5x7 ASCII, un byte por columna, bit0 = fila superior --- */
static const uint8_t Font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 }
    {0x0C, 0x02, 0x0C, 0x02, 0x0C}, // 126 ~
};

/*
 * Map
 * Reescala x linealmente del rango [in_min, in_max] al rango [out_min, out_max]
 * (igual que map() de Arduino). Se usa para convertir la lectura cruda del
 * joystick (0..4095) en coordenadas de pixel sobre la pantalla 128x64.
 */
long Map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
 * WriteCmd
 * Envia un byte de comando al SSD1315 por I2C. Escribir en la "direccion de
 * memoria" 0x00 pone el bit D/C# del byte de control en 0, es decir "esto es
 * un comando, no dato de pixeles" (datasheet SSD1306/SSD1315, interfaz I2C).
 */
static void WriteCmd(uint8_t c) {
    HAL_I2C_Mem_Write(&hi2c1, ssd1306_addr, 0x00, 1, &c, 1, 10);
}

/*
 * SSD1306_Init
 * Secuencia de arranque del controlador (paginas 20-28 del datasheet
 * GME12864-41, command table SSD1315 = compatible SSD1306):
 *   - Modo de direccionamiento por paginas (8 paginas x 128 columnas)
 *   - Orientacion "espejada" (Segment Remap + COM Scan invertido) para que
 *     el modulo se lea correcto segun su montaje fisico tipico
 *   - Multiplex 1:64, offset 0 -> panel 128x64
 *   - Charge pump interno habilitado (obligatorio: el modulo solo recibe
 *     VDD logico, sin VCC de panel externo)
 *   - Display apagado hasta el ultimo comando (0xAF) para no mostrar basura
 *     mientras se configura
 */
void SSD1306_Init(void) {
    HAL_Delay(100); // estabilizacion tras el power-on

    WriteCmd(0xAE); // Display OFF mientras se configura

    WriteCmd(0x20); // Set Memory Addressing Mode...
    WriteCmd(0x10); // ...Page Addressing Mode

    WriteCmd(0xB0); // Page Start Address = PAGE0

    WriteCmd(0xC8); // COM Output Scan Direction remapeado (voltea verticalmente)

    WriteCmd(0x00); // Column Start Address, nibble bajo = 0
    WriteCmd(0x10); // Column Start Address, nibble alto = 0

    WriteCmd(0x40); // Display Start Line = 0

    WriteCmd(0x81); // Contrast Control...
    WriteCmd(0xFF); // ...maximo (0xFF de 256 pasos)

    WriteCmd(0xA1); // Segment Re-map (voltea horizontalmente)

    WriteCmd(0xA6); // Normal Display (bit RAM en 1 = pixel encendido)

    WriteCmd(0xA8); // Multiplex Ratio...
    WriteCmd(0x3F); // ...N-1=63 -> MUX 1:64

    WriteCmd(0xA4); // Entire Display sigue el contenido de GDDRAM

    WriteCmd(0xD3); // Display Offset...
    WriteCmd(0x00); // ...sin desplazamiento vertical

    WriteCmd(0xD5); // Divide Ratio / Oscillator Frequency...
    WriteCmd(0xF0); // ...Fosc alto, ratio=1

    WriteCmd(0xD9); // Pre-charge Period...
    WriteCmd(0x22); // ...valores por defecto

    WriteCmd(0xDA); // COM Pins Hardware Configuration...
    WriteCmd(0x12); // ...config alternativa, sin remap izq/der

    WriteCmd(0xDB); // VCOMH Deselect Level...
    WriteCmd(0x20); // ...~0.77 x VCC

    WriteCmd(0x8D); // Charge Pump Setting...
    WriteCmd(0x14); // ...habilita el charge pump interno

    WriteCmd(0xAF); // Display ON
}

/*
 * SSD1306_Fill
 * Llena el framebuffer local con un color solido. Solo toca la copia en RAM
 * del MCU -- nada se envia al display hasta SSD1306_UpdateScreen().
 */
void SSD1306_Fill(uint8_t color) {
    memset(SSD1306_Buffer, (color == 0) ? 0 : 0xFF, sizeof(SSD1306_Buffer));
}

/*
 * SSD1306_UpdateScreen
 * Vuelca el framebuffer local a la GDDRAM del SSD1315 por I2C, una pagina
 * (8 filas de pixeles) a la vez, siguiendo el modo de direccionamiento por
 * paginas configurado en SSD1306_Init().
 */
void SSD1306_UpdateScreen(void) {
    for (int i = 0; i < 8; i++) {
        WriteCmd(0xB0 + i); // pagina i
        WriteCmd(0x00);     // columna 0, nibble bajo
        WriteCmd(0x10);     // columna 0, nibble alto

        // registro 0x40 (en vez de 0x00) pone D/C#=1 -> "esto es dato de
        // pixeles". El puntero de columna auto-incrementa, asi que la pagina
        // completa se escribe de una sola vez.
        HAL_I2C_Mem_Write(&hi2c1, ssd1306_addr, 0x40, 1, &SSD1306_Buffer[128 * i], 128, 50);
    }
}

/*
 * SSD1306_DrawPixel
 * Prende o apaga un pixel individual en el framebuffer local, usando el
 * mismo layout de bytes que espera la GDDRAM: 8 paginas de 128 columnas,
 * bit0 = fila superior de la pagina, bit7 = fila inferior.
 */
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= 128 || y >= 64) return; // fuera de pantalla, se ignora

    if (color) {
        SSD1306_Buffer[x + (y / 8) * 128] |= (1 << (y % 8));
    } else {
        SSD1306_Buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
    }
}

/*
 * SSD1306_WriteString
 * Dibuja una cadena ASCII terminada en null en el framebuffer, empezando en
 * (x,y), usando la fuente 5x7 (Font5x7[]). Cada glifo mide 5 pixeles de
 * ancho mas 1 columna de espacio, asi que el cursor avanza 6 px por caracter.
 */
void SSD1306_WriteString(uint8_t x, uint8_t y, char *str) {
    while (*str) {
        char c = *str;
        uint8_t idx = 0;
        if (c >= 32 && c <= 126) {
            idx = c - 32; // Font5x7[] empieza en el caracter espacio (ASCII 32)
        }

        for (int i = 0; i < 5; i++) {
            uint8_t b = Font5x7[idx][i];
            for (int j = 0; j < 8; j++) {
                if ((b >> j) & 1) {
                    SSD1306_DrawPixel(x + i, y + j, 1);
                }
            }
        }
        x += 6;
        str++;
    }
}

/*
 * SSD1306_DrawCross
 * Dibuja un cursor "+" centrado en (x,y) -- se usa para marcar la posicion
 * del joystick en pantalla.
 */
void SSD1306_DrawCross(uint8_t x, uint8_t y) {
    SSD1306_DrawPixel(x, y, 1);
    SSD1306_DrawPixel(x - 1, y, 1);
    SSD1306_DrawPixel(x + 1, y, 1);
    SSD1306_DrawPixel(x, y - 1, 1);
    SSD1306_DrawPixel(x, y + 1, 1);
}

/*
 * SSD1306_DrawEmptyRect
 * Dibuja el contorno de un rectangulo sin relleno: esquina superior
 * izquierda en (x_zero, y_zero), x_wide de ancho, y_height de alto.
 */
void SSD1306_DrawEmptyRect(uint8_t x_zero, uint8_t y_zero, uint8_t x_wide, uint8_t y_height) {
    uint8_t x_end = x_zero + x_wide;
    uint8_t y_end = y_zero + y_height;

    for (int i = x_zero; i <= x_end; i++) {
        SSD1306_DrawPixel(i, y_zero, 1);
        SSD1306_DrawPixel(i, y_end, 1);
    }
    for (int i = y_zero; i <= y_end; i++) {
        SSD1306_DrawPixel(x_zero, i, 1);
        SSD1306_DrawPixel(x_end, i, 1);
    }
}

/*
 * OLED_I2C1_Init
 * Configura I2C1 en modo Fast Mode (400 kHz) sobre PB8=SCL/PB9=SDA, en
 * polling puro -- la rubrica del examen indica explicitamente que el manejo
 * de I2C NO requiere interrupciones.
 */
void OLED_I2C1_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_i2c_config = {0};
    GPIO_i2c_config.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_i2c_config.Mode      = GPIO_MODE_AF_OD; // open-drain: obligatorio en I2C
    GPIO_i2c_config.Pull      = GPIO_NOPULL;     // el modulo OLED trae sus propias pull-ups
    GPIO_i2c_config.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_i2c_config.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_i2c_config);

    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000; // Fast Mode
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&hi2c1);
}

/*
 * OLED_ScanAddress
 * Escanea el bus I2C1 completo (direcciones de 7 bits 0x03-0x77, rango
 * valido segun la especificacion I2C) probando cada una con
 * HAL_I2C_IsDeviceReady(). Reporta por huart cada direccion que respondio y,
 * si encuentra el modulo OLED en 0x3C (0x78) o 0x3D (0x7A) -- las dos
 * posiciones del jumper "IIC ADDRESS SELECT" que trae este modulo segun su
 * datasheet -- actualiza ssd1306_addr automaticamente, sin depender de leer
 * bien la serigrafia del jumper en la placa fisica.
 *
 * Debe llamarse DESPUES de OLED_I2C1_Init() y de inicializar el uart, y
 * ANTES de SSD1306_Init().
 */
void OLED_ScanAddress(UART_HandleTypeDef *huart) {
    char linea[64];
    uint8_t encontrados = 0;

    snprintf(linea, sizeof(linea), "\r\n=== ESCANEO BUS I2C1 ===\r\n");
    HAL_UART_Transmit(huart, (uint8_t *)linea, strlen(linea), 100);

    for (uint8_t addr7 = 0x03; addr7 <= 0x77; addr7++) {
        uint16_t addr8 = (uint16_t)(addr7 << 1);
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr8, 2, 5) == HAL_OK) {
            encontrados++;
            snprintf(linea, sizeof(linea), "  Dispositivo encontrado: 0x%02X (7b) / 0x%02X (8b)\r\n",
                     addr7, (unsigned int)addr8);
            HAL_UART_Transmit(huart, (uint8_t *)linea, strlen(linea), 100);

            if (addr7 == 0x3C || addr7 == 0x3D) {
                ssd1306_addr = (uint8_t)addr8; /* jumper 0x78 o 0x7A del oled */
            }
        }
    }

    if (encontrados == 0) {
        snprintf(linea, sizeof(linea), "  Nada respondio -- revisa cableado/pull-ups.\r\n");
    } else {
        snprintf(linea, sizeof(linea), "  OLED usara direccion 0x%02X.\r\n", ssd1306_addr);
    }
    HAL_UART_Transmit(huart, (uint8_t *)linea, strlen(linea), 100);
    HAL_UART_Transmit(huart, (uint8_t *)"========================\r\n\r\n", 30, 100);
}
