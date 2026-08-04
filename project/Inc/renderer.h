/**
 ******************************************************************************
 * @file    renderer.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Capa de renderizado para Beat Clash sobre pantalla ILI9341.
 *          Prototipos de las funciones de dibujo (pantallas, HUD, notas);
 *          la implementacion define colores, tamaños y estilo visual.
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

/* Nombre de 3 letras elegido por el jugador (ver DEMO_INICIALES en main.c) --
 * implementada en main.c, expuesta aca para que las funciones Cockpit_* de
 * abajo puedan mostrarlo en vez de un generico "J1"/"J2". */
const char *Nombre_Jugador(uint8_t jugador);

/* Pantalla de entrada de iniciales (3 letras): joystick arriba/abajo cambia
 * la letra A-Z de la posicion actual, cualquier boton confirma esa
 * posicion (logica en main.c, MenuIniciales_Procesar). nombre: 3 letras +
 * terminador nulo. pos_actual: 0-2, cual letra esta resaltada (amarillo). */
void Renderer_DrawNombre(uint8_t jugador, const char nombre[4], uint8_t pos_actual);
/* Redibuja SOLO la letra en `pos` (mientras se cicla A-Z), resaltada como
 * "en edicion" (amarillo). */
void Renderer_UpdateNombreLetra(uint8_t jugador, uint8_t pos, char letra);
/* Redibuja la letra en `pos` como "ya confirmada" (gris, no editable). */
void Renderer_ConfirmarNombreLetra(uint8_t pos, char letra);

/* Dibujar el menu de seleccion de modo (0=Simon, 1=Simon+Joystick, 2=Guitar Hero) */
void Renderer_DrawSeleccionModo(uint8_t cursor);

/* Mueve el cursor de seleccion de modo sin FillScreen — solo repinta las
 * tarjetas que cambian de estado y el texto de descripcion. */
void Renderer_UpdateSeleccionModo(uint8_t cursor_ant, uint8_t cursor);

/* Invierte la posicion en pantalla de ROJO<->AMARILLO y VERDE<->AZUL en la
 * cuadricula 2x2 de Simon Clasico, para que coincida con la disposicion
 * fisica real de los botones arcade (pedido explicito del usuario, solo
 * para la partida de 2 jugadores -- el modo de 1 jugador no debe tocarse,
 * ver Botones_IniciarSolo en main.c). Llamar ANTES de dibujar la pantalla
 * (Renderer_DrawModoSimonClasico / *Jugador); el valor queda vigente hasta
 * la proxima llamada. */
void Renderer_SetSimonClasicoInvertido(uint8_t invertido);

/* Vista previa de modo Simon Clasico — paso_activo 0-3 encendido, 0xFF ninguno */
void Renderer_DrawModoSimonClasico(uint8_t paso_j1, uint8_t paso_j2);
/* Redibuja SOLO un jugador (etiqueta + cuadricula 2x2) sin tocar al otro --
 * usar al reiniciar un jugador que perdio. */
void Renderer_DrawModoSimonClasicoJugador(uint8_t jugador, uint8_t paso);
void Renderer_UpdateModoSimonClasicoPaso(uint8_t jugador, uint8_t paso_ant, uint8_t paso);
void Renderer_ActualizarRachaBotones(uint8_t jugador, uint16_t racha);
void Renderer_DibujarGameOverBotones(uint8_t jugador, uint16_t racha, uint16_t mejor);

/* Modo Simon Botones a PANTALLA COMPLETA para 1 jugador (retrato 240x320,
 * sin dividir) -- 4 domos gigantes en cuadricula 2x2, mismo espiritu que
 * Renderer_DrawModoSimonJoystick1P. Usar en vez de las 2 funciones de
 * arriba cuando btn_modo_1p este activo (ver Botones_ReiniciarJugador en
 * main.c). */
void Renderer_DrawModoSimonClasico1P(uint8_t paso);
void Renderer_UpdateModoSimonClasico1PPaso(uint8_t paso_ant, uint8_t paso);

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

/* Cursor "X" por jugador (mismo cursor que la version de 1 jugador, uno
 * independiente por mitad) -- para verificar a simple vista que cada lado
 * mueve el joystick fisico correcto. joy_x/joy_y: cuenta cruda del adc. */
void Renderer_ActualizarCursorJoystick2P(uint8_t jugador, uint16_t joy_x, uint16_t joy_y, uint8_t listo);
void Renderer_ResetCursorJoystick2P(uint8_t jugador);


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

/* ========================================================================== */
/* === GUITAR HERO — CARA A CARA (portrait, cockpit) ========================= */
/* ========================================================================== */
/* Mismo mecanismo cara-a-cara (portrait, mitad de arriba rotada 180) que
 * Renderer_DrawModoSimon* arriba. Restyle inspirado en video_box/video_ellipse
 * del repo FPGA de referencia (ver guitar_hero/ANALISIS_REFERENCIA.md):
 * carril = barra angosta de color sobre fondo negro, zona de golpe = circulo
 * relleno. Llamar con ILI9341_SetPortrait(1) ya activado. */

