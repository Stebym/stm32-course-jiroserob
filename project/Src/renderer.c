/**
 ******************************************************************************
 * @file    renderer.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Motor de renderizado para Beat Clash — pantalla ILI9341 paisaje.
 *
 * Layout pantalla 320x240 paisaje:
 *   P1 area: x=[0..158]   P2 area: x=[161..319]   Divisor: x=[159..160]
 *   Score bar: y=[0..24]
 *   Lane 0 (R): y=[25..76]    Sep: y=[77..78]
 *   Lane 1 (G): y=[79..130]   Sep: y=[131..132]
 *   Lane 2 (B): y=[133..184]  Sep: y=[185..186]
 *   Lane 3 (Y): y=[187..238]
 *
 * Estrategia de render:
 *   - Background estatico dibujado UNA vez por estado (Renderer_DrawBackground)
 *   - Notas: render delta — solo los 3px de borde izquierdo (nuevo) y
 *     los 3px de borde derecho (a borrar del paso anterior)
 *   - 7-segment para puntajes (digits 0-9, segmentos como fill_rect)
 *
 * Aqui puedo cambiar colores, velocidades de nota y tamanio de segmentos.
 ******************************************************************************
 */

#include "renderer.h"
#include "ili9341.h"
#include "board_pins.h"
#include <string.h>

/* ========================================================================== */
/* === CONSTANTES DE LAYOUT ================================================= */
/* ========================================================================== */

#define DIV_X       159             /* columna izquierda del divisor          */
#define DIV_W       2               /* ancho del divisor                      */

/* Y del borde superior de notas dentro de un carril (con padding) */
static inline uint16_t note_y(uint8_t carril) {
    return (uint16_t)(LANE_TOP(carril) + NOTE_Y_PAD);
}

/* X absoluta izquierda del area del jugador */
static const uint16_t PLAYER_X_OFF[2] = { P1_X_OFF, P2_X_OFF };

/* ========================================================================== */
/* === SEGMENTOS 7-SEG PARA PUNTAJE ========================================= */
/* ========================================================================== */

/* s = largo del segmento. Total digit: (s+4) wide x (2*s+6) tall.          */
/* Con s=8: 12 x 22 px. 4 digitos + 3 gaps de 2px = 54px.                  */
#define SEG_S   8           /* largo de segmento                              */
#define SEG_T   2           /* grosor de segmento                             */
#define SEG_DX  (SEG_S+SEG_T+2)    /* paso horizontal por digito (12px)      */

#define SEG_A (1<<0)
#define SEG_B (1<<1)
#define SEG_C (1<<2)
#define SEG_D (1<<3)
#define SEG_E (1<<4)
#define SEG_F (1<<5)
#define SEG_G (1<<6)

static const uint8_t seg_table[10] = {
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,          /* 0 */
    SEG_B|SEG_C,                                    /* 1 */
    SEG_A|SEG_B|SEG_D|SEG_E|SEG_G,                /* 2 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_G,                /* 3 */
    SEG_B|SEG_C|SEG_F|SEG_G,                       /* 4 */
    SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,                /* 5 */
    SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,          /* 6 */
    SEG_A|SEG_B|SEG_C,                             /* 7 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,   /* 8 */
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G,          /* 9 */
};

static void draw_seg_digit(int16_t x, int16_t y, uint8_t digit, uint16_t fg, uint16_t bg) {
    uint8_t segs = seg_table[digit % 10];
    /* Fondo del digito */
    ILI9341_FillRect(x, y, SEG_S + 2*SEG_T, 2*SEG_S + 3*SEG_T, bg);
    /* Segmentos horizontales */
    if (segs & SEG_A) ILI9341_FillRect(x+SEG_T,       y,                   SEG_S, SEG_T, fg);
    if (segs & SEG_G) ILI9341_FillRect(x+SEG_T,       y + SEG_S + SEG_T,  SEG_S, SEG_T, fg);
    if (segs & SEG_D) ILI9341_FillRect(x+SEG_T,       y + 2*SEG_S+2*SEG_T,SEG_S, SEG_T, fg);
    /* Segmentos verticales */
    if (segs & SEG_F) ILI9341_FillRect(x,             y + SEG_T,           SEG_T, SEG_S, fg);
    if (segs & SEG_E) ILI9341_FillRect(x,             y + SEG_S+2*SEG_T,  SEG_T, SEG_S, fg);
    if (segs & SEG_B) ILI9341_FillRect(x+SEG_S+SEG_T, y + SEG_T,          SEG_T, SEG_S, fg);
    if (segs & SEG_C) ILI9341_FillRect(x+SEG_S+SEG_T, y + SEG_S+2*SEG_T, SEG_T, SEG_S, fg);
}

