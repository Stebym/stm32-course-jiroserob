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

#include "renderer.h"
#include "ili9341.h"
#include "board_pins.h"
#include "splash_bg.h"
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"

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
#define CHAR_ADV(scale)  ((uint16_t)(6u * (scale)))
/* Alto de glifo segun escala */
#define CHAR_H(scale)    ((uint16_t)(7u * (scale)))

/* ========================================================================== */
/* === HELPERS DE FUENTE ==================================================== */
/* ========================================================================== */

static void draw_char(uint16_t x, uint16_t y, char c,
                      uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 32 || c > 90) c = '?';
    const uint8_t *g = font5x7[(uint8_t)c - 32];
    /* Fondo del slot del caracter */
    ILI9341_FillRect(x, y, CHAR_ADV(scale), CHAR_H(scale), bg);
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1u << row)) {
                ILI9341_FillRect(x + (uint16_t)col * scale,
                                 y + (uint16_t)row * scale,
                                 scale, scale, fg);
            }
        }
    }
}

static void draw_string(uint16_t x, uint16_t y, const char *s,
                        uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        draw_char(x, y, *s, fg, bg, scale);
        x += CHAR_ADV(scale);
        s++;
    }
}

static uint16_t str_pixel_w(const char *s, uint8_t scale) {
    uint16_t n = 0;
    while (*s++) n++;
    return (uint16_t)(n * CHAR_ADV(scale));
}

/* Dibuja la cadena centrada en cx (coordenada X del centro). */
static void draw_string_c(uint16_t cx, uint16_t y, const char *s,
                           uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t w = str_pixel_w(s, scale);
    int16_t x  = (int16_t)cx - (int16_t)(w / 2u);
    if (x < 0) x = 0;
    draw_string((uint16_t)x, y, s, fg, bg, scale);
}

/* Dibuja un entero sin signo de hasta 5 digitos. */
static void draw_uint16(uint16_t x, uint16_t y, uint16_t val,
                        uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[6];
    int8_t i = 5;
    buf[5] = '\0';
    if (val == 0) { buf[4] = '0'; i = 4; }
    else {
        while (val > 0 && i > 0) { buf[--i] = (char)('0' + val % 10); val /= 10; }
    }
    draw_string(x, y, &buf[i], fg, bg, scale);
}

/* ========================================================================== */
/* === CONSTANTES DE LAYOUT ================================================= */
/* ========================================================================== */

#define DIV_X       159
#define DIV_W       2
static const uint16_t PLAYER_X_OFF[2] = { P1_X_OFF, P2_X_OFF };

static inline uint16_t note_y(uint8_t carril) {
    return (uint16_t)(LANE_TOP(carril) + NOTE_Y_PAD);
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

#define SEG_S   8
#define SEG_T   2
#define SEG_DX  (SEG_S + SEG_T + 2)

#define SEG_A (1<<0)
#define SEG_B (1<<1)
#define SEG_C (1<<2)
#define SEG_D (1<<3)
#define SEG_E (1<<4)
#define SEG_F (1<<5)
#define SEG_G (1<<6)

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

static void draw_seg_digit(int16_t x, int16_t y, uint8_t digit,
                            uint16_t fg, uint16_t bg) {
    uint8_t segs = seg_table[digit % 10];
    ILI9341_FillRect((uint16_t)x, (uint16_t)y,
                     SEG_S + 2*SEG_T, 2*SEG_S + 3*SEG_T, bg);
    if (segs & SEG_A) ILI9341_FillRect((uint16_t)(x+SEG_T), (uint16_t)y,
                                        SEG_S, SEG_T, fg);
    if (segs & SEG_G) ILI9341_FillRect((uint16_t)(x+SEG_T),
                                        (uint16_t)(y+SEG_S+SEG_T), SEG_S, SEG_T, fg);
    if (segs & SEG_D) ILI9341_FillRect((uint16_t)(x+SEG_T),
                                        (uint16_t)(y+2*SEG_S+2*SEG_T), SEG_S, SEG_T, fg);
    if (segs & SEG_F) ILI9341_FillRect((uint16_t)x,
                                        (uint16_t)(y+SEG_T), SEG_T, SEG_S, fg);
    if (segs & SEG_E) ILI9341_FillRect((uint16_t)x,
                                        (uint16_t)(y+SEG_S+2*SEG_T), SEG_T, SEG_S, fg);
    if (segs & SEG_B) ILI9341_FillRect((uint16_t)(x+SEG_S+SEG_T),
                                        (uint16_t)(y+SEG_T), SEG_T, SEG_S, fg);
    if (segs & SEG_C) ILI9341_FillRect((uint16_t)(x+SEG_S+SEG_T),
                                        (uint16_t)(y+SEG_S+2*SEG_T), SEG_T, SEG_S, fg);
}

static void draw_score_bar(uint16_t x_off, uint16_t score, uint16_t fg) {
    uint16_t bx   = x_off + 23;
    uint16_t bw   = 78, bh = 10, by = 7;
    uint16_t fill = (uint16_t)((uint32_t)score * bw / SCORE_MAX);
    if (fill) ILI9341_FillRect(bx,        by, fill,      bh, fg);
    if (fill < bw) ILI9341_FillRect(bx + fill, by, bw - fill, bh, COLOR_DARKGRAY);
}

static void draw_score_digits(uint16_t x_off, uint16_t score,
                               uint16_t fg, uint16_t bg) {
    int16_t dx = (int16_t)(x_off + 103);
    uint8_t d[4] = {
        (uint8_t)((score / 1000) % 10),
        (uint8_t)((score / 100)  % 10),
        (uint8_t)((score / 10)   % 10),
        (uint8_t)(score % 10)
    };
    for (int i = 0; i < 4; i++) {
        draw_seg_digit(dx, 1, d[i], fg, bg);
        dx += SEG_DX + 1;
    }
}

/* ========================================================================== */
/* === FONDO ESTATICO (ESTADO_JUGANDO) ====================================== */
/* ========================================================================== */

void Renderer_DrawBackground(const GameState_t *gs) {
    (void)gs;
    /* Barras de score */
    ILI9341_FillRect(P1_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);
    ILI9341_FillRect(P2_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);

    /* Bloques indicador de jugador */
    ILI9341_FillRect(P1_X_OFF + 1, 2, 20, 20, COLOR_P1);
    ILI9341_FillRect(P2_X_OFF + 1, 2, 20, 20, COLOR_P2);

    /* "J1" y "J2" sobre los bloques de indicador */
    draw_char(P1_X_OFF + 5,  7, 'J', COLOR_WHITE, COLOR_P1, 1);
    draw_char(P1_X_OFF + 12, 7, '1', COLOR_WHITE, COLOR_P1, 1);
    draw_char(P2_X_OFF + 5,  7, 'J', COLOR_WHITE, COLOR_P2, 1);
    draw_char(P2_X_OFF + 12, 7, '2', COLOR_WHITE, COLOR_P2, 1);

    /* Divisor central */
    ILI9341_FillRect(DIV_X, 0, DIV_W, LCD_H, COLOR_WHITE);

    /* Carriles y separadores */
    for (uint8_t p = 0; p < 2; p++) {
        uint16_t xo = PLAYER_X_OFF[p];
        for (uint8_t c = 0; c < 4; c++) {
            uint16_t ly = LANE_TOP(c);
            ILI9341_FillRect(xo, ly, PLAYER_W, LANE_H, LANE_COLOR[c]);
            ILI9341_FillRect(xo, ly, PRESS_ZONE_W, LANE_H, PRESS_COLOR[c]);
        }
        for (uint8_t s = 0; s < 3; s++) {
            ILI9341_FillRect(xo, LANE_TOP(s) + LANE_H, PLAYER_W, SEP_H, COLOR_GRAY);
        }
    }
}

/* ========================================================================== */
/* === SPLASH SCREEN ======================================================== */
/* ========================================================================== */

void Renderer_DrawSplash(void) {
    /* Fondo: "NEON RIFF" generada por IA, recortada 1024x768 y reescalada a
     * 320x240 (ver splash_bg.h) — ya trae su propio titulo y panel Simon,
     * asi que no hace falta redibujar iconos/franjas/rejilla encima. */
    ILI9341_DrawImage(0, 0, LCD_W, LCD_H, splash_bg);

    /* Franja inferior semi-fija con el texto funcional (credito, llamado a
     * la accion) — unico contenido dibujado a mano sobre la imagen */
    ILI9341_FillRect(0, 196, LCD_W, 44, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 200, "POR: JIMMY STEBYM ROSERO BARRERA",
                  COLOR_GRAY, COLOR_DARKGRAY, 1);

    /* Subtitulo parpadeante "LISTO PARA JUGAR?" en y=213 — dibujado por
     * Renderer_Update (se actualiza cada 500ms en el dispatcher) */

    draw_string_c(LCD_W / 2, 226, "B1 O UN BOTON PARA CONTINUAR",
                  COLOR_CYAN, COLOR_DARKGRAY, 1);
}

/* ========================================================================== */
/* === MENU DE NIVEL (LEGADO, SIN USO EN EL RECORRIDO ACTUAL) ================ */
/* ========================================================================== */
/* Pantalla de la primera version del proyecto (seleccion de dificultad por
 * velocidad FACIL/MEDIO/PRO), anterior a introducir la seleccion de cantidad
 * de jugadores y de modo de juego -- el recorrido actual (ver DemoScreen_t
 * en main.c) ya no la incluye. Se deja implementada por si se retoma un
 * ajuste de dificultad mas adelante. */

void Renderer_DrawMenu(uint8_t cursor) {
    ILI9341_FillScreen(COLOR_BLACK);

    /* Cabecera */
    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 9, "SELECCIONA DIFICULTAD",
                  COLOR_WHITE, COLOR_DARKGRAY, 1);

    /* Tres tarjetas de nivel: x=15,115,215, cada una 90x90 */
    static const uint16_t card_col[3]  = { COLOR_GREEN,  COLOR_YELLOW, COLOR_RED };
    static const char *const card_name[3] = { "FACIL", "MEDIO", "PRO"  };
    static const char *const card_desc[3] = {
        "VELOCIDAD 1",
        "VELOCIDAD 2",
        "VELOCIDAD 3"
    };

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t bx = (uint16_t)(15 + i * 100);
        uint16_t by = 38;
        uint16_t bw = 90, bh = 90;
        uint16_t inner_bg = (cursor == i) ? COLOR_DARKGRAY : COLOR_BLACK;

        /* Marco exterior del color del nivel */
        ILI9341_FillRect(bx, by, bw, bh, card_col[i]);
        /* Interior */
        ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);

        /* Numero del nivel en 7-seg grande */
        draw_seg_digit((int16_t)(bx + 37), (int16_t)(by + 10),
                       (uint8_t)(i + 1), card_col[i], inner_bg);

        /* Nombre del nivel centrado en la tarjeta */
        uint16_t name_w = str_pixel_w(card_name[i], 2);
        uint16_t name_x = bx + (bw - name_w) / 2;
        draw_string(name_x, by + 60, card_name[i], card_col[i], inner_bg, 2);

        /* Flecha de seleccion bajo la tarjeta activa */
        if (cursor == i) {
            ILI9341_FillRect(bx + 33, by + bh + 4, 24, 8, card_col[i]);
            ILI9341_FillRect(bx + 37, by + bh + 12, 16, 6, card_col[i]);
            ILI9341_FillRect(bx + 41, by + bh + 18, 8,  4, card_col[i]);
        }
    }

    /* Descripcion del nivel seleccionado */
    uint16_t dy = 175;
    ILI9341_FillRect(20, dy, 280, 40, COLOR_DARKGRAY);

    /* Barras de velocidad: 1, 2 o 3 barras */
    for (uint8_t v = 0; v <= cursor; v++) {
        ILI9341_FillRect((uint16_t)(28 + v * 22), dy + 12, 18, 16, card_col[cursor]);
    }

    /* Descripcion de texto */
    draw_string_c(LCD_W / 2, dy + 14, card_desc[cursor],
                  COLOR_WHITE, COLOR_DARKGRAY, 1);

    /* "INICIO / BTN-R PARA CONFIRMAR" pequeno */
    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 226, "BOTON=MUEVE   B1=CONFIRMAR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

