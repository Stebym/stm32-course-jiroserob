/**
 ******************************************************************************
 * @file    game_fsm.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Maquina de estados del juego Beat Clash.
 *          Aqui puedo ajustar la logica de niveles y la generacion de notas.
 ******************************************************************************
 */

#ifndef __GAME_FSM_H
#define __GAME_FSM_H

#include "game_state.h"

/* ========================================================================== */
/* === API PUBLICA ========================================================== */
/* ========================================================================== */

/* Inicializar el estado del juego y el generador de numeros aleatorios      */
void Game_Init(GameState_t *gs);

/* Actualizar la FSM — llamar cada GAME_TICK_MS desde el loop principal      */
void Game_Update(GameState_t *gs, uint32_t now);

/* Sembrar el generador aleatorio con ruido ADC                              */
void Game_SeedRandom(uint32_t seed);

#endif /* __GAME_FSM_H */
