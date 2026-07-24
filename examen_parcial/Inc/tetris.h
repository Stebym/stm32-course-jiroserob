/**
 ******************************************************************************
 * @file    tetris.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mini-Tetris escondido (easter egg, no forma parte de la rubrica).
 *          Vive aparte de main.c para no ensuciarlo, mismo criterio que
 *          oled_display.c/.h. Se activa con 5 clicks rapidos del joystick
 *          dentro de ESTADO_MONITOR (ver main.c).
 ******************************************************************************
 */

#ifndef __TETRIS_H
#define __TETRIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* arranca una partida nueva desde cero (tablero vacio) */
void Tetris_Iniciar(void);

/* se llama una vez por vuelta del loop principal mientras la fsm esta en
 * ESTADO_TETRIS. joy_x/joy_y son los mismos joystick_x/joystick_y ya
 * filtrados que usa el resto del proyecto; click_pulsado es el flag
 * boton_sw_pulsado de ese ciclo (1 = hubo click nuevo) */
void Tetris_Actualizar(uint16_t joy_x, uint16_t joy_y, uint8_t click_pulsado);

/* 1 si el usuario pidio salir (game over + click); main.c vuelve a
 * ESTADO_MONITOR cuando esto es 1 */
uint8_t Tetris_DebeSalir(void);

#ifdef __cplusplus
}
#endif

#endif /* __TETRIS_H */