/* ========================================================================== */
/* === SELECCION DE JUGADORES ================================================ */
/* ========================================================================== */

/* Dibuja (o borra) UNA tarjeta de "cuantos jugadores", incluida la flechita
 * de seleccion debajo (se borra sola cuando selected=0). */
static void rsj_draw_card(uint8_t i, uint8_t selected) {
    static const char *const nombre[2] = { "1 JUGADOR", "2 JUGADORES" };
    static const uint16_t    color[2]  = { COLOR_P1, COLOR_P2 };

    uint16_t bx = (uint16_t)(30 + i * 150);
    uint16_t by = 55, bw = 130, bh = 140;
    uint16_t inner_bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;

    ILI9341_FillRect(bx, by, bw, bh, color[i]);
    ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);

    draw_seg_digit((int16_t)(bx + bw / 2 - 12), (int16_t)(by + 15),
                   (uint8_t)(i + 1), color[i], inner_bg);

    uint16_t name_w = str_pixel_w(nombre[i], 1);
    draw_string((uint16_t)(bx + (bw - name_w) / 2), (uint16_t)(by + 100),
                nombre[i], color[i], inner_bg, 1);

    if (selected) {
        ILI9341_FillRect(bx + bw / 2 - 12, by + bh + 4,  24, 8, color[i]);
        ILI9341_FillRect(bx + bw / 2 - 8,  by + bh + 12, 16, 6, color[i]);
        ILI9341_FillRect(bx + bw / 2 - 4,  by + bh + 18, 8,  4, color[i]);
    } else {
        ILI9341_FillRect(bx + bw / 2 - 12, by + bh + 4, 24, 18, COLOR_BLACK);
    }
}

void Renderer_DrawSeleccionJugadores(uint8_t cursor) {
    ILI9341_FillScreen(COLOR_BLACK);

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 9, "CUANTOS JUGADORES", COLOR_WHITE, COLOR_DARKGRAY, 1);

    for (uint8_t i = 0; i < 2; i++) rsj_draw_card(i, cursor == i);

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 226, "JOYSTICK/BOTON=MUEVE  CUALQUIERA=CONFIRMA",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

void Renderer_UpdateSeleccionJugadores(uint8_t cursor_ant, uint8_t cursor) {
    if (cursor_ant == cursor) return;
    if (cursor_ant < 2) rsj_draw_card(cursor_ant, 0);
    if (cursor     < 2) rsj_draw_card(cursor, 1);
}

/* ========================================================================== */
/* === INICIALES (NOMBRE DE 3 LETRAS POR JUGADOR) ============================ */
/* ========================================================================== */
/* Pantalla entre JUGADORES y MODO (2026-07-31, pedido explicito del
 * usuario): cada jugador elige 3 letras tipo "iniciales de arcade". La
 * logica de ciclar A-Z y confirmar vive en main.c (MenuIniciales_Procesar) --
 * aca solo se dibuja. */

#define RN_BOX_W    60
#define RN_BOX_H    80
#define RN_BOX_GAP  20
#define RN_BOX_Y    70
#define RN_LETRA_SCALE 6

static uint16_t rn_box_x(uint8_t pos) {
    uint16_t total = 3 * RN_BOX_W + 2 * RN_BOX_GAP;
    uint16_t x0    = (uint16_t)((LCD_W - total) / 2);
    return (uint16_t)(x0 + pos * (RN_BOX_W + RN_BOX_GAP));
}

/* activo=1 (amarillo) -- letra que se esta editando ahora mismo.
 * activo=0 (gris) -- letra ya confirmada. */
static void rn_draw_letra(uint8_t pos, char c, uint8_t activo) {
    uint16_t bx     = rn_box_x(pos);
    uint16_t borde  = activo ? COLOR_YELLOW : COLOR_GRAY;
    char     buf[2] = { c, 0 };

    ILI9341_FillRect(bx, RN_BOX_Y, RN_BOX_W, RN_BOX_H, COLOR_BLACK);
    ILI9341_DrawRect(bx, RN_BOX_Y, RN_BOX_W, RN_BOX_H, borde);
    ILI9341_DrawRect((uint16_t)(bx + 1), (uint16_t)(RN_BOX_Y + 1),
                      (uint16_t)(RN_BOX_W - 2), (uint16_t)(RN_BOX_H - 2), borde);

    uint16_t cy = (uint16_t)(RN_BOX_Y + (RN_BOX_H - CHAR_H(RN_LETRA_SCALE)) / 2);
    draw_string_c((uint16_t)(bx + RN_BOX_W / 2), cy, buf, COLOR_WHITE, COLOR_BLACK, RN_LETRA_SCALE);
}

