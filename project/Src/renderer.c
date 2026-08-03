/**
 ******************************************************************************
 * @file    renderer.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Motor de renderizado Beat Clash — ILI9341 320x240 paisaje.
 *
 * Layout pantalla 320x240:
 *   P1 x=[0..158]   Divisor x=[159..160]   P2 x=[161..319]
 *   Score bar: y=[0..24]
 *   Lane 0 (R): y=[25..76]    Sep: y=[77..78]
 *   Lane 1 (G): y=[79..130]   Sep: y=[131..132]
 *   Lane 2 (B): y=[133..184]  Sep: y=[185..186]
 *   Lane 3 (Y): y=[187..238]
 *
 * Fuente bitmap 5x7 (col-major, bit0=fila top) cubre ASCII 32-90.
 * Render delta en JUGANDO: solo actualiza los px que cambian por tick.
 ******************************************************************************
 */

#include "renderer.h"      // prototipos de todas las funciones Renderer_* que este archivo implementa
#include "ili9341.h"        // primitivas de bajo nivel del driver (FillRect/DrawLine/DrawCircle/etc.) que arman todo el dibujo
#include "board_pins.h"     // colores (COLOR_*) y constantes de layout (P1_X_OFF, LANE_TOP, etc.)
#include "splash_bg.h"      // arreglo splash_bg[] con los pixeles RGB565 de la imagen de bienvenida
#include <string.h>          // incluido por costumbre/legado -- no se usa ninguna funcion de string.h en este archivo
#include <stdio.h>            // snprintf(), usado para formatear numeros/texto antes de dibujarlos
#include "stm32f4xx_hal.h"    // HAL_GetTick(), usado para temporizar parpadeos (heartbeat, blink de splash)

/* ========================================================================== */
/* === FUENTE BITMAP 5x7 ==================================================== */
/* ========================================================================== */
/* Matriz de fuente 5x7 estandar (el mismo patron de bits que usa la libreria
 * Adafruit GFX en su glcdfont.c, ampliamente reutilizado en proyectos con
 * pantallas pequeñas) -- no es un diseño propio, se adopto por ser un formato
 * de dominio publico ya probado, compacto (5 bytes por caracter) y facil de
 * recorrer columna por columna con SPI. Cada char: 5 bytes (columnas
 * izq→der). Cada byte: bit0=fila top, bit6=bot. Cubre ASCII 32 (espacio) a
 * ASCII 90 ('Z') -- suficiente para todo el texto en mayusculas que usa este
 * proyecto, no se agregaron minusculas ni simbolos extra por no hacer falta. */

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

/* Ancho de avance por caracter segun escala (5 cols + 1 gap) */
#define CHAR_ADV(scale)  ((uint16_t)(6u * (scale)))  // 6px de ancho por unidad de escala (5 de glifo + 1 de espacio) -- subir "scale" separa mas las letras
/* Alto de glifo segun escala */
#define CHAR_H(scale)    ((uint16_t)(7u * (scale)))  // 7px de alto por unidad de escala -- subir "scale" agranda el texto en vertical

/* ========================================================================== */
/* === HELPERS DE FUENTE ==================================================== */
/* ========================================================================== */

static void draw_char(uint16_t x, uint16_t y, char c,   // dibuja UN caracter de la fuente 5x7 en (x,y), escalado
                      uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 32 || c > 90) c = '?';                    // fuera del rango cubierto por font5x7 (32-90) -> se dibuja '?' en vez de leer memoria fuera de la tabla
    const uint8_t *g = font5x7[(uint8_t)c - 32];       // puntero a las 5 columnas de bits del glifo de "c" (offset -32 porque la tabla arranca en el espacio, ASCII 32)
    /* Fondo del slot del caracter */
    ILI9341_FillRect(x, y, CHAR_ADV(scale), CHAR_H(scale), bg);  // pinta todo el rectangulo del caracter con el color de fondo antes de dibujar los pixeles del glifo encima
    for (uint8_t col = 0; col < 5; col++) {            // recorre las 5 columnas del glifo, izquierda a derecha
        uint8_t bits = g[col];                          // byte de la columna actual, cada bit es un pixel de esa columna (bit0=fila de arriba)
        for (uint8_t row = 0; row < 7; row++) {         // recorre las 7 filas de esa columna
            if (bits & (1u << row)) {                    // si el bit de esta fila esta encendido, hay que pintar ese pixel del glifo
                ILI9341_FillRect(x + (uint16_t)col * scale,
                                 y + (uint16_t)row * scale,
                                 scale, scale, fg);      // pinta un cuadrado de "scale x scale" pixeles reales por cada "pixel logico" del glifo -- asi escala el tamaño de letra
            }
        }
    }
}

static void draw_string(uint16_t x, uint16_t y, const char *s,   // dibuja una cadena completa encadenando draw_char caracter por caracter
                        uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {                        // recorre la cadena hasta el '\0' final
        draw_char(x, y, *s, fg, bg, scale);  // dibuja el caracter actual en la posicion (x,y)
        x += CHAR_ADV(scale);                // avanza x el ancho de un caracter (ya escalado) para el siguiente
        s++;                                  // pasa al siguiente caracter de la cadena
    }
}

static uint16_t str_pixel_w(const char *s, uint8_t scale) {   // calcula el ancho en pixeles que ocuparia la cadena a esa escala, sin dibujarla (para poder centrarla antes)
    uint16_t n = 0;                     // cuenta de caracteres de la cadena
    while (*s++) n++;                    // recorre la cadena contando caracteres (sin el '\0')
    return (uint16_t)(n * CHAR_ADV(scale));  // ancho total en pixeles = cantidad de caracteres * ancho de avance por caracter
}

/* Dibuja la cadena centrada en cx (coordenada X del centro). */
static void draw_string_c(uint16_t cx, uint16_t y, const char *s,   // ver comentario de arriba: centra la cadena horizontalmente en cx
                           uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t w = str_pixel_w(s, scale);            // ancho total en pixeles que va a ocupar la cadena
    int16_t x  = (int16_t)cx - (int16_t)(w / 2u);  // arranca el texto medio ancho a la izquierda del centro pedido, para que quede centrado
    if (x < 0) x = 0;                               // evita coordenada negativa (se saldria de pantalla / underflow al castear a uint16_t) si el texto es mas ancho que el espacio disponible a la izquierda
    draw_string((uint16_t)x, y, s, fg, bg, scale);  // dibuja la cadena ya con el x centrado calculado
}

/* Dibuja un entero sin signo de hasta 5 digitos. */
static void draw_uint16(uint16_t x, uint16_t y, uint16_t val,   // ver comentario de arriba: convierte val a texto decimal y lo dibuja
                        uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[6];               // hasta 5 digitos (uint16_t max es 65535) + terminador '\0'
    int8_t i = 5;               // indice desde el que se va llenando el buffer, de atras hacia adelante
    buf[5] = '\0';               // fin de cadena fijo en la ultima posicion
    if (val == 0) { buf[4] = '0'; i = 4; }  // caso especial: el bucle de abajo no genera ningun digito si val ya es 0
    else {
        while (val > 0 && i > 0) { buf[--i] = (char)('0' + val % 10); val /= 10; }  // extrae un digito por vuelta (el resto de dividir entre 10), de derecha a izquierda, hasta agotar val o el buffer
    }
    draw_string(x, y, &buf[i], fg, bg, scale);  // dibuja solo desde donde arrancaron los digitos generados (i), no todo el buffer
}

/* ========================================================================== */
/* === CONSTANTES DE LAYOUT ================================================= */
/* ========================================================================== */

#define DIV_X       159  // columna x donde arranca la franja divisoria vertical entre las mitades de P1 y P2 (layout paisaje de Guitar Hero "maqueta")
#define DIV_W       2     // ancho en pixeles de esa franja divisoria
static const uint16_t PLAYER_X_OFF[2] = { P1_X_OFF, P2_X_OFF };  // offset x de arranque de la mitad de cada jugador (definidos en board_pins.h), indexado [0]=J1 [1]=J2

static inline uint16_t note_y(uint8_t carril) {   // y absoluta donde dibujar una nota de ese carril (ver el cuerpo)
    return (uint16_t)(LANE_TOP(carril) + NOTE_Y_PAD);  // y donde dibujar una nota en ese carril = borde superior del carril + relleno vertical fijo (para centrarla dentro del carril)
}

/* ========================================================================== */
/* === SEGMENTOS 7-SEG PARA PUNTAJE ========================================= */
/* ========================================================================== */
/* Simula un display de 7 segmentos dibujando rectangulos en vez de usar la
 * fuente 5x7 -- se eligio este estilo para el puntaje porque se queria un
 * look mas "arcade/marcador" que texto normal. La tabla de segmentos por
 * digito (seg_table) sigue la convencion estandar de nombrar los segmentos
 * A (arriba), B/C (derecha arriba/abajo), D (abajo), E/F (izquierda
 * abajo/arriba), G (medio), la misma que se usa en cualquier datasheet de
 * display 7-segmentos. */

#define SEG_S   8               // largo de cada segmento recto (arriba/abajo/medio) en pixeles -- subirlo agranda todo el digito
#define SEG_T   2                // grosor de cada segmento en pixeles
#define SEG_DX  (SEG_S + SEG_T + 2)  // separacion horizontal entre un digito y el siguiente al dibujar varios en fila

#define SEG_A (1<<0)  // segmento de ARRIBA
#define SEG_B (1<<1)  // segmento derecha-arriba
#define SEG_C (1<<2)  // segmento derecha-abajo
#define SEG_D (1<<3)  // segmento de ABAJO
#define SEG_E (1<<4)  // segmento izquierda-abajo
#define SEG_F (1<<5)  // segmento izquierda-arriba
#define SEG_G (1<<6)  // segmento del MEDIO

static const uint8_t seg_table[10] = {
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,        /* 0 */
    SEG_B|SEG_C,                                  /* 1 */
    SEG_A|SEG_B|SEG_D|SEG_E|SEG_G,              /* 2 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_G,              /* 3 */
    SEG_B|SEG_C|SEG_F|SEG_G,                     /* 4 */
    SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,              /* 5 */
    SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,        /* 6 */
    SEG_A|SEG_B|SEG_C,                           /* 7 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G, /* 8 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G,        /* 9 */
};

static void draw_seg_digit(int16_t x, int16_t y, uint8_t digit,   // dibuja UN digito estilo 7 segmentos en (x,y)
                            uint16_t fg, uint16_t bg) {
    uint8_t segs = seg_table[digit % 10];  // que segmentos encender para este digito (%10 protege de un digit>9 invalido)
    ILI9341_FillRect((uint16_t)x, (uint16_t)y,
                     SEG_S + 2*SEG_T, 2*SEG_S + 3*SEG_T, bg);  // borra/pinta todo el bounding box del digito con el color de fondo antes de dibujar los segmentos encendidos
    if (segs & SEG_A) ILI9341_FillRect((uint16_t)(x+SEG_T), (uint16_t)y,
                                        SEG_S, SEG_T, fg);  // segmento ARRIBA: franja horizontal en el borde superior
    if (segs & SEG_G) ILI9341_FillRect((uint16_t)(x+SEG_T),
                                        (uint16_t)(y+SEG_S+SEG_T), SEG_S, SEG_T, fg);  // segmento MEDIO: franja horizontal a media altura
    if (segs & SEG_D) ILI9341_FillRect((uint16_t)(x+SEG_T),
                                        (uint16_t)(y+2*SEG_S+2*SEG_T), SEG_S, SEG_T, fg);  // segmento ABAJO: franja horizontal en el borde inferior
    if (segs & SEG_F) ILI9341_FillRect((uint16_t)x,
                                        (uint16_t)(y+SEG_T), SEG_T, SEG_S, fg);  // segmento izquierda-arriba: franja vertical en el borde izquierdo, mitad de arriba
    if (segs & SEG_E) ILI9341_FillRect((uint16_t)x,
                                        (uint16_t)(y+SEG_S+2*SEG_T), SEG_T, SEG_S, fg);  // segmento izquierda-abajo: franja vertical en el borde izquierdo, mitad de abajo
    if (segs & SEG_B) ILI9341_FillRect((uint16_t)(x+SEG_S+SEG_T),
                                        (uint16_t)(y+SEG_T), SEG_T, SEG_S, fg);  // segmento derecha-arriba: franja vertical en el borde derecho, mitad de arriba
    if (segs & SEG_C) ILI9341_FillRect((uint16_t)(x+SEG_S+SEG_T),
                                        (uint16_t)(y+SEG_S+2*SEG_T), SEG_T, SEG_S, fg);  // segmento derecha-abajo: franja vertical en el borde derecho, mitad de abajo
}

static void draw_score_bar(uint16_t x_off, uint16_t score, uint16_t fg) {   // barra de progreso de puntaje (relleno proporcional a score/SCORE_MAX)
    uint16_t bx   = x_off + 23;                 // x donde arranca la barra, relativa al offset de este jugador
    uint16_t bw   = 78, bh = 10, by = 7;          // ancho/alto/y fijos de la barra -- cambiar bw estira/acorta la barra completa
    uint16_t fill = (uint16_t)((uint32_t)score * bw / SCORE_MAX);  // cuantos pixeles de los bw totales representan el puntaje actual (regla de 3 contra SCORE_MAX); el cast a uint32_t evita overflow de la multiplicacion en 16 bits
    if (fill) ILI9341_FillRect(bx,        by, fill,      bh, fg);            // parte "llena" de la barra, del color del jugador
    if (fill < bw) ILI9341_FillRect(bx + fill, by, bw - fill, bh, COLOR_DARKGRAY);  // resto de la barra sin llenar, en gris oscuro (fondo)
}

static void draw_score_digits(uint16_t x_off, uint16_t score,   // dibuja el puntaje como 4 digitos de 7 segmentos
                               uint16_t fg, uint16_t bg) {
    int16_t dx = (int16_t)(x_off + 103);  // x de arranque del primer digito (millares), relativo al offset del jugador
    uint8_t d[4] = {
        (uint8_t)((score / 1000) % 10),  // digito de millares
        (uint8_t)((score / 100)  % 10),  // digito de centenas
        (uint8_t)((score / 10)   % 10),  // digito de decenas
        (uint8_t)(score % 10)             // digito de unidades
    };
    for (int i = 0; i < 4; i++) {
        draw_seg_digit(dx, 1, d[i], fg, bg);  // dibuja el digito i-esimo en su posicion actual
        dx += SEG_DX + 1;                      // avanza a la posicion x del siguiente digito
    }
}

/* ========================================================================== */
/* === FONDO ESTATICO (ESTADO_JUGANDO) ====================================== */
/* ========================================================================== */

void Renderer_DrawBackground(const GameState_t *gs) {   // dibuja el fondo completo del modo JUGANDO (recorrido de diseño): franjas de puntaje, carriles y divisor
    (void)gs;  // parametro sin usar -- se recibe por si en el futuro el fondo depende del estado, hoy es siempre el mismo layout fijo
    /* Barras de score */
    ILI9341_FillRect(P1_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);  // franja superior gris para el marcador de J1
    ILI9341_FillRect(P2_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);  // franja superior gris para el marcador de J2

    /* Bloques indicador de jugador */
    ILI9341_FillRect(P1_X_OFF + 1, 2, 20, 20, COLOR_P1);  // cuadrado de color de J1 (identifica visualmente a quien pertenece esa mitad)
    ILI9341_FillRect(P2_X_OFF + 1, 2, 20, 20, COLOR_P2);  // cuadrado de color de J2

    /* "J1" y "J2" sobre los bloques de indicador */
    draw_char(P1_X_OFF + 5,  7, 'J', COLOR_WHITE, COLOR_P1, 1);  // letra 'J' sobre el cuadrado de J1
    draw_char(P1_X_OFF + 12, 7, '1', COLOR_WHITE, COLOR_P1, 1);  // numero '1' junto a la 'J', completando "J1"
    draw_char(P2_X_OFF + 5,  7, 'J', COLOR_WHITE, COLOR_P2, 1);  // letra 'J' sobre el cuadrado de J2
    draw_char(P2_X_OFF + 12, 7, '2', COLOR_WHITE, COLOR_P2, 1);  // numero '2' junto a la 'J', completando "J2"

    /* Divisor central */
    ILI9341_FillRect(DIV_X, 0, DIV_W, LCD_H, COLOR_WHITE);  // franja blanca vertical que separa visualmente las 2 mitades de pantalla

    /* Carriles y separadores */
    for (uint8_t p = 0; p < 2; p++) {                  // recorre los 2 jugadores
        uint16_t xo = PLAYER_X_OFF[p];                  // offset x de arranque de la mitad de este jugador
        for (uint8_t c = 0; c < 4; c++) {                // recorre los 4 carriles/colores de este jugador
            uint16_t ly = LANE_TOP(c);                    // y donde arranca este carril
            ILI9341_FillRect(xo, ly, PLAYER_W, LANE_H, LANE_COLOR[c]);       // pinta todo el carril con su color "apagado" de fondo
            ILI9341_FillRect(xo, ly, PRESS_ZONE_W, LANE_H, PRESS_COLOR[c]);  // sobre-pinta la franja de la izquierda (zona de golpe) con su propio tono distinto, para marcarla
        }
        for (uint8_t s = 0; s < 3; s++) {                // dibuja las 3 lineas separadoras entre los 4 carriles (no hace falta una despues del ultimo)
            ILI9341_FillRect(xo, LANE_TOP(s) + LANE_H, PLAYER_W, SEP_H, COLOR_GRAY);  // linea gris fina justo debajo del carril "s"
        }
    }
}

/* ========================================================================== */
/* === SPLASH SCREEN ======================================================== */
/* ========================================================================== */

void Renderer_DrawSplash(void) {   // dibuja la pantalla de bienvenida (imagen de fondo + credito + instruccion)
    /* Fondo: "NEON RIFF" generada por IA, recortada 1024x768 y reescalada a
     * 320x240 (ver splash_bg.h) — ya trae su propio titulo y panel Simon,
     * asi que no hace falta redibujar iconos/franjas/rejilla encima. */
    ILI9341_DrawImage(0, 0, LCD_W, LCD_H, splash_bg);  // vuelca la imagen completa de fondo, pixel por pixel, sobre toda la pantalla

    /* Franja inferior semi-fija con el texto funcional (credito, llamado a
     * la accion) — unico contenido dibujado a mano sobre la imagen */
    ILI9341_FillRect(0, 196, LCD_W, 44, COLOR_DARKGRAY);  // tapa la franja inferior de la imagen con gris solido, para poner texto legible encima
    draw_string_c(LCD_W / 2, 200, "POR: JIMMY STEBYM ROSERO BARRERA",
                  COLOR_GRAY, COLOR_DARKGRAY, 1);  // credito del autor, centrado

    /* Subtitulo parpadeante "LISTO PARA JUGAR?" en y=213 — dibujado por
     * Renderer_Update (se actualiza cada 500ms en el dispatcher) */

    draw_string_c(LCD_W / 2, 226, "B1 O UN BOTON PARA CONTINUAR",
                  COLOR_CYAN, COLOR_DARKGRAY, 1);  // instruccion fija de como avanzar, centrada
}

