/**
 ******************************************************************************
 * @file    game_state.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Tipos de datos y estado global para Beat Clash.
 *          Aqui puedo cambiar la dificultad, numero de notas y velocidades.
 ******************************************************************************
 */

#ifndef __GAME_STATE_H
#define __GAME_STATE_H

#include <stdint.h>

/* ========================================================================== */
/* === CONSTANTES DE PANTALLA =============================================== */
/* ========================================================================== */

#define LCD_W           320     /* ancho en landscape                         */
#define LCD_H           240     /* alto en landscape                          */

#define P1_X_OFF        0       /* borde izquierdo del area del jugador 1     */
#define P2_X_OFF        161     /* borde izquierdo del area del jugador 2     */
#define PLAYER_W        159     /* ancho de cada area de jugador en pixeles   */
#define LCD_DIV_X       159     /* columna del divisor central (2px)          */

#define SCORE_BAR_H     25      /* altura de la barra de puntaje (top)        */

/* Coordenadas Y de los carriles (iguales para ambos jugadores) */
#define LANE_Y0         25      /* inicio del area de juego (bajo score bar)  */
#define LANE_H          52      /* alto de cada carril                        */
#define SEP_H           2       /* separador entre carriles                   */

/* Calculo: LANE_Y0 + n*(LANE_H + SEP_H) */
#define LANE_TOP(n)     (LANE_Y0 + (n) * (LANE_H + SEP_H))

/* Zona de presion (press zone) — lado izquierdo de cada jugador */
#define PRESS_ZONE_W    25      /* ancho de la zona de presion en pixeles     */

/* ========================================================================== */
/* === CONSTANTES DE LAS NOTAS ============================================== */
/* ========================================================================== */

#define NOTE_W          22      /* ancho de una nota en pixeles               */
#define NOTE_H          48      /* alto de una nota en pixeles (con padding)  */
#define NOTE_Y_PAD      2       /* padding vertical dentro del carril         */

#define MAX_NOTES       16      /* maximo de notas activas (8 por jugador)    */
#define NOTES_PER_GAME  20      /* notas por jugador por partida              */

/* Velocidades por nivel (pixeles por tick de logica) */
#define NOTE_SPEED_L1   3
#define NOTE_SPEED_L2   4
#define NOTE_SPEED_L3   5

/* Intervalo de spawn por nivel (ms entre notas por jugador) */
#define SPAWN_INTERVAL_L1   2000
#define SPAWN_INTERVAL_L2   1500
#define SPAWN_INTERVAL_L3   1000

/* Ventanas de puntuacion (distancia del centro de la nota al centro de press zone) */
#define HIT_PERFECT     5       /* dentro de 5px del centro → 100 puntos     */
#define HIT_GOOD        13      /* dentro de 13px → 50 puntos                */
#define HIT_OK          PRESS_ZONE_W  /* cualquier overlap → 25 puntos       */

#define SCORE_PERFECT   100
#define SCORE_GOOD      50
#define SCORE_OK        25

/* ========================================================================== */
/* === TIMING DEL LOOP ====================================================== */
/* ========================================================================== */

#define GAME_TICK_MS    33      /* periodo de actualizacion de logica (~30fps) */
#define RENDER_TICK_MS  33      /* periodo de render (~30fps)                 */
#define INPUT_POLL_MS   5       /* periodo de muestreo de botones (TIM5 ISR) */

/* ========================================================================== */
/* === TIPOS DE DATOS ======================================================= */
/* ========================================================================== */

typedef struct {
    int16_t  x_rel;        /* posicion relativa al area del jugador (px izq) */
    uint8_t  carril;       /* 0=Rojo 1=Verde 2=Azul 3=Amarillo               */
    uint8_t  jugador;      /* 0=J1, 1=J2                                     */
    uint8_t  activa;       /* 1=en pantalla, 0=libre                         */
    uint8_t  golpeada;     /* 1=ya fue presionada exitosamente               */
    uint8_t  erased;       /* 1=ya se borro de la pantalla                   */
    int16_t  x_prev;       /* posicion anterior para el render delta         */
} Nota_t;