void Renderer_DrawNombre(uint8_t jugador, const char nombre[4], uint8_t pos_actual) {
    char titulo[24];
    ILI9341_FillScreen(COLOR_BLACK);

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    snprintf(titulo, sizeof(titulo), "JUGADOR %u - TU NOMBRE", (unsigned)(jugador + 1));
    draw_string_c(LCD_W / 2, 9, titulo, (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_DARKGRAY, 1);

    for (uint8_t i = 0; i < 3; i++) rn_draw_letra(i, nombre[i], (i == pos_actual));

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 226, "JOYSTICK=LETRA  BOTON=CONFIRMAR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

void Renderer_UpdateNombreLetra(uint8_t jugador, uint8_t pos, char letra) {
    (void)jugador;
    rn_draw_letra(pos, letra, 1);
}

void Renderer_ConfirmarNombreLetra(uint8_t pos, char letra) {
    rn_draw_letra(pos, letra, 0);
}

/* ========================================================================== */
/* === SELECCION DE MODO DE JUEGO ============================================ */
/* ========================================================================== */

/* Nombres renombrados el 2026-07-28 ("SIMON"->"BOTONES", "SIM+JOY"->"JOYS",
 * "GT HERO" sin cambios) para que coincidan con el dispositivo de entrada de
 * cada modo, mas claro para quien juega que el nombre generico anterior. Los
 * enums internos (DEMO_MODO_SIMON/_SIMONJOY/_GUITAR en main.c) no cambiaron,
 * solo este texto visible. */
static const uint16_t    RSM_CARD_COL[3]  = { COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW };
static const char *const RSM_CARD_NAME[3] = { "BOTONES", "JOYS", "GT HERO" };
static const char *const RSM_CARD_DESC[3] = {
    "4 BOTONES, VELOCIDAD CRECIENTE",
    "SIGUE LA DIRECCION DEL JOYSTICK",
    "NOTAS CAYENDO ESTILO GUITAR HERO"
};

/* Dibuja (o borra) UNA tarjeta de modo, incluida la flechita de seleccion */
static void rsm_draw_card(uint8_t i, uint8_t selected) {
    uint16_t bx = (uint16_t)(15 + i * 100);
    uint16_t by = 38, bw = 90, bh = 90;
    uint16_t inner_bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;

    ILI9341_FillRect(bx, by, bw, bh, RSM_CARD_COL[i]);
    ILI9341_FillRect(bx + 3, by + 3, bw - 6, bh - 6, inner_bg);

    draw_seg_digit((int16_t)(bx + 37), (int16_t)(by + 10),
                   (uint8_t)(i + 1), RSM_CARD_COL[i], inner_bg);

    uint16_t name_w = str_pixel_w(RSM_CARD_NAME[i], 1);
    draw_string((uint16_t)(bx + (bw - name_w) / 2), by + 62, RSM_CARD_NAME[i], RSM_CARD_COL[i], inner_bg, 1);

    if (selected) {
        ILI9341_FillRect(bx + 33, by + bh + 4, 24, 8, RSM_CARD_COL[i]);
        ILI9341_FillRect(bx + 37, by + bh + 12, 16, 6, RSM_CARD_COL[i]);
        ILI9341_FillRect(bx + 41, by + bh + 18, 8,  4, RSM_CARD_COL[i]);
    } else {
        ILI9341_FillRect(bx + 33, by + bh + 4, 24, 18, COLOR_BLACK);
    }
}

static void rsm_draw_desc(uint8_t cursor) {
    uint16_t dy = 175;
    ILI9341_FillRect(20, dy, 280, 40, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, dy + 14, RSM_CARD_DESC[cursor], COLOR_WHITE, COLOR_DARKGRAY, 1);
}

void Renderer_DrawSeleccionModo(uint8_t cursor) {
    ILI9341_FillScreen(COLOR_BLACK);

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 9, "SELECCIONA MODO", COLOR_WHITE, COLOR_DARKGRAY, 1);

    for (uint8_t i = 0; i < 3; i++) rsm_draw_card(i, cursor == i);
    rsm_draw_desc(cursor);

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 226, "BOTON=BOTONES   JOYSTICK=JOYS",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

void Renderer_UpdateSeleccionModo(uint8_t cursor_ant, uint8_t cursor) {
    if (cursor_ant == cursor) return;
    if (cursor_ant < 3) rsm_draw_card(cursor_ant, 0);
    if (cursor     < 3) rsm_draw_card(cursor, 1);
    rsm_draw_desc(cursor);
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

#define COCK_ZONE_W   240
#define COCK_ZONE_H   155
#define COCK_P2_Y     0
#define COCK_DIV_Y    155
#define COCK_DIV_H    10
#define COCK_P1_Y     165

/* Invierte el orden de los 7 bits (filas) de una columna del glifo 5x7. */
static uint8_t reverse7(uint8_t b) {
    uint8_t r = 0;
    for (uint8_t i = 0; i < 7; i++) {
        if (b & (1u << i)) r |= (uint8_t)(1u << (6 - i));
    }
    return r;
}

/* Dibuja un caracter rotado 180 grados (para la mitad invertida del cocktail). */
static void draw_char_180(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c < 32 || c > 90) c = '?';
    const uint8_t *g = font5x7[(uint8_t)c - 32];

    uint16_t adv = CHAR_ADV(scale);
    uint16_t hgt = CHAR_H(scale);
    ILI9341_FillRect(x, y, adv, hgt, bg);

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = reverse7(g[4 - col]);
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1u << row)) {
                ILI9341_FillRect((uint16_t)(x + col * scale), (uint16_t)(y + row * scale),
                                 scale, scale, fg);
            }
        }
    }
}

/* Cadena rotada 180 grados: caracteres en orden inverso, cada uno volteado. */
static void draw_string_180(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t n = 0;
    for (const char *p = s; *p; p++) n++;
    for (uint16_t i = 0; i < n; i++) {
        draw_char_180((uint16_t)(x + i * 6u * scale), y, s[n - 1 - i], fg, bg, scale);
    }
}

/* jugador: 0=J1 (abajo, normal) 1=J2 (arriba, rotado 180). Coordenadas (lx,ly)
 * son LOCALES a la zona 240x155 de cada jugador. */
/* jugador==1 (no 0) es la mitad de ABAJO/normal -- invertido a proposito
 * (2026-07-31, confirmado en hardware real): en el cabinet fisico del
 * usuario la mitad "de abajo" del panel en retrato queda del lado de
 * jugador 2, no del lado del LED rojo de heartbeat de jugador 1. Las 3
 * funciones Cockpit_* de este bloque son las UNICAS que deciden que mitad
 * es cual -- Simon Clasico, Simon+Joystick 2P y Guitar Hero las comparten,
 * asi que este cambio corrige los 3 modos a la vez sin tocar su logica de
 * juego (que sigue indexada 0=jugador1 fisico via BTN1_*, 1=jugador2 fisico
 * via BTN2_*, eso no cambia). */
static void Cockpit_FillRect(uint8_t jugador, uint16_t lx, uint16_t ly, uint16_t lw, uint16_t lh, uint16_t color) {
    if (jugador == 1) {
        ILI9341_FillRect(lx, (uint16_t)(COCK_P1_Y + ly), lw, lh, color);
    } else {
        uint16_t px = (uint16_t)(COCK_ZONE_W - lx - lw);
        uint16_t py = (uint16_t)(COCK_P2_Y + (COCK_ZONE_H - ly - lh));
        ILI9341_FillRect(px, py, lw, lh, color);
    }
}

static void Cockpit_DrawString(uint8_t jugador, uint16_t lx, uint16_t ly, const char *s,
                                uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t sw = str_pixel_w(s, scale);
    uint16_t sh = CHAR_H(scale);
    if (jugador == 1) {
        draw_string(lx, (uint16_t)(COCK_P1_Y + ly), s, fg, bg, scale);
    } else {
        uint16_t px = (uint16_t)(COCK_ZONE_W - lx - sw);
        uint16_t py = (uint16_t)(COCK_P2_Y + (COCK_ZONE_H - ly - sh));
        draw_string_180(px, py, s, fg, bg, scale);
    }
}

/* Cuadricula 2x2 local: 0=Rojo(arriba-izq) 1=Verde(arriba-der)
 *                       2=Azul(abajo-DER)  3=Amarillo(abajo-IZQ)
 * Azul/Amarillo invertidos (2026-07-31, confirmado en hardware real): en el
 * cabinet fisico del usuario el boton azul de la fila de abajo esta a la
 * DERECHA y el amarillo a la IZQUIERDA -- al reves de la suposicion
 * original. El boton/LED fisico ya funcionaba bien (BTN_SW/BTN_LED en
 * main.c no cambian), esto es solo el dibujo en pantalla. */
/* jugador==1 (2026-07-31, confirmado en hardware real): en jugador 2 salia
 * arriba/abajo cruzado en pantalla (verde aparecia donde iba azul, rojo
 * donde iba amarillo -- un intercambio VERTICAL puro, izq/der si estaba
 * bien) aunque jugador 2 usa el camino de dibujo SIN espejo de
 * Cockpit_FillRect -- se compensa aca, a nivel de esta cuadricula
 * especifica, invirtiendo solo la fila (es_abajo) para jugador 2, sin tocar
 * la columna (es_derecha, que ya estaba bien) ni el jugador 1 (confirmado
 * correcto por separado). */
static void cockpit_2x2_rect(uint8_t jugador, uint8_t c, uint16_t *lx, uint16_t *ly, uint16_t *lw, uint16_t *lh) {
    static const uint8_t es_derecha[4] = { 0, 1, 1, 0 };
    static const uint8_t es_abajo[4]   = { 0, 0, 1, 1 };
    uint16_t cw = 70, ch = 55, gap = 10;
    uint16_t gx0 = (COCK_ZONE_W - (2 * cw + gap)) / 2;
    uint16_t gy0 = 30;
    uint8_t  abajo = (jugador == 1) ? (uint8_t)(!es_abajo[c]) : es_abajo[c];
    *lw = cw;
    *lh = ch;
    *lx = es_derecha[c] ? (uint16_t)(gx0 + cw + gap) : gx0;
    *ly = abajo         ? (uint16_t)(gy0 + ch + gap) : gy0;
}

