/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Rutinas de servicio de interrupcion para Beat Clash.
 *          Aqui agrego nuevos IRQHandler si integro mas perifericos.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

/* Handles definidos en main.c */
extern TIM_HandleTypeDef htim5;
extern DMA_HandleTypeDef hdma_adc1;

/* ========================================================================== */
/* === EXCEPCIONES DEL NUCLEO =============================================== */
/* ========================================================================== */

void NMI_Handler(void) {
    while (1) {}
}

void HardFault_Handler(void) {
    while (1) {}
}

void MemManage_Handler(void) {
    while (1) {}
}

void BusFault_Handler(void) {
    while (1) {}
}

void UsageFault_Handler(void) {
    while (1) {}
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* SysTick: tick del HAL a 1ms — no tocar */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/* ========================================================================== */
/* === INTERRUPCIONES DE PERIFERICOS ======================================== */
/* ========================================================================== */

/* TIM5: game tick a 5ms — llama Input_TIM_Callback via HAL callback         */
void TIM5_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim5);
}

/* DMA2 Stream0: necesario para que HAL_ADC_Start_DMA funcione correctamente */
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_adc1);
}