/* Dimensiones de cada mitad "cockpit" (retrato dividido a la mitad) --
 * deben coincidir con COCK_ZONE_W/H en renderer.c. */
#define COCKPIT_ZONE_W  240
#define COCKPIT_ZONE_H  155

/* Centro X LOCAL de la zona de golpe circular de cada carril -- debe
 * coincidir con GH_ZONA_CX en renderer.c. Expuesto para que main.c calcule
 * la distancia de un golpe sin duplicar el numero magico en 2 archivos sin
 * relacion directa entre si. */
#define GH_ZONA_CX  16

/* Radios de la zona de golpe y de las notas, en los 2 layouts de Guitar
 * Hero (2 jugadores/cockpit y 1 jugador/pantalla completa) -- deben
 * coincidir con GH_ZONA_R/GH_NOTE_R/GH1P_ZONA_R/GH1P_NOTE_R en renderer.c.
 * Expuestos para que main.c pueda calcular a que distancia una nota
 * "toca" visualmente la zona de golpe (radio zona + radio nota) sin
 * duplicar los numeros magicos -- ver GH_HIT_OK_2P/GH_HIT_OK_1P mas abajo. */
#define GH_ZONA_R    13   // radio de la zona de golpe en el layout de 2 jugadores
#define GH_NOTE_R    12   // radio de las notas en el layout de 2 jugadores
#define GH1P_ZONA_R  16   // radio de la zona de golpe en el layout de 1 jugador (pantalla completa)
#define GH1P_NOTE_R  14   // radio de las notas en el layout de 1 jugador (pantalla completa)

/* Distancia entre centros a la que la nota y la zona de golpe EMPIEZAN a
 * tocarse visualmente (suma de radios) -- usada como ventana "OK" (el
 * golpe de menor puntaje, pero todavia valido) del hit-test de main.c, para
 * que un golpe cuente apenas la nota entra en contacto con el circulo, sin
 * tener que esperar a que este mas centrada. Cada modo usa su propia
 * constante porque el layout de 1 jugador tiene circulos mucho mas grandes
 * (ver GH1P_ZONA_R/GH1P_NOTE_R arriba) -- un umbral fijo compartido con 2
 * jugadores dejaba el modo 1P sintiendose poco responsivo (la nota tocaba
 * el circulo antes de que el golpe se aceptara). */
#define GH_HIT_OK_2P   (GH_ZONA_R   + GH_NOTE_R)     // 13+12 = 25px
#define GH_HIT_OK_1P   (GH1P_ZONA_R + GH1P_NOTE_R)   // 24+22 = 46px

void Renderer_DrawModoGuitarHero2P(void);
/* Redibuja SOLO un jugador (etiqueta + 4 carriles) -- usar tanto para el
 * dibujo inicial de 1 jugador como para arrancar una ronda nueva. */
void Renderer_DrawModoGuitarHeroJugador(uint8_t jugador);
void Renderer_GH_DrawNota(uint8_t jugador, const Nota_t *nota);
void Renderer_GH_EraseNotaTrail(uint8_t jugador, const Nota_t *nota, uint8_t speed);
void Renderer_GH_ActualizarPuntaje(uint8_t jugador, uint16_t puntaje, uint16_t combo);
void Renderer_GH_DibujarFin(uint8_t jugador, uint16_t puntaje);

/* Efecto de impacto: pinta la zona de golpe de `carril` en blanco brillante
 * durante 1 frame cuando el jugador acierta -- llamar
 * Renderer_GH_ActualizarFlashes(jugador) una vez por jugador en cada tick
 * del loop principal para que revierta automaticamente al frame siguiente. */
void Renderer_GH_FlashZona(uint8_t jugador, uint8_t carril);
void Renderer_GH_ActualizarFlashes(uint8_t jugador);

/* Guitar Hero a PANTALLA COMPLETA para 1 jugador (retrato 240x320, sin
 * dividir) -- mismos nombres que los de arriba sin el sufijo "Jugador"/
 * parametro "jugador" (siempre es el unico jugador en pantalla). Usar en
 * vez de las funciones de arriba cuando guitar_modo_1p este activo (ver
 * GuitarHero_ActualizarJugador en main.c). La geometria horizontal
 * (GH_ZONA_CX, COCKPIT_ZONE_W, NOTE_W) es la misma que en el modo de 2
 * jugadores -- solo cambia el alto de carril y el radio de zona/nota. */
void Renderer_DrawModoGuitarHero1P(void);
void Renderer_GH1P_DrawNota(const Nota_t *nota);
void Renderer_GH1P_EraseNotaTrail(const Nota_t *nota, uint8_t speed);
void Renderer_GH1P_ActualizarPuntaje(uint16_t puntaje, uint16_t combo);
void Renderer_GH1P_DibujarFin(uint16_t puntaje);
void Renderer_GH1P_FlashZona(uint8_t carril);
void Renderer_GH1P_ActualizarFlashes(void);

#endif /* __RENDERER_H */