static void cockpit_2x2_draw(uint8_t jugador, uint8_t c, uint8_t activo) {
    uint16_t lx, ly, lw, lh;
    cockpit_2x2_rect(jugador, c, &lx, &ly, &lw, &lh);
    uint16_t col = activo ? NOTE_COLOR[c] : LANE_COLOR[c];
    Cockpit_FillRect(jugador, lx, ly, lw, lh, col);
}

/* Redibuja TODO un jugador (etiqueta + cuadricula 2x2) sin tocar al otro --
 * usado tanto para el dibujo inicial completo como para reiniciar solo un
 * jugador que perdio, sin borrar la partida en curso del otro. */
static void cockpit_clasico_draw_jugador(uint8_t jugador, uint8_t paso) {
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 8, 5, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 2);
    for (uint8_t c = 0; c < 4; c++) cockpit_2x2_draw(jugador, c, paso == c);
}

void Renderer_DrawModoSimonClasico(uint8_t paso_j1, uint8_t paso_j2) {
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY);
    cockpit_clasico_draw_jugador(0, paso_j1);
    cockpit_clasico_draw_jugador(1, paso_j2);
}

void Renderer_DrawModoSimonClasicoJugador(uint8_t jugador, uint8_t paso) {
    cockpit_clasico_draw_jugador(jugador, paso);
}

void Renderer_UpdateModoSimonClasicoPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso) {
    if (paso_ant == paso) return;
    if (paso_ant < 4) cockpit_2x2_draw(jugador, paso_ant, 0);
    if (paso     < 4) cockpit_2x2_draw(jugador, paso, 1);
}

static uint16_t cockpit_clasico_racha_dibujada[2] = { 0xFFFF, 0xFFFF };

void Renderer_ActualizarRachaBotones(uint8_t jugador, uint16_t racha) {
    if (racha == cockpit_clasico_racha_dibujada[jugador]) return;
    char buf[10];
    snprintf(buf, sizeof(buf), "R:%u", (unsigned)racha);
    Cockpit_FillRect(jugador, 186, 2, 50, 14, COLOR_BLACK);
    Cockpit_DrawString(jugador, 188, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    cockpit_clasico_racha_dibujada[jugador] = racha;
}

void Renderer_DibujarGameOverBotones(uint8_t jugador, uint16_t racha, uint16_t mejor) {
    char linea[20];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 50, 35, "GAME OVER", COLOR_RED, COLOR_BLACK, 2);
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)racha);
    Cockpit_DrawString(jugador, 70, 70, linea, COLOR_WHITE, COLOR_BLACK, 1);
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)mejor);
    Cockpit_DrawString(jugador, 70, 90, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 15, 120, "click=cualquier boton", COLOR_GRAY, COLOR_BLACK, 1);
    cockpit_clasico_racha_dibujada[jugador] = 0xFFFF;   /* fuerza redibujo al reiniciar */
}

/* ========================================================================== */
/* === SIMON+JOYSTICK — 2 JUGADORES CARA A CARA (portrait, cockpit) ========== */
/* ========================================================================== */
/* Cada jugador tiene su propio D-pad de flechas (mismo estilo que la version
 * de 1 jugador de mas abajo) dentro de su mitad de la mesa -- usando el
 * mismo mecanismo de rotacion 180 grados que Simon Clasico arriba. */

/* Transforma un PUNTO local (no un rectangulo) -- una rotacion de 180 grados
 * deja el radio de un circulo intacto, asi que para los badges circulares
 * solo hace falta transformar su centro, no todo el area. */
static void Cockpit_Punto(uint8_t jugador, int16_t lx, int16_t ly, int16_t *px, int16_t *py) {
    if (jugador == 1) {
        *px = lx;
        *py = (int16_t)(COCK_P1_Y + ly);
    } else {
        *px = (int16_t)(COCK_ZONE_W - lx);
        *py = (int16_t)(COCK_P2_Y + (COCK_ZONE_H - ly));
    }
}

/* Cuadro local (240x155) de cada boton del D-pad -- cruz centrada, mas
 * angosta que la version de 1 jugador en pantalla completa porque aca
 * comparte la mitad de una pantalla en retrato. */
static void cockpit_pad_rect(uint8_t d, uint16_t *lx, uint16_t *ly, uint16_t *lw, uint16_t *lh) {
    static const int16_t  cx4[4] = { 120, 120,  45, 195 };
    static const int16_t  cy4[4] = {  58, 128,  93,  93 };
    static const uint16_t r4[4]  = {  22,  22,  24,  24 };
    uint16_t r = r4[d];
    *lw = (uint16_t)(2 * r);
    *lh = (uint16_t)(2 * r);
    *lx = (uint16_t)(cx4[d] - r);
    *ly = (uint16_t)(cy4[d] - r);
}

/* Flecha triangular en coordenadas LOCALES, igual construccion que
 * sj1p_dibujar_flecha (FillRect apiladas) pero cada rectangulo pasa por
 * Cockpit_FillRect -- asi "arriba" en local siempre sale como "arriba" para
 * ESE jugador ya rotado, sin duplicar la logica de la flecha en si. */
static void cockpit_dibujar_flecha(uint8_t jugador, uint16_t lbx, uint16_t lby, uint16_t lbw, uint16_t lbh,
                                    uint8_t dir, uint16_t color) {
    int16_t cx   = (int16_t)(lbx + lbw / 2);
    int16_t cy   = (int16_t)(lby + lbh / 2);
    int16_t size = (int16_t)((lbw < lbh ? lbw : lbh) / 2) - 5;
    if (size < 7) size = 7;
    int16_t tallo = (int16_t)(size / 3) + 1;

    switch (dir) {
    case 0:  /* ARRIBA (local) */
        for (int16_t i = 0; i <= size; i++) {
            Cockpit_FillRect(jugador, (uint16_t)(cx - i), (uint16_t)(cy - size + i), (uint16_t)(2 * i + 1), 1, color);
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - tallo / 2), (uint16_t)cy, (uint16_t)tallo, (uint16_t)(size / 2), color);
        break;
    case 1:  /* ABAJO */
        for (int16_t i = 0; i <= size; i++) {
            Cockpit_FillRect(jugador, (uint16_t)(cx - i), (uint16_t)(cy + size - i), (uint16_t)(2 * i + 1), 1, color);
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - tallo / 2), (uint16_t)(cy - size / 2), (uint16_t)tallo, (uint16_t)(size / 2), color);
        break;
    case 2:  /* IZQ */
        for (int16_t i = 0; i <= size; i++) {
            Cockpit_FillRect(jugador, (uint16_t)(cx - size + i), (uint16_t)(cy - i), 1, (uint16_t)(2 * i + 1), color);
        }
        Cockpit_FillRect(jugador, (uint16_t)cx, (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);
        break;
    default: /* DER */
        for (int16_t i = 0; i <= size; i++) {
            Cockpit_FillRect(jugador, (uint16_t)(cx + size - i), (uint16_t)(cy - i), 1, (uint16_t)(2 * i + 1), color);
        }
        Cockpit_FillRect(jugador, (uint16_t)(cx - size / 2), (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);
        break;
    }
}

static void cockpit_pad_draw(uint8_t jugador, uint8_t d, uint8_t activo) {
    uint16_t lx, ly, lw, lh;
    cockpit_pad_rect(d, &lx, &ly, &lw, &lh);

    int16_t lcx = (int16_t)(lx + lw / 2);
    int16_t lcy = (int16_t)(ly + lh / 2);
    int16_t r   = (int16_t)(lw / 2);
    uint16_t badge_col  = activo ? NOTE_COLOR[d] : LANE_COLOR[d];
    uint16_t flecha_col = activo ? COLOR_BLACK   : NOTE_COLOR[d];

    int16_t acx, acy;
    Cockpit_Punto(jugador, lcx, lcy, &acx, &acy);
    ILI9341_FillCircle(acx, acy, r, badge_col);

    /* SIN compensar la direccion (2026-07-31, dos intentos anteriores de
     * "espejar" la flecha resultaron mal -- confirmado en hardware real que
     * las hacia apuntar hacia ADENTRO del D-pad en vez de afuera). El texto
     * necesita draw_string_180 porque queremos que siga leyendose igual
     * (una convencion humana fija) pese a rotar 180 -- pero una flecha que
     * apunta "hacia afuera" es rotacionalmente consistente: al dejar que la
     * MISMA transformacion de Cockpit_FillRect rote posicion Y forma juntas
     * (sin agregar ningun espejado extra), la flecha sigue apuntando hacia
     * afuera del D-pad para cualquiera de los 2 jugadores. */
    cockpit_dibujar_flecha(jugador, lx, ly, lw, lh, d, flecha_col);
}