typedef struct {
    /* Estado de botones — actualizado por TIM5 ISR cada 5ms                 */
    volatile uint8_t  btns;          /* estado actual (bit i = boton i, activo BAJO) */
    volatile uint8_t  btns_prev;     /* estado anterior para detectar flancos */
    volatile uint8_t  press;         /* flanco de presion esta iteracion      */
    volatile uint8_t  release;       /* flanco de soltar                      */

    /* Joystick ADC (leido del buffer DMA) */
    uint16_t joy_x;        /* valor crudo 0..4095                            */
    uint16_t joy_y;

    /* Puntaje */
    uint16_t puntaje;
    uint8_t  combo;

    /* Notas spawneadas para este jugador */
    uint8_t  notas_spawneadas;

    /* Tick del ultimo spawn */
    uint32_t tick_ultimo_spawn;

    /* Animacion de LED al presionar */
    uint32_t led_flash_tick[4];   /* tick de inicio del flash por carril     */
    uint8_t  led_on[4];           /* estado LED por carril                   */
} EstadoJugador_t;

typedef enum {
    ESTADO_SPLASH = 0,
    ESTADO_MENU_NIVEL,
    ESTADO_CONTEO,
    ESTADO_JUGANDO,
    ESTADO_RESULTADO
} EstadoApp_t;

typedef struct {
    EstadoApp_t     estado;
    EstadoApp_t     estado_prev;

    uint8_t         nivel;           /* 1, 2 o 3 (se elige en menu)          */
    uint8_t         menu_cursor;     /* opcion seleccionada en menu (0,1,2)  */

    EstadoJugador_t j[2];
    Nota_t          notas[MAX_NOTES];

    uint32_t        tick_estado;     /* HAL_GetTick al entrar a este estado  */
    uint32_t        tick_logica;     /* ultimo tick de actualizacion logica  */
    uint32_t        tick_render;     /* ultimo tick de render                */
    uint32_t        tick_conteo;     /* tick del ultimo numero del conteo    */
    uint8_t         conteo_num;      /* numero actual del conteo (3,2,1,0=GO)*/

    /* Buffer ADC DMA — actualizado automaticamente por el DMA en circular   */
    volatile uint16_t adc_raw[4];   /* [J1_X, J1_Y, J2_X, J2_Y]            */

    /* Flags de sincronizacion con ISRs — deben ser volatile                 */
    volatile uint8_t  input_flag;   /* set por TIM5 ISR cada 5ms            */

    /* Flag de inicio de pantalla — para saber cuando redibujar el fondo     */
    uint8_t         pantalla_init;  /* 0=hay que redibujar fondo, 1=limpio  */

    uint8_t         nota_speed;     /* velocidad actual por nivel            */
    uint32_t        spawn_intervalo;/* ms entre spawns por jugador           */

    /* PC13 boton de inicio */
    uint8_t         start_btn;
    uint8_t         start_btn_prev;

} GameState_t;

/* ========================================================================== */
/* === COLORES RGB565 ======================================================= */
/* ========================================================================== */

#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x8410
#define COLOR_DARKGRAY  0x2104

/* Colores de fondo de carril (versiones oscuras para contraste con las notas) */
#define COLOR_LANE_R    0x1000   /* rojo oscuro                               */
#define COLOR_LANE_G    0x0060   /* verde oscuro                              */
#define COLOR_LANE_B    0x0003   /* azul oscuro                               */
#define COLOR_LANE_Y    0x1040   /* amarillo oscuro                           */

/* Colores de la zona de presion */
#define COLOR_PRESS_R   0x5000
#define COLOR_PRESS_G   0x0180
#define COLOR_PRESS_B   0x000C
#define COLOR_PRESS_Y   0x5120

/* Colores segun carril — para acceso por indice */
static const uint16_t NOTE_COLOR[4]  = {COLOR_RED,     COLOR_GREEN,   COLOR_BLUE,    COLOR_YELLOW};
static const uint16_t LANE_COLOR[4]  = {COLOR_LANE_R,  COLOR_LANE_G,  COLOR_LANE_B,  COLOR_LANE_Y};
static const uint16_t PRESS_COLOR[4] = {COLOR_PRESS_R, COLOR_PRESS_G, COLOR_PRESS_B, COLOR_PRESS_Y};

/* Color de jugador para HUD */
#define COLOR_P1        COLOR_RED
#define COLOR_P2        COLOR_BLUE

/* Puntaje maximo posible: NOTES_PER_GAME * SCORE_PERFECT */
#define SCORE_MAX       (NOTES_PER_GAME * SCORE_PERFECT)

#endif /* __GAME_STATE_H */
