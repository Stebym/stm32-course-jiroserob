/**
 ******************************************************************************
 * @file    input.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Lectura de botones arcade y joysticks para Beat Clash.
 *          Aqui puedo ajustar el umbral de debounce y la zona muerta del joy.
 ******************************************************************************
 */

#ifndef __INPUT_H
#define __INPUT_H

#include "game_state.h"
#include <stdint.h>

/* ========================================================================== */
/* === CONSTANTES DE ENTRADA ================================================ */
/* ========================================================================== */

/* Debounce: N muestras consecutivas iguales para confirmar estado           */
/* Con TIM5 a 5ms: 3 muestras = 15ms de debounce                            */
#define DEBOUNCE_N      3

/* Zona muerta del joystick (centrado en 2048) */
#define JOY_DEADZONE    200

/* Umbral para detectar direccion del joystick */
#define JOY_THRESH_HI   (2048 + JOY_DEADZONE)
#define JOY_THRESH_LO   (2048 - JOY_DEADZONE)

/* ========================================================================== */
/* === API PUBLICA ========================================================== */
/* ========================================================================== */

/* Inicializar el modulo de entrada */
void Input_Init(GameState_t *gs);

/* Llamar desde el loop principal cuando gs->input_flag == 1.               */
/* Actualiza press/release/btns en gs->j[0] y gs->j[1].                    */
void Input_Update(GameState_t *gs);

/* Llamar desde TIM5_IRQHandler (ISR) — muestrea botones y setea input_flag */
void Input_TIM_Callback(GameState_t *gs);

/* Leer joystick del jugador: 0=J1, 1=J2. Llena joy_x y joy_y en gs->j[]   */
void Input_ReadJoystick(GameState_t *gs);

/* Estado del boton START (PC13) — activo BAJO */
uint8_t Input_StartPressed(void);

#endif /* __INPUT_H */