/* ========================================================================== */
/* === MENU DE DIFICULTAD (Guitar Hero) ======================================= */
/* ========================================================================== */
/* Pantalla de la primera version del proyecto (seleccion de dificultad por
 * velocidad FACIL/MEDIO/PRO). Reconectada al recorrido actual (ver
 * DEMO_MENU_DIFICULTAD en DemoScreen_t, main.c): aparece SOLO para Guitar
 * Hero, justo despues de confirmar el modo -- fija la velocidad/spawn
 * inicial de las notas, que despues sigue subiendo sola con el tiempo de
 * partida (ver GuitarHero_Actualizar en main.c). */

void Renderer_DrawMenu(uint8_t cursor) {   // dibuja el menu de seleccion de dificultad (3 tarjetas FACIL/MEDIO/PRO), usado antes de Guitar Hero
    ILI9341_FillScreen(COLOR_BLACK);  // borra toda la pantalla a negro antes de dibujar el menu desde cero

    /* Cabecera */
    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja superior gris de titulo
    draw_string_c(LCD_W / 2, 9, "SELECCIONA DIFICULTAD",
                  COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado sobre la franja

    /* Tres tarjetas de nivel: x=15,115,215, cada una 90x90 */
    static const uint16_t card_col[3]  = { COLOR_GREEN,  COLOR_YELLOW, COLOR_RED };  // color de cada tarjeta segun dificultad (verde=facil, rojo=dificil)
    static const char *const card_name[3] = { "FACIL", "MEDIO", "PRO"  };  // texto corto de cada tarjeta
    static const char *const card_desc[3] = {
        "VELOCIDAD 1",  // descripcion de FACIL
        "VELOCIDAD 2",  // descripcion de MEDIO
        "VELOCIDAD 3"   // descripcion de PRO
    };

    for (uint8_t i = 0; i < 3; i++) {                     // dibuja las 3 tarjetas una por una
        uint16_t bx = (uint16_t)(15 + i * 100);            // separa cada tarjeta 100px en x (cambiar este 100 acerca/aleja las tarjetas)
        uint16_t by = 38;                                    // misma altura y para las 3
        uint16_t bw = 90, bh = 90;                            // tamaño de cada tarjeta (cuadrada de 90x90)
        uint16_t inner_bg = (cursor == i) ? COLOR_DARKGRAY : COLOR_BLACK;  // la tarjeta bajo el cursor se resalta con fondo gris en vez de negro

        /* Marco exterior del color del nivel */
        ILI9341_FillRect(bx, by, bw, bh, card_col[i]);  // rectangulo completo del color de la tarjeta (hace de "marco" porque el interior se pinta encima, mas chico)
        /* Interior */
        ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);  // interior 3px mas chico por lado, deja ver 3px de marco de color alrededor

        /* Numero del nivel en 7-seg grande */
        draw_seg_digit((int16_t)(bx + 37), (int16_t)(by + 10),
                       (uint8_t)(i + 1), card_col[i], inner_bg);  // numero de nivel (1,2,3) estilo 7 segmentos, del color de la tarjeta

        /* Nombre del nivel centrado en la tarjeta */
        uint16_t name_w = str_pixel_w(card_name[i], 2);        // ancho en pixeles que va a ocupar el nombre a escala 2
        uint16_t name_x = bx + (bw - name_w) / 2;                // x para que el nombre quede centrado horizontalmente en la tarjeta
        draw_string(name_x, by + 60, card_name[i], card_col[i], inner_bg, 2);  // dibuja el nombre del nivel

        /* Flecha de seleccion bajo la tarjeta activa */
        if (cursor == i) {                                    // solo la tarjeta actualmente seleccionada muestra la flecha debajo
            ILI9341_FillRect(bx + 33, by + bh + 4, 24, 8, card_col[i]);   // base ancha de la flecha (triangulo armado con 3 rectangulos decrecientes)
            ILI9341_FillRect(bx + 37, by + bh + 12, 16, 6, card_col[i]);  // segmento medio de la flecha
            ILI9341_FillRect(bx + 41, by + bh + 18, 8,  4, card_col[i]);  // punta angosta de la flecha
        }
    }

    /* Descripcion del nivel seleccionado */
    uint16_t dy = 175;  // y donde arranca el panel de descripcion, debajo de las tarjetas
    ILI9341_FillRect(20, dy, 280, 40, COLOR_DARKGRAY);  // panel de fondo gris para la descripcion

    /* Barras de velocidad: 1, 2 o 3 barras */
    for (uint8_t v = 0; v <= cursor; v++) {  // dibuja tantas barras como el indice del nivel seleccionado (0=1 barra ... 2=3 barras)
        ILI9341_FillRect((uint16_t)(28 + v * 22), dy + 12, 18, 16, card_col[cursor]);  // cada barra 22px mas a la derecha que la anterior
    }

    /* Descripcion de texto */
    draw_string_c(LCD_W / 2, dy + 14, card_desc[cursor],
                  COLOR_WHITE, COLOR_DARKGRAY, 1);  // texto de descripcion del nivel actualmente seleccionado, centrado

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);  // franja inferior de ayuda
    draw_string_c(LCD_W / 2, 226, "JOYSTICK=MUEVE  CUALQUIERA=CONFIRMA",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);  // texto de ayuda centrado -- igual patron que el resto de los menus del recorrido (joystick mueve, cualquier boton confirma)
}

/* ========================================================================== */
/* === SELECCION DE JUGADORES ================================================ */
/* ========================================================================== */

/* Dibuja (o borra) UNA tarjeta de "cuantos jugadores", incluida la flechita
 * de seleccion debajo (se borra sola cuando selected=0). */
static void rsj_draw_card(uint8_t i, uint8_t selected) {   // dibuja UNA tarjeta del menu de seleccion de jugadores (1/2), resaltada si esta seleccionada
    static const char *const nombre[2] = { "1 JUGADOR", "2 JUGADORES" };  // texto de cada tarjeta (i=0 -> 1 jugador, i=1 -> 2 jugadores)
    static const uint16_t    color[2]  = { COLOR_P1, COLOR_P2 };          // tarjeta 0 en el color de J1, tarjeta 1 en el color de J2

    uint16_t bx = (uint16_t)(30 + i * 150);           // separa las 2 tarjetas 150px en x
    uint16_t by = 55, bw = 130, bh = 140;               // posicion/tamaño fijo de cada tarjeta
    uint16_t inner_bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;  // la tarjeta seleccionada se resalta con fondo gris

    ILI9341_FillRect(bx, by, bw, bh, color[i]);                       // marco exterior del color de la tarjeta
    ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);        // interior, deja ver 3px de marco alrededor

    draw_seg_digit((int16_t)(bx + bw / 2 - 12), (int16_t)(by + 15),
                   (uint8_t)(i + 1), color[i], inner_bg);              // numero grande (1 o 2) centrado arriba de la tarjeta

    uint16_t name_w = str_pixel_w(nombre[i], 1);                        // ancho en pixeles del texto de la tarjeta
    draw_string((uint16_t)(bx + (bw - name_w) / 2), (uint16_t)(by + 100),
                nombre[i], color[i], inner_bg, 1);                     // texto centrado horizontalmente, cerca de la parte baja de la tarjeta

    if (selected) {
        ILI9341_FillRect(bx + bw / 2 - 12, by + bh + 4,  24, 8, color[i]);   // base de la flecha de seleccion debajo de la tarjeta
        ILI9341_FillRect(bx + bw / 2 - 8,  by + bh + 12, 16, 6, color[i]);   // segmento medio de la flecha
        ILI9341_FillRect(bx + bw / 2 - 4,  by + bh + 18, 8,  4, color[i]);   // punta de la flecha
    } else {
        ILI9341_FillRect(bx + bw / 2 - 12, by + bh + 4, 24, 18, COLOR_BLACK);  // si no esta seleccionada, borra el espacio de la flecha (por si antes SI estaba dibujada ahi)
    }
}

void Renderer_DrawSeleccionJugadores(uint8_t cursor) {   // dibuja el menu completo de "cuantos jugadores" desde cero
    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja de titulo
    draw_string_c(LCD_W / 2, 9, "CUANTOS JUGADORES", COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado

    for (uint8_t i = 0; i < 2; i++) rsj_draw_card(i, cursor == i);  // dibuja las 2 tarjetas, resaltando la que coincide con el cursor actual

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);  // franja de ayuda inferior
    draw_string_c(LCD_W / 2, 226, "JOYSTICK/BOTON=MUEVE  CUALQUIERA=CONFIRMA",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);  // texto de ayuda
}

void Renderer_UpdateSeleccionJugadores(uint8_t cursor_ant, uint8_t cursor) {   // redibuja solo las 2 tarjetas que cambiaron de estado (la que se apaga y la que se prende)
    if (cursor_ant == cursor) return;                          // el cursor no se movio -- no hay nada que redibujar (evita trafico SPI innecesario)
    if (cursor_ant < 2) rsj_draw_card(cursor_ant, 0);            // apaga el resaltado de la tarjeta donde estaba antes el cursor
    if (cursor     < 2) rsj_draw_card(cursor, 1);                 // enciende el resaltado de la tarjeta donde esta ahora
}

/* ========================================================================== */
/* === INICIALES (NOMBRE DE 3 LETRAS POR JUGADOR) ============================ */
/* ========================================================================== */
/* Pantalla intermedia entre la seleccion de JUGADORES y la de MODO: cada
 * jugador elige 3 letras tipo "iniciales de arcade". La logica de ciclar
 * A-Z y confirmar vive en main.c (MenuIniciales_Procesar) -- aca solo se
 * dibuja. */

#define RN_BOX_W    60          // ancho de cada casillero de letra
#define RN_BOX_H    80           // alto de cada casillero de letra
#define RN_BOX_GAP  20            // separacion horizontal entre casilleros
#define RN_BOX_Y    70             // y donde arrancan los 3 casilleros
#define RN_LETRA_SCALE 6            // escala de la letra grande dentro del casillero (6x el glifo base de 5x7 = 30x42px)

static uint16_t rn_box_x(uint8_t pos) {   // x del casillero de la letra `pos` (0-2) del nombre de 3 letras
    uint16_t total = 3 * RN_BOX_W + 2 * RN_BOX_GAP;               // ancho total ocupado por los 3 casilleros + 2 huecos entre ellos
    uint16_t x0    = (uint16_t)((LCD_W - total) / 2);              // x de arranque para que el conjunto de 3 casilleros quede centrado en pantalla
    return (uint16_t)(x0 + pos * (RN_BOX_W + RN_BOX_GAP));        // x del casillero "pos" (0,1,2), cada uno corrido su ancho+separacion respecto al anterior
}

/* activo=1 (amarillo) -- letra que se esta editando ahora mismo.
 * activo=0 (gris) -- letra ya confirmada. */
static void rn_draw_letra(uint8_t pos, char c, uint8_t activo) {   // dibuja UN casillero del nombre con su letra actual, resaltado si es el que se esta editando
    uint16_t bx     = rn_box_x(pos);                    // x de este casillero
    uint16_t borde  = activo ? COLOR_YELLOW : COLOR_GRAY;  // color del marco: amarillo si se esta editando, gris si ya se confirmo
    char     buf[2] = { c, 0 };                          // cadena de 1 caracter (mas terminador) para poder usar draw_string_c

    ILI9341_FillRect(bx, RN_BOX_Y, RN_BOX_W, RN_BOX_H, COLOR_BLACK);  // limpia el casillero completo a negro antes de redibujar
    ILI9341_DrawRect(bx, RN_BOX_Y, RN_BOX_W, RN_BOX_H, borde);         // contorno exterior del casillero
    ILI9341_DrawRect((uint16_t)(bx + 1), (uint16_t)(RN_BOX_Y + 1),
                      (uint16_t)(RN_BOX_W - 2), (uint16_t)(RN_BOX_H - 2), borde);  // segundo contorno 1px adentro, para que el marco se vea mas grueso (2px)

    uint16_t cy = (uint16_t)(RN_BOX_Y + (RN_BOX_H - CHAR_H(RN_LETRA_SCALE)) / 2);  // y para que la letra quede centrada verticalmente en el casillero
    draw_string_c((uint16_t)(bx + RN_BOX_W / 2), cy, buf, COLOR_WHITE, COLOR_BLACK, RN_LETRA_SCALE);  // dibuja la letra grande, centrada en el casillero
}

void Renderer_DrawNombre(uint8_t jugador, const char nombre[4], uint8_t pos_actual) {   // dibuja la pantalla completa de captura de iniciales de un jugador
    char titulo[24];                                 // buffer para el titulo formateado ("JUGADOR 1 - TU NOMBRE", etc.)
    ILI9341_FillScreen(COLOR_BLACK);                  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja de titulo
    snprintf(titulo, sizeof(titulo), "JUGADOR %u - TU NOMBRE", (unsigned)(jugador + 1));  // arma el texto con el numero de jugador (jugador+1 porque internamente es 0/1 pero se muestra 1/2)
    draw_string_c(LCD_W / 2, 9, titulo, (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_DARKGRAY, 1);  // titulo centrado, en el color del jugador que esta escribiendo

    for (uint8_t i = 0; i < 3; i++) rn_draw_letra(i, nombre[i], (i == pos_actual));  // dibuja las 3 letras; la que coincide con pos_actual se resalta como "activa"

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);  // franja de ayuda inferior
    draw_string_c(LCD_W / 2, 226, "JOYSTICK=LETRA  BOTON=CONFIRMAR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);  // texto de ayuda
}

void Renderer_UpdateNombreLetra(uint8_t jugador, uint8_t pos, char letra) {   // redibuja solo el casillero que esta editando el jugador ahora mismo (letra girando con el joystick)
    (void)jugador;               // no hace falta: esta pantalla es siempre pantalla completa (no cara-a-cara), no depende de a que jugador pertenece
    rn_draw_letra(pos, letra, 1);  // redibuja SOLO ese casillero, como "activo" (se esta editando) -- evita repintar toda la pantalla por cada letra que se cicla
}

void Renderer_ConfirmarNombreLetra(uint8_t pos, char letra) {   // redibuja el casillero ya CONFIRMADO (deja de estar resaltado) al pasar al siguiente
    rn_draw_letra(pos, letra, 0);  // redibuja ese casillero como "confirmado" (marco gris en vez de amarillo), al pasar a la siguiente letra
}

/* ========================================================================== */
/* === SELECCION DE MODO DE JUEGO ============================================ */
/* ========================================================================== */

/* Los nombres mostrados en pantalla ("BOTONES", "JOYS", "GT HERO") se
 * definieron a partir del dispositivo de entrada de cada modo en vez de un
 * nombre generico tipo "SIMON", para que resulte mas claro para quien juega.
 * Los enums internos (DEMO_MODO_SIMON/_SIMONJOY/_GUITAR en main.c) conservan
 * sus nombres originales; solo cambia el texto visible en pantalla. */
static const uint16_t    RSM_CARD_COL[3]  = { COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW };  // color de cada tarjeta de modo (indice = DEMO_MODO_SIMON/_SIMONJOY/_GUITAR - DEMO_MODO_SIMON)
static const char *const RSM_CARD_NAME[3] = { "BOTONES", "JOYS", "GT HERO" };  // nombre corto mostrado en cada tarjeta
static const char *const RSM_CARD_DESC[3] = {
    "4 BOTONES, VELOCIDAD CRECIENTE",   // descripcion del modo BOTONES
    "SIGUE LA DIRECCION DEL JOYSTICK",  // descripcion del modo JOYS
    "NOTAS CAYENDO ESTILO GUITAR HERO"  // descripcion del modo GT HERO
};

/* Dibuja (o borra) UNA tarjeta de modo, incluida la flechita de seleccion */
static void rsm_draw_card(uint8_t i, uint8_t selected) {   // dibuja UNA tarjeta del menu de seleccion de MODO de juego
    uint16_t bx = (uint16_t)(15 + i * 100);  // separa las 3 tarjetas 100px en x
    uint16_t by = 38, bw = 90, bh = 90;       // posicion/tamaño fijo, igual que las tarjetas del menu legado de arriba
    uint16_t inner_bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;  // resalta con fondo gris la tarjeta bajo el cursor

    ILI9341_FillRect(bx, by, bw, bh, RSM_CARD_COL[i]);  // marco exterior del color de esta tarjeta
    ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);  // interior, deja ver 3px de marco

    draw_seg_digit((int16_t)(bx + 37), (int16_t)(by + 10),
                   (uint8_t)(i + 1), RSM_CARD_COL[i], inner_bg);  // numero de tarjeta (1,2,3) estilo 7-seg

    uint16_t name_w = str_pixel_w(RSM_CARD_NAME[i], 1);  // ancho en pixeles del nombre del modo
    draw_string((uint16_t)(bx + (bw - name_w) / 2), by + 62, RSM_CARD_NAME[i], RSM_CARD_COL[i], inner_bg, 1);  // nombre centrado horizontalmente en la tarjeta

    if (selected) {
        ILI9341_FillRect(bx + 33, by + bh + 4, 24, 8, RSM_CARD_COL[i]);   // base de la flecha de seleccion
        ILI9341_FillRect(bx + 37, by + bh + 12, 16, 6, RSM_CARD_COL[i]);  // segmento medio de la flecha
        ILI9341_FillRect(bx + 41, by + bh + 18, 8,  4, RSM_CARD_COL[i]);  // punta de la flecha
    } else {
        ILI9341_FillRect(bx + 33, by + bh + 4, 24, 18, COLOR_BLACK);  // borra el espacio de la flecha si esta tarjeta no esta seleccionada
    }
}

static void rsm_draw_desc(uint8_t cursor) {   // dibuja el texto de descripcion del modo actualmente resaltado por el cursor
    uint16_t dy = 175;  // y del panel de descripcion, debajo de las tarjetas
    ILI9341_FillRect(20, dy, 280, 40, COLOR_DARKGRAY);  // fondo del panel de descripcion
    draw_string_c(LCD_W / 2, dy + 14, RSM_CARD_DESC[cursor], COLOR_WHITE, COLOR_DARKGRAY, 1);  // texto de descripcion del modo actualmente seleccionado
}

void Renderer_DrawSeleccionModo(uint8_t cursor) {   // dibuja el menu completo de seleccion de modo (Simon/SimonJoy/Guitar) desde cero
    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja de titulo
    draw_string_c(LCD_W / 2, 9, "SELECCIONA MODO", COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado

    for (uint8_t i = 0; i < 3; i++) rsm_draw_card(i, cursor == i);  // dibuja las 3 tarjetas, resaltando la del cursor
    rsm_draw_desc(cursor);  // descripcion del modo actual

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);  // franja de ayuda inferior
    draw_string_c(LCD_W / 2, 226, "BOTON=BOTONES   JOYSTICK=JOYS",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);  // recuerda que cualquier boton entra directo a BOTONES y cualquier joystick entra directo a JOYS (seleccion directa, sin pasos intermedios)
}

