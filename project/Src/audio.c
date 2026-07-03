/**
 ******************************************************************************
 * @file    audio.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Control de buzzers pasivos via TIM2/TIM3 PWM.
 *          Aqui puedo ajustar duraciones y frecuencias de los efectos.
 *
 * Buzzer J1 — PA15 (TIM2_CH1, AF1). PSC=15 → timer clock = 1MHz.
 * Frecuencia en Hz: ARR = 1000000 / freq - 1. CCR = ARR / 2 (50% duty).
 * Para silencio: CCR = 0 (sin flanco → sin sonido aunque el timer corra).
 *
 * ADVERTENCIA HARDWARE: si el buzzer es electromagnetico (bobina < 200Ω),
 * usar transistor driver o canal de ULN2003A entre GPIO y buzzer.
 * Los buzzers piezoelectricos pueden conectarse directamente al pin.
 ******************************************************************************
 */

#include "audio.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

static TIM_HandleTypeDef *p_htim2;
static TIM_HandleTypeDef *p_htim3;

/* tick de inicio y duracion para cada jugador */
static uint32_t tone_start[2];
static uint32_t tone_dur[2];
static uint8_t  tone_active[2];

/* ========================================================================== */
/* === IMPLEMENTACION ======================================================= */
/* ========================================================================== */

void Audio_Init(TIM_HandleTypeDef *h2, TIM_HandleTypeDef *h3) {
    p_htim2 = h2;
    p_htim3 = h3;
    tone_active[0] = 0;
    tone_active[1] = 0;
}

static void set_freq(TIM_HandleTypeDef *ht, uint8_t ch, uint32_t freq_hz) {
    if (freq_hz == 0) {
        /* Silencio — CCR = 0 deja el pin en bajo continuamente             */
        __HAL_TIM_SET_COMPARE(ht, ch == 1 ? TIM_CHANNEL_1 : TIM_CHANNEL_1, 0);
        return;
    }
    /* 1MHz timer clock. ARR = 1000000/freq - 1. CCR = ARR/2 (50% duty).   */
    uint32_t arr = (1000000U / freq_hz);
    if (arr < 2) arr = 2;
    arr -= 1;
    __HAL_TIM_SET_AUTORELOAD(ht, arr);
    __HAL_TIM_SET_COMPARE(ht, TIM_CHANNEL_1, arr / 2);
}

void Audio_PlayTone(uint8_t jugador, uint32_t freq_hz, uint32_t dur_ms) {
    TIM_HandleTypeDef *ht = (jugador == 0) ? p_htim2 : p_htim3;
    if (!ht) return;
    set_freq(ht, 1, freq_hz);
    tone_start[jugador]  = HAL_GetTick();
    tone_dur[jugador]    = dur_ms;
    tone_active[jugador] = 1;
}

void Audio_Update(void) {
    uint32_t now = HAL_GetTick();
    for (uint8_t j = 0; j < 2; j++) {
        if (tone_active[j] && (now - tone_start[j] >= tone_dur[j])) {
            tone_active[j] = 0;
            TIM_HandleTypeDef *ht = (j == 0) ? p_htim2 : p_htim3;
            if (ht) __HAL_TIM_SET_COMPARE(ht, TIM_CHANNEL_1, 0);  /* silencio */
        }
    }
}

void Audio_Silence(void) {
    if (p_htim2) __HAL_TIM_SET_COMPARE(p_htim2, TIM_CHANNEL_1, 0);
    if (p_htim3) __HAL_TIM_SET_COMPARE(p_htim3, TIM_CHANNEL_1, 0);
    tone_active[0] = tone_active[1] = 0;
}
