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
#include <string.h>
#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* === FUENTE BITMAP 5x7 ==================================================== */
/* ========================================================================== */
/* Cada char: 5 bytes (columnas izq→der). Cada byte: bit0=fila top, bit6=bot. */
/* Cubre ASCII 32 (espacio) a ASCII 90 ('Z').                                 */

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
    ILI9341_FillScreen(COLOR_BLACK);

    /* Bordes estilo Guitar Hero: 4 franjas verticales de colores (izq y der) */
    static const uint16_t fringe[4] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW
    };
    for (uint8_t i = 0; i < 4; i++) {
        ILI9341_FillRect((uint16_t)(i * 5), 0, 5, LCD_H, fringe[i]);
        ILI9341_FillRect((uint16_t)(LCD_W - 20 + i * 5), 0, 5, LCD_H, fringe[3 - i]);
    }

    /* Titulo: "BEAT" (MAGENTA) espacio "CLASH" (CYAN) — escala 4
     * Cada char: 24px de avance, 28px alto. Total 10 chars = 240px.
     * x_inicio = (320 - 240) / 2 = 40. Pero las franjas ocupan x=0..19 y x=300..319.
     * Rango libre: 20..299 = 280px. Centrado: (280-240)/2 + 20 = 40. OK. */
    uint16_t tx = 40, ty = 75;
    draw_string(tx,          ty, "BEAT",  COLOR_MAGENTA, COLOR_BLACK, 4);
    draw_char  (tx + 4*24,   ty, ' ',    COLOR_BLACK,   COLOR_BLACK, 4);
    draw_string(tx + 5*24,   ty, "CLASH", COLOR_CYAN,   COLOR_BLACK, 4);

    /* Linea separadora bajo el titulo */
    ILI9341_FillRect(20, 112, 280, 3, COLOR_WHITE);

    /* Notas decorativas — simulan una seccion de Guitar Hero
     * 4 "carriles" con bloques de notas escalonados */
    static const uint16_t nc[4] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW
    };
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t nx = (uint16_t)(52 + i * 58);
        ILI9341_FillRect(nx,      120, 46, 16, nc[i]);
        ILI9341_FillRect(nx + 20, 143, 46, 16, nc[(i + 1) % 4]);
        ILI9341_FillRect(nx,      166, 46, 16, nc[(i + 2) % 4]);
    }

    /* Subtitulo parpadeante — texto dibujado por Renderer_Update */
    /* (se actualiza cada 500ms en el dispatcher) */
}

/* ========================================================================== */
/* === MENU DE NIVEL ======================================================== */
/* ========================================================================== */

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
    draw_string_c(LCD_W / 2, 226, "BTN-ROJO O START PARA CONFIRMAR",
                  COLOR_GREEN, COLOR_DARKGRAY, 1);
}

/* ========================================================================== */
/* === CONTEO REGRESIVO ===================================================== */
/* ========================================================================== */

void Renderer_DrawConteo(uint8_t numero) {
    uint16_t bx = 100, by = 55, bw = 120, bh = 130;
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
        /* Parpadeo "PRESIONA START" cada 500ms */
        {
            static uint32_t blink_tick = 0;
            static uint8_t  blink_on   = 1;
            uint32_t now = HAL_GetTick();
            if (now - blink_tick >= 500) {
                blink_tick = now;
                blink_on   = !blink_on;
                draw_string_c(LCD_W / 2, 205,
                              "PRESIONA START",
                              blink_on ? COLOR_GREEN : COLOR_BLACK,
                              COLOR_BLACK, 1);
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