void Renderer_UpdateSeleccionModo(uint8_t cursor_ant, uint8_t cursor) {   // redibuja solo el cambio de cursor entre 2 tarjetas del menu de modo
    if (cursor_ant == cursor) return;                    // sin cambio de cursor, nada que redibujar
    if (cursor_ant < 3) rsm_draw_card(cursor_ant, 0);      // apaga el resaltado de la tarjeta anterior
    if (cursor     < 3) rsm_draw_card(cursor, 1);          // enciende el resaltado de la tarjeta nueva
    rsm_draw_desc(cursor);                                  // actualiza el texto de descripcion al del nuevo cursor
}

/* ========================================================================== */
/* === VISTA PREVIA — MODO SIMON CLASICO Y SIMON+JOYSTICK ==================== */
/* ========================================================================== */
/* Ambos modos son competitivos SIMULTANEOS con pantalla dividida (igual que  */
/* el Guitar Hero): cada jugador ve su propia mitad y su propia secuencia.    */
/* paso_activo = indice (0-3) del bloque que esta "encendido" en este cuadro, */
/* o 0xFF si ninguno esta activo (todos en su color apagado).                 */

/* ========================================================================== */
/* === MODO SIMON CLASICO — LAYOUT "COCKTAIL" (MESA, JUGADORES ENFRENTADOS) == */
/* ========================================================================== */
/* Pantalla fisica en RETRATO (240x320, ver ILI9341_SetPortrait). El Jugador 2
 * se sienta en el extremo opuesto de la mesa, asi que su mitad (arriba) se
 * dibuja rotada 180 grados para que el la vea al derecho. El Jugador 1
 * (abajo) se dibuja normal. Las funciones Cockpit_* reciben coordenadas
 * LOCALES (como si cada mitad fuera su propia pantalla 240x155) y hacen la
 * transformacion — el codigo de la pantalla no necesita saber cual mitad
 * esta invertida.                                                          */

#define COCK_ZONE_W   240  // ancho (en modo retrato) de la mitad de mesa de cada jugador -- coincide con el ancho de pantalla en retrato (240x320)
#define COCK_ZONE_H   155  // alto de la mitad de cada jugador -- subir esto agranda la zona de juego de cada uno, a costa de achicar el divisor/la otra mitad
#define COCK_P2_Y     0    // y donde arranca en el buffer la zona "espejada" (jugador 1, ver Cockpit_FillRect mas abajo)
#define COCK_DIV_Y    155  // y de la franja divisoria gris entre las 2 mitades de mesa
#define COCK_DIV_H    10   // alto de esa franja divisoria
#define COCK_P1_Y     165  // y donde arranca en el buffer la zona "normal" (jugador 2) -- 165 = COCK_DIV_Y + COCK_DIV_H, justo debajo del divisor

/* Invierte el orden de los 7 bits (filas) de una columna del glifo 5x7. */
static uint8_t reverse7(uint8_t b) {   // invierte el orden de los 7 bits (filas) de una columna del glifo 5x7, usado para rotar texto 180 grados
    uint8_t r = 0;                                          // acumulador del byte con las filas invertidas
    for (uint8_t i = 0; i < 7; i++) {                        // recorre las 7 filas del glifo
        if (b & (1u << i)) r |= (uint8_t)(1u << (6 - i));    // si la fila "i" esta encendida, prende la fila espejada "6-i" en el resultado
    }
    return r;                                                // byte con las 7 filas en orden inverso (arriba<->abajo)
}

/* Dibuja un caracter rotado 180 grados (para la mitad invertida del cocktail). */
static void draw_char_180(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {   // dibuja UN caracter rotado 180 grados (para la mitad "cara a cara" invertida del cockpit)
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);             // normaliza minusculas a mayusculas (la tabla font5x7 solo cubre mayusculas)
    if (c < 32 || c > 90) c = '?';                             // fuera de rango cubierto -> se dibuja '?'
    const uint8_t *g = font5x7[(uint8_t)c - 32];                // puntero a las 5 columnas de bits del glifo

    uint16_t adv = CHAR_ADV(scale);                            // ancho total del slot del caracter a esta escala
    uint16_t hgt = CHAR_H(scale);                               // alto total del slot del caracter a esta escala
    ILI9341_FillRect(x, y, adv, hgt, bg);                       // pinta el fondo del slot antes de dibujar el glifo encima

    for (uint8_t col = 0; col < 5; col++) {                    // recorre las 5 columnas del glifo
        uint8_t bits = reverse7(g[4 - col]);                    // columna en orden inverso (4-col en vez de col) Y con filas invertidas -- asi el caracter completo queda rotado 180 grados (izq<->der y arriba<->abajo a la vez)
        for (uint8_t row = 0; row < 7; row++) {                 // recorre las 7 filas de esa columna ya invertida
            if (bits & (1u << row)) {                            // si el bit de esta fila esta encendido
                ILI9341_FillRect((uint16_t)(x + col * scale), (uint16_t)(y + row * scale),
                                 scale, scale, fg);              // pinta el pixel (escalado) del glifo
            }
        }
    }
}

/* Cadena rotada 180 grados: caracteres en orden inverso, cada uno volteado. */
static void draw_string_180(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {   // dibuja una cadena entera rotada 180 grados (orden de caracteres invertido + cada uno volteado)
    uint16_t n = 0;                                    // cuenta de caracteres de la cadena
    for (const char *p = s; *p; p++) n++;                // recorre la cadena contando caracteres
    for (uint16_t i = 0; i < n; i++) {                   // recorre la cadena de nuevo, esta vez para dibujar
        draw_char_180((uint16_t)(x + i * 6u * scale), y, s[n - 1 - i], fg, bg, scale);  // dibuja el caracter i-esimo EN ORDEN INVERSO (s[n-1-i]) para que la cadena rotada 180 se siga leyendo igual que la original
    }
}

/* jugador: 1=J2 (abajo, normal) 0=J1 (arriba, rotado 180). Coordenadas (lx,ly)
 * son LOCALES a la zona 240x155 de cada jugador. Las 3 funciones Cockpit_*
 * de este bloque son las UNICAS que deciden que mitad de PANTALLA es cual --
 * Simon Clasico, Simon+Joystick 2P y Guitar Hero las comparten, asi que
 * cualquier ajuste aca corrige los 3 modos a la vez sin tocar su logica de
 * juego (que sigue indexada 0=jugador1 fisico via BTN1_*, 1=jugador2 fisico
 * via BTN2_*, eso no cambia). */
/* El jugador logico 1 usa la rama "normal" (sin rotar, desplazada a la zona
 * inferior de la pantalla) y el jugador logico 0 usa la rama espejada en x e
 * y (equivalente a una rotacion de 180 grados, desplazada a la zona
 * superior). Esta asignacion determina en que mitad FISICA de la pantalla
 * aparece el nombre/cursor/D-pad de cada jugador, y es independiente de la
 * indexacion de hardware de los botones (0=jugador1 fisico via BTN1_*,
 * 1=jugador2 fisico via BTN2_*), que no se ve afectada por este bloque. */
static void Cockpit_FillRect(uint8_t jugador, uint16_t lx, uint16_t ly, uint16_t lw, uint16_t lh, uint16_t color) {   // rectangulo en coordenadas LOCALES de la mitad de `jugador`, transformado (rotado o no) a la mitad fisica de pantalla que corresponda
    if (jugador == 1) {
        ILI9341_FillRect(lx, (uint16_t)(COCK_P1_Y + ly), lw, lh, color);  // jugador 2 (fisico): rectangulo "normal", solo desplazado a la zona de abajo (COCK_P1_Y), sin rotar ni espejar
    } else {
        uint16_t px = (uint16_t)(COCK_ZONE_W - lx - lw);                  // jugador 1 (fisico): x espejada en el ancho de la zona (para que "izquierda" local salga a la derecha fisica y viceversa)
        uint16_t py = (uint16_t)(COCK_P2_Y + (COCK_ZONE_H - ly - lh));    // y espejada en el alto de la zona (arriba<->abajo), dentro de la zona de arriba (COCK_P2_Y)
        ILI9341_FillRect(px, py, lw, lh, color);                          // dibuja el rectangulo ya transformado -- el efecto neto de espejar x e y es una rotacion de 180 grados
    }
}

static void Cockpit_DrawString(uint8_t jugador, uint16_t lx, uint16_t ly, const char *s,   // texto en coordenadas LOCALES de la mitad de `jugador`, con la misma transformacion que Cockpit_FillRect
                                uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t sw = str_pixel_w(s, scale);                                  // ancho en pixeles que va a ocupar la cadena, necesario para poder espejar su posicion x
    uint16_t sh = CHAR_H(scale);                                          // alto en pixeles de la cadena, necesario para espejar su posicion y
    if (jugador == 1) {
        draw_string(lx, (uint16_t)(COCK_P1_Y + ly), s, fg, bg, scale);    // jugador 2 (fisico): texto normal (sin rotar), en la zona de abajo
    } else {
        uint16_t px = (uint16_t)(COCK_ZONE_W - lx - sw);                  // x espejada, restando tambien el ancho del texto (para que el string completo quede espejado, no solo su punto de arranque)
        uint16_t py = (uint16_t)(COCK_P2_Y + (COCK_ZONE_H - ly - sh));    // y espejada, restando tambien el alto del texto
        draw_string_180(px, py, s, fg, bg, scale);                        // dibuja el texto con draw_string_180 (cada letra tambien rotada), no solo reposicionado, para que jugador 1 lo lea al derecho desde su lado
    }
}

/* Transforma un PUNTO local (no un rectangulo) -- una rotacion de 180 grados
 * deja el radio de un circulo intacto, asi que para elementos circulares
 * (badges del D-pad, botones arcade redondos, notas/zona de Guitar Hero)
 * solo hace falta transformar su centro, no todo el area. Se declara en esta
 * seccion porque tanto el diseño circular de Simon Clasico como el de
 * Simon+Joystick la utilizan. Misma asignacion de ramas jugador==0/1 que
 * Cockpit_FillRect. */
static void Cockpit_Punto(uint8_t jugador, int16_t lx, int16_t ly, int16_t *px, int16_t *py) {   // transforma un PUNTO local (sin ancho/alto) a coordenadas absolutas de pantalla, para elementos circulares
    if (jugador == 1) {
        *px = lx;                              // jugador 2 (fisico): x sin cambios
        *py = (int16_t)(COCK_P1_Y + ly);        // y solo desplazada a la zona de abajo
    } else {
        *px = (int16_t)(COCK_ZONE_W - lx);              // jugador 1 (fisico): x espejada (sin restar ancho porque un punto no tiene ancho)
        *py = (int16_t)(COCK_P2_Y + (COCK_ZONE_H - ly));  // y espejada, dentro de la zona de arriba
    }
}

/* 1 = intercambiar ROJO<->AMARILLO y VERDE<->AZUL en la cuadricula (ver
 * Renderer_SetSimonClasicoInvertido); 0 = layout historico. Solo se activa
 * desde main.c para la partida de 2 jugadores -- el modo de 1 jugador sigue
 * arrancando siempre en 0 (ver Botones_IniciarSolo), asi que su disposicion
 * en pantalla no cambia. */
static uint8_t cockpit_simon_invertido = 0;  // 0=layout historico, 1=ROJO<->AMARILLO y VERDE<->AZUL intercambiados (ver Renderer_SetSimonClasicoInvertido)

void Renderer_SetSimonClasicoInvertido(uint8_t invertido) {   // activa/desactiva el intercambio ROJO<->AMARILLO y VERDE<->AZUL de la cuadricula 2x2 (solo 2 jugadores)
    cockpit_simon_invertido = invertido ? 1 : 0;
}

/* Cuadricula 2x2 local: ABAJO=Rojo+Verde, ARRIBA=Azul+Amarillo (fila
 * invertida respecto a es_abajo[]), con el mismo layout de colores para
 * los dos jugadores. La columna (es_derecha) tampoco varia entre jugadores.
 * Con cockpit_simon_invertido activo, la fila deja de invertirse: da
 * ARRIBA=Rojo+Verde, ABAJO=Amarillo+Azul (Rojo<->Amarillo y Verde<->Azul
 * intercambian de fila respecto al layout historico). */
static void cockpit_2x2_rect(uint8_t jugador, uint8_t c, uint16_t *lx, uint16_t *ly, uint16_t *lw, uint16_t *lh) {   // bounding box LOCAL (sin rotar) del boton de color `c` en la cuadricula 2x2 de Simon Clasico
    static const uint8_t es_derecha[4] = { 0, 1, 1, 0 };  // por color (0=ROJO 1=VERDE 2=AZUL 3=AMARILLO): que colores van en la columna derecha de la cuadricula 2x2
    static const uint8_t es_abajo[4]   = { 0, 0, 1, 1 };  // que colores van en la fila de abajo (antes de la inversion de mas abajo)
    uint16_t cw = 70, ch = 55, gap = 10;                  // ancho/alto de cada boton y separacion entre ellos -- agrandar cw/ch agranda los botones, gap los separa mas
    uint16_t gx0 = (COCK_ZONE_W - (2 * cw + gap)) / 2;     // x de arranque de la cuadricula para que quede centrada horizontalmente en la zona del jugador
    uint16_t gy0 = 30;                                      // y de arranque de la cuadricula (fija, deja espacio arriba para el nombre del jugador)
    (void)jugador;                                          // ya no se usa para decidir la fila (ambos jugadores comparten el mismo layout de colores, ver comentario de arriba) -- se mantiene el parametro para no cambiar la firma que llaman cockpit_2x2_draw/cockpit_pad_rect
    uint8_t  abajo = cockpit_simon_invertido ? es_abajo[c] : (uint8_t)(!es_abajo[c]);   // fila normal o invertida segun cockpit_simon_invertido (ver comentario de arriba)
    *lw = cw;
    *lh = ch;
    *lx = es_derecha[c] ? (uint16_t)(gx0 + cw + gap) : gx0;  // columna izquierda o derecha segun es_derecha[c]
    *ly = abajo         ? (uint16_t)(gy0 + ch + gap) : gy0;  // fila arriba o abajo segun "abajo" ya invertido
}

/* Boton arcade circular estilo "domo": un anillo exterior oscuro (marco del
 * boton fisico) + cuerpo circular del color de carril +, si esta encendido,
 * un brillo interior descentrado (arriba-izquierda) para simular el reflejo
 * de luz de un domo iluminado; imita la apariencia de un boton arcade real
 * con mayor fidelidad que un rectangulo plano. El bounding box de
 * cockpit_2x2_rect se sigue usando solo para calcular centro/radio -- las
 * esquinas del cuadrado quedan negras (fondo ya limpio por
 * cockpit_clasico_draw_jugador), nunca se pintan, asi que no hace falta
 * borrar nada antes de redibujar. */
static void cockpit_2x2_draw(uint8_t jugador, uint8_t c, uint8_t activo) {   // dibuja UN boton/domo circular de la cuadricula 2x2, encendido o apagado
    uint16_t lx, ly, lw, lh;
    cockpit_2x2_rect(jugador, c, &lx, &ly, &lw, &lh);       // bounding box local (sin rotar/espejar) de este boton
    uint16_t col = activo ? NOTE_COLOR[c] : LANE_COLOR[c];   // color brillante si es el paso activo de la secuencia, apagado si no

    int16_t lcx   = (int16_t)(lx + lw / 2);                 // centro x local del boton
    int16_t lcy   = (int16_t)(ly + lh / 2);                 // centro y local del boton
    int16_t r_out = (int16_t)((lw < lh ? lw : lh) / 2);      // radio exterior = mitad del lado mas chico del bounding box (para que el circulo quepa)
    int16_t r_in  = r_out - 5;                                // radio interior 5px mas chico -- ese anillo de 5px es el "marco" oscuro del boton
    if (r_in < 4) r_in = 4;                                    // evita que el interior desaparezca si el boton fuera muy chico

    int16_t acx, acy;
    Cockpit_Punto(jugador, lcx, lcy, &acx, &acy);            // transforma el centro local a coordenadas absolutas de pantalla (rotando si corresponde)

    ILI9341_FillCircle2(acx, acy, r_out, COLOR_DARKGRAY, r_in, col);   /* anillo exterior + cuerpo del domo en 1 solo pase de SPI */

    if (activo) {
        int16_t r_brillo = r_in / 3;                        // el brillo ocupa un tercio del radio interior
        if (r_brillo < 2) r_brillo = 2;                       // brillo minimo de 2px de radio, para que no desaparezca en botones chicos
        ILI9341_FillCircle((int16_t)(acx - r_in / 3), (int16_t)(acy - r_in / 3),
                            r_brillo, COLOR_WHITE);          // circulo blanco descentrado arriba-izquierda, simulando un reflejo de luz -- no es concentrico con el domo, se deja aparte
    }
}

/* Redibuja TODO un jugador (etiqueta + cuadricula 2x2) sin tocar al otro --
 * usado tanto para el dibujo inicial completo como para reiniciar solo un
 * jugador que perdio, sin borrar la partida en curso del otro. */
static void cockpit_clasico_draw_jugador(uint8_t jugador, uint8_t paso) {   // redibuja toda la mitad de un jugador de Simon Clasico (nombre + los 4 botones)
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador a negro antes de redibujar
    Cockpit_DrawString(jugador, 8, 5, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 2);  // nombre del jugador (iniciales de 3 letras) en su color, esquina superior izquierda LOCAL
    for (uint8_t c = 0; c < 4; c++) cockpit_2x2_draw(jugador, c, paso == c);  // dibuja los 4 botones de color; el que coincide con "paso" se dibuja encendido
}

/* === MODO SIMON BOTONES — PANTALLA COMPLETA PARA 1 JUGADOR (retrato 240x320) =
 * Mismo espiritu que Renderer_DrawModoSimonJoystick1P: dibujo directo, sin
 * pasar por las transformaciones Cockpit_* (esas son solo para repartir la
 * pantalla entre 2 mitades encontradas). Reusa el mismo orden de colores
 * ROJO/VERDE/AZUL/AMARILLO en la cuadricula 2x2 que cockpit_2x2_rect con
 * invertido=0 (el modo 1P nunca invierte, ver Botones_IniciarSolo en
 * main.c), pero con domos mucho mas grandes al no tener que compartir la
 * pantalla con otro jugador. */
#define S1P_ZONE_W  240   // ancho de pantalla en retrato
#define S1P_BOT_W   100   // ancho de cada domo
#define S1P_BOT_H   110   // alto de cada domo
#define S1P_GAP     20    // separacion entre domos (horizontal y vertical)
#define S1P_GX0     ((S1P_ZONE_W - (2 * S1P_BOT_W + S1P_GAP)) / 2)  // x de arranque de la cuadricula, centrada
#define S1P_GY0     40    // y de arranque de la cuadricula, debajo de la franja de titulo

static void simon1p_rect(uint8_t c, uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h) {   // bounding box del domo de color `c` en el layout de pantalla completa (1 jugador)
    static const uint8_t es_derecha[4] = { 0, 1, 1, 0 };  // por color (0=ROJO 1=VERDE 2=AZUL 3=AMARILLO): igual orden que cockpit_2x2_rect
    static const uint8_t es_abajo[4]   = { 0, 0, 1, 1 };  // idem -- con invertido=0 fijo (ver comentario de arriba), abajo = !es_abajo[c]
    *w = S1P_BOT_W;   // ancho fijo del boton en el layout de pantalla completa
    *h = S1P_BOT_H;   // alto fijo del boton
    *x = es_derecha[c] ? (uint16_t)(S1P_GX0 + S1P_BOT_W + S1P_GAP) : (uint16_t)S1P_GX0;  // columna izquierda o derecha segun el color
    *y = (uint8_t)(!es_abajo[c]) ? (uint16_t)(S1P_GY0 + S1P_BOT_H + S1P_GAP) : (uint16_t)S1P_GY0;  // fila arriba o abajo segun el color
}

/* Mismo domo circular "estilo boton arcade" que cockpit_2x2_draw, pero
 * dibujado directo en coordenadas absolutas de pantalla (sin Cockpit_Punto,
 * no hace falta rotar nada en el modo de 1 jugador). */
static void simon1p_draw(uint8_t c, uint8_t activo) {   // dibuja UN domo del layout de pantalla completa (1 jugador), encendido o apagado
    uint16_t x, y, w, h;
    simon1p_rect(c, &x, &y, &w, &h);
    uint16_t col = activo ? NOTE_COLOR[c] : LANE_COLOR[c];  // color brillante si esta activo, apagado (LANE_COLOR) si no

    int16_t cx    = (int16_t)(x + w / 2);   // centro x del domo
    int16_t cy    = (int16_t)(y + h / 2);   // centro y del domo
    int16_t r_out = (int16_t)((w < h ? w : h) / 2);   // radio exterior = mitad del lado mas chico del rectangulo (para que el circulo entre completo)
    int16_t r_in  = r_out - 6;   // radio interior, 6px mas chico que el exterior (grosor del anillo)
    if (r_in < 4) r_in = 4;   // limite minimo para que el nucleo no desaparezca en botones muy chicos

    ILI9341_FillCircle2(cx, cy, r_out, COLOR_DARKGRAY, r_in, col);  /* anillo exterior + cuerpo del domo en 1 solo pase de SPI */

    if (activo) {   // solo dibuja el reflejo de luz si el boton esta encendido
        int16_t r_brillo = r_in / 3;   // radio del reflejo, un tercio del nucleo
        if (r_brillo < 2) r_brillo = 2;   // minimo 2px para que no desaparezca
        ILI9341_FillCircle((int16_t)(cx - r_in / 3), (int16_t)(cy - r_in / 3),
                            r_brillo, COLOR_WHITE);      /* reflejo de luz -- no concentrico, se deja aparte */
    }
}

void Renderer_DrawModoSimonClasico1P(uint8_t paso) {   // dibuja la pantalla completa de Simon Botones para 1 jugador desde cero
    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, S1P_ZONE_W, 26, COLOR_DARKGRAY);  // franja de titulo
    draw_string_c(S1P_ZONE_W / 2, 9, "SIMON BOTONES", COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado (ancho de RETRATO, no LCD_W que es el ancho de paisaje)

    for (uint8_t c = 0; c < 4; c++) simon1p_draw(c, paso == c);  // dibuja los 4 domos; el que coincide con "paso" se dibuja encendido
}

void Renderer_UpdateModoSimonClasico1PPaso(uint8_t paso_ant, uint8_t paso) {   // redibuja solo los 2 domos que cambiaron (el que se apaga y el que se prende)
    if (paso_ant == paso) return;                // sin cambio, nada que redibujar
    if (paso_ant < 4) simon1p_draw(paso_ant, 0);   // apaga el domo anterior
    if (paso     < 4) simon1p_draw(paso, 1);       // enciende el nuevo
}

void Renderer_DrawModoSimonClasico(uint8_t paso_j1, uint8_t paso_j2) {   // dibuja las 2 mitades completas de Simon Clasico (2 jugadores) desde cero
    ILI9341_FillScreen(COLOR_BLACK);                                        // pantalla nueva desde cero
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY); // franja divisoria entre las 2 mitades de mesa
    cockpit_clasico_draw_jugador(0, paso_j1);                                // dibuja la mitad de jugador logico 0 (fisico BTN1_*) con su paso activo
    cockpit_clasico_draw_jugador(1, paso_j2);                                // dibuja la mitad de jugador logico 1 (fisico BTN2_*) con su paso activo
}

