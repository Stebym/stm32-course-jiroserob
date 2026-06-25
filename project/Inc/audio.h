/**
 ******************************************************************************
 * @file    audio.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Control de buzzers via PWM (TIM2 CH1 = J1, TIM3 CH1 = J2).
 *          Aqui puedo cambiar las frecuencias y duraciones de los sonidos.
 ******************************************************************************
 */

#ifndef __AUDIO_H
#define __AUDIO_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ========================================================================== */
/* === FRECUENCIAS EN Hz (TIM clock = 1MHz con PSC=15) ====================== */
/* ========================================================================== */

#define AUDIO_NOTE_HIT_PERFECT  1047    /* C6 — golpe perfecto               */
#define AUDIO_NOTE_HIT_GOOD      880    /* A5 — golpe bueno                  */
#define AUDIO_NOTE_HIT_OK        660    /* E5 — golpe OK                     */
#define AUDIO_NOTE_MISS          196    /* G3 — fallo                        */
#define AUDIO_NOTE_BEEP_HIGH     880    /* pitido del conteo (3,2,1)         */
#define AUDIO_NOTE_GO           1319    /* E6 — GO!                          */
#define AUDIO_NOTE_VICTORY      1047    /* melodia de victoria               */
#define AUDIO_SILENCE              0    /* sin sonido                        */

/* ========================================================================== */
/* === API PUBLICA ========================================================== */
/* ========================================================================== */

/* Inicializar el modulo — llamar despues de MX_TIM2_Init y MX_TIM3_Init     */
void Audio_Init(TIM_HandleTypeDef *h2, TIM_HandleTypeDef *h3);

/* Reproducir un tono para el jugador dado (0=J1 usa TIM2, 1=J2 usa TIM3)   */
/* freq_hz=0 silencia el buzzer. La duracion se maneja en Audio_Update().    */
void Audio_PlayTone(uint8_t jugador, uint32_t freq_hz, uint32_t dur_ms);

/* Actualizar el modulo — llamar cada iteracion del loop principal            */
/* Detiene el tono cuando expira dur_ms.                                     */
void Audio_Update(void);

/* Silenciar inmediatamente ambos buzzers */
void Audio_Silence(void);

#endif /* __AUDIO_H */
