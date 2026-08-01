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

/* Dibujar el menu de seleccion de nivel (velocidad, legado) -- pantalla de
 * la primera version del proyecto, anterior a introducir la seleccion de
 * cantidad de jugadores y de modo de juego. Se conserva sin usar en el
 * recorrido actual por si se retoma el ajuste de dificultad por niveles. */
void Renderer_DrawMenu(uint8_t cursor);

/* Dibujar la pantalla de "cuantos jugadores" (0=1 jugador, 1=2 jugadores) */
void Renderer_DrawSeleccionJugadores(uint8_t cursor);

/* Mueve el cursor de seleccion de jugadores sin FillScreen — solo repinta
 * las 2 tarjetas que cambian de estado (evita el parpadeo al navegar). */
void Renderer_UpdateSeleccionJugadores(uint8_t cursor_ant, uint8_t cursor);

/* Dibujar el menu de seleccion de modo (0=Simon, 1=Simon+Joystick, 2=Guitar Hero) */
void Renderer_DrawSeleccionModo(uint8_t cursor);

/* Mueve el cursor de seleccion de modo sin FillScreen — solo repinta las
 * tarjetas que cambian de estado y el texto de descripcion. */
void Renderer_UpdateSeleccionModo(uint8_t cursor_ant, uint8_t cursor);

/* Vista previa de modo Simon Clasico — paso_activo 0-3 encendido, 0xFF ninguno */
void Renderer_DrawModoSimonClasico(uint8_t paso_j1, uint8_t paso_j2);
/* Redibuja SOLO un jugador (etiqueta + cuadricula 2x2) sin tocar al otro --
 * usar al reiniciar un jugador que perdio. */
void Renderer_DrawModoSimonClasicoJugador(uint8_t jugador, uint8_t paso);
void Renderer_UpdateModoSimonClasicoPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso);
void Renderer_ActualizarRachaBotones(uint8_t jugador, uint16_t racha);
void Renderer_DibujarGameOverBotones(uint8_t jugador, uint16_t racha, uint16_t mejor);

/* Simon+Joystick a 2 jugadores cara a cara (portrait, cockpit) -- cada
 * jugador juega su PROPIA secuencia independiente en su mitad de la mesa,
 * con el mismo mecanismo de rotacion 180 grados que Simon Clasico. Llamar
 * con ILI9341_SetPortrait(1) ya activado. */
void Renderer_DrawModoSimonJoystick2P(uint8_t paso_j1, uint8_t paso_j2);
/* Redibuja SOLO un jugador (etiqueta + 4 badges) -- usar al reiniciar un
 * jugador que perdio, sin tocar la partida en curso del otro. */
void Renderer_DrawModoSimonJoystick2PJugador(uint8_t jugador, uint8_t paso);
void Renderer_UpdateModoSimonJoystick2PPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso);
void Renderer_ActualizarRachaJoystick2P(uint8_t jugador, uint16_t racha);
void Renderer_DibujarGameOverJoystick2P(uint8_t jugador, uint16_t racha, uint16_t mejor);


/* Version a pantalla completa para 1 solo jugador (mientras no haya 2do
 * joystick conectado) — D-pad grande y centrado, sin dividir la pantalla. */
void Renderer_DrawModoSimonJoystick1P(uint8_t paso);
void Renderer_UpdateModoSimonJoystick1P(uint8_t paso_ant, uint8_t paso);

/* Cursor "X" que sigue la posicion cruda del joystick (como en examen_parcial),
 * dentro del hueco del centro del D-pad de 1 jugador. joy_x/joy_y: cuenta
 * cruda del adc (0-4095). listo: verde si el joystick esta centrado (armado
 * para el siguiente movimiento), gris si sigue inclinado. */
void Renderer_ActualizarCursorJoystick(uint16_t joy_x, uint16_t joy_y, uint8_t listo);

/* Olvida la ultima posicion dibujada del cursor -- llamar al reiniciar el
 * juego para que el proximo Renderer_ActualizarCursorJoystick no intente
 * borrar una posicion de una partida anterior. */
void Renderer_ResetCursorJoystick(void);

/* Lista de canciones tipo "reproductor" -- 6 items, navegable con el eje Y
 * del joystick. cursor: indice 0-5. El ORDEN debe coincidir exactamente con
 * CANCIONES_NOMBRE/DATA/LEN en main.c (BIENVENIDA, ESTRELLITA, HIMNO
 * ALEGRIA, MARTINILLO, NAVIDAD, TETRIS). */
void Renderer_DrawListaCanciones(uint8_t cursor);
void Renderer_UpdateListaCanciones(uint8_t cursor_ant, uint8_t cursor);

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
