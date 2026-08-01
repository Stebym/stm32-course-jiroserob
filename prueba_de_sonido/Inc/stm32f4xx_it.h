/**
 ******************************************************************************
 * @file    : stm32f4xx_it.h
 * @author  : Jimmy Stebym Rosero Barrera
 * @brief   : Cabeceras de las rutinas de servicio de interrupcion (ISR).
 ******************************************************************************
 */

#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* ===== EXCEPCIONES DEL NUCLEO CORTEX-M4 =================================== */
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

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */
