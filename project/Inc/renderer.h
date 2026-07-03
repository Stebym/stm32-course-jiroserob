/**
 ******************************************************************************
 * @file    renderer.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Capa de renderizado para Beat Clash — pantalla ILI9341 paisaje.
 *          Aqui puedo cambiar colores, tamaños y el estilo de los elementos.
 ******************************************************************************
 */

#ifndef __RENDERER_H
#define __RENDERER_H

#include "game_state.h"
#include <stdint.h>

/* ========================================================================== */
/* === API PUBLICA ========================================================== */
/* ========================================================================== */

/* Dibujar el fondo estatico completo (carriles, divisor, barras de puntaje).*/
/* Llamar solo cuando cambia el estado de pantalla — tarda ~80ms en SPI 8MHz */
void Renderer_DrawBackground(const GameState_t *gs);

/* Actualizar la pantalla segun el estado actual del juego                   */
/* Usa render delta (solo actualiza lo que cambia) para mantener 30fps       */
void Renderer_Update(GameState_t *gs);

/* Dibujar la pantalla de splash */
void Renderer_DrawSplash(void);

/* Dibujar el menu de seleccion de nivel */
void Renderer_DrawMenu(uint8_t cursor);

/* Dibujar el conteo regresivo 3-2-1-GO */
void Renderer_DrawConteo(uint8_t numero);

/* Dibujar la pantalla de resultado (ganador / empate) */
void Renderer_DrawResultado(const GameState_t *gs);

/* Actualizar solo los puntajes en la barra superior */
void Renderer_UpdateScores(const GameState_t *gs);

/* Flash de la zona de presion cuando el jugador presiona */
void Renderer_FlashPressZone(uint8_t jugador, uint8_t carril, uint16_t color);

/* Dibujar o borrar una nota — aqui se aplica el render delta               */
void Renderer_DrawNota(const Nota_t *nota, uint16_t x_off);
void Renderer_EraseNotaTrail(const Nota_t *nota, uint16_t x_off, uint8_t speed);

#endif /* __RENDERER_H */