/* Dibuja 4 digitos del puntaje en la barra de score */
static void draw_score_digits(uint16_t x_off, uint16_t score, uint16_t fg, uint16_t bg) {
    /* 4 digitos empezando a x_off + 103 (lado derecho de la barra) */
    int16_t dx = (int16_t)(x_off + 103);
    uint8_t d[4] = {
        (score / 1000) % 10,
        (score / 100)  % 10,
        (score / 10)   % 10,
        score % 10
    };
    for (int i = 0; i < 4; i++) {
        draw_seg_digit(dx, 1, d[i], fg, bg);
        dx += SEG_DX + 1;
    }
}

/* Barra de progreso de puntaje (48px wide, 10px tall) */
static void draw_score_bar(uint16_t x_off, uint16_t score, uint16_t fg) {
    uint16_t bx = x_off + 23;
    uint16_t bw = 78;
    uint16_t bh = 10;
    uint16_t by = 7;
    uint16_t fill = (uint16_t)((uint32_t)score * bw / SCORE_MAX);
    ILI9341_FillRect(bx, by, fill,    bh, fg);
    ILI9341_FillRect(bx + fill, by, bw - fill, bh, COLOR_DARKGRAY);
}

/* ========================================================================== */
/* === FONDO ESTATICO ======================================================= */
/* ========================================================================== */

void Renderer_DrawBackground(const GameState_t *gs) {
    (void)gs;
    /* Score bars */
    ILI9341_FillRect(P1_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);
    ILI9341_FillRect(P2_X_OFF, 0, PLAYER_W, SCORE_BAR_H, COLOR_DARKGRAY);

    /* Indicador de jugador (bloque de 20x20) */
    ILI9341_FillRect(P1_X_OFF + 1, 2, 20, 20, COLOR_P1);
    ILI9341_FillRect(P2_X_OFF + 1, 2, 20, 20, COLOR_P2);

    /* Divisor central */
    ILI9341_FillRect(DIV_X, 0, DIV_W, LCD_H, COLOR_WHITE);

    /* Carriles de cada jugador */
    for (uint8_t p = 0; p < 2; p++) {
        uint16_t xo = PLAYER_X_OFF[p];
        for (uint8_t c = 0; c < 4; c++) {
            uint16_t ly = LANE_TOP(c);
            /* Fondo del carril */
            ILI9341_FillRect(xo, ly, PLAYER_W, LANE_H, LANE_COLOR[c]);
            /* Indicador de zona de presion (3px al borde izquierdo) */
            ILI9341_FillRect(xo, ly, 3, LANE_H, PRESS_COLOR[c]);
        }
        /* Separadores entre carriles */
        for (uint8_t s = 0; s < 3; s++) {
            uint16_t sy = LANE_TOP(s) + LANE_H;
            ILI9341_FillRect(xo, sy, PLAYER_W, SEP_H, COLOR_GRAY);
        }
    }
}

/* ========================================================================== */
/* === SPLASH SCREEN ======================================================== */
/* ========================================================================== */

