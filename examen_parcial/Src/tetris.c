/**
 ******************************************************************************
 * @file    tetris.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mini-Tetris escondido sobre el OLED (easter egg). Tablero real
 *          de 10x12, las 7 piezas estandar con rotacion (metodo del indice
 *          4x4, sin tablas de rotacion precalculadas), colision, caida
 *          automatica y limpieza de lineas completas. Version simplificada
 *          a proposito: sin puntaje ni niveles de velocidad ni wall-kicks.
 * ******************************************************************************
 */

#include <string.h>
#include "tetris.h"
#include "oled_display.h"

/* -----------------------------------------------------------------------
 * TABLERO Y GEOMETRIA EN EL OLED (128x64)
 * tablero: 10 columnas x 12 filas, bloques de 5x5 px -> ocupa 50x60 px.
 * Arranca en (BOARD_X0, BOARD_Y0) en vez de pegado al borde izquierdo,
 * para que quede mas centrado y el panel de texto (a la derecha del
 * tablero) tenga suficiente espacio.
 * ----------------------------------------------------------------------- */
#define BOARD_W    10
#define BOARD_H    12
#define BLOCK_PX    5
#define BOARD_X0    6
#define BOARD_Y0    2
#define PANEL_X0   (BOARD_X0 + BOARD_W * BLOCK_PX + 6)

#define TETRIS_FALL_MS    500U   /* cada cuanto baja la pieza sola */
#define TETRIS_RENDER_MS  150U   /* mismo ritmo de refresco que el resto del oled */

/* mismos umbrales "por flanco" que usa el resto del proyecto para leer el
 * joystick en los menus de ajuste de hora/fecha (ver main.c) */
#define JOY_CENTRO_MIN   1700U
#define JOY_CENTRO_MAX   2400U
#define JOY_UMBRAL_ALTO  3200U
#define JOY_UMBRAL_BAJO   900U

typedef enum { TETRIS_JUGANDO = 0, TETRIS_GAMEOVER } Tetris_EstadoJuego_t;

/* las 7 piezas en su rotacion 0, como grilla 4x4 (16 caracteres, 'X'=lleno).
 * la rotacion se calcula con el indice de abajo (Rotar), no hace falta
 * guardar las 4 rotaciones de cada pieza. */
static const char *tetromino[7] =
{
    "....XXXX........", /* I */
    ".....XX..XX.....", /* O */
    "....XXX..X......", /* T */
    ".....XX.XX......", /* S */
    "....XX...XX.....", /* Z */
    ".X...X..XX......", /* J */
    ".X...X...XX....."  /* L */
};

static uint8_t tablero[BOARD_H][BOARD_W];
static uint8_t pieza_actual;
static uint8_t pieza_siguiente;
static int8_t  pieza_rot;
static int8_t  pieza_x;
static int8_t  pieza_y;

static Tetris_EstadoJuego_t estado_juego;
static uint8_t  debe_salir;
static uint32_t semilla_rng;

static uint32_t tick_ultima_caida;
static uint32_t tick_ultimo_render;
static uint8_t  joy_listo_lr;   /* eje izquierda/derecha */
static uint8_t  joy_listo_baja; /* eje soft-drop */

static void Tetris_Dibujar(void);
static void SpawnPieza(void);

/* -----------------------------------------------------------------------
 * ROTACION POR INDICE: dado (px,py) dentro de una grilla 4x4 y una
 * rotacion r (0-3), devuelve el indice dentro del string de 16 caracteres
 * de la pieza que corresponde a esa celda ya rotada. Evita tener que
 * guardar 4 grillas por pieza.
 * ----------------------------------------------------------------------- */
static int8_t Rotar(int8_t px, int8_t py, int8_t r)
{
    switch (r % 4)
    {
        case 0:  return (int8_t)(py * 4 + px);
        case 1:  return (int8_t)(12 + py - (px * 4));
        case 2:  return (int8_t)(15 - (py * 4) - px);
        default: return (int8_t)(3 - py + (px * 4));
    }
}

static uint8_t NumeroAleatorioSimple(void)
{
    /* generador congruencial lineal simple -- no hace falta mas para
     * elegir la siguiente pieza, solo que no se repita siempre el mismo
     * patron entre partidas */
    semilla_rng = semilla_rng * 1103515245u + 12345u;
    return (uint8_t)((semilla_rng >> 16) % 7u);
}

