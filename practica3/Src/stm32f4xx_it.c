/**
 ******************************************************************************
 * @file    : stm32f4xx_it.c
 * @author  : Jimmy Stebym Rosero Barrera
 * @brief   : Rutinas de servicio de interrupcion para la Nucleo F411RE.
 *            Aqui el micro "salta" cuando el hardware necesita atencion.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

// nos traemos los handles del main para pasarselos al hal
extern TIM_HandleTypeDef  htim10;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef  hadc1;

/* ========================================================================== */
/* ===== EXCEPCIONES DEL NUCLEO CORTEX-M4 =================================== */
/* ========================================================================== */

// interrupcion no enmascarable, no deberia pasar nunca en condicion normal
void NMI_Handler(void) {}

// si llegamos aca es porque algo salio muy mal, nos quedamos en el loop
void HardFault_Handler(void)
{
    while (1) {}
}

// fallo de acceso a memoria (puntero nulo, desborde de stack, etc)
void MemManage_Handler(void)
{
    while (1) {}
}

// error en el bus de datos o instruciones
void BusFault_Handler(void)
{
    while (1) {}
}

// instruccion ilegal o division por cero (si se habilita en el ccr)
void UsageFault_Handler(void)
{
    while (1) {}
}

void SVC_Handler(void)    {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

// esta es re importante: sin ella el HAL_Delay y los timeouts del hal se traban
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ========================================================================== */
/* ===== INTERRUPCIONES DE LOS PERIFERICOS ================================== */
/* ========================================================================== */

// tim10 comparte este vector con tim1; le pasamos la bola al hal
// el hal revisa las banderas y llama a HAL_TIM_PeriodElapsedCallback en main.c
void TIM1_UP_TIM10_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim10);
}

// cuando llega un byte por el usart2, el hal maneja las banderas
// y llama a HAL_UART_RxCpltCallback en main.c
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

// cuando el adc termina una conversion dispara esta interrupcion
// el hal llama a HAL_ADC_ConvCpltCallback en main.c
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