/* Redibuja TODO un jugador (etiqueta + 4 badges) sin tocar al otro --
 * usado tanto para el dibujo inicial completo como para reiniciar solo un
 * jugador que perdio, sin borrar la partida en curso del otro. */
static void cockpit_draw_jugador(uint8_t jugador, uint8_t paso) {
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 8, 4, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 1);
    for (uint8_t d = 0; d < 4; d++) cockpit_pad_draw(jugador, d, paso == d);
}

void Renderer_DrawModoSimonJoystick2P(uint8_t paso_j1, uint8_t paso_j2) {
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY);
    cockpit_draw_jugador(0, paso_j1);
    cockpit_draw_jugador(1, paso_j2);
}

void Renderer_DrawModoSimonJoystick2PJugador(uint8_t jugador, uint8_t paso) {
    cockpit_draw_jugador(jugador, paso);
}

void Renderer_UpdateModoSimonJoystick2PPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso) {
    if (paso_ant == paso) return;
    if (paso_ant < 4) cockpit_pad_draw(jugador, paso_ant, 0);
    if (paso     < 4) cockpit_pad_draw(jugador, paso, 1);
}

static uint16_t cockpit_racha_dibujada[2] = { 0xFFFF, 0xFFFF };

void Renderer_ActualizarRachaJoystick2P(uint8_t jugador, uint16_t racha) {
    if (racha == cockpit_racha_dibujada[jugador]) return;
    char buf[10];
    snprintf(buf, sizeof(buf), "R:%u", (unsigned)racha);
    Cockpit_FillRect(jugador, 186, 2, 50, 14, COLOR_BLACK);
    Cockpit_DrawString(jugador, 188, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    cockpit_racha_dibujada[jugador] = racha;
}

void Renderer_DibujarGameOverJoystick2P(uint8_t jugador, uint16_t racha, uint16_t mejor) {
    char linea[20];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 50, 35, "GAME OVER", COLOR_RED, COLOR_BLACK, 2);
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)racha);
    Cockpit_DrawString(jugador, 70, 70, linea, COLOR_WHITE, COLOR_BLACK, 1);
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)mejor);
    Cockpit_DrawString(jugador, 70, 90, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 18, 120, "mueve=reintentar", COLOR_GRAY, COLOR_BLACK, 1);
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
#define GH_LANE_H      32
#define GH_SEP_H       2
#define GH_LANE_TOP(n) ((uint16_t)(GH_LANE_Y0 + (n) * (GH_LANE_H + GH_SEP_H)))
#define GH_ZONA_R      13     /* radio visual de la zona de golpe circular   */
#define GH_NOTE_H      24     /* < GH_LANE_H para no tocar los bordes finos  */

static inline uint16_t gh_note_ly(uint8_t carril) {
    return (uint16_t)(GH_LANE_TOP(carril) + (GH_LANE_H - GH_NOTE_H) / 2);
}

static void gh_draw_carril(uint8_t jugador, uint8_t c) {
    uint16_t ly = GH_LANE_TOP(c);
    Cockpit_FillRect(jugador, 0, ly, COCK_ZONE_W, GH_LANE_H, COLOR_BLACK);
    Cockpit_FillRect(jugador, 0, ly, COCK_ZONE_W, 1, NOTE_COLOR[c]);
    Cockpit_FillRect(jugador, 0, (uint16_t)(ly + GH_LANE_H - 1), COCK_ZONE_W, 1, NOTE_COLOR[c]);

    int16_t acx, acy;
    Cockpit_Punto(jugador, GH_ZONA_CX, (int16_t)(ly + GH_LANE_H / 2), &acx, &acy);
    ILI9341_FillCircle(acx, acy, GH_ZONA_R, PRESS_COLOR[c]);
}

static void gh_draw_jugador(uint8_t jugador) {
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 8, 4, Nombre_Jugador(jugador),
                        (jugador == 0) ? COLOR_P1 : COLOR_P2, COLOR_BLACK, 1);
    for (uint8_t c = 0; c < 4; c++) gh_draw_carril(jugador, c);
}

void Renderer_DrawModoGuitarHero2P(void) {
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_FillRect(0, COCK_DIV_Y, COCK_ZONE_W, COCK_DIV_H, COLOR_DARKGRAY);
    gh_draw_jugador(0);
    gh_draw_jugador(1);
}

void Renderer_DrawModoGuitarHeroJugador(uint8_t jugador) {
    gh_draw_jugador(jugador);
}

static uint16_t gh_puntaje_dibujado[2] = { 0xFFFF, 0xFFFF };
static uint16_t gh_combo_dibujado[2]   = { 0xFFFF, 0xFFFF };

void Renderer_GH_ActualizarPuntaje(uint8_t jugador, uint16_t puntaje, uint16_t combo) {
    if (puntaje == gh_puntaje_dibujado[jugador] && combo == gh_combo_dibujado[jugador]) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "P:%u C:%u", (unsigned)puntaje, (unsigned)combo);
    Cockpit_FillRect(jugador, 130, 2, 106, 14, COLOR_BLACK);
    Cockpit_DrawString(jugador, 132, 3, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    gh_puntaje_dibujado[jugador] = puntaje;
    gh_combo_dibujado[jugador]   = combo;
}

void Renderer_GH_DrawNota(uint8_t jugador, const Nota_t *nota) {
    if (!nota->activa) return;
    int16_t x = nota->x_rel;
    if (x + (int16_t)NOTE_W <= 0 || x >= (int16_t)COCK_ZONE_W) return;

    int16_t dx = x, dw = NOTE_W;
    if (dx < 0) { dw += dx; dx = 0; }
    if (dx + dw > (int16_t)COCK_ZONE_W) dw = (int16_t)COCK_ZONE_W - dx;
    if (dw <= 0) return;

    Cockpit_FillRect(jugador, (uint16_t)dx, gh_note_ly(nota->carril), (uint16_t)dw, GH_NOTE_H, NOTE_COLOR[nota->carril]);
}

void Renderer_GH_EraseNotaTrail(uint8_t jugador, const Nota_t *nota, uint8_t speed) {
    if (!nota->activa) return;

    /* la nota viaja hacia IZQUIERDA (x decreciente) -- la franja que queda
     * al descubierto es el borde DERECHO de la posicion anterior. */
    int16_t ex_start = nota->x_prev + (int16_t)NOTE_W - (int16_t)speed;
    int16_t ex_end   = nota->x_prev + (int16_t)NOTE_W;
    if (ex_start < 0) ex_start = 0;
    if (ex_end > (int16_t)COCK_ZONE_W) ex_end = (int16_t)COCK_ZONE_W;
    int16_t ew = ex_end - ex_start;
    if (ew <= 0) return;

    uint16_t ly = gh_note_ly(nota->carril);
    Cockpit_FillRect(jugador, (uint16_t)ex_start, ly, (uint16_t)ew, GH_NOTE_H, COLOR_BLACK);

    /* si la franja borrada se mete en el area de la zona de golpe circular,
     * redibujar el circulo completo encima -- un FillRect no sabe recortar
     * un circulo, asi que se repinta entero (barato, un FillCircle) en vez
     * de intentar una interseccion rectangulo/circulo exacta. */
    int16_t zona_x0 = (int16_t)GH_ZONA_CX - (int16_t)GH_ZONA_R;
    int16_t zona_x1 = (int16_t)GH_ZONA_CX + (int16_t)GH_ZONA_R;
    if (ex_start < zona_x1 && ex_end > zona_x0) {
        int16_t acx, acy;
        Cockpit_Punto(jugador, GH_ZONA_CX, (int16_t)(GH_LANE_TOP(nota->carril) + GH_LANE_H / 2), &acx, &acy);
        ILI9341_FillCircle(acx, acy, GH_ZONA_R, PRESS_COLOR[nota->carril]);
    }
}

void Renderer_GH_DibujarFin(uint8_t jugador, uint16_t puntaje) {
    char linea[24];
    Cockpit_FillRect(jugador, 0, 0, COCK_ZONE_W, COCK_ZONE_H, COLOR_BLACK);
    Cockpit_DrawString(jugador, 30, 40, "RONDA COMPLETA", COLOR_GREEN, COLOR_BLACK, 1);
    snprintf(linea, sizeof(linea), "Puntaje: %u", (unsigned)puntaje);
    Cockpit_DrawString(jugador, 60, 70, linea, COLOR_YELLOW, COLOR_BLACK, 1);
    Cockpit_DrawString(jugador, 20, 100, "boton=jugar de nuevo", COLOR_GRAY, COLOR_BLACK, 1);
    gh_puntaje_dibujado[jugador] = 0xFFFF;
    gh_combo_dibujado[jugador]   = 0xFFFF;
}

