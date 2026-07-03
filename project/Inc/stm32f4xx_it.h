/**
 ******************************************************************************
 * @file    stm32f4xx_it.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Declaraciones de ISR para Beat Clash.
 *          Aqui agrego/quito ISRs segun los perifericos que use.
 ******************************************************************************
 */

#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* === EXCEPCIONES DEL NUCLEO CORTEX-M4 ==================================== */
/* ========================================================================== */

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* ========================================================================== */
/* === INTERRUPCIONES DE PERIFERICOS DEL PROYECTO =========================== */
/* ========================================================================== */

/* TIM5: game tick a 5ms — muestrea botones y setea input_flag              */
void TIM5_IRQHandler(void);

/* DMA2 Stream0: transferencia circular ADC1 → adc_raw[]                    */
void DMA2_Stream0_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */
