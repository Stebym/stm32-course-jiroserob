/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Rutinas de servicio de interrupcion (ISR) para la Nucleo F411RE.
 *          SPI1 se usa por polling, asi que solo se requieren las
 *          excepciones del nucleo y el SysTick.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim4;

/* ========================================================================== */
/* ===== EXCEPCIONES DEL NUCLEO CORTEX-M4 =================================== */
/* ========================================================================== */

/* Handlers de fallas del nucleo: implementacion minima generada por CubeIDE
 * (bucle infinito) -- este proyecto no usa MPU, USB ni ninguna libreria que
 * dispare estas excepciones en operacion normal, asi que quedan como
 * "trampa" de depuracion (si el programa cae aca, hay un fallo de hardware
 * o un acceso invalido a memoria) en vez de con logica de recuperacion. */
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

/* ========================================================================== */
/* ===== PERIFERICOS DEL JOYSTICK ============================================ */
/* ========================================================================== */

/* El disparo periodico de la conversion ADC (cada 20ms) viene de TIM3 (ver
 * TIM3_ADCTrigger_Init en main.c); esta interrupcion solo atiende el "fin de
 * conversion" del propio ADC1 para que HAL_ADC_ConvCpltCallback (main.c)
 * pueda leer el canal recien convertido y encadenar el siguiente. No hay un
 * handler EXTI en este archivo: los clicks de los joystick se leian antes
 * por EXTI0 (SW del joystick 1), pero ese pin y el SW del joystick 2 se
 * retiraron fisicamente el 2026-07-30 (ver board_pins.h) y el handler
 * correspondiente (EXTI0_IRQHandler) se elimino junto con el resto del
 * codigo asociado. */
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}

/* ========================================================================== */
/* ===== BUZZER (TONO POR SOFTWARE) =========================================== */
/* ========================================================================== */

void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}