/* ========================================================================== */
/* === SIMON+JOYSTICK — PANTALLA COMPLETA PARA 1 SOLO JUGADOR ================ */
/* ========================================================================== */
/* Solo existe la version de 1 jugador -- flechas grandes centradas, usando
 * toda la pantalla. La version vieja dividida en 2 mitades (con texto
 * ARRIBA/ABAJO/IZQ/DER) se borro por pedido del usuario: ya no se usaba
 * desde ningun lado (el juego real y la vista previa del recorrido de
 * diseño usan esta version de 1 jugador desde 2026-07-28). */

static void sj1p_pad_rect(uint8_t d, uint16_t *bx, uint16_t *by, uint16_t *bw, uint16_t *bh) {
    /* ARRIBA/ABAJO un poco mas altas (84x68), IZQ/DER un poco mas angostas
     * (84x60) para que quepan en el hueco vertical entre las otras dos sin
     * traslaparse -- todas mas grandes que antes (74x60). */
    uint16_t xw = 84, xh4[4] = { 68, 68, 60, 60 };
    uint16_t x4[4] = { (uint16_t)(LCD_W / 2 - xw / 2), (uint16_t)(LCD_W / 2 - xw / 2), 16, (uint16_t)(LCD_W - 16 - xw) };
    uint16_t y4[4] = { 34, 166, 104, 104 };
    *bw = xw; *bh = xh4[d];
    *bx = x4[d]; *by = y4[d];
}

/* Flecha triangular (0=ARRIBA 1=ABAJO 2=IZQ 3=DER) centrada en el cuadro
 * del D-pad, armada con FillRect apiladas (sin primitiva de triangulo en
 * el driver). Mismo tamaño/posicion siempre -- redibujar con otro color
 * sobreescribe la anterior por completo, sin dejar residuos. */
static void sj1p_dibujar_flecha(uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,
                                 uint8_t dir, uint16_t color) {
    int16_t cx   = (int16_t)(bx + bw / 2);
    int16_t cy   = (int16_t)(by + bh / 2);
    int16_t size = (int16_t)((bw < bh ? bw : bh) / 2) - 6;
    if (size < 8) size = 8;
    int16_t tallo = (int16_t)(size / 3) + 1;

    switch (dir) {
    case 0:  /* ARRIBA */
        for (int16_t i = 0; i <= size; i++) {
            ILI9341_FillRect((uint16_t)(cx - i), (uint16_t)(cy - size + i), (uint16_t)(2 * i + 1), 1, color);
        }
        ILI9341_FillRect((uint16_t)(cx - tallo / 2), (uint16_t)cy, (uint16_t)tallo, (uint16_t)(size / 2), color);
        break;
    case 1:  /* ABAJO */
        for (int16_t i = 0; i <= size; i++) {
            ILI9341_FillRect((uint16_t)(cx - i), (uint16_t)(cy + size - i), (uint16_t)(2 * i + 1), 1, color);
        }
        ILI9341_FillRect((uint16_t)(cx - tallo / 2), (uint16_t)(cy - size / 2), (uint16_t)tallo, (uint16_t)(size / 2), color);
        break;
    case 2:  /* IZQ */
        for (int16_t i = 0; i <= size; i++) {
            ILI9341_FillRect((uint16_t)(cx - size + i), (uint16_t)(cy - i), 1, (uint16_t)(2 * i + 1), color);
        }
        ILI9341_FillRect((uint16_t)cx, (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);
        break;
    default: /* DER */
        for (int16_t i = 0; i <= size; i++) {
            ILI9341_FillRect((uint16_t)(cx + size - i), (uint16_t)(cy - i), 1, (uint16_t)(2 * i + 1), color);
        }
        ILI9341_FillRect((uint16_t)(cx - size / 2), (uint16_t)(cy - tallo / 2), (uint16_t)(size / 2), (uint16_t)tallo, color);
        break;
    }
}

/* Solo flecha, sin caja de fondo ni nombre de texto, pero cada direccion
 * con su propio color (NOTE_COLOR/LANE_COLOR, los mismos 4 de siempre) --
 * apagada = version oscura, encendida = version brillante. */
static void sj1p_pad_draw(uint8_t d, uint8_t activo) {
    uint16_t bx, by, bw, bh;
    sj1p_pad_rect(d, &bx, &by, &bw, &bh);
    sj1p_dibujar_flecha(bx, by, bw, bh, d, activo ? NOTE_COLOR[d] : LANE_COLOR[d]);
}

void Renderer_DrawModoSimonJoystick1P(uint8_t paso) {
    ILI9341_FillScreen(COLOR_BLACK);

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 9, "SIMON + JOYSTICK", COLOR_WHITE, COLOR_DARKGRAY, 1);

    for (uint8_t d = 0; d < 4; d++) sj1p_pad_draw(d, paso == d);
}

void Renderer_UpdateModoSimonJoystick1P(uint8_t paso_ant, uint8_t paso) {
    if (paso_ant == paso) return;
    if (paso_ant < 4) sj1p_pad_draw(paso_ant, 0);
    if (paso     < 4) sj1p_pad_draw(paso, 1);
}

/* Cursor en forma de "X" que sigue la posicion CRUDA del joystick dentro
 * del hueco vacio del centro del D-pad -- igual que el cursor del OLED de
 * examen_parcial, pero moviendose de verdad en vez de una marca fija.
 * Verde = centrado y listo para el siguiente movimiento, gris = inclinado. */
#define SJ1P_CURSOR_X0 100
#define SJ1P_CURSOR_X1 220
#define SJ1P_CURSOR_Y0 108
#define SJ1P_CURSOR_Y1 158
#define SJ1P_CURSOR_R    6

static void sj1p_borrar_cursor(uint16_t px, uint16_t py) {
    uint16_t d = (SJ1P_CURSOR_R + 1) * 2 + 1;
    ILI9341_FillRect((uint16_t)(px - SJ1P_CURSOR_R - 1), (uint16_t)(py - SJ1P_CURSOR_R - 1),
                      d, d, COLOR_BLACK);
}

static void sj1p_dibujar_cursor(uint16_t px, uint16_t py, uint16_t col) {
    ILI9341_DrawLine((int16_t)(px - SJ1P_CURSOR_R), (int16_t)(py - SJ1P_CURSOR_R),
                      (int16_t)(px + SJ1P_CURSOR_R), (int16_t)(py + SJ1P_CURSOR_R), col);
    ILI9341_DrawLine((int16_t)(px - SJ1P_CURSOR_R), (int16_t)(py + SJ1P_CURSOR_R),
                      (int16_t)(px + SJ1P_CURSOR_R), (int16_t)(py - SJ1P_CURSOR_R), col);
}

/* ultima posicion dibujada -- 0xFFFF = todavia no se ha dibujado */
static uint16_t sj1p_cursor_px = 0xFFFF, sj1p_cursor_py = 0xFFFF;

void Renderer_ResetCursorJoystick(void) {
    sj1p_cursor_px = 0xFFFF;
    sj1p_cursor_py = 0xFFFF;
}

/* joy_x/joy_y: cuenta cruda del adc (0-4095). Mapea internamente al hueco
 * del centro del D-pad y solo redibuja si la posicion en pantalla cambio
 * (evita trafico SPI innecesario por jitter sub-pixel del filtro). */
void Renderer_ActualizarCursorJoystick(uint16_t joy_x, uint16_t joy_y, uint8_t listo) {
    /* EJES CRUZADOS -- mismo cruce electrico X<->Y de Joystick_LeerDireccion
     * en main.c (confirmado contra examen_parcial, mismo joystick): px se
     * mueve con el canal electrico "y" y py con el canal "x". */
    uint16_t px = (uint16_t)(SJ1P_CURSOR_X0 + ((uint32_t)joy_y * (SJ1P_CURSOR_X1 - SJ1P_CURSOR_X0)) / 4095u);
    uint16_t py = (uint16_t)(SJ1P_CURSOR_Y0 + ((uint32_t)joy_x * (SJ1P_CURSOR_Y1 - SJ1P_CURSOR_Y0)) / 4095u);

    if (px == sj1p_cursor_px && py == sj1p_cursor_py) return;

    /* DIAGNOSTICO TEMPORAL (2026-07-31) -- para confirmar con datos reales,
     * no razonamiento, si la formula de px/py esta realmente mal. Borrar
     * despues de confirmar. */
    printf("[CURSOR1P] joy_x=%u joy_y=%u -> px=%u py=%u\r\n", joy_x, joy_y, px, py);

    if (sj1p_cursor_px != 0xFFFF) sj1p_borrar_cursor(sj1p_cursor_px, sj1p_cursor_py);
    sj1p_dibujar_cursor(px, py, listo ? COLOR_GREEN : COLOR_GRAY);
    sj1p_cursor_px = px;
    sj1p_cursor_py = py;
}

/* Cursor "X" por jugador para el modo cara-a-cara (2026-07-31, pedido
 * explicito del usuario para poder verificar a simple vista que cada lado
 * mueve el joystick fisico correcto -- mismo cursor que ya existia para 1
 * jugador, pero uno independiente por mitad, pasando por Cockpit_Punto para
 * que la orientacion (mitad normal/rotada) salga bien sola. Vive en el
 * hueco vacio del centro de la cruz de flechas de cockpit_pad_rect. */
