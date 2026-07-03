/**
 ******************************************************************************
 * @file    input.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Lectura de botones arcade y joysticks para Beat Clash.
 *
 * Estrategia de debounce: TIM5 ISR muestrea los pines cada 5ms.
 * Usa un contador de muestras consecutivas iguales (DEBOUNCE_N = 3 → 15ms).
 * Los flancos press/release se generan cuando el estado cambia de forma
 * estable. Esto evita ruido mecanico sin necesidad de EXTI ni timers extra.
 *
 * Los joysticks se leen desde el buffer DMA (gs->adc_raw[]) — siempre fresco.
 ******************************************************************************
 */

#include "input.h"
#include "board_pins.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Estado persistente del debounce (ISR lo actualiza, Update() lo consume)   */
static uint8_t debounce_cnt[2][4];    /* conteo de muestras iguales por btn  */
static uint8_t raw_state[2][4];       /* ultimo estado raw leido              */
static volatile uint8_t stable[2];   /* estado estable 4-bit por jugador    */

static GameState_t *p_gs;

/* ========================================================================== */
/* === LECTURA RAW DE PINES ================================================= */
/* ========================================================================== */

/* Retorna 1 si el boton esta presionado (activo BAJO → GPIO=0 → presionado) */
static uint8_t read_btn(uint8_t jugador, uint8_t btn) {
    GPIO_TypeDef *port;
    uint16_t      pin;
    if (jugador == 0) {
        /* J1: PC0-PC3 */
        port = J1_BTN_R_PORT;
        switch (btn) {
            case 0: pin = J1_BTN_R_PIN; break;
            case 1: pin = J1_BTN_G_PIN; break;
            case 2: pin = J1_BTN_B_PIN; break;
            default: pin = J1_BTN_Y_PIN; break;
        }
    } else {
        /* J2: PC4, PC5, PB1, PB3 */
        switch (btn) {
            case 0: port = J2_BTN_R_PORT; pin = J2_BTN_R_PIN; break;
            case 1: port = J2_BTN_R_PORT; pin = J2_BTN_G_PIN; break;
            case 2: port = J2_BTN_B_PORT; pin = J2_BTN_B_PIN; break;
            default: port = J2_BTN_B_PORT; pin = J2_BTN_Y_PIN; break;
        }
    }
    /* Activo BAJO: GPIO=0 → retorna 1 (presionado) */
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

/* ========================================================================== */
/* === INIT ================================================================= */
/* ========================================================================== */

void Input_Init(GameState_t *gs) {
    p_gs = gs;
    memset(debounce_cnt, 0, sizeof(debounce_cnt));
    memset(raw_state,    0, sizeof(raw_state));
    stable[0] = stable[1] = 0;
    gs->j[0].btns = gs->j[1].btns = 0;
    gs->j[0].press = gs->j[1].press = 0;
    gs->j[0].release = gs->j[1].release = 0;
}

/* ========================================================================== */
/* === CALLBACK DE TIM5 ISR (cada 5ms) ====================================== */
/* ========================================================================== */

/* Esta funcion corre en contexto de ISR — solo acceso rapido a registros    */
void Input_TIM_Callback(GameState_t *gs) {
    for (uint8_t j = 0; j < 2; j++) {
        for (uint8_t b = 0; b < 4; b++) {
            uint8_t r = read_btn(j, b);
            if (r == raw_state[j][b]) {
                if (debounce_cnt[j][b] < DEBOUNCE_N)
                    debounce_cnt[j][b]++;
                if (debounce_cnt[j][b] >= DEBOUNCE_N) {
                    /* estado estable — actualizar bit */
                    uint8_t bit = (1 << b);
                    if (r)  stable[j] |=  bit;
                    else    stable[j] &= ~bit;
                }
            } else {
                raw_state[j][b]    = r;
                debounce_cnt[j][b] = 0;
            }
        }
    }
    gs->input_flag = 1;
}

/* ========================================================================== */
/* === ACTUALIZACION (loop principal, con input_flag activo) ================ */
/* ========================================================================== */

void Input_Update(GameState_t *gs) {
    for (uint8_t j = 0; j < 2; j++) {
        uint8_t prev = gs->j[j].btns;
        uint8_t curr = stable[j];
        gs->j[j].btns_prev = prev;
        gs->j[j].btns      = curr;
        gs->j[j].press     = curr & ~prev;   /* flancos de presion           */
        gs->j[j].release   = prev & ~curr;   /* flancos de soltar            */
    }

    /* START button (PC13) — activo BAJO */
    gs->start_btn_prev = gs->start_btn;
    gs->start_btn = (HAL_GPIO_ReadPin(BTN_START_PORT, BTN_START_PIN) == GPIO_PIN_RESET) ? 1U : 0U;

    /* Copiar joysticks desde buffer ADC DMA */
    Input_ReadJoystick(gs);
}

void Input_ReadJoystick(GameState_t *gs) {
    gs->j[0].joy_x = gs->adc_raw[ADC_IDX_J1_X];
    gs->j[0].joy_y = gs->adc_raw[ADC_IDX_J1_Y];
    gs->j[1].joy_x = gs->adc_raw[ADC_IDX_J2_X];
    gs->j[1].joy_y = gs->adc_raw[ADC_IDX_J2_Y];
}

uint8_t Input_StartPressed(void) {
    return (HAL_GPIO_ReadPin(BTN_START_PORT, BTN_START_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}
