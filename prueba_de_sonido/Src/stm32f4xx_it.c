/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Rutinas de servicio de interrupcion (ISR) para la Nucleo F411RE.
 *          Solo hay GPIO por polling (buzzer + boton b1), asi que unicamente
 *          se requieren las excepciones del nucleo y el SysTick.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

/* ========================================================================== */
/* ===== EXCEPCIONES DEL NUCLEO CORTEX-M4 =================================== */
/* ========================================================================== */

void NMI_Handler(void) {}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void SVC_Handler(void)    {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* Sin esta, HAL_Delay y los timeouts internos del HAL se traban */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