void Renderer_DrawModoSimonClasicoJugador(uint8_t jugador, uint8_t paso) {   // redibuja SOLO la mitad de un jugador (2 jugadores), sin tocar al otro
    cockpit_clasico_draw_jugador(jugador, paso);  // redibuja solo la mitad de UN jugador (usado al reiniciar solo a quien perdio, sin tocar al otro)
}

void Renderer_UpdateModoSimonClasicoPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso) {   // redibuja solo el boton que cambio de estado en la mitad de ese jugador
    if (paso_ant == paso) return;                          // el paso activo no cambio -- nada que redibujar
    if (paso_ant < 4) cockpit_2x2_draw(jugador, paso_ant, 0);  // apaga (color oscuro) el boton que estaba activo antes
    if (paso     < 4) cockpit_2x2_draw(jugador, paso, 1);       // enciende (color brillante) el boton nuevo
}

static uint16_t cockpit_clasico_racha_dibujada[2] = { 0xFFFF, 0xFFFF };  // ultima racha ya dibujada por jugador -- 0xFFFF fuerza el primer dibujo (una racha real nunca llega a ese valor)

/* Misma mitigacion que Nombre_Jugador() en main.c, aplicada aca a
 * cualquier buffer "linea"/"buf" armado con snprintf antes de dibujarlo
 * (marcadores en vivo Y pantallas de resultado): reemplaza en el lugar
 * cualquier byte fuera del rango imprimible que soporta la fuente (32-90,
 * mas minusculas 97-122 -- ver mas abajo) por '-', para que una eventual
 * corrupcion de memoria (ver el comentario extenso de Nombre_Jugador en
 * main.c, el mismo problema observado ahi) se vea como un simbolo neutro en
 * vez de una fila de '?' -- no corrige la causa de la corrupcion, solo evita
 * que se propague a la pantalla.
 *
 * BUG previo (ya corregido aca): la primera version solo aceptaba 32-90,
 * tratando CUALQUIER minuscula como "corrupta" -- pero literales como
 * "Racha: "/"Mejor: "/"Puntaje: " tienen minusculas legitimas, que
 * ILI9341_DrawChar ya normaliza a mayuscula antes de validar el rango. El
 * resultado visible era literalmente el sintoma que se queria evitar
 * ("Racha:" saliendo como "R----:" en pantalla). Por eso esta version deja
 * pasar 'a'-'z' sin tocar, ademas de 32-90. */
static void Texto_Sanear(char *s) {   // reemplaza bytes fuera del rango imprimible de la fuente por '-', para no mostrar simbolos corruptos en pantalla
    for (; *s; s++) {   // recorre la cadena caracter por caracter hasta el '\0'
        char c = *s;   // caracter actual
        if (c >= 'a' && c <= 'z') {   // es una minuscula legitima (no corrupcion)
            *s = (char)(c - 32);  // Convierte minusculas a MAYUSCULAS
            continue;   // ya se normalizo, pasa al siguiente caracter sin validar mas
        }
        if (c < 32 || c > 90) *s = '-';   // fuera del rango imprimible de la fuente (ni espacio-Z ni minuscula ya convertida) -- se reemplaza por '-'
    }
}

void Renderer_ActualizarRachaBotones(uint8_t jugador, uint16_t racha) {   // redibuja solo el numero de ronda de ese jugador (2 jugadores)
    if (racha == cockpit_clasico_racha_dibujada[jugador]) return;  // ya esta dibujada esta racha, no repetir el trabajo
    char buf[10];
    snprintf(buf, sizeof(buf), "R:%u", (unsigned)racha);            // formatea "R:<numero>"
    Texto_Sanear(buf);
    Cockpit_FillRect(jugador, 186, 2, 50, 14, COLOR_BLACK);          // borra solo el rincon donde va el contador de racha (esquina superior derecha local)
    Cockpit_DrawString(jugador, 188, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);  // dibuja el nuevo numero de racha
    cockpit_clasico_racha_dibujada[jugador] = racha;                  // recuerda que ya se dibujo esta racha, para no repetir en el proximo tick
}

void Renderer_DibujarGameOverBotones(uint8_t jugador, uint16_t racha, uint16_t mejor) {   // dibuja la pantalla de GAME OVER en la mitad de ese jugador (2 jugadores)
    char linea[20];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador
    Cockpit_DrawString(jugador, 50, 35, "GAME OVER", COLOR_RED, COLOR_BLACK, 2);  // titulo grande en rojo
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)racha);            // racha de esta partida que acaba de perder
    Texto_Sanear(linea);
    Cockpit_DrawString(jugador, 70, 70, linea, COLOR_WHITE, COLOR_BLACK, 1);
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)mejor);            // mejor racha historica de la sesion
    Texto_Sanear(linea);
    Cockpit_DrawString(jugador, 70, 90, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 15, 120, "click=cualquier boton", COLOR_GRAY, COLOR_BLACK, 1);  // instruccion de como reintentar
    cockpit_clasico_racha_dibujada[jugador] = 0xFFFF;   /* fuerza redibujo al reiniciar */
}

/* Asigna un color de pantalla a cada direccion del D-pad de Simon+Joystick
 * (d = indice de direccion, 0=ARRIBA 1=ABAJO 2=IZQ 3=DER). No usa
 * NOTE_COLOR[d]/LANE_COLOR[d] de forma directa porque el orden resultante
 * (ARRIBA en rojo) no correspondia a la convencion visual deseada para este
 * modo (ARRIBA en amarillo, con el resto de las posiciones desplazadas 90
 * grados en el mismo sentido). Es una asignacion puramente visual: no tiene
 * relacion con la lectura del joystick, que se resuelve por separado en
 * Joystick_LeerDireccion/Joystick2_LeerDireccion (main.c). Indexado por "d":
 * ARRIBA->AMARILLO(3), ABAJO->AZUL(2), IZQ->ROJO(0), DER->VERDE(1). Usada
 * unicamente por el D-pad de 1 jugador (sj1p_pad_draw); el D-pad de 2
 * jugadores usa la tabla siguiente. */
static const uint8_t SJ_DPAD_COLOR_IDX[4] = { 3, 2, 0, 1 };

/* Variante de la tabla anterior para el D-pad de Simon+Joystick a 2
 * jugadores (cockpit_pad_draw): intercambia los colores de IZQUIERDA y
 * DERECHA (de rojo(0)/verde(1) a verde(1)/rojo(0); ARRIBA/ABAJO quedan
 * iguales) porque, al leerse el D-pad desde el lado fisico de cada jugador
 * en la mesa, el sentido percibido de izquierda/derecha resulta invertido
 * respecto al orden usado en el D-pad de 1 jugador. Se usa la misma tabla
 * para los dos jugadores del modo cara a cara. */
static const uint8_t SJ_DPAD_COLOR_IDX_P0[4] = { 2, 3, 0, 1 }; // Inverte los pares de colores en la pantalla

/* ========================================================================== */
/* === SIMON+JOYSTICK — 2 JUGADORES CARA A CARA (portrait, cockpit) ========== */
/* ========================================================================== */
/* Cada jugador tiene su propio D-pad de flechas (mismo estilo que la version
 * de 1 jugador de mas abajo) dentro de su mitad de la mesa -- usando el
 * mismo mecanismo de rotacion 180 grados que Simon Clasico arriba. */

/* Cuadro local (240x155) de cada boton del D-pad -- cruz centrada, mas
 * angosta que la version de 1 jugador en pantalla completa porque aca
 * comparte la mitad de una pantalla en retrato. */
static void cockpit_pad_rect(uint8_t d, uint16_t *lx, uint16_t *ly, uint16_t *lw, uint16_t *lh) {   // bounding box LOCAL de la flecha de direccion `d` del D-pad de Simon+Joystick
    static const int16_t  cx4[4] = { 120, 120,  32, 208 };  // centro x local por direccion (d=0 ARRIBA .. 3 DER): ARRIBA/ABAJO centrados en x=120, IZQ a la izquierda, DER a la derecha
    static const int16_t  cy4[4] = {  48, 128,  91,  91 };  // centro y local: ARRIBA arriba (y chico), ABAJO abajo (y grande), IZQ/DER a media altura
    static const uint16_t r4[4]  = {  25,  22,  27,  27 };  // radio de cada badge -- IZQ/DER un poco mas grandes (24 vs 22) para balancear visualmente la cruz
    uint16_t r = r4[d];
    *lw = (uint16_t)(2 * r);          // ancho del bounding box = diametro
    *lh = (uint16_t)(2 * r);          // alto del bounding box = diametro
    *lx = (uint16_t)(cx4[d] - r);     // esquina izquierda del bounding box = centro menos el radio
    *ly = (uint16_t)(cy4[d] - r);     // esquina superior del bounding box = centro menos el radio
}

/* Flecha triangular en coordenadas LOCALES, igual construccion que
 * sj1p_dibujar_flecha (FillRect apiladas) pero cada rectangulo pasa por
 * Cockpit_FillRect -- asi "arriba" en local siempre sale como "arriba" para
 * ESE jugador ya rotado, sin duplicar la logica de la flecha en si. */
static void cockpit_dibujar_flecha(uint8_t jugador, uint16_t lbx, uint16_t lby, uint16_t lbw, uint16_t lbh,   // dibuja UNA flecha triangular del D-pad, orientada segun su direccion
                                    uint8_t dir, uint16_t color) {
    int16_t cx   = (int16_t)(lbx + lbw / 2);              // centro x local de la caja del D-pad
    int16_t cy   = (int16_t)(lby + lbh / 2);              // centro y local de la caja
    int16_t size = (int16_t)((lbw < lbh ? lbw : lbh) / 2) - 5;  // "radio" de la flecha, un poco mas chico que la caja para dejar margen
    if (size < 7) size = 7;                                // tamaño minimo de flecha, para que no desaparezca en cajas muy chicas
    /* La mitad de la base crece 1.5 veces mas rapido que el alto (en vez de
     * 1:1, que equivaldria a 45 grados), dando un triangulo corto y ancho de
     * estilo arcade en vez de una flecha fina. Se acota al radio de la caja
     * para que la base no se salga del boton. */
    int16_t ancho_max = (int16_t)(size * 3 / 2);           // ancho maximo de la base del triangulo -- subir el "3/2" hace la flecha mas ancha/corta, bajarlo la hace mas fina/larga
    int16_t max_ancho = (int16_t)((lbw < lbh ? lbw : lbh) / 2) - 2;  // limite duro para que la base no se salga del boton
    if (ancho_max > max_ancho) ancho_max = max_ancho;        // recorta si hiciera falta
    int16_t tallo = (int16_t)(size / 3) + 2;                 // grosor del "palito" de la flecha (la parte rectangular detras de la punta triangular)

    switch (dir) {
    case 0:  /* ARRIBA (local) */
        for (int16_t i = 0; i <= size; i++) {                // recorre la punta triangular de abajo hacia arriba, fila por fila
            int16_t hw = (int16_t)(ancho_max * i / size);      // medio-ancho de esta fila -- crece linealmente desde 0 (punta) hasta ancho_max (base)
            Cockpit_FillRect(jugador, (uint16_t)(cx - hw), (uint16_t)(cy - size + i), (uint16_t)(2 * hw + 1), 1, color);  // una franja horizontal de 1px de alto por fila, transformada por Cockpit_FillRect segun el jugador
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - tallo / 2), (uint16_t)cy, (uint16_t)tallo, (uint16_t)(size / 2), color);  // palito rectangular debajo de la punta, apuntando hacia arriba
        break;
    case 1:  /* ABAJO */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            Cockpit_FillRect(jugador, (uint16_t)(cx - hw), (uint16_t)(cy + size - i), (uint16_t)(2 * hw + 1), 1, color);  // misma construccion que ARRIBA pero espejada en y (punta hacia abajo)
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - tallo / 2), (uint16_t)(cy - size / 2), (uint16_t)tallo, (uint16_t)(size / 2), color);  // palito arriba de la punta
        break;
    case 2:  /* IZQ */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            Cockpit_FillRect(jugador, (uint16_t)(cx - size + i), (uint16_t)(cy - hw), 1, (uint16_t)(2 * hw + 1), color);  // igual que ARRIBA pero con ejes x/y intercambiados (franjas verticales, no horizontales) -- apunta a la izquierda
        }
        Cockpit_FillRect(jugador, (uint16_t)cx, (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);  // palito a la derecha de la punta
        break;
    default: /* DER */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            Cockpit_FillRect(jugador, (uint16_t)(cx + size - i), (uint16_t)(cy - hw), 1, (uint16_t)(2 * hw + 1), color);  // espejo de IZQ en x -- apunta a la derecha
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - size / 2), (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);  // palito a la izquierda de la punta
        break;
    }
}

static void cockpit_pad_draw(uint8_t jugador, uint8_t d, uint8_t activo) {   // dibuja UNA flecha del D-pad de Simon+Joystick (2 jugadores), encendida o apagada
    uint16_t lx, ly, lw, lh;
    cockpit_pad_rect(d, &lx, &ly, &lw, &lh);                // bounding box local de este badge de direccion

    int16_t lcx = (int16_t)(lx + lw / 2);                   // centro x local
    int16_t lcy = (int16_t)(ly + lh / 2);                   // centro y local
    int16_t r   = (int16_t)(lw / 2);                        // radio del badge circular
    uint8_t  cidx       = SJ_DPAD_COLOR_IDX_P0[d];  // los 2 jugadores usan la tabla con IZQ/DER intercambiados (ver comentario de SJ_DPAD_COLOR_IDX_P0)
    uint16_t badge_col  = activo ? NOTE_COLOR[cidx] : LANE_COLOR[cidx];  // color del circulo de fondo: brillante si es el paso activo, apagado si no
    uint16_t flecha_col = activo ? COLOR_BLACK      : NOTE_COLOR[cidx];  // color de la flecha: negra sobre badge brillante (contraste), o del color brillante sobre badge apagado

    int16_t acx, acy;
    Cockpit_Punto(jugador, lcx, lcy, &acx, &acy);            // centro absoluto en pantalla, ya rotado/desplazado segun el jugador
    ILI9341_FillCircle(acx, acy, r, badge_col);              // circulo de fondo del badge
    ILI9341_DrawCircle(acx, acy, r, COLOR_DARKGRAY);  /* delinea el badge */

    /* La direccion de la flecha no recibe ninguna compensacion adicional de
     * espejado. El texto si necesita draw_string_180 porque debe seguir
     * leyendose igual (una convencion de lectura humana fija) a pesar de la
     * rotacion de 180 grados; en cambio, una flecha que apunta "hacia
     * afuera" del D-pad es rotacionalmente consistente por si sola: al dejar
     * que la misma transformacion de Cockpit_FillRect rote posicion y forma
     * juntas, sin espejado extra, la flecha continua apuntando hacia afuera
     * del D-pad para cualquiera de los 2 jugadores. */
    cockpit_dibujar_flecha(jugador, lx, ly, lw, lh, d, flecha_col);  // flecha encima del badge, apuntando hacia afuera del D-pad
}