#define COCKP_CURSOR_X0  95
#define COCKP_CURSOR_X1  145
#define COCKP_CURSOR_Y0  83
#define COCKP_CURSOR_Y1  103
#define COCKP_CURSOR_R    5

static uint16_t cockp_cursor_px[2] = { 0xFFFF, 0xFFFF };
static uint16_t cockp_cursor_py[2] = { 0xFFFF, 0xFFFF };

static void cockp_borrar_cursor(uint8_t jugador, uint16_t lx, uint16_t ly) {
    uint16_t d = (COCKP_CURSOR_R + 1) * 2 + 1;
    Cockpit_FillRect(jugador, (uint16_t)(lx - COCKP_CURSOR_R - 1), (uint16_t)(ly - COCKP_CURSOR_R - 1), d, d, COLOR_BLACK);
}

static void cockp_dibujar_cursor(uint8_t jugador, uint16_t lx, uint16_t ly, uint16_t col) {
    int16_t ax, ay, bx, by;
    Cockpit_Punto(jugador, (int16_t)(lx - COCKP_CURSOR_R), (int16_t)(ly - COCKP_CURSOR_R), &ax, &ay);
    Cockpit_Punto(jugador, (int16_t)(lx + COCKP_CURSOR_R), (int16_t)(ly + COCKP_CURSOR_R), &bx, &by);
    ILI9341_DrawLine(ax, ay, bx, by, col);
    Cockpit_Punto(jugador, (int16_t)(lx - COCKP_CURSOR_R), (int16_t)(ly + COCKP_CURSOR_R), &ax, &ay);
    Cockpit_Punto(jugador, (int16_t)(lx + COCKP_CURSOR_R), (int16_t)(ly - COCKP_CURSOR_R), &bx, &by);
    ILI9341_DrawLine(ax, ay, bx, by, col);
}

/* olvida la ultima posicion dibujada de ESE jugador -- llamar al (re)iniciar
 * su lado para que el proximo Actualizar no intente borrar una posicion de
 * una partida anterior */
void Renderer_ResetCursorJoystick2P(uint8_t jugador) {
    cockp_cursor_px[jugador] = 0xFFFF;
    cockp_cursor_py[jugador] = 0xFFFF;
}

void Renderer_ActualizarCursorJoystick2P(uint8_t jugador, uint16_t joy_x, uint16_t joy_y, uint8_t listo) {
    /* EJES CRUZADOS -- ver comentario en Renderer_ActualizarCursorJoystick. */
    uint16_t px = (uint16_t)(COCKP_CURSOR_X0 + ((uint32_t)joy_y * (COCKP_CURSOR_X1 - COCKP_CURSOR_X0)) / 4095u);
    uint16_t py = (uint16_t)(COCKP_CURSOR_Y0 + ((uint32_t)joy_x * (COCKP_CURSOR_Y1 - COCKP_CURSOR_Y0)) / 4095u);

    if (px == cockp_cursor_px[jugador] && py == cockp_cursor_py[jugador]) return;

    if (cockp_cursor_px[jugador] != 0xFFFF) cockp_borrar_cursor(jugador, cockp_cursor_px[jugador], cockp_cursor_py[jugador]);
    cockp_dibujar_cursor(jugador, px, py, listo ? COLOR_GREEN : COLOR_GRAY);
    cockp_cursor_px[jugador] = px;
    cockp_cursor_py[jugador] = py;
}

/* ========================================================================== */
/* === LISTA DE CANCIONES (pantalla "reproductor") =========================== */
/* ========================================================================== */
/* El ORDEN debe coincidir exactamente con CANCIONES_NOMBRE/DATA/LEN en main.c */
#define RLC_N 6
static const char *const RLC_NOMBRE[RLC_N] = {
    "BIENVENIDA", "ESTRELLITA", "HIMNO ALEGRIA", "MARTINILLO", "NAVIDAD", "TETRIS"
};

#define RLC_ROW_Y0  30
#define RLC_ROW_H   30

static void rlc_draw_row(uint8_t i, uint8_t selected) {
    uint16_t ry = (uint16_t)(RLC_ROW_Y0 + i * RLC_ROW_H);
    uint16_t bg = selected ? COLOR_DARKGRAY : COLOR_BLACK;

    ILI9341_FillRect(0, ry, LCD_W, RLC_ROW_H - 2, bg);
    draw_string(selected ? 30 : 20, (uint16_t)(ry + 8), RLC_NOMBRE[i],
                selected ? COLOR_YELLOW : COLOR_WHITE, bg, 1);
    if (selected) {
        draw_string(10, (uint16_t)(ry + 8), ">", COLOR_YELLOW, bg, 1);
    }
}

void Renderer_DrawListaCanciones(uint8_t cursor) {
    ILI9341_FillScreen(COLOR_BLACK);

    ILI9341_FillRect(0, 0, LCD_W, 26, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 9, "CANCIONES", COLOR_WHITE, COLOR_DARKGRAY, 1);

    for (uint8_t i = 0; i < RLC_N; i++) rlc_draw_row(i, cursor == i);

    ILI9341_FillRect(0, 220, LCD_W, 20, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 226, "BOTON=REPRODUCIR   B1=SALIR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

void Renderer_UpdateListaCanciones(uint8_t cursor_ant, uint8_t cursor) {
    if (cursor_ant == cursor) return;
    if (cursor_ant < RLC_N) rlc_draw_row(cursor_ant, 0);
    if (cursor     < RLC_N) rlc_draw_row(cursor, 1);
}

/* ========================================================================== */
/* === CONTEO REGRESIVO ===================================================== */
/* ========================================================================== */

void Renderer_DrawConteo(uint8_t numero) {
    uint16_t bx = 100, by = 55, bw = 120, bh = 130;
    if (numero == 3) ILI9341_FillScreen(COLOR_BLACK);  /* solo al entrar, limpia lo anterior */
    ILI9341_FillRect(bx, by, bw, bh, COLOR_BLACK);

    if (numero > 0) {
        static const uint16_t cnt_col[4] = {
            COLOR_BLACK, COLOR_GREEN, COLOR_YELLOW, COLOR_RED
        };
        uint16_t col = cnt_col[numero < 4 ? numero : 3];

        /* Circulo simulado */
        ILI9341_FillRect(bx + 15, by + 10, 90, 90, col);
        ILI9341_FillRect(bx + 25, by + 20, 70, 70, COLOR_BLACK);

        /* Numero grande (dos juegos de 7-seg superpuestos para efecto grueso) */
        draw_seg_digit((int16_t)(bx + 42), (int16_t)(by + 28),
                       numero, col, COLOR_BLACK);
        draw_seg_digit((int16_t)(bx + 44), (int16_t)(by + 30),
                       numero, col, COLOR_BLACK);

    } else {
        /* "GO!" en pixel art */
        ILI9341_FillRect(bx + 5,  by + 20, 110, 28, COLOR_GREEN);
        ILI9341_FillRect(bx + 10, by + 25,  100, 18, COLOR_BLACK);
        ILI9341_FillRect(bx + 5,  by + 55, 110, 28, COLOR_GREEN);
        ILI9341_FillRect(bx + 10, by + 60, 100, 18, COLOR_BLACK);

        /* "GO" con fuente escala 4 */
        draw_string((uint16_t)(bx + 12), (uint16_t)(by + 47), "GO",
                    COLOR_GREEN, COLOR_BLACK, 4);
    }
}

/* ========================================================================== */
/* === PANTALLA DE RESULTADO ================================================ */
/* ========================================================================== */

void Renderer_DrawResultado(const GameState_t *gs) {
    ILI9341_FillScreen(COLOR_BLACK);

    uint16_t s1 = gs->j[0].puntaje;
    uint16_t s2 = gs->j[1].puntaje;

    /* === Cabecera "FIN DEL JUEGO" === */
    ILI9341_FillRect(0, 0, LCD_W, 24, COLOR_DARKGRAY);
    draw_string_c(LCD_W / 2, 8, "FIN DEL JUEGO", COLOR_WHITE, COLOR_DARKGRAY, 1);

    /* === Puntajes individuales === */
    /* J1 — izquierda */
    ILI9341_FillRect(10, 30, 130, 70, COLOR_DARKGRAY);
    ILI9341_FillRect(12, 32, 20, 20, COLOR_P1);
    draw_char(16, 37, 'J', COLOR_WHITE, COLOR_P1, 1);
    draw_char(23, 37, '1', COLOR_WHITE, COLOR_P1, 1);
    /* 4 digitos del puntaje J1 */
    {
        uint8_t d[4] = {
            (uint8_t)((s1/1000)%10),(uint8_t)((s1/100)%10),
            (uint8_t)((s1/10)%10),(uint8_t)(s1%10)
        };
        for (int i = 0; i < 4; i++) {
            draw_seg_digit((int16_t)(20 + i * (SEG_DX + 1)), 55,
                           d[i], COLOR_P1, COLOR_DARKGRAY);
        }
    }

    /* J2 — derecha */
    ILI9341_FillRect(180, 30, 130, 70, COLOR_DARKGRAY);
    ILI9341_FillRect(182, 32, 20, 20, COLOR_P2);
    draw_char(186, 37, 'J', COLOR_WHITE, COLOR_P2, 1);
    draw_char(193, 37, '2', COLOR_WHITE, COLOR_P2, 1);
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
    uint16_t gy = 108;
    if (s1 > s2) {
        /* Jugador 1 gana */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_P1);
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);
        draw_string_c(LCD_W/2, gy + 12, "GANADOR", COLOR_P1, COLOR_BLACK, 2);
        draw_string_c(LCD_W/2, gy + 40, "JUGADOR 1", COLOR_P1, COLOR_BLACK, 2);

    } else if (s2 > s1) {
        /* Jugador 2 gana */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_P2);
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);
        draw_string_c(LCD_W/2, gy + 12, "GANADOR", COLOR_P2, COLOR_BLACK, 2);
        draw_string_c(LCD_W/2, gy + 40, "JUGADOR 2", COLOR_P2, COLOR_BLACK, 2);

    } else {
        /* Empate */
        ILI9341_FillRect(20, gy, 280, 80, COLOR_YELLOW);
        ILI9341_FillRect(24, gy+4, 272, 72, COLOR_BLACK);
        draw_string_c(LCD_W/2, gy + 22, "EMPATE", COLOR_YELLOW, COLOR_BLACK, 3);
    }

    /* === Comparacion de combo maximo (texto) === */
    ILI9341_FillRect(0, 200, LCD_W, 16, COLOR_DARKGRAY);
    draw_string(10,  203, "COMBO:", COLOR_GRAY, COLOR_DARKGRAY, 1);
    draw_uint16(52,  203, gs->j[0].combo, COLOR_P1, COLOR_DARKGRAY, 1);
    draw_string(220, 203, "COMBO:", COLOR_GRAY, COLOR_DARKGRAY, 1);
    draw_uint16(262, 203, gs->j[1].combo, COLOR_P2, COLOR_DARKGRAY, 1);

    /* === "Presiona START" === */
    ILI9341_FillRect(0, 222, LCD_W, 18, COLOR_DARKGRAY);
    draw_string_c(LCD_W/2, 227, "PRESIONA START", COLOR_GREEN, COLOR_DARKGRAY, 1);
}