void Renderer_DrawSplash(void) {
    ILI9341_FillScreen(COLOR_BLACK);

    /* Titulo "BEAT CLASH" como bloques de colores */
    /* B - bloque rojo */
    ILI9341_FillRect(30,  50, 20, 60, COLOR_RED);
    ILI9341_FillRect(30,  50, 50, 10, COLOR_RED);
    ILI9341_FillRect(30,  75, 45, 10, COLOR_RED);
    ILI9341_FillRect(30, 100, 50, 10, COLOR_RED);
    ILI9341_FillRect(75,  55, 10, 20, COLOR_RED);
    ILI9341_FillRect(72,  80, 10, 20, COLOR_RED);

    /* BEAT — barra verde bajo el titulo */
    ILI9341_FillRect(20, 130, 130, 6, COLOR_GREEN);

    /* CLASH — barra azul */
    ILI9341_FillRect(170, 130, 130, 6, COLOR_BLUE);

    /* Bloque amarillo de decoracion */
    ILI9341_FillRect(100, 60, 120, 60, COLOR_YELLOW);
    ILI9341_FillRect(110, 70, 100, 40, COLOR_BLACK);

    /* Texto "1" y "2" en los bloques de jugadores */
    ILI9341_FillRect(135, 75, 14, 40, COLOR_YELLOW);
    ILI9341_FillRect(180, 75, 14, 40, COLOR_YELLOW);

    /* "Presiona START" — barra pulsante en la parte inferior */
    ILI9341_FillRect(60, 190, 200, 15, COLOR_GREEN);
    ILI9341_FillRect(62, 192, 196, 11, COLOR_BLACK);
    ILI9341_FillRect(70, 195, 180, 5,  COLOR_GREEN);

    /* Nombre del juego en bloques */
    ILI9341_FillRect(20,  20, 280, 25, COLOR_DARKGRAY);
    /* "BEAT CLASH" en pixeles grandes */
    for (int i = 0; i < 5; i++) {
        ILI9341_FillRect(22 + i*56, 22, 50, 21,
            (i == 0) ? COLOR_RED :
            (i == 1) ? COLOR_GREEN :
            (i == 2) ? COLOR_BLUE :
            (i == 3) ? COLOR_YELLOW : COLOR_CYAN);
    }

    /* Lineas decorativas de ritmo */
    for (int i = 0; i < 4; i++) {
        ILI9341_FillRect(10, 155 + i*8, 300, 3,
            (i==0) ? COLOR_RED : (i==1) ? COLOR_GREEN :
            (i==2) ? COLOR_BLUE : COLOR_YELLOW);
    }
}

/* ========================================================================== */
/* === MENU DE NIVEL ======================================================== */
/* ========================================================================== */

void Renderer_DrawMenu(uint8_t cursor) {
    ILI9341_FillScreen(COLOR_BLACK);

    /* Cabecera */
    ILI9341_FillRect(0, 0, LCD_W, 30, COLOR_DARKGRAY);
    ILI9341_FillRect(5, 8, 50, 14, COLOR_GREEN);
    ILI9341_FillRect(60, 8, 50, 14, COLOR_YELLOW);
    ILI9341_FillRect(115, 8, 50, 14, COLOR_RED);

    /* Tres opciones de nivel */
    static const uint16_t colors[3] = {COLOR_GREEN, COLOR_YELLOW, COLOR_RED};
    static const char *names[3] = {"NIVEL 1", "NIVEL 2", "NIVEL 3"};
    (void)names;

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t bx = 20 + i * 100;
        uint16_t by = 60;
        uint16_t bw = 80, bh = 80;

        /* Caja del nivel */
        ILI9341_FillRect(bx, by, bw, bh, colors[i]);
        ILI9341_FillRect(bx+3, by+3, bw-6, bh-6,
                          (cursor == i) ? COLOR_WHITE : COLOR_BLACK);

        /* Numero del nivel como segmentos grandes */
        draw_seg_digit(bx + 30, by + 25, (uint8_t)(i + 1), colors[i],
                       (cursor == i) ? COLOR_WHITE : COLOR_BLACK);

        /* Indicador de cursor */
        if (cursor == i) {
            ILI9341_FillRect(bx + 10, by + bh + 5, bw - 20, 6, colors[i]);
        }
    }

    /* Descripcion del nivel seleccionado */
    uint16_t dy = 165;
    ILI9341_FillRect(20, dy, 280, 55, COLOR_DARKGRAY);

    /* Barras de velocidad — 1, 2 o 3 barras segun nivel */
    for (uint8_t v = 0; v < cursor + 1; v++) {
        ILI9341_FillRect(30 + v*30, dy+15, 20, 25, colors[cursor]);
    }
    /* Indicador de notas simultaneas */
    uint8_t notas_sim = cursor + 1;
    for (uint8_t n = 0; n < notas_sim; n++) {
        ILI9341_FillRect(130 + n*28, dy+20, 22, 15, NOTE_COLOR[n]);
    }
}

/* ========================================================================== */
/* === CONTEO REGRESIVO ===================================================== */
/* ========================================================================== */