/* Redibuja TODO un jugador (etiqueta + 4 badges) sin tocar al otro --
 * usado tanto para el dibujo inicial completo como para reiniciar solo un
 * jugador que perdio, sin borrar la partida en curso del otro. */
static void cockpit_draw_jugador(uint8_t jugador, uint8_t paso) {   // redibuja toda la mitad de un jugador de Simon+Joystick (nombre + D-pad completo)
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador
    Cockpit_DrawString(jugador, 8, 4, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 1);  // nombre del jugador en su color
    for (uint8_t d = 0; d < 4; d++) cockpit_pad_draw(jugador, d, paso == d);  // dibuja las 4 direcciones; la que coincide con "paso" se dibuja encendida
}

void Renderer_DrawModoSimonJoystick2P(uint8_t paso_j1, uint8_t paso_j2) {   // dibuja las 2 mitades completas de Simon+Joystick (2 jugadores) desde cero
    ILI9341_FillScreen(COLOR_BLACK);                                          // pantalla nueva desde cero
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY);  // franja divisoria entre las 2 mitades
    cockpit_draw_jugador(0, paso_j1);                                          // dibuja jugador logico 0
    cockpit_draw_jugador(1, paso_j2);                                          // dibuja jugador logico 1
}

void Renderer_DrawModoSimonJoystick2PJugador(uint8_t jugador, uint8_t paso) {   // redibuja SOLO la mitad de un jugador (2 jugadores), sin tocar al otro
    cockpit_draw_jugador(jugador, paso);  // redibuja solo la mitad de UN jugador
}

void Renderer_UpdateModoSimonJoystick2PPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso) {   // redibuja solo la flecha del D-pad que cambio de estado
    if (paso_ant == paso) return;                             // sin cambio de paso activo, nada que redibujar
    if (paso_ant < 4) cockpit_pad_draw(jugador, paso_ant, 0);   // apaga la direccion que estaba activa
    if (paso     < 4) cockpit_pad_draw(jugador, paso, 1);       // enciende la direccion nueva
}

static uint16_t cockpit_racha_dibujada[2] = { 0xFFFF, 0xFFFF };  // ultima racha dibujada por jugador, 0xFFFF = ninguna todavia

void Renderer_ActualizarRachaJoystick2P(uint8_t jugador, uint16_t racha) {   // redibuja solo el numero de ronda de ese jugador (2 jugadores)
    if (racha == cockpit_racha_dibujada[jugador]) return;  // ya dibujada, no repetir
    char buf[10];
    snprintf(buf, sizeof(buf), "R:%u", (unsigned)racha);    // formatea "R:<numero>"
    Texto_Sanear(buf);
    Cockpit_FillRect(jugador, 186, 2, 50, 14, COLOR_BLACK);  // borra el rincon del contador
    Cockpit_DrawString(jugador, 188, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);  // dibuja el numero nuevo
    cockpit_racha_dibujada[jugador] = racha;                 // recuerda la ultima racha dibujada
}

void Renderer_DibujarGameOverJoystick2P(uint8_t jugador, uint16_t racha, uint16_t mejor) {   // dibuja la pantalla de GAME OVER en la mitad de ese jugador (2 jugadores)
    char linea[20];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador
    Cockpit_DrawString(jugador, 50, 35, "GAME OVER", COLOR_RED, COLOR_BLACK, 2);  // titulo grande en rojo
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)racha);  // racha de esta partida
    Texto_Sanear(linea);
    Cockpit_DrawString(jugador, 70, 70, linea, COLOR_WHITE, COLOR_BLACK, 1);
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)mejor);   // mejor racha historica
    Texto_Sanear(linea);
    Cockpit_DrawString(jugador, 70, 90, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 18, 120, "mueve=reintentar", COLOR_GRAY, COLOR_BLACK, 1);  // instruccion de reintento (mover el propio joystick, no un boton dedicado)
    cockpit_racha_dibujada[jugador] = 0xFFFF;   /* fuerza redibujo al reiniciar */
}

/* ========================================================================== */
/* === GUITAR HERO — CARA A CARA (portrait, cockpit) ========================= */
/* ========================================================================== */
/* Mismo mecanismo cara-a-cara (portrait, Cockpit_*) que Simon Clasico y
 * Simon+Joystick arriba. Restyle inspirado en el codigo de referencia FPGA
 * (video_box/video_ellipse, ver guitar_hero/ANALISIS_REFERENCIA.md): en vez
 * de un carril solido y una zona de golpe rectangular (como la maqueta
 * original en paisaje, DEMO_JUGANDO), cada carril es una barra angosta (2
 * lineas finas de color sobre fondo negro) y la zona de golpe es un circulo
 * relleno cerca del borde izquierdo LOCAL -- las notas nacen en el borde
 * derecho LOCAL (lejos de la zona) y viajan hacia la izquierda, como un
 * juego de ritmo real (la maqueta original viajaba al reves, sin razon de
 * diseño, solo para probar la matematica de distancia). */

#define GH_LANE_Y0     18     /* debajo de la etiqueta J1/J2 + puntaje       */
#define GH_LANE_H      32  // alto de cada uno de los 4 carriles -- agrandarlo separa mas las notas verticalmente
#define GH_SEP_H       2   // separacion entre carriles consecutivos
#define GH_LANE_TOP(n) ((uint16_t)(GH_LANE_Y0 + (n) * (GH_LANE_H + GH_SEP_H)))  // y de arranque del carril "n": apila carriles de alto+separacion desde GH_LANE_Y0
/* GH_ZONA_R y GH_NOTE_R (radio de la zona de golpe y de las notas) ya estan
 * definidos en renderer.h -- expuestos ahi para que main.c pueda calcular
 * GH_HIT_OK_2P sin duplicar estos numeros en 2 archivos. */

static inline uint16_t gh_note_cy(uint8_t carril) {   // y (centro) de un carril de Guitar Hero 2 jugadores
    return (uint16_t)(GH_LANE_TOP(carril) + GH_LANE_H / 2);  // centro y del carril = borde superior + mitad del alto
}

/* Bandera de flash de impacto pendiente de revertir -- declarada aca arriba
 * porque tanto gh_draw_jugador (limpia banderas viejas al redibujar el
 * jugador desde cero) como Renderer_GH_EraseNotaTrail (no pisa un flash
 * recien pintado) necesitan consultarla; Renderer_GH_FlashZona/
 * ActualizarFlashes, definidas mas abajo, son las que la ponen/limpian en
 * el uso normal. */
static uint8_t gh_flash_pendiente[2][4];

/* Zona de golpe estilo "blanco/diana": anillo exterior del color de carril
 * (NOTE_COLOR, el mismo tono brillante de las notas) + centro del tono
 * "presionado" (PRESS_COLOR). Factorizada aparte porque se repinta desde 2
 * lugares (dibujo inicial del carril y el borrado delta de notas cuando la
 * franja borrada invade la zona). */
static void gh_draw_zona(uint8_t jugador, uint8_t c, uint16_t ly) {   // dibuja la zona de golpe circular (anillo + centro) de un carril
    int16_t acx, acy;
    Cockpit_Punto(jugador, GH_ZONA_CX, (int16_t)(ly + GH_LANE_H / 2), &acx, &acy);  // centro de la zona de golpe transformado al jugador correspondiente
    ILI9341_FillCircle2(acx, acy, GH_ZONA_R, NOTE_COLOR[c], GH_ZONA_R - 4, PRESS_COLOR[c]);   // anillo + centro "presionado" en 1 solo pase de SPI
}

static void gh_draw_carril(uint8_t jugador, uint8_t c) {   // dibuja UN carril completo (fondo + bordes + zona de golpe)
    uint16_t ly = GH_LANE_TOP(c);                                              // y de arranque de este carril
    Cockpit_FillRect(jugador, 0, ly, COCK_ZONE_W, GH_LANE_H, COLOR_BLACK);       // fondo negro de todo el carril
    Cockpit_FillRect(jugador, 0, ly, COCK_ZONE_W, 1, NOTE_COLOR[c]);              // linea fina de color en el borde superior del carril
    Cockpit_FillRect(jugador, 0, (uint16_t)(ly + GH_LANE_H - 1), COCK_ZONE_W, 1, NOTE_COLOR[c]);  // linea fina de color en el borde inferior del carril
    gh_draw_zona(jugador, c, ly);                                                // zona de golpe circular de este carril
}

static void gh_draw_jugador(uint8_t jugador) {   // redibuja toda la mitad de un jugador de Guitar Hero (nombre + los 4 carriles)
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador
    Cockpit_DrawString(jugador, 8, 4, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 1);  // nombre del jugador
    for (uint8_t c = 0; c < 4; c++) gh_draw_carril(jugador, c);  // dibuja los 4 carriles

    /* Esta pantalla se redibuja desde cero al arrancar o reiniciar una
     * ronda. Si quedaba una bandera de flash pendiente de una ronda
     * anterior (por ejemplo, el ultimo golpe de la ronda coincidio con el
     * mismo tick que "RONDA COMPLETA"), se limpia aca para que
     * Renderer_GH_ActualizarFlashes no dibuje un circulo residual encima de
     * esta pantalla nueva. */
    for (uint8_t c = 0; c < 4; c++) gh_flash_pendiente[jugador][c] = 0;  // limpia banderas de flash de una ronda anterior, para no arrastrar un flash residual a la pantalla nueva
}

void Renderer_DrawModoGuitarHero2P(void) {   // dibuja las 2 mitades completas de Guitar Hero (2 jugadores) desde cero
    ILI9341_FillScreen(COLOR_BLACK);                                          // pantalla nueva desde cero
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY);  // franja divisoria entre las 2 mitades
    gh_draw_jugador(0);  // dibuja jugador logico 0
    gh_draw_jugador(1);  // dibuja jugador logico 1
}

void Renderer_DrawModoGuitarHeroJugador(uint8_t jugador) {   // redibuja SOLO la mitad de un jugador (usado tanto para el dibujo inicial 1P como para reiniciar una ronda)
    gh_draw_jugador(jugador);  // redibuja solo la mitad de UN jugador
}

static uint16_t gh_puntaje_dibujado[2] = { 0xFFFF, 0xFFFF };  // ultimo puntaje ya dibujado por jugador (0xFFFF = ninguno todavia)
static uint16_t gh_combo_dibujado[2]   = { 0xFFFF, 0xFFFF };  // ultimo combo ya dibujado por jugador

void Renderer_GH_ActualizarPuntaje(uint8_t jugador, uint16_t puntaje, uint16_t combo) {   // redibuja solo el texto de puntaje/combo de ese jugador, si cambio
    if (puntaje == gh_puntaje_dibujado[jugador] && combo == gh_combo_dibujado[jugador]) return;  // ninguno de los 2 valores cambio, no hay nada que redibujar
    char buf[16];
    snprintf(buf, sizeof(buf), "P:%u C:%u", (unsigned)puntaje, (unsigned)combo);  // formatea "P:<puntaje> C:<combo>"
    Texto_Sanear(buf);
    Cockpit_FillRect(jugador, 130, 2, 106, 14, COLOR_BLACK);                       // borra la franja donde va el texto de puntaje/combo
    Cockpit_DrawString(jugador, 132, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);        // dibuja el texto nuevo
    gh_puntaje_dibujado[jugador] = puntaje;   // recuerda el puntaje ya dibujado
    gh_combo_dibujado[jugador]   = combo;      // recuerda el combo ya dibujado
}

/* Ficha circular: cuerpo del color de carril + un nucleo brillante blanco
 * centrado (radio/3) para dar efecto de relieve, mismo estilo que el boton
 * arcade circular de Simon Clasico. El recorte de bordes (nota que asoma
 * apenas por el borde de la pantalla) se calcula en coordenadas LOCALES
 * (antes de pasar por Cockpit_Punto), porque un circulo es rotacionalmente
 * simetrico: solo su CENTRO necesita pasar por la transformacion de
 * espejo, no su forma. */
void Renderer_GH_DrawNota(uint8_t jugador, const Nota_t *nota) {   // dibuja UNA nota circular en su posicion actual
    if (!nota->activa) return;                                   // nota inactiva (ya golpeada o sin spawnear) -- no dibujar nada
    int16_t ccx = (int16_t)(nota->x_rel + NOTE_W / 2);            // centro x local de la nota (su x_rel es la esquina, se le suma medio ancho)
    if (ccx + (int16_t)GH_NOTE_R <= 0 || ccx - (int16_t)GH_NOTE_R >= (int16_t)COCK_ZONE_W) return;  // la nota esta completamente fuera de la zona visible -- no dibujar (evita trabajo de SPI de sobra)

    int16_t acx, acy;
    Cockpit_Punto(jugador, ccx, (int16_t)gh_note_cy(nota->carril), &acx, &acy);  // centro transformado a coordenadas absolutas de pantalla
    ILI9341_FillCircle2(acx, acy, GH_NOTE_R, NOTE_COLOR[nota->carril], GH_NOTE_R / 3, COLOR_WHITE);  // cuerpo + nucleo blanco en 1 solo pase de SPI (menos trabajo por nota, importa con varias notas en pantalla a 2 jugadores)
}

/* Borrado DELTA por caja delimitadora (bounding box), no por arco circular
 * -- el driver ILI9341 de este proyecto no tiene primitiva de segmento de
 * circulo, asi que se borra el rectangulo trasero que la ficha circular deja
 * al moverse.
 *
 * El calculo del bounding box parte del CENTRO real de la ficha (ccx_prev)
 * y de su radio real (GH_NOTE_R), incluyendo el borde derecho inclusive del
 * circulo: ILI9341_FillCircle rellena filas de -r a +r inclusive (ver
 * Src/ili9341.c), por lo que el bounding box real de la ficha mide
 * (2*GH_NOTE_R+1) x (2*GH_NOTE_R+1) pixeles, no 2*GH_NOTE_R -- una formula
 * basada solo en el diametro (2*GH_NOTE_R) dejaria sin borrar la columna o
 * fila mas externa del circulo en cada movimiento, generando un rastro fino
 * a lo largo de todo el recorrido de la nota. */
void Renderer_GH_EraseNotaTrail(uint8_t jugador, const Nota_t *nota, uint8_t speed) {   // borra solo la franja de pantalla que la nota dejo atras al moverse (no toda la pantalla)
    if (!nota->activa) return;  // nota inactiva -- no habia nada dibujado, nada que borrar

    /* la nota viaja hacia IZQUIERDA (x decreciente) -- la franja que queda
     * al descubierto es el borde DERECHO (inclusive) de la posicion
     * anterior, un ancho `speed` hacia atras desde ese borde. */
    int16_t ccx_prev = nota->x_prev + (int16_t)NOTE_W / 2;                 // centro x local de la nota en su posicion ANTERIOR (antes de moverse este tick)
    int16_t ex_end    = (int16_t)(ccx_prev + (int16_t)GH_NOTE_R + 1);       // borde derecho (inclusive) del circulo en su posicion anterior
    int16_t ex_start  = (int16_t)(ex_end - (int16_t)speed);                 // borde izquierdo de la franja a borrar = borde derecho menos lo que avanzo la nota este tick
    if (ex_start < 0) ex_start = 0;                                          // recorta contra el borde izquierdo de la zona
    if (ex_end > (int16_t)COCK_ZONE_W) ex_end = (int16_t)COCK_ZONE_W;        // recorta contra el borde derecho de la zona
    int16_t ew = ex_end - ex_start;                                          // ancho final de la franja a borrar
    if (ew <= 0) return;                                                     // nada que borrar (la nota estaba fuera de la zona visible)

    uint16_t ly = (uint16_t)(gh_note_cy(nota->carril) - GH_NOTE_R);          // borde superior del bounding box de la nota en su carril
    Cockpit_FillRect(jugador, (uint16_t)ex_start, ly, (uint16_t)ew, (uint16_t)(2 * GH_NOTE_R + 1), COLOR_BLACK);  // pinta de negro la franja que la nota dejo atras

    /* si la franja borrada se mete en el area de la zona de golpe circular,
     * redibujar la zona completa (anillo+centro) encima -- un FillRect no
     * sabe recortar un circulo, asi que se repinta entera (barato) en vez
     * de intentar una interseccion rectangulo/circulo exacta.
     *
     * Si esta pendiente un flash blanco de impacto en ESTE carril (ver
     * Renderer_GH_FlashZona mas abajo), no se lo pisa con la zona normal:
     * dentro del mismo tick que un golpe puede llegar otra nota del mismo
     * carril cruzando la zona, y repintar la zona normal encima taparia el
     * blanco del flash antes de que se alcance a ver un solo frame.
     * Renderer_GH_ActualizarFlashes se encarga de revertir el flash despues. */
    int16_t zona_x0 = (int16_t)GH_ZONA_CX - (int16_t)GH_ZONA_R;  // borde izquierdo del bounding box de la zona de golpe
    int16_t zona_x1 = (int16_t)GH_ZONA_CX + (int16_t)GH_ZONA_R;  // borde derecho del bounding box de la zona
    if (ex_start < zona_x1 && ex_end > zona_x0 && !gh_flash_pendiente[jugador][nota->carril]) {  // la franja borrada se solapa con la zona Y no hay un flash blanco pendiente en este carril
        gh_draw_zona(jugador, nota->carril, GH_LANE_TOP(nota->carril));  // repinta la zona completa encima de lo que se acaba de borrar
    }
}

/* Efecto de impacto: la zona de golpe circular parpadea blanco brillante
 * por 1 frame cuando el jugador acierta. Renderer_GH_FlashZona la pinta
 * blanca y marca la bandera;
 * Renderer_GH_ActualizarFlashes (llamada 1 vez por jugador al inicio de
 * cada tick del loop principal, ~33ms) la revierte a su estilo normal de
 * "blanco/diana" en la iteracion siguiente -- eso da exactamente 1 frame
 * de blanco, sin necesitar un timer aparte. */

void Renderer_GH_FlashZona(uint8_t jugador, uint8_t carril) {   // pinta la zona de golpe de blanco brillante por 1 frame (efecto de impacto al acertar)
    int16_t acx, acy;
    Cockpit_Punto(jugador, GH_ZONA_CX, (int16_t)(GH_LANE_TOP(carril) + GH_LANE_H / 2), &acx, &acy);  // centro de la zona de este carril, transformado a pantalla
    ILI9341_FillCircle(acx, acy, GH_ZONA_R, COLOR_WHITE);  // pinta la zona entera de blanco brillante (efecto de flash)
    gh_flash_pendiente[jugador][carril] = 1;                // marca que hay que revertir este flash en el proximo tick
}