static uint8_t ColisionaPos(uint8_t pieza, int8_t rot, int8_t nx, int8_t ny)
{
    for (int8_t py = 0; py < 4; py++)
    {
        for (int8_t px = 0; px < 4; px++)
        {
            if (tetromino[pieza][(uint8_t)Rotar(px, py, rot)] == 'X')
            {
                int16_t bx = (int16_t)(nx + px);
                int16_t by = (int16_t)(ny + py);

                if (bx < 0 || bx >= BOARD_W || by >= BOARD_H)
                {
                    return 1;
                }
                if (by >= 0 && tablero[by][bx])
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void FijarPiezaYLimpiarLineas(void)
{
    for (int8_t py = 0; py < 4; py++)
    {
        for (int8_t px = 0; px < 4; px++)
        {
            if (tetromino[pieza_actual][(uint8_t)Rotar(px, py, pieza_rot)] == 'X')
            {
                int16_t bx = (int16_t)(pieza_x + px);
                int16_t by = (int16_t)(pieza_y + py);
                if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
                {
                    tablero[by][bx] = 1;
                }
            }
        }
    }

    /* limpia cualquier fila que haya quedado completamente llena */
    for (int8_t fila = BOARD_H - 1; fila >= 0; fila--)
    {
        uint8_t llena = 1;
        for (uint8_t col = 0; col < BOARD_W; col++)
        {
            if (!tablero[fila][col]) { llena = 0; break; }
        }

        if (llena)
        {
            for (int8_t f = fila; f > 0; f--)
            {
                memcpy(tablero[f], tablero[f - 1], BOARD_W);
            }
            memset(tablero[0], 0, BOARD_W);
            fila++; /* la fila que bajo puede tambien estar llena: revisala de nuevo */
        }
    }
}

static void SpawnPieza(void)
{
    pieza_actual = pieza_siguiente;
    pieza_rot    = 0;
    pieza_x      = 3;  /* centrada en las 10 columnas */
    pieza_y      = -1; /* arranca justo arriba del borde visible */
    pieza_siguiente = NumeroAleatorioSimple();

    if (ColisionaPos(pieza_actual, pieza_rot, pieza_x, pieza_y))
    {
        estado_juego = TETRIS_GAMEOVER;
    }
}

void Tetris_Iniciar(void)
{
    memset(tablero, 0, sizeof(tablero));
    estado_juego   = TETRIS_JUGANDO;
    debe_salir     = 0;
    joy_listo_lr   = 1;
    joy_listo_baja = 1;

    semilla_rng = HAL_GetTick() | 1u; /* semilla distinta cada partida */
    pieza_siguiente = NumeroAleatorioSimple();
    SpawnPieza();

    tick_ultima_caida  = HAL_GetTick();
    tick_ultimo_render = 0; /* fuerza el primer dibujado ya */
}

void Tetris_Actualizar(uint16_t joy_x, uint16_t joy_y, uint8_t click_pulsado)
{
    uint32_t ahora = HAL_GetTick();

    if (estado_juego == TETRIS_GAMEOVER)
    {
        if (click_pulsado)
        {
            debe_salir = 1;
        }
        if ((ahora - tick_ultimo_render) >= TETRIS_RENDER_MS)
        {
            Tetris_Dibujar();
            tick_ultimo_render = ahora;
        }
        return;
    }

    /* eje "izquierda/derecha": fisicamente responde joystick_y, no
     * joystick_x -- el modulo quedo montado rotado 90 grados en la
     * protoboard (mismo motivo documentado en main.c para el cursor del
     * oled y el ajuste de hora/fecha con joystick) */
    if (joy_y >= JOY_UMBRAL_ALTO && joy_listo_lr)
    {
        if (!ColisionaPos(pieza_actual, pieza_rot, (int8_t)(pieza_x + 1), pieza_y)) { pieza_x++; }
        joy_listo_lr = 0;
    }
    else if (joy_y <= JOY_UMBRAL_BAJO && joy_listo_lr)
    {
        if (!ColisionaPos(pieza_actual, pieza_rot, (int8_t)(pieza_x - 1), pieza_y)) { pieza_x--; }
        joy_listo_lr = 0;
    }
    else if (joy_y >= JOY_CENTRO_MIN && joy_y <= JOY_CENTRO_MAX)
    {
        joy_listo_lr = 1;
    }

    /* eje "arriba/abajo": responde joystick_x. no importa la direccion
     * del empujon, cualquiera de los dos lados hace caer la pieza una
     * fila de una vez (soft drop) */
    if ((joy_x >= JOY_UMBRAL_ALTO || joy_x <= JOY_UMBRAL_BAJO) && joy_listo_baja)
    {
        if (!ColisionaPos(pieza_actual, pieza_rot, pieza_x, (int8_t)(pieza_y + 1))) { pieza_y++; }
        joy_listo_baja = 0;
    }
    else if (joy_x >= JOY_CENTRO_MIN && joy_x <= JOY_CENTRO_MAX)
    {
        joy_listo_baja = 1;
    }

    /* click: rota la pieza (si no cabe rotada contra el borde o contra
     * otro bloque, se ignora el giro -- sin wall-kicks) */
    if (click_pulsado)
    {
        int8_t nueva_rot = (int8_t)((pieza_rot + 1) % 4);
        if (!ColisionaPos(pieza_actual, nueva_rot, pieza_x, pieza_y))
        {
            pieza_rot = nueva_rot;
        }
    }

    /* caida automatica por tiempo */
    if ((ahora - tick_ultima_caida) >= TETRIS_FALL_MS)
    {
        if (!ColisionaPos(pieza_actual, pieza_rot, pieza_x, (int8_t)(pieza_y + 1)))
        {
            pieza_y++;
        }
        else
        {
            FijarPiezaYLimpiarLineas();
            SpawnPieza();
        }
        tick_ultima_caida = ahora;
    }

    if ((ahora - tick_ultimo_render) >= TETRIS_RENDER_MS)
    {
        Tetris_Dibujar();
        tick_ultimo_render = ahora;
    }
}

uint8_t Tetris_DebeSalir(void)
{
    return debe_salir;
}

static void DibujarBloque(int8_t col, int8_t fila)
{
    uint8_t x0 = (uint8_t)(BOARD_X0 + col * BLOCK_PX);
    uint8_t y0 = (uint8_t)(BOARD_Y0 + fila * BLOCK_PX);

    for (uint8_t dy = 0; dy < BLOCK_PX - 1; dy++)
    {
        for (uint8_t dx = 0; dx < BLOCK_PX - 1; dx++)
        {
            SSD1306_DrawPixel((uint8_t)(x0 + dx), (uint8_t)(y0 + dy), 1);
        }
    }
}

static void Tetris_Dibujar(void)
{
    SSD1306_Fill(0);

    for (uint8_t f = 0; f < BOARD_H; f++)
    {
        for (uint8_t c = 0; c < BOARD_W; c++)
        {
            if (tablero[f][c])
            {
                DibujarBloque((int8_t)c, (int8_t)f);
            }
        }
    }

    if (estado_juego == TETRIS_JUGANDO)
    {
        for (int8_t py = 0; py < 4; py++)
        {
            for (int8_t px = 0; px < 4; px++)
            {
                if (tetromino[pieza_actual][(uint8_t)Rotar(px, py, pieza_rot)] == 'X')
                {
                    int8_t bx = (int8_t)(pieza_x + px);
                    int8_t by = (int8_t)(pieza_y + py);
                    if (by >= 0)
                    {
                        DibujarBloque(bx, by);
                    }
                }
            }
        }
    }

    SSD1306_WriteString(PANEL_X0, 0, "TETRIS");

    if (estado_juego == TETRIS_GAMEOVER)
    {
        SSD1306_WriteString(PANEL_X0, 24, "GAME");
        SSD1306_WriteString(PANEL_X0, 32, "OVER");
        SSD1306_WriteString(PANEL_X0, 48, "Click:");
        SSD1306_WriteString(PANEL_X0, 56, "salir");
    }
    else
    {
        SSD1306_WriteString(PANEL_X0, 44, "3 clicks");
        SSD1306_WriteString(PANEL_X0, 52, "= salir");
    }

    SSD1306_UpdateScreen();
}