void Renderer_DrawConteo(uint8_t numero) {
    /* Fondo del area central */
    ILI9341_FillRect(100, 60, 120, 120, COLOR_BLACK);

    uint16_t col = (numero == 3) ? COLOR_RED :
                   (numero == 2) ? COLOR_YELLOW :
                   (numero == 1) ? COLOR_GREEN : COLOR_CYAN;

    if (numero > 0) {
        /* Circulo simulado como cuadrado redondeado */
        ILI9341_FillRect(120, 70, 80, 80, col);
        ILI9341_FillRect(130, 80, 60, 60, COLOR_BLACK);
        /* Numero en 7-seg grande (s=20) — version manual para numeros 1-3 */
        /* Reuso draw_seg_digit con escala visual extra */
        draw_seg_digit(143, 90, numero, col, COLOR_BLACK);
        /* Ampliamos con un segundo juego de segmentos superpuesto desplazado */
        if (numero != 1) {
            draw_seg_digit(148, 93, numero, col, COLOR_BLACK);
        }
    } else {
        /* "GO!" — bloques de color */
        ILI9341_FillRect(105, 80, 40, 60, COLOR_GREEN);
        ILI9341_FillRect(110, 85, 30, 50, COLOR_BLACK);
        ILI9341_FillRect(155, 80, 55, 60, COLOR_GREEN);
        ILI9341_FillRect(160, 85, 45, 50, COLOR_BLACK);
        ILI9341_FillRect(162, 95, 40, 10, COLOR_GREEN);
    }
}

/* ========================================================================== */
/* === RESULTADO ============================================================ */
/* ========================================================================== */

void Renderer_DrawResultado(const GameState_t *gs) {
    ILI9341_FillScreen(COLOR_BLACK);

    uint16_t s1 = gs->j[0].puntaje;
    uint16_t s2 = gs->j[1].puntaje;

    /* Barras de puntaje final */
    draw_score_bar(P1_X_OFF, s1, COLOR_P1);
    draw_score_bar(P2_X_OFF, s2, COLOR_P2);

    /* Numeros grandes de puntaje */
    int16_t dx = 20;
    uint8_t d1[4] = {(s1/1000)%10,(s1/100)%10,(s1/10)%10,s1%10};
    uint8_t d2[4] = {(s2/1000)%10,(s2/100)%10,(s2/10)%10,s2%10};
    for (int i = 0; i < 4; i++) {
        draw_seg_digit(dx,     80, d1[i], COLOR_P1, COLOR_BLACK);
        draw_seg_digit(dx+170, 80, d2[i], COLOR_P2, COLOR_BLACK);
        dx += SEG_DX + 1;
    }

    /* Banner del ganador */
    uint16_t bx, col;
    if (s1 > s2)      { bx = 20;  col = COLOR_P1; }
    else if (s2 > s1) { bx = 170; col = COLOR_P2; }
    else               { bx = 80;  col = COLOR_YELLOW; }

    ILI9341_FillRect(bx, 120, 140, 70, col);
    ILI9341_FillRect(bx+4, 124, 132, 62, COLOR_BLACK);

    /* Texto ganador con bloques */
    if (s1 == s2) {
        /* EMPATE: barras amarillas simetricas */
        ILI9341_FillRect(90, 135, 140, 15, COLOR_YELLOW);
        ILI9341_FillRect(90, 158, 140, 15, COLOR_YELLOW);
    } else {
        /* Ganador: bloque de color + "WIN" en pequeños seg */
        uint8_t g = (s1 > s2) ? 0 : 1;
        uint16_t gx = bx + 15;
        /* "1" o "2" grande */
        draw_seg_digit(gx + 30, 135, g + 1, col, COLOR_BLACK);
        ILI9341_FillRect(gx, 167, 100, 6, col);
    }

    /* Barra "presiona INICIO para reiniciar" */
    ILI9341_FillRect(60, 210, 200, 20, COLOR_DARKGRAY);
    ILI9341_FillRect(65, 215, 190, 10, COLOR_GREEN);
}

/* ========================================================================== */
/* === UPDATE POR FRAME ===================================================== */
/* ========================================================================== */