void Renderer_GH_ActualizarFlashes(uint8_t jugador) {   // revierte a estilo normal cualquier zona que quedo en blanco del frame anterior
    for (uint8_t c = 0; c < 4; c++) {                        // recorre los 4 carriles de este jugador
        if (!gh_flash_pendiente[jugador][c]) continue;        // este carril no tiene flash pendiente, seguir con el siguiente
        gh_draw_zona(jugador, c, GH_LANE_TOP(c));               // repinta la zona con su estilo normal (anillo+centro), tapando el blanco
        gh_flash_pendiente[jugador][c] = 0;                      // ya se revirtio, baja la bandera
    }
}

void Renderer_GH_DibujarFin(uint8_t jugador, uint16_t puntaje) {   // dibuja "RONDA COMPLETA" con el puntaje final en la mitad de ese jugador
    char linea[24];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);  // borra toda la mitad de este jugador
    Cockpit_DrawString(jugador, 30, 40, "RONDA COMPLETA", COLOR_GREEN, COLOR_BLACK, 1);  // titulo de fin de ronda
    snprintf(linea, sizeof(linea), "Puntaje: %u", (unsigned)puntaje);  // formatea el puntaje final
    Texto_Sanear(linea);
    Cockpit_DrawString(jugador, 60, 70, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 20, 93,  "V/A/AM=repetir",       COLOR_GRAY, COLOR_BLACK, 1);  // instruccion de reintento
    Cockpit_DrawString(jugador, 20, 106, "ROJO 2s=menu",         COLOR_GRAY, COLOR_BLACK, 1);  // atajo de salida (ver SalirGameOver1P/2P_Detectado en main.c)
    Cockpit_DrawString(jugador, 20, 119, "VERDE 2s=cancion",     COLOR_GRAY, COLOR_BLACK, 1);  // atajo de cambio de cancion (ver GH_BotonSostenidoDetectado en main.c)
    Cockpit_DrawString(jugador, 20, 132, "AZUL 2s=dificultad",   COLOR_GRAY, COLOR_BLACK, 1);  // atajo de cambio de dificultad
    gh_puntaje_dibujado[jugador] = 0xFFFF;  // fuerza que el proximo Renderer_GH_ActualizarPuntaje redibuje (aunque el valor "coincida" con el de la ronda anterior)
    gh_combo_dibujado[jugador]   = 0xFFFF;

    /* Mismo motivo que en gh_draw_jugador: esta pantalla puede dibujarse en
     * el mismo tick que el ultimo acierto de la ronda, dejando una bandera
     * de flash pendiente que de otro modo pintaria un circulo residual
     * encima en el siguiente tick. */
    for (uint8_t c = 0; c < 4; c++) gh_flash_pendiente[jugador][c] = 0;
}

/* ========================================================================== */
/* === GUITAR HERO — PANTALLA COMPLETA PARA 1 SOLO JUGADOR (retrato 240x320) =
 * Mismo mecanismo de carriles/notas/zona de golpe que las funciones
 * Renderer_GH_* de arriba, pero dibujado DIRECTO en coordenadas absolutas de
 * pantalla (sin Cockpit_*, el modo de 1 jugador nunca rota) y con carriles
 * mucho mas altos al no compartir la pantalla con otro jugador. La geometria
 * HORIZONTAL (GH_ZONA_CX, COCKPIT_ZONE_W, NOTE_W, en renderer.h) es la MISMA
 * que en el modo de 2 jugadores -- main.c la usa igual para ambos modos en
 * GuitarHero_ActualizarJugador, asi que el spawn/movimiento/deteccion de
 * golpe de las notas no necesita saber si esta jugando 1 o 2 jugadores.
 * Unicamente cambia la geometria VERTICAL (alto de carril) y el radio de
 * zona/nota, mas grandes aca al sobrar espacio de pantalla. */
#define GH1P_LANE_Y0     46   // debajo de la franja de titulo (26px) + fila de puntaje (20px)
#define GH1P_LANE_H      60   // alto de cada carril -- mucho mas alto que GH_LANE_H (32) del modo de 2 jugadores
#define GH1P_SEP_H       6    // separacion entre carriles consecutivos
#define GH1P_LANE_TOP(n) ((uint16_t)(GH1P_LANE_Y0 + (n) * (GH1P_LANE_H + GH1P_SEP_H)))  // y de arranque del carril "n"
/* GH1P_ZONA_R y GH1P_NOTE_R ya estan definidos en renderer.h -- expuestos
 * ahi para que main.c calcule GH_HIT_OK_1P sin duplicar estos numeros. */

static inline uint16_t gh1p_note_cy(uint8_t carril) {   // y (centro) de un carril de Guitar Hero a pantalla completa (1 jugador)
    return (uint16_t)(GH1P_LANE_TOP(carril) + GH1P_LANE_H / 2);  // centro y del carril
}

/* Misma bandera de flash pendiente que gh_flash_pendiente[2][4], pero para
 * un solo jugador (sin dimension de jugador). */
static uint8_t gh1p_flash_pendiente[4];

static void gh1p_draw_zona(uint8_t c, uint16_t ly) {   // zona de golpe circular de un carril, layout de pantalla completa
    int16_t acx = (int16_t)GH_ZONA_CX;                 // x absoluta = local (el modo de 1 jugador nunca rota ni desplaza en x)
    int16_t acy = (int16_t)(ly + GH1P_LANE_H / 2);      // y absoluta = local (idem)
    ILI9341_FillCircle2(acx, acy, GH1P_ZONA_R, NOTE_COLOR[c], GH1P_ZONA_R - 6, PRESS_COLOR[c]);   // anillo + centro "presionado" en 1 solo pase de SPI
}

static void gh1p_draw_carril(uint8_t c) {   // dibuja UN carril completo del layout de pantalla completa
    uint16_t ly = GH1P_LANE_TOP(c);
    ILI9341_FillRect(0, ly, COCKPIT_ZONE_W, GH1P_LANE_H, COLOR_BLACK);                          // fondo negro del carril
    ILI9341_FillRect(0, ly, COCKPIT_ZONE_W, 1, NOTE_COLOR[c]);                                    // linea fina de color arriba
    ILI9341_FillRect(0, (uint16_t)(ly + GH1P_LANE_H - 1), COCKPIT_ZONE_W, 1, NOTE_COLOR[c]);      // linea fina de color abajo
    gh1p_draw_zona(c, ly);
}

void Renderer_DrawModoGuitarHero1P(void) {   // dibuja la pantalla completa de Guitar Hero para 1 jugador desde cero
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_FillRect(0, 0, COCKPIT_ZONE_W, 26, COLOR_DARKGRAY);   // franja de titulo, igual estilo que los otros modos 1P
    draw_string_c(COCKPIT_ZONE_W / 2, 9, "GUITAR HERO", COLOR_WHITE, COLOR_DARKGRAY, 1);   // titulo centrado (ancho de RETRATO, no LCD_W que es paisaje)
    for (uint8_t c = 0; c < 4; c++) gh1p_draw_carril(c);
    for (uint8_t c = 0; c < 4; c++) gh1p_flash_pendiente[c] = 0;   // limpia flashes pendientes de una ronda anterior, ver gh_draw_jugador
}

static uint16_t gh1p_puntaje_dibujado = 0xFFFF;  // 0xFFFF fuerza el primer dibujo
static uint16_t gh1p_combo_dibujado   = 0xFFFF;

void Renderer_GH1P_ActualizarPuntaje(uint16_t puntaje, uint16_t combo) {   // redibuja solo el texto de puntaje/combo, si cambio (pantalla completa)
    if (puntaje == gh1p_puntaje_dibujado && combo == gh1p_combo_dibujado) return;   // sin cambios, no repetir el trabajo
    char buf[28];
    snprintf(buf, sizeof(buf), "PUNTAJE:%u  COMBO:%u", (unsigned)puntaje, (unsigned)combo);   // arma el texto combinado
    Texto_Sanear(buf);   // limpia cualquier byte no imprimible antes de dibujarlo
    ILI9341_FillRect(0, 28, COCKPIT_ZONE_W, 14, COLOR_BLACK);           // borra la fila de puntaje antes de redibujar
    draw_string_c(COCKPIT_ZONE_W / 2, 29, buf, COLOR_YELLOW, COLOR_BLACK, 1);  // centrado, debajo de la franja de titulo
    gh1p_puntaje_dibujado = puntaje;
    gh1p_combo_dibujado   = combo;
}

void Renderer_GH1P_DrawNota(const Nota_t *nota) {   // dibuja UNA nota circular en su posicion actual (pantalla completa)
    if (!nota->activa) return;
    int16_t ccx = (int16_t)(nota->x_rel + NOTE_W / 2);
    if (ccx + (int16_t)GH1P_NOTE_R <= 0 || ccx - (int16_t)GH1P_NOTE_R >= (int16_t)COCKPIT_ZONE_W) return;  // fuera de la zona visible
    int16_t acy = (int16_t)gh1p_note_cy(nota->carril);
    ILI9341_FillCircle2(ccx, acy, GH1P_NOTE_R, NOTE_COLOR[nota->carril], GH1P_NOTE_R / 3, COLOR_WHITE);  // cuerpo + nucleo en 1 solo pase de SPI
}

void Renderer_GH1P_EraseNotaTrail(const Nota_t *nota, uint8_t speed) {   // borra la franja que la nota dejo atras (pantalla completa)
    if (!nota->activa) return;   // nota inactiva, no habia nada dibujado
    int16_t ccx_prev = nota->x_prev + (int16_t)NOTE_W / 2;   // centro x anterior de la nota
    int16_t ex_end    = (int16_t)(ccx_prev + (int16_t)GH1P_NOTE_R + 1);   // borde derecho de la franja a borrar
    int16_t ex_start  = (int16_t)(ex_end - (int16_t)speed);   // borde izquierdo = borde derecho menos lo que avanzo este tick
    if (ex_start < 0) ex_start = 0;   // recorta contra el borde izquierdo de pantalla
    if (ex_end > (int16_t)COCKPIT_ZONE_W) ex_end = (int16_t)COCKPIT_ZONE_W;   // recorta contra el borde derecho
    int16_t ew = ex_end - ex_start;   // ancho final de la franja a borrar
    if (ew <= 0) return;   // nada que borrar tras recortar

    uint16_t ly = (uint16_t)(gh1p_note_cy(nota->carril) - GH1P_NOTE_R);
    ILI9341_FillRect((uint16_t)ex_start, ly, (uint16_t)ew, (uint16_t)(2 * GH1P_NOTE_R + 1), COLOR_BLACK);

    int16_t zona_x0 = (int16_t)GH_ZONA_CX - (int16_t)GH1P_ZONA_R;
    int16_t zona_x1 = (int16_t)GH_ZONA_CX + (int16_t)GH1P_ZONA_R;
    if (ex_start < zona_x1 && ex_end > zona_x0 && !gh1p_flash_pendiente[nota->carril]) {
        gh1p_draw_zona(nota->carril, GH1P_LANE_TOP(nota->carril));   // repinta la zona completa encima de lo que se acaba de borrar
    }
}

void Renderer_GH1P_FlashZona(uint8_t carril) {   // flash blanco de impacto (pantalla completa)
    int16_t acx = (int16_t)GH_ZONA_CX;
    int16_t acy = (int16_t)(GH1P_LANE_TOP(carril) + GH1P_LANE_H / 2);
    ILI9341_FillCircle(acx, acy, GH1P_ZONA_R, COLOR_WHITE);   // flash blanco de impacto
    gh1p_flash_pendiente[carril] = 1;
}

void Renderer_GH1P_ActualizarFlashes(void) {   // revierte los flashes pendientes del frame anterior (pantalla completa)
    for (uint8_t c = 0; c < 4; c++) {
        if (!gh1p_flash_pendiente[c]) continue;
        gh1p_draw_zona(c, GH1P_LANE_TOP(c));   // revierte al estilo normal (anillo+centro)
        gh1p_flash_pendiente[c] = 0;
    }
}

void Renderer_GH1P_DibujarFin(uint16_t puntaje) {   // dibuja "RONDA COMPLETA" a pantalla completa
    char linea[24];

    /* NO tocar la orientacion aca: el modo 1 jugador se mantiene en retrato
     * (240x320) durante TODA la partida, desde GuitarHero_IniciarSolo hasta
     * el reintento -- esta pantalla es solo el final de una ronda, no un
     * cambio de modo. Forzar paisaje (SetPortrait(0)) aca giraba la
     * pantalla fisica a mitad de partida sin motivo. */
    ILI9341_FillScreen(COLOR_BLACK);

    draw_string_c(COCKPIT_ZONE_W / 2, 50, "RONDA COMPLETA", COLOR_GREEN, COLOR_BLACK, 2);

    snprintf(linea, sizeof(linea), "Puntaje: %u", (unsigned)puntaje);
    Texto_Sanear(linea);
    draw_string_c(COCKPIT_ZONE_W / 2, 110, linea, COLOR_YELLOW, COLOR_BLACK, 2);

    draw_string_c(COCKPIT_ZONE_W / 2, 170, "V/A/AM=jugar de nuevo", COLOR_GRAY, COLOR_BLACK, 1);   // pista: cualquier boton propio reintenta
    draw_string_c(COCKPIT_ZONE_W / 2, 185, "ROJO 2s=menu", COLOR_GRAY, COLOR_BLACK, 1);              // pista: atajo de salida
    draw_string_c(COCKPIT_ZONE_W / 2, 200, "VERDE 2s=cancion", COLOR_GRAY, COLOR_BLACK, 1);           // pista: atajo de cambio de cancion
    draw_string_c(COCKPIT_ZONE_W / 2, 215, "AZUL 2s=dificultad", COLOR_GRAY, COLOR_BLACK, 1);         // pista: atajo de cambio de dificultad

    gh1p_puntaje_dibujado = 0xFFFF;   // fuerza redibujo al reiniciar
    gh1p_combo_dibujado   = 0xFFFF;
    for (uint8_t c = 0; c < 4; c++) gh1p_flash_pendiente[c] = 0;
}

/* ========================================================================== */
/* === SIMON+JOYSTICK — PANTALLA COMPLETA PARA 1 SOLO JUGADOR ================ */
/* ========================================================================== */
/* Solo existe la version de 1 jugador -- flechas grandes centradas, usando
 * toda la pantalla. La version anterior, dividida en 2 mitades con texto
 * ARRIBA/ABAJO/IZQ/DER, se elimino por quedar sin uso: tanto el juego real
 * como la vista previa del recorrido de diseño utilizan esta version de 1
 * jugador. */

static void sj1p_pad_rect(uint8_t d, uint16_t *bx, uint16_t *by, uint16_t *bw, uint16_t *bh) {   // bounding box de la flecha `d` del D-pad de Simon+Joystick 1 jugador
    /* ARRIBA/ABAJO un poco mas altas (84x68), IZQ/DER un poco mas angostas
     * (84x60) para que quepan en el hueco vertical entre las otras dos sin
     * traslaparse -- todas mas grandes que antes (74x60). */
    uint16_t xw = 84, xh4[4] = { 68, 68, 60, 60 };  // mismo ancho para las 4 cajas; alto por direccion (ARRIBA/ABAJO mas altas que IZQ/DER)
    uint16_t x4[4] = { (uint16_t)(LCD_W / 2 - xw / 2), (uint16_t)(LCD_W / 2 - xw / 2), 16, (uint16_t)(LCD_W - 16 - xw) };  // x por direccion: ARRIBA/ABAJO centradas horizontalmente, IZQ pegada al borde izquierdo, DER pegada al borde derecho
    uint16_t y4[4] = { 34, 166, 104, 104 };  // y por direccion: ARRIBA arriba de todo, ABAJO abajo de todo, IZQ/DER a media altura
    *bw = xw; *bh = xh4[d];
    *bx = x4[d]; *by = y4[d];
}

/* Flecha triangular (0=ARRIBA 1=ABAJO 2=IZQ 3=DER) centrada en el cuadro
 * del D-pad, armada con FillRect apiladas (sin primitiva de triangulo en
 * el driver). Mismo tamaño/posicion siempre -- redibujar con otro color
 * sobreescribe la anterior por completo, sin dejar residuos. */
static void sj1p_dibujar_flecha(uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,   // dibuja UNA flecha triangular del D-pad de 1 jugador
                                 uint8_t dir, uint16_t color) {
    int16_t cx   = (int16_t)(bx + bw / 2);                  // centro x de la caja
    int16_t cy   = (int16_t)(by + bh / 2);                  // centro y de la caja
    int16_t size = (int16_t)((bw < bh ? bw : bh) / 2) - 6;   // "radio" de la flecha, un poco mas chico que la caja
    if (size < 8) size = 8;                                  // tamaño minimo de flecha
    /* Mismo criterio que cockpit_dibujar_flecha: base 1.5 veces mas ancha
     * que alta en vez de 45 grados, acotada al radio de la caja. */
    int16_t ancho_max = (int16_t)(size * 3 / 2);              // ancho maximo de la base del triangulo
    int16_t max_ancho = (int16_t)((bw < bh ? bw : bh) / 2) - 2;  // limite duro para no salirse de la caja
    if (ancho_max > max_ancho) ancho_max = max_ancho;
    int16_t tallo = (int16_t)(size / 3) + 2;                  // grosor del palito detras de la punta

    switch (dir) {
    case 0:  /* ARRIBA */
        for (int16_t i = 0; i <= size; i++) {                 // punta triangular, de abajo hacia arriba fila por fila
            int16_t hw = (int16_t)(ancho_max * i / size);       // medio-ancho de esta fila, crece desde 0 (punta) hasta ancho_max (base)
            ILI9341_FillRect((uint16_t)(cx - hw), (uint16_t)(cy - size + i), (uint16_t)(2 * hw + 1), 1, color);  // franja horizontal de esta fila
        }
        ILI9341_FillRect((uint16_t)(cx - tallo / 2), (uint16_t)cy, (uint16_t)tallo, (uint16_t)(size / 2), color);  // palito debajo de la punta
        break;
    case 1:  /* ABAJO */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            ILI9341_FillRect((uint16_t)(cx - hw), (uint16_t)(cy + size - i), (uint16_t)(2 * hw + 1), 1, color);  // igual que ARRIBA pero espejado en y
        }
        ILI9341_FillRect((uint16_t)(cx - tallo / 2), (uint16_t)(cy - size / 2), (uint16_t)tallo, (uint16_t)(size / 2), color);  // palito arriba de la punta
        break;
    case 2:  /* IZQ */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            ILI9341_FillRect((uint16_t)(cx - size + i), (uint16_t)(cy - hw), 1, (uint16_t)(2 * hw + 1), color);  // ejes intercambiados respecto a ARRIBA: franjas verticales
        }
        ILI9341_FillRect((uint16_t)cx, (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);  // palito a la derecha de la punta
        break;
    default: /* DER */
        for (int16_t i = 0; i <= size; i++) {
            int16_t hw = (int16_t)(ancho_max * i / size);
            ILI9341_FillRect((uint16_t)(cx + size - i), (uint16_t)(cy - hw), 1, (uint16_t)(2 * hw + 1), color);  // espejo de IZQ en x
        }
        ILI9341_FillRect((uint16_t)(cx - size / 2), (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);  // palito a la izquierda de la punta
        break;
    }
}

