/**
 ******************************************************************************
 * @file    board_pins.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Mapa de pines del buzzer en Nucleo-F411RE.
 ******************************************************************************
 */

#ifndef __BOARD_PINS_H
#define __BOARD_PINS_H

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* === BUZZER — TIPO SIN CONFIRMAR (activo u pasivo) ========================= */
/* ========================================================================== */
/* PA6: mismo pin ya usado y probado en practica_pantalla (libre porque el   */
/* SPI1 de la pantalla no usa MISO). Aca no hay pantalla, pero se deja el    */
/* mismo pin para no tener que recablear el buzzer entre proyectos.         */
/*                                                                            */
/* El usuario no esta seguro si el modulo que compro es activo (oscilador    */
/* interno, un solo tono, suena con DC) o pasivo (sin oscilador, necesita     */
/* onda cuadrada de 2-5kHz, NO suena con DC segun su propia hoja de datos    */
/* "TAR-BUZZER-PAS"). Por eso PA6 se maneja por TIM3_CH1 (AF2) en modo PWM:  */
/* permite generar tanto una senal DC pura (duty 100%, prueba de activo)     */
/* como un tono variable de verdad (prueba de pasivo), sin recablear nada.  */

#define BUZZER_PORT     GPIOA
#define BUZZER_PIN      GPIO_PIN_6
#define BUZZER_TIM_AF   GPIO_AF2_TIM3   /* PA6 = TIM3_CH1 */

/* Boton de usuario B1 de la Nucleo — dispara/avanza pruebas de sonido */
#define BTN_USER_PORT   GPIOC
#define BTN_USER_PIN    GPIO_PIN_13

/* ========================================================================== */
/* === CLICK (SW) DEL JOYSTICK — para la cancion actual ====================== */
/* ========================================================================== */
/* Mismo pin que en practica_pantalla/examen_parcial: PA0, activo BAJO con
 * pull-up interno. Aca solo se usa el click (no los ejes X/Y, no hace falta
 * ADC en este proyecto). Se lee por polling, igual que B1. */
#define JOY_SW_PORT     GPIOA
#define JOY_SW_PIN      GPIO_PIN_0

#endif /* __BOARD_PINS_H */