void Renderer_UpdateScores(const GameState_t *gs) {
    /* Actualizar barra y digitos de puntaje */
    draw_score_bar(P1_X_OFF, gs->j[0].puntaje, COLOR_P1);
    draw_score_bar(P2_X_OFF, gs->j[1].puntaje, COLOR_P2);
    draw_score_digits(P1_X_OFF, gs->j[0].puntaje, COLOR_P1, COLOR_DARKGRAY);
    draw_score_digits(P2_X_OFF, gs->j[1].puntaje, COLOR_P2, COLOR_DARKGRAY);
}

void Renderer_FlashPressZone(uint8_t jugador, uint8_t carril, uint16_t color) {
    uint16_t xo = PLAYER_X_OFF[jugador];
    uint16_t ly = LANE_TOP(carril);
    ILI9341_FillRect(xo, ly + NOTE_Y_PAD, PRESS_ZONE_W, NOTE_H, color);
}

/* Dibuja la nota completa en su posicion actual */
void Renderer_DrawNota(const Nota_t *nota, uint16_t x_off) {
    if (!nota->activa) return;
    int16_t x_abs = (int16_t)x_off + nota->x_rel;
    if (x_abs + NOTE_W <= 0 || x_abs >= (int16_t)PLAYER_W) return;

    /* Clip al area del jugador */
    int16_t dx = x_abs;
    int16_t dw = NOTE_W;
    if (dx < 0) { dw += dx; dx = 0; }
    if (dx + dw > (int16_t)PLAYER_W) dw = (int16_t)PLAYER_W - dx;
    if (dw <= 0) return;

    ILI9341_FillRect((uint16_t)(x_off + dx),
                     note_y(nota->carril),
                     (uint16_t)dw, NOTE_H,
                     NOTE_COLOR[nota->carril]);
}

/* Borra el borde trasero de la nota (render delta — solo los pixeles abandonados) */
void Renderer_EraseNotaTrail(const Nota_t *nota, uint16_t x_off, uint8_t speed) {
    if (!nota->activa) return;
    /* El borde derecho de la posicion ANTERIOR era: x_prev + NOTE_W - 1     */
    /* Esos 'speed' pixeles ya no estan cubiertos por la nota en la pos nueva */
    int16_t ex_start = nota->x_prev + NOTE_W - speed;  /* inicio del borrado */
    int16_t ex_end   = nota->x_prev + NOTE_W;          /* fin (exclusivo)     */

    if (ex_start < 0) ex_start = 0;
    if (ex_end > (int16_t)PLAYER_W) ex_end = (int16_t)PLAYER_W;
    if (ex_end <= ex_start) return;

    int16_t ew = ex_end - ex_start;
    if (ew <= 0) return;

    /* Repintar con el fondo del carril */
    uint16_t abs_ex = (uint16_t)((int16_t)x_off + ex_start);
    uint16_t ly = note_y(nota->carril);

    /* Determinar el color de fondo segun si esta en la zona de presion        */
    uint16_t bg = (ex_start < (int16_t)PRESS_ZONE_W)
                  ? PRESS_COLOR[nota->carril]
                  : LANE_COLOR[nota->carril];

    ILI9341_FillRect(abs_ex, ly, (uint16_t)ew, NOTE_H, bg);

    /* Si el borrado cruza la frontera press/lane, hacer las dos secciones    */
    if (ex_start < (int16_t)PRESS_ZONE_W && ex_end > (int16_t)PRESS_ZONE_W) {
        int16_t lane_start = PRESS_ZONE_W;
        uint16_t abs_ls = (uint16_t)((int16_t)x_off + lane_start);
        ILI9341_FillRect(abs_ls, ly, (uint16_t)(ex_end - lane_start), NOTE_H,
                         LANE_COLOR[nota->carril]);
    }
}

/* ========================================================================== */
/* === DISPATCHER PRINCIPAL ================================================= */
/* ========================================================================== */

void Renderer_Update(GameState_t *gs) {
    switch (gs->estado) {
    case ESTADO_SPLASH:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawSplash();
        }
        break;

    case ESTADO_MENU_NIVEL:
        if (!gs->pantalla_init) {
            gs->pantalla_init = 1;
            Renderer_DrawMenu(gs->menu_cursor);
        }
        break;

    case ESTADO_CONTEO:
        /* DrawConteo se llama desde Game_Update cuando cambia el numero     */
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