/* Solo flecha, sin caja de fondo ni nombre de texto, pero cada direccion
 * con su propio color (NOTE_COLOR/LANE_COLOR, los mismos 4 de siempre) --
 * apagada = version oscura, encendida = version brillante. */
static void sj1p_pad_draw(uint8_t d, uint8_t activo) {   // dibuja UNA flecha del D-pad (1 jugador), encendida o apagada
    uint16_t bx, by, bw, bh;
    sj1p_pad_rect(d, &bx, &by, &bw, &bh);                 // bounding box de esta direccion en pantalla completa
    uint8_t cidx = SJ_DPAD_COLOR_IDX[d];                    // color rotado para esta direccion (ver SJ_DPAD_COLOR_IDX)
    sj1p_dibujar_flecha(bx, by, bw, bh, d, activo ? NOTE_COLOR[cidx] : LANE_COLOR[cidx]);  // brillante si es el paso activo, apagado si no
}

void Renderer_DrawModoSimonJoystick1P(uint8_t paso) {   // dibuja la pantalla completa del D-pad de Simon+Joystick 1 jugador desde cero
    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja de titulo
    draw_string_c(LCD_W / 2, 9, "SIMON + JOYSTICK", COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado

    for (uint8_t d = 0; d < 4; d++) sj1p_pad_draw(d, paso == d);  // dibuja las 4 direcciones; la que coincide con "paso" se dibuja encendida
}

void Renderer_UpdateModoSimonJoystick1P(uint8_t paso_ant, uint8_t paso) {   // redibuja solo las 2 flechas que cambiaron de estado
    if (paso_ant == paso) return;                     // sin cambio, nada que redibujar
    if (paso_ant < 4) sj1p_pad_draw(paso_ant, 0);        // apaga la direccion anterior
    if (paso     < 4) sj1p_pad_draw(paso, 1);            // enciende la nueva
}

/* Cursor en forma de "X" que sigue la posicion CRUDA del joystick dentro
 * del hueco vacio del centro del D-pad -- igual que el cursor del OLED de
 * examen_parcial, pero moviendose de verdad en vez de una marca fija.
 * Verde = centrado y listo para el siguiente movimiento, gris = inclinado. */
#define SJ1P_CURSOR_X0 100  // x minima del rango de movimiento dibujado del cursor (hueco vacio del centro del D-pad)
#define SJ1P_CURSOR_X1 220  // x maxima de ese rango
#define SJ1P_CURSOR_Y0 108  // y minima
#define SJ1P_CURSOR_Y1 158  // y maxima
#define SJ1P_CURSOR_R    6  // "radio" (medio ancho/alto) de la X del cursor

static void sj1p_borrar_cursor(uint16_t px, uint16_t py) {   // borra el cursor "X" de su posicion anterior
    uint16_t d = (SJ1P_CURSOR_R + 1) * 2 + 1;  // lado del cuadrado a borrar, 1px mas grande por lado que la X dibujada (para no dejar residuo de las lineas diagonales)
    ILI9341_FillRect((uint16_t)(px - SJ1P_CURSOR_R - 1), (uint16_t)(py - SJ1P_CURSOR_R - 1),
                      d, d, COLOR_BLACK);  // borra el cuadrado centrado en la posicion anterior del cursor
}

static void sj1p_dibujar_cursor(uint16_t px, uint16_t py, uint16_t col) {   // dibuja el cursor "X" en su posicion actual
    ILI9341_DrawLine((int16_t)(px - SJ1P_CURSOR_R), (int16_t)(py - SJ1P_CURSOR_R),
                      (int16_t)(px + SJ1P_CURSOR_R), (int16_t)(py + SJ1P_CURSOR_R), col);  // diagonal \ de la X
    ILI9341_DrawLine((int16_t)(px - SJ1P_CURSOR_R), (int16_t)(py + SJ1P_CURSOR_R),
                      (int16_t)(px + SJ1P_CURSOR_R), (int16_t)(py - SJ1P_CURSOR_R), col);  // diagonal / de la X
}

/* ultima posicion dibujada -- 0xFFFF = todavia no se ha dibujado */
static uint16_t sj1p_cursor_px = 0xFFFF, sj1p_cursor_py = 0xFFFF;

void Renderer_ResetCursorJoystick(void) {   // vuelve el cursor del centro del D-pad (1 jugador) a su posicion neutral
    sj1p_cursor_px = 0xFFFF;  // olvida la ultima posicion dibujada
    sj1p_cursor_py = 0xFFFF;  // -- el proximo Actualizar no intentara borrar una posicion vieja que ya no esta en pantalla (p.ej. tras un FillScreen)
}

/* joy_x/joy_y: cuenta cruda del adc (0-4095). Mapea internamente al hueco
 * del centro del D-pad y solo redibuja si la posicion en pantalla cambio
 * (evita trafico SPI innecesario por jitter sub-pixel del filtro). */
void Renderer_ActualizarCursorJoystick(uint16_t joy_x, uint16_t joy_y, uint8_t listo) {   // mueve el cursor "X" segun la posicion cruda actual del joystick
    /* EJES CRUZADOS: el joystick fisico usado en el modo de 1 jugador quedo
     * montado girado 90 grados respecto a la orientacion asumida por el
     * software, de modo que el canal "y" del ADC controla el movimiento
     * horizontal en pantalla y el canal "x" controla el vertical (mismo
     * cruce que Joystick_LeerDireccion en main.c). px se calcula a partir
     * de joy_y de forma invertida (un valor bajo de joy_y corresponde a un
     * empujon hacia la derecha) y py se calcula a partir de joy_x sin
     * invertir. */
    uint16_t px = (uint16_t)(SJ1P_CURSOR_X1 - ((uint32_t)joy_y * (SJ1P_CURSOR_X1 - SJ1P_CURSOR_X0)) / 4095u);  // x en pantalla proporcional (invertido) al canal "y" del ADC (0..4095 -> X1..X0)
    uint16_t py = (uint16_t)(SJ1P_CURSOR_Y0 + ((uint32_t)joy_x * (SJ1P_CURSOR_Y1 - SJ1P_CURSOR_Y0)) / 4095u);  // y en pantalla proporcional al canal "x" del ADC (0..4095 -> Y0..Y1)

    if (px == sj1p_cursor_px && py == sj1p_cursor_py) return;  // el cursor no se movio en pantalla, no redibujar

    /* Registro de diagnostico por UART: permite verificar en consola la
     * correspondencia entre la lectura cruda del ADC y la posicion en
     * pantalla calculada, util para depurar el mapeo de ejes en hardware. */
    printf("[CURSOR1P] joy_x=%u joy_y=%u -> px=%u py=%u\r\n", joy_x, joy_y, px, py);  // log de diagnostico, uno por cada movimiento real detectado

    if (sj1p_cursor_px != 0xFFFF) sj1p_borrar_cursor(sj1p_cursor_px, sj1p_cursor_py);  // borra la X de la posicion anterior (si ya habia una dibujada)
    sj1p_dibujar_cursor(px, py, listo ? COLOR_GREEN : COLOR_GRAY);  // dibuja la X nueva: verde si el stick ya volvio al centro (listo para el siguiente movimiento), gris si sigue inclinado
    sj1p_cursor_px = px;  // recuerda la posicion dibujada
    sj1p_cursor_py = py;
}

/* Cursor "X" por jugador para el modo cara-a-cara: permite verificar a
 * simple vista que cada lado mueve el joystick fisico correcto. Mismo
 * cursor que el del modo de 1 jugador, pero uno independiente por mitad de
 * pantalla, transformado por Cockpit_Punto para que la orientacion (mitad
 * normal/rotada) se resuelva automaticamente. Vive en el hueco vacio del
 * centro de la cruz de flechas de cockpit_pad_rect. */
#define COCKP_CURSOR_X0  95   // x local minima del rango de movimiento del cursor (hueco del centro del D-pad, mitad de un jugador)
#define COCKP_CURSOR_X1  145  // x local maxima
#define COCKP_CURSOR_Y0  83   // y local minima
#define COCKP_CURSOR_Y1  103  // y local maxima
#define COCKP_CURSOR_R    5   // "radio" de la X del cursor

static uint16_t cockp_cursor_px[2] = { 0xFFFF, 0xFFFF };  // ultima x local dibujada por jugador, 0xFFFF = ninguna todavia
static uint16_t cockp_cursor_py[2] = { 0xFFFF, 0xFFFF };  // ultima y local dibujada por jugador

static void cockp_borrar_cursor(uint8_t jugador, uint16_t lx, uint16_t ly) {   // borra el cursor "X" de la mitad de un jugador (2 jugadores)
    uint16_t d = (COCKP_CURSOR_R + 1) * 2 + 1;  // lado del cuadrado a borrar, 1px mas grande que la X por lado
    Cockpit_FillRect(jugador, (uint16_t)(lx - COCKP_CURSOR_R - 1), (uint16_t)(ly - COCKP_CURSOR_R - 1), d, d, COLOR_BLACK);  // borra (ya transformado segun el jugador)
}

static void cockp_dibujar_cursor(uint8_t jugador, uint16_t lx, uint16_t ly, uint16_t col) {   // dibuja el cursor "X" en la mitad de un jugador (2 jugadores)
    int16_t ax, ay, bx, by;
    Cockpit_Punto(jugador, (int16_t)(lx - COCKP_CURSOR_R), (int16_t)(ly - COCKP_CURSOR_R), &ax, &ay);  // extremo superior-izquierdo de la diagonal \, transformado
    Cockpit_Punto(jugador, (int16_t)(lx + COCKP_CURSOR_R), (int16_t)(ly + COCKP_CURSOR_R), &bx, &by);  // extremo inferior-derecho de esa misma diagonal
    ILI9341_DrawLine(ax, ay, bx, by, col);                                                              // dibuja la diagonal \ ya transformada
    Cockpit_Punto(jugador, (int16_t)(lx - COCKP_CURSOR_R), (int16_t)(ly + COCKP_CURSOR_R), &ax, &ay);  // extremo inferior-izquierdo de la diagonal /
    Cockpit_Punto(jugador, (int16_t)(lx + COCKP_CURSOR_R), (int16_t)(ly - COCKP_CURSOR_R), &bx, &by);  // extremo superior-derecho de esa diagonal
    ILI9341_DrawLine(ax, ay, bx, by, col);                                                              // dibuja la diagonal /
}

/* olvida la ultima posicion dibujada de ESE jugador -- llamar al (re)iniciar
 * su lado para que el proximo Actualizar no intente borrar una posicion de
 * una partida anterior */
void Renderer_ResetCursorJoystick2P(uint8_t jugador) {   // vuelve el cursor de un jugador a su posicion neutral (2 jugadores)
    cockp_cursor_px[jugador] = 0xFFFF;
    cockp_cursor_py[jugador] = 0xFFFF;
}

void Renderer_ActualizarCursorJoystick2P(uint8_t jugador, uint16_t joy_x, uint16_t joy_y, uint8_t listo) {   // mueve el cursor de un jugador segun su joystick fisico correspondiente
    /* px se calcula a partir del canal "y" del ADC de forma invertida (un
     * valor bajo corresponde a un empujon hacia el lado derecho del D-pad,
     * ver SJ_DPAD_COLOR_IDX_P0), y py a partir del canal "x" sin invertir.
     * Se aplica la misma formula a los 2 jugadores. */
    uint32_t canal_x = 4095u - joy_y;   // invertido para los 2 jugadores
    uint16_t px = (uint16_t)(COCKP_CURSOR_X0 + (canal_x * (COCKP_CURSOR_X1 - COCKP_CURSOR_X0)) / 4095u);  // x local proporcional (invertido) al canal "y" del ADC
    uint16_t py = (uint16_t)(COCKP_CURSOR_Y0 + ((uint32_t)joy_x * (COCKP_CURSOR_Y1 - COCKP_CURSOR_Y0)) / 4095u);  // y local proporcional al canal "x" del ADC

    if (px == cockp_cursor_px[jugador] && py == cockp_cursor_py[jugador]) return;  // sin cambio de posicion, no redibujar

    /* Registro de diagnostico por UART, mismo proposito que [CURSOR1P] en
     * Renderer_ActualizarCursorJoystick: permite verificar en consola la
     * correspondencia entre la lectura cruda del ADC y la posicion en
     * pantalla calculada para cada jugador. */
    printf("[CURSOR2P] jugador=%u joy_x=%u joy_y=%u -> px=%u py=%u\r\n", jugador, joy_x, joy_y, px, py);  // log de diagnostico por movimiento real

    if (cockp_cursor_px[jugador] != 0xFFFF) cockp_borrar_cursor(jugador, cockp_cursor_px[jugador], cockp_cursor_py[jugador]);  // borra la X anterior de este jugador
    cockp_dibujar_cursor(jugador, px, py, listo ? COLOR_GREEN : COLOR_GRAY);  // dibuja la X nueva (verde=centrado y listo, gris=inclinado)
    cockp_cursor_px[jugador] = px;
    cockp_cursor_py[jugador] = py;
}

/* ========================================================================== */
/* === LISTA DE CANCIONES (pantalla "reproductor") =========================== */
/* ========================================================================== */
/* El ORDEN debe coincidir exactamente con CANCIONES_NOMBRE/DATA/LEN en main.c */
#define RLC_N 12  // cantidad de canciones en la lista -- debe coincidir con CANCIONES_DATA/LEN en main.c
static const char *const RLC_NOMBRE[RLC_N] = {
    "BIENVENIDA", "ESTRELLITA", "HIMNO ALEGRIA", "MARTINILLO", "NAVIDAD", "TETRIS",
    "BOHEMIAN RHAPSODY", "DR DRE - STILL DRE", "NARUTO OP9", "ONE PIECE", "PIRATES CARIBBEAN", "TOKYO GHOUL"
};

#define RLC_ROW_Y0   30  // y donde arranca la primera fila de la lista
#define RLC_ROW_H    30  // alto de cada fila -- agrandarlo separa mas las canciones entre si
#define RLC_VISIBLE   6  // filas visibles a la vez (RLC_ROW_Y0 + RLC_VISIBLE*RLC_ROW_H debe entrar antes de la franja de ayuda en y=220) -- con RLC_N > RLC_VISIBLE la lista scrollea

/* Indice (dentro de RLC_NOMBRE) de la primera cancion visible en la ventana
 * actual -- rlc_ajustar_scroll() la mueve lo minimo necesario para que el
 * cursor quede siempre dentro de la ventana visible. */
static uint8_t rlc_scroll_top = 0;

static uint8_t rlc_ajustar_scroll(uint8_t cursor) {   // corrige rlc_scroll_top si hace falta para que `cursor` quede visible, y lo devuelve
    if (cursor < rlc_scroll_top) rlc_scroll_top = cursor;   // el cursor se movio arriba de la ventana actual -- la ventana sube con el
    else if (cursor >= (uint8_t)(rlc_scroll_top + RLC_VISIBLE)) rlc_scroll_top = (uint8_t)(cursor - RLC_VISIBLE + 1);   // se movio debajo de la ventana -- la ventana baja lo justo para mostrarlo al final
    return rlc_scroll_top;
}

static void rlc_draw_row(uint8_t i, uint8_t selected) {   // dibuja UNA fila del menu de seleccion de cancion de Guitar Hero (i = indice dentro de RLC_NOMBRE, no fila visual)
    uint8_t fila = (uint8_t)(i - rlc_scroll_top);              // posicion visual (0..RLC_VISIBLE-1) de esta cancion dentro de la ventana actual
    uint16_t ry = (uint16_t)(RLC_ROW_Y0 + fila * RLC_ROW_H);    // y de esta fila = base + fila_visual*alto de fila
    uint16_t bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;       // fondo resaltado si es la fila del cursor

    ILI9341_FillRect(0, ry, LCD_W, RLC_ROW_H - 2, bg);           // fondo de la fila (2px menos de alto que RLC_ROW_H, deja un hueco entre filas)
    draw_string(selected ? 30 : 20, (uint16_t)(ry + 8), RLC_NOMBRE[i],
                selected ? COLOR_YELLOW : COLOR_WHITE, bg, 1);    // nombre de la cancion; la seleccionada se corre 10px a la derecha (deja lugar a la flecha ">") y cambia a amarillo
    if (selected) {
        draw_string(10, (uint16_t)(ry + 8), ">", COLOR_YELLOW, bg, 1);  // flecha indicadora solo en la fila seleccionada
    }
}

void Renderer_DrawListaCanciones(uint8_t cursor) {   // dibuja el menu completo de seleccion de cancion desde cero
    rlc_ajustar_scroll(cursor);   // asegura que la ventana visible incluya al cursor ANTES de dibujar

    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);  // franja de titulo
    draw_string_c(LCD_W / 2, 9, "CANCIONES", COLOR_WHITE, COLOR_DARKGRAY, 1);  // titulo centrado

    uint8_t desde  = rlc_scroll_top;                                                            // primera cancion visible
    uint8_t hasta  = (uint8_t)((desde + RLC_VISIBLE < RLC_N) ? (desde + RLC_VISIBLE) : RLC_N);   // ultima visible +1, recortada contra RLC_N
    for (uint8_t i = desde; i < hasta; i++) rlc_draw_row(i, cursor == i);   // dibuja solo las filas de la ventana actual, resaltando la del cursor

    char pos[12];
    snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(cursor + 1), (unsigned)RLC_N);   // indicador "N/TOTAL" -- unica pista de que hay mas canciones fuera de la ventana visible
    draw_string(LCD_W - 46, 9, pos, COLOR_WHITE, COLOR_DARKGRAY, 1);

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);  // franja de ayuda inferior
    draw_string_c(LCD_W / 2, 226, "BOTON=CAMBIAR   JOYSTICK=EMPEZAR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);  // texto de ayuda: boton recorre/previsualiza la lista, cualquier joystick confirma y arranca Guitar Hero (unico modo que llega a esta pantalla, ver MenuCanciones_Procesar en main.c)
}