/* ========================================================================== */
/* === ACTUALIZACION DE SCORES Y COMBO (JUGANDO) ============================ */
/* ========================================================================== */

void Renderer_UpdateScores(const GameState_t *gs) {
    for (uint8_t p = 0; p < 2; p++) {
        uint16_t xo  = PLAYER_X_OFF[p];
        uint16_t col = (p == 0) ? COLOR_P1 : COLOR_P2;

        draw_score_bar(xo, gs->j[p].puntaje, col);
        draw_score_digits(xo, gs->j[p].puntaje, col, COLOR_DARKGRAY);

        /* Combo: "X{N}" en la zona libre de la score bar (y=18, x=xo+23) */
        uint16_t combo = gs->j[p].combo;
        if (combo > 1) {
            draw_char(xo + 23, 18, 'X', COLOR_WHITE, COLOR_DARKGRAY, 1);
            draw_uint16(xo + 30, 18, combo, COLOR_YELLOW, COLOR_DARKGRAY, 1);
        } else {
            ILI9341_FillRect(xo + 23, 18, 36, 7, COLOR_DARKGRAY);
        }
    }
}

/* ========================================================================== */
/* === NOTA: DIBUJO Y BORRADO DELTA ========================================= */
/* ========================================================================== */

void Renderer_FlashPressZone(uint8_t jugador, uint8_t carril, uint16_t color) {
    uint16_t xo = PLAYER_X_OFF[jugador];
    uint16_t ly = LANE_TOP(carril);
    ILI9341_FillRect(xo, ly + NOTE_Y_PAD, PRESS_ZONE_W, NOTE_H, color);
}

void Renderer_DrawNota(const Nota_t *nota, uint16_t x_off) {
    if (!nota->activa) return;
    int16_t x_abs = (int16_t)x_off + nota->x_rel;
    if (x_abs + (int16_t)NOTE_W <= 0 || x_abs >= (int16_t)PLAYER_W) return;

    int16_t dx = x_abs;
    int16_t dw = NOTE_W;
    if (dx < 0)              { dw += dx; dx = 0; }
    if (dx + dw > (int16_t)PLAYER_W) dw = (int16_t)PLAYER_W - dx;
    if (dw <= 0) return;

    ILI9341_FillRect((uint16_t)((int16_t)x_off + dx),
                     note_y(nota->carril),
                     (uint16_t)dw, NOTE_H,
                     NOTE_COLOR[nota->carril]);
}

void Renderer_EraseNotaTrail(const Nota_t *nota, uint16_t x_off, uint8_t speed) {
    if (!nota->activa) return;

    int16_t ex_start = nota->x_prev + (int16_t)NOTE_W - (int16_t)speed;
    int16_t ex_end   = nota->x_prev + (int16_t)NOTE_W;

    if (ex_start < 0)               ex_start = 0;
    if (ex_end > (int16_t)PLAYER_W) ex_end   = (int16_t)PLAYER_W;
    int16_t ew = ex_end - ex_start;
    if (ew <= 0) return;

    uint16_t abs_ex = (uint16_t)((int16_t)x_off + ex_start);
    uint16_t ly     = note_y(nota->carril);

    /* Repintar con el fondo correcto segun zona */
    if (ex_start >= (int16_t)PRESS_ZONE_W) {
        /* Todo en zona de carril */
        ILI9341_FillRect(abs_ex, ly, (uint16_t)ew, NOTE_H,
                         LANE_COLOR[nota->carril]);
    } else if (ex_end <= (int16_t)PRESS_ZONE_W) {
        /* Todo en zona de presion */
        ILI9341_FillRect(abs_ex, ly, (uint16_t)ew, NOTE_H,
                         PRESS_COLOR[nota->carril]);
    } else {
        /* Cruza la frontera: dos rectangulos */
        int16_t press_w = (int16_t)PRESS_ZONE_W - ex_start;
        ILI9341_FillRect(abs_ex, ly, (uint16_t)press_w, NOTE_H,
                         PRESS_COLOR[nota->carril]);
        uint16_t abs_ls = (uint16_t)((int16_t)x_off + (int16_t)PRESS_ZONE_W);
        ILI9341_FillRect(abs_ls, ly,
                         (uint16_t)(ex_end - (int16_t)PRESS_ZONE_W), NOTE_H,
                         LANE_COLOR[nota->carril]);
    }
}

/* ========================================================================== */
/* === DISPATCHER PRINCIPAL ================================================= */
/* ========================================================================== */

static void draw_heartbeat(void) {
    static uint32_t hb_tick = 0;
    static uint8_t  hb_on   = 0;
    uint32_t now = HAL_GetTick();
    if (now - hb_tick >= 500) {
        hb_tick = now;
        hb_on   = !hb_on;
        ILI9341_FillRect(LCD_W - 10, 1, 8, 8,
                         hb_on ? COLOR_WHITE : COLOR_BLACK);
    }
}

void Renderer_Update(GameState_t *gs) {
    draw_heartbeat();

    switch (gs->estado) {

    case ESTADO_SPLASH:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawSplash();
        }
        /* Parpadeo "LISTO PARA JUGAR?" cada 500ms */
        {
            static uint32_t blink_tick = 0;
            static uint8_t  blink_on   = 1;
            uint32_t now = HAL_GetTick();
            if (now - blink_tick >= 500) {
                blink_tick = now;
                blink_on   = !blink_on;
                draw_string_c(LCD_W / 2, 213,
                              "LISTO PARA JUGAR?",
                              blink_on ? COLOR_GREEN : COLOR_DARKGRAY,
                              COLOR_DARKGRAY, 1);
            }
        }
        break;

    case ESTADO_MENU_NIVEL:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawMenu(gs->menu_cursor);
        }
        break;

    case ESTADO_CONTEO:
        /* Renderer_DrawConteo() es llamado desde Game_Update */
        break;

    case ESTADO_JUGANDO:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawBackground(gs);
            Renderer_UpdateScores(gs);
        }
        /* Render delta de notas */
        for (uint8_t i = 0; i < MAX_NOTES; i++) {
            Nota_t *n = &gs->notas[i];
            if (!n->activa) continue;
            Renderer_EraseNotaTrail(n, PLAYER_X_OFF[n->jugador], gs->nota_speed);
            Renderer_DrawNota(n, PLAYER_X_OFF[n->jugador]);
        }
        break;

    case ESTADO_RESULTADO:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawResultado(gs);
        }
        break;
    }
}