void Renderer_UpdateListaCanciones(uint8_t cursor_ant, uint8_t cursor) {   // redibuja solo las filas que cambiaron de estado (o todo, si el cursor se salio de la ventana visible)
    if (cursor_ant == cursor) return;                      // sin cambio de cursor, nada que redibujar

    uint8_t scroll_ant = rlc_scroll_top;                   // ventana ANTES de mover el cursor
    if (rlc_ajustar_scroll(cursor) != scroll_ant) {         // el cursor se salio de la ventana visible -- hace falta redibujar todo (cambiaron las canciones mostradas Y el indicador de posicion)
        Renderer_DrawListaCanciones(cursor);
        return;
    }
    if (cursor_ant < RLC_N) rlc_draw_row(cursor_ant, 0);      // apaga el resaltado de la fila anterior
    if (cursor     < RLC_N) rlc_draw_row(cursor, 1);           // enciende el resaltado de la fila nueva
    char pos[12];
    snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(cursor + 1), (unsigned)RLC_N);
    ILI9341_FillRect(LCD_W - 46, 9, 46, 9, COLOR_DARKGRAY);   // borra el indicador de posicion anterior antes de redibujarlo
    draw_string(LCD_W - 46, 9, pos, COLOR_WHITE, COLOR_DARKGRAY, 1);
}

/* ========================================================================== */
/* === CONTEO REGRESIVO ===================================================== */
/* ========================================================================== */

void Renderer_DrawConteo(uint8_t numero) {   // dibuja la pantalla del conteo regresivo 3-2-1-GO
    uint16_t bx = 100, by = 55, bw = 120, bh = 130;  // caja fija donde se dibuja el numero/GO!, centrada en pantalla
    if (numero == 3) ILI9341_FillScreen(COLOR_BLACK);  /* solo al entrar, limpia lo anterior */
    ILI9341_FillRect(bx, by, bw, bh, COLOR_BLACK);  // borra la caja antes de dibujar el numero nuevo (los numeros siguientes no limpian toda la pantalla, solo esta caja)

    if (numero > 0) {
        static const uint16_t cnt_col[4] = {
            COLOR_BLACK, COLOR_GREEN, COLOR_YELLOW, COLOR_RED  // color por numero: 1=verde 2=amarillo 3=rojo (indice 0 sin usar, numero nunca es 0 en esta rama)
        };
        uint16_t col = cnt_col[numero < 4 ? numero : 3];  // clampa a 3 por si "numero" llegara a ser mayor (defensivo)

        /* Circulo simulado */
        ILI9341_FillRect(bx + 15, by + 10, 90, 90, col);        // cuadrado grande de color (hace de "circulo" simplificado)
        ILI9341_FillRect(bx + 25, by + 20, 70, 70, COLOR_BLACK);  // cuadrado negro mas chico encima, deja solo un marco de color visible (parece un anillo)

        /* Numero grande (dos juegos de 7-seg superpuestos para efecto grueso) */
        draw_seg_digit((int16_t)(bx + 42), (int16_t)(by + 28),
                       numero, col, COLOR_BLACK);                // primer trazo del digito 7-seg
        draw_seg_digit((int16_t)(bx + 44), (int16_t)(by + 30),
                       numero, col, COLOR_BLACK);                // segundo trazo, corrido 2px -- la superposicion engrosa visualmente el digito

    } else {
        /* "GO!" en pixel art */
        ILI9341_FillRect(bx + 5,  by + 20, 110, 28, COLOR_GREEN);   // franja verde superior
        ILI9341_FillRect(bx + 10, by + 25,  100, 18, COLOR_BLACK);   // hueco negro dentro de esa franja, deja un marco visible
        ILI9341_FillRect(bx + 5,  by + 55, 110, 28, COLOR_GREEN);   // franja verde inferior
        ILI9341_FillRect(bx + 10, by + 60, 100, 18, COLOR_BLACK);   // hueco negro correspondiente

        /* "GO" con fuente escala 4 */
        draw_string((uint16_t)(bx + 12), (uint16_t)(by + 47), "GO",
                    COLOR_GREEN, COLOR_BLACK, 4);  // texto "GO" grande, superpuesto sobre las 2 franjas de arriba
    }
}

/* ========================================================================== */
/* === PANTALLA DE RESULTADO ================================================ */
/* ========================================================================== */

void Renderer_DrawResultado(const GameState_t *gs) {   // dibuja la pantalla de resultado del recorrido de diseño (no la real de cada modo)
    ILI9341_FillScreen(COLOR_BLACK);  // pantalla nueva desde cero

    uint16_t s1 = gs->j[0].puntaje;  // puntaje final de J1
    uint16_t s2 = gs->j[1].puntaje;  // puntaje final de J2

    /* === Cabecera "FIN DEL JUEGO" === */
    ILI9341_FillRect(0, 0, LCD_W, 24, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 8, "FIN DEL JUEGO", COLOR_WHITE, COLOR_DARKGRAY, 1);

    /* === Puntajes individuales === */
    /* J1 — izquierda */
    ILI9341_FillRect(10, 30, 130, 70, COLOR_DARKGRAY);          // panel de fondo del lado J1
    ILI9341_FillRect(12, 32, 20, 20, COLOR_P1);                  // cuadrado indicador del color de J1
    draw_char(16, 37, 'J', COLOR_WHITE, COLOR_P1, 1);             // letra 'J'
    draw_char(23, 37, '1', COLOR_WHITE, COLOR_P1, 1);             // numero '1'
    /* 4 digitos del puntaje J1 */
    {
        uint8_t d[4] = {
            (uint8_t)((s1/1000)%10),(uint8_t)((s1/100)%10),   // digitos de millares y centenas
            (uint8_t)((s1/10)%10),(uint8_t)(s1%10)              // digitos de decenas y unidades
        };
        for (int i = 0; i < 4; i++) {
            draw_seg_digit((int16_t)(20 + i * (SEG_DX + 1)), 55,
                           d[i], COLOR_P1, COLOR_DARKGRAY);      // dibuja cada digito en su posicion, estilo 7-seg
        }
    }

    /* J2 — derecha */
    ILI9341_FillRect(180, 30, 130, 70, COLOR_DARKGRAY);         // panel de fondo del lado J2
    ILI9341_FillRect(182, 32, 20, 20, COLOR_P2);                 // cuadrado indicador del color de J2
    draw_char(186, 37, 'J', COLOR_WHITE, COLOR_P2, 1);   // etiqueta "J2" dentro del cuadrado -- letra J
    draw_char(193, 37, '2', COLOR_WHITE, COLOR_P2, 1);   // etiqueta "J2" -- digito 2
    {
        uint8_t d[4] = {
            (uint8_t)((s2/1000)%10),(uint8_t)((s2/100)%10),
            (uint8_t)((s2/10)%10),(uint8_t)(s2%10)
        };
        for (int i = 0; i < 4; i++) {
            draw_seg_digit((int16_t)(190 + i * (SEG_DX + 1)), 55,
                           d[i], COLOR_P2, COLOR_DARKGRAY);
        }
    }

    /* === Banner del ganador === */
    uint16_t gy = 108;  // y donde arranca el banner del ganador
    if (s1 > s2) {
        /* Jugador 1 gana */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_P1);            // marco exterior del color de J1
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);        // interior negro, deja ver el marco de color
        draw_string_c(LCD_W/2, gy + 12, "GANADOR", COLOR_P1, COLOR_BLACK, 2);
        draw_string_c(LCD_W/2, gy + 40, "JUGADOR 1", COLOR_P1, COLOR_BLACK, 2);

    } else if (s2 > s1) {
        /* Jugador 2 gana */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_P2);              // marco exterior del color de J2 (mismo patron que el bloque de J1, en su color)
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);          // interior negro
        draw_string_c(LCD_W/2, gy + 12, "GANADOR", COLOR_P2, COLOR_BLACK, 2);
        draw_string_c(LCD_W/2, gy + 40, "JUGADOR 2", COLOR_P2, COLOR_BLACK, 2);

    } else {
        /* Empate */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_YELLOW);         // marco amarillo (ni color de J1 ni de J2, para no favorecer visualmente a ninguno)
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);         // interior negro
        draw_string_c(LCD_W/2, gy + 22, "EMPATE", COLOR_YELLOW, COLOR_BLACK, 3);   // texto centrado a escala 3, mas grande que "GANADOR" al no tener nombre de jugador debajo
    }

    /* === Comparacion de combo maximo (texto) === */
    ILI9341_FillRect(0, 200, LCD_W, 16, COLOR_DARKGRAY);            // franja de fondo para la fila de combos
    draw_string(10,  203, "COMBO:", COLOR_GRAY, COLOR_DARKGRAY, 1);  // etiqueta lado J1
    draw_uint16(52,  203, gs->j[0].combo, COLOR_P1, COLOR_DARKGRAY, 1);  // valor de combo de J1
    draw_string(220, 203, "COMBO:", COLOR_GRAY, COLOR_DARKGRAY, 1);  // etiqueta lado J2
    draw_uint16(262, 203, gs->j[1].combo, COLOR_P2, COLOR_DARKGRAY, 1);  // valor de combo de J2

    /* === "Presiona START" === */
    ILI9341_FillRect(0, 222, LCD_W, 18, COLOR_DARKGRAY);  // franja inferior de instruccion
    draw_string_c(LCD_W/2, 227, "PRESIONA START", COLOR_GREEN, COLOR_DARKGRAY, 1);  // texto de instruccion centrado
}

/* ========================================================================== */
/* === ACTUALIZACION DE SCORES Y COMBO (JUGANDO) ============================ */
/* ========================================================================== */

void Renderer_UpdateScores(const GameState_t *gs) {   // redibuja solo la barra y los digitos de puntaje que cambiaron
    for (uint8_t p = 0; p < 2; p++) {                        // recorre los 2 jugadores
        uint16_t xo  = PLAYER_X_OFF[p];                        // offset x de este jugador
        uint16_t col = (p == 0) ? COLOR_P1 : COLOR_P2;          // color de este jugador

        draw_score_bar(xo, gs->j[p].puntaje, col);              // barra de progreso de puntaje
        draw_score_digits(xo, gs->j[p].puntaje, col, COLOR_DARKGRAY);  // digitos numericos del puntaje

        /* Combo: "X{N}" en la zona libre de la score bar (y=18, x=xo+23) */
        uint16_t combo = gs->j[p].combo;
        if (combo > 1) {
            draw_char(xo + 23, 18, 'X', COLOR_WHITE, COLOR_DARKGRAY, 1);         // letra 'X' (indicador de combo/multiplicador)
            draw_uint16(xo + 30, 18, combo, COLOR_YELLOW, COLOR_DARKGRAY, 1);    // numero de combo actual
        } else {
            ILI9341_FillRect(xo + 23, 18, 36, 7, COLOR_DARKGRAY);  // combo 0 o 1 no se muestra -- borra el espacio (por si antes SI habia un combo visible ahi)
        }
    }
}

/* ========================================================================== */
/* === NOTA: DIBUJO Y BORRADO DELTA ========================================= */
/* ========================================================================== */

void Renderer_FlashPressZone(uint8_t jugador, uint8_t carril, uint16_t color) {   // pinta la zona de golpe rectangular del recorrido de diseño de un color (efecto de flash)
    uint16_t xo = PLAYER_X_OFF[jugador];                                   // offset x de este jugador
    uint16_t ly = LANE_TOP(carril);                                        // y de este carril
    ILI9341_FillRect(xo, ly + NOTE_Y_PAD, PRESS_ZONE_W, NOTE_H, color);     // repinta la zona de golpe del color pedido (usado para el flash al acertar/fallar)
}

void Renderer_DrawNota(const Nota_t *nota, uint16_t x_off) {   // dibuja una nota del recorrido de diseño (rectangulo, no circulo) en su carril
    if (!nota->activa) return;                                    // nota inactiva, no dibujar
    int16_t x_abs = (int16_t)x_off + nota->x_rel;                  // x absoluta en pantalla = offset del jugador + x relativa de la nota
    if (x_abs + (int16_t)NOTE_W <= 0 || x_abs >= (int16_t)PLAYER_W) return;  // la nota esta completamente fuera de la mitad de este jugador, no dibujar

    int16_t dx = x_abs;                                            // x de arranque a dibujar (se recorta mas abajo si hace falta)
    int16_t dw = NOTE_W;                                            // ancho a dibujar (se recorta mas abajo si hace falta)
    if (dx < 0)              { dw += dx; dx = 0; }                  // si la nota arranca antes del borde izquierdo, recorta el ancho y mueve dx a 0
    if (dx + dw > (int16_t)PLAYER_W) dw = (int16_t)PLAYER_W - dx;   // si se pasa del borde derecho, recorta el ancho
    if (dw <= 0) return;                                            // nada visible que dibujar tras recortar

    ILI9341_FillRect((uint16_t)((int16_t)x_off + dx),
                     note_y(nota->carril),
                     (uint16_t)dw, NOTE_H,
                     NOTE_COLOR[nota->carril]);                     // dibuja el rectangulo de la nota (o la parte visible de ella), del color de su carril
}

void Renderer_EraseNotaTrail(const Nota_t *nota, uint16_t x_off, uint8_t speed) {   // borra la franja que la nota del recorrido de diseño dejo atras al caer
    if (!nota->activa) return;                                     // nota inactiva, no habia nada dibujado

    int16_t ex_start = nota->x_prev + (int16_t)NOTE_W - (int16_t)speed;  // borde izquierdo de la franja a borrar: borde derecho de la posicion anterior menos lo que avanzo
    int16_t ex_end   = nota->x_prev + (int16_t)NOTE_W;                    // borde derecho de la franja a borrar = borde derecho de la posicion anterior

    if (ex_start < 0)               ex_start = 0;                    // recorta contra el borde izquierdo
    if (ex_end > (int16_t)PLAYER_W) ex_end   = (int16_t)PLAYER_W;    // recorta contra el borde derecho
    int16_t ew = ex_end - ex_start;                                   // ancho final de la franja
    if (ew <= 0) return;                                              // nada que borrar

    uint16_t abs_ex = (uint16_t)((int16_t)x_off + ex_start);          // x absoluta de arranque de la franja
    uint16_t ly     = note_y(nota->carril);                            // y del carril de esta nota

    /* Repintar con el fondo correcto segun zona */
    if (ex_start >= (int16_t)PRESS_ZONE_W) {
        /* Todo en zona de carril */
        ILI9341_FillRect(abs_ex, ly, (uint16_t)ew, NOTE_H,
                         LANE_COLOR[nota->carril]);                    // toda la franja esta despues de la zona de golpe -- repinta con el color "apagado" del carril
    } else if (ex_end <= (int16_t)PRESS_ZONE_W) {
        /* Todo en zona de presion */
        ILI9341_FillRect(abs_ex, ly, (uint16_t)ew, NOTE_H,
                         PRESS_COLOR[nota->carril]);                    // toda la franja esta dentro de la zona de golpe -- repinta con su color propio
    } else {
        /* Cruza la frontera: dos rectangulos */
        int16_t press_w = (int16_t)PRESS_ZONE_W - ex_start;             // ancho de la parte que cae dentro de la zona de golpe
        ILI9341_FillRect(abs_ex, ly, (uint16_t)press_w, NOTE_H,
                         PRESS_COLOR[nota->carril]);                    // repinta esa parte con el color de zona de golpe
        uint16_t abs_ls = (uint16_t)((int16_t)x_off + (int16_t)PRESS_ZONE_W);  // x absoluta donde termina la zona de golpe / arranca el carril normal
        ILI9341_FillRect(abs_ls, ly,
                         (uint16_t)(ex_end - (int16_t)PRESS_ZONE_W), NOTE_H,
                         LANE_COLOR[nota->carril]);                     // repinta el resto con el color de carril normal
    }
}

/* ========================================================================== */
/* === DISPATCHER PRINCIPAL ================================================= */
/* ========================================================================== */

static void draw_heartbeat(void) {   // parpadea el LED rojo de J1 cada 500ms como heartbeat, mientras no hay una partida de BOTONES en curso
    static uint32_t hb_tick = 0;   // tick del ultimo cambio de estado
    static uint8_t  hb_on   = 0;   // 1=encendido, 0=apagado
    uint32_t now = HAL_GetTick();   // tick actual
    if (now - hb_tick >= 500) {   // pasaron 500ms desde el ultimo cambio
        hb_tick = now;   // reinicia el temporizador
        hb_on   = !hb_on;   // invierte el estado
        ILI9341_FillRect(LCD_W - 10, 1, 8, 8,
                         hb_on ? COLOR_WHITE : COLOR_BLACK);   // pinta el cuadradito en la esquina superior derecha, blanco u negro segun el estado
    }
}

void Renderer_Update(GameState_t *gs) {   // tick de render del recorrido de diseño (modo JUGANDO): mueve/dibuja notas y actualiza puntajes
    draw_heartbeat();   // parpadeo de diagnostico, corre siempre sin importar la pantalla actual

    switch (gs->estado) {   // que dibujar depende del estado actual de esta maquina de estados legada (GameState_t) -- no confundir con DemoScreen_t, la que usa el flujo real del juego en main.c

    case ESTADO_SPLASH:
        if (!gs->pantalla_init) {   // solo la primera vez que se entra a este estado
            gs->pantalla_init = 1;   // marca que ya se dibujo el fondo, para no repetirlo cada tick
            Renderer_DrawSplash();
        }
        /* Parpadeo "LISTO PARA JUGAR?" cada 500ms */
        {
            static uint32_t blink_tick = 0;   // tick del ultimo cambio de estado del parpadeo
            static uint8_t  blink_on   = 1;   // 1=texto visible, 0=texto oculto (mismo color que el fondo)
            uint32_t now = HAL_GetTick();
            if (now - blink_tick >= 500) {   // pasaron 500ms
                blink_tick = now;   // reinicia el temporizador
                blink_on   = !blink_on;   // invierte el estado
                draw_string_c(LCD_W / 2, 213,
                              "LISTO PARA JUGAR?",
                              blink_on ? COLOR_GREEN : COLOR_DARKGRAY,
                              COLOR_DARKGRAY, 1);   // texto visible en verde, u oculto (mismo color que el fondo) segun el estado
            }
        }
        break;

    case ESTADO_MENU_NIVEL:
        if (!gs->pantalla_init) {   // solo la primera vez que se entra a este estado
            gs->pantalla_init = 1;
            Renderer_DrawMenu(gs->menu_cursor);
        }
        break;

    case ESTADO_CONTEO:
        /* Renderer_DrawConteo() es llamado desde Game_Update */
        break;

    case ESTADO_JUGANDO:
        if (!gs->pantalla_init) {   // solo la primera vez que se entra a este estado
            gs->pantalla_init = 1;
            Renderer_DrawBackground(gs);
            Renderer_UpdateScores(gs);
        }
        /* Render delta de notas */
        for (uint8_t i = 0; i < MAX_NOTES; i++) {   // recorre TODAS las notas posibles (activas o no)
            Nota_t *n = &gs->notas[i];
            if (!n->activa) continue;   // nota libre, nada que dibujar
            Renderer_EraseNotaTrail(n, PLAYER_X_OFF[n->jugador], gs->nota_speed);   // borra la franja que dejo en el tick anterior
            Renderer_DrawNota(n, PLAYER_X_OFF[n->jugador]);   // dibuja la nota en su posicion actual
        }
        break;

    case ESTADO_RESULTADO:
        if (!gs->pantalla_init) {   // solo la primera vez que se entra a este estado
            gs->pantalla_init = 1;
            Renderer_DrawResultado(gs);
        }
        break;
    }
}
