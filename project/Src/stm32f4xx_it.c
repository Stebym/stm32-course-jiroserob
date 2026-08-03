/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Rutinas de servicio de interrupcion (ISR) para la Nucleo F411RE.
 *          SPI1 se usa por polling, asi que solo se requieren las
 *          excepciones del nucleo y el SysTick.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"          // HAL completo (necesario para los tipos ADC_HandleTypeDef/TIM_HandleTypeDef y las funciones HAL_*_IRQHandler que se llaman abajo)
#include "stm32f4xx_it.h"           // prototipos de estos mismos handlers (coinciden con el vector de interrupciones del arranque en Startup/)

extern ADC_HandleTypeDef hadc1;     // handle del ADC1 definido en main.c; se necesita aca para pasarselo a HAL_ADC_IRQHandler
extern TIM_HandleTypeDef htim4;     // handle del TIM4 (buzzer) definido en main.c; se necesita aca para pasarselo a HAL_TIM_IRQHandler

/* ========================================================================== */
/* ===== EXCEPCIONES DEL NUCLEO CORTEX-M4 =================================== */
/* ========================================================================== */

/* Handlers de fallas del nucleo: implementacion minima generada por CubeIDE
 * (bucle infinito) -- este proyecto no usa MPU, USB ni ninguna libreria que
 * dispare estas excepciones en operacion normal, asi que quedan como
 * "trampa" de depuracion (si el programa cae aca, hay un fallo de hardware
 * o un acceso invalido a memoria) en vez de con logica de recuperacion. */
void NMI_Handler(void) {}   // interrupcion no enmascarable -- no se usa en este proyecto, cuerpo vacio

void HardFault_Handler(void)   // falla grave de CPU (acceso invalido, instruccion ilegal, etc.)
{
    while (1) {}   // atrapa la ejecucion aca para poder inspeccionar el estado con el debugger
}

void MemManage_Handler(void)   // violacion de la MPU -- no aplica, este proyecto no configura la MPU
{
    while (1) {}   // idem HardFault_Handler: trampa de depuracion
}

void BusFault_Handler(void)   // error de bus (acceso a una direccion de memoria invalida)
{
    while (1) {}   // idem HardFault_Handler: trampa de depuracion
}

void UsageFault_Handler(void)   // instruccion invalida o error de uso del set de instrucciones
{
    while (1) {}   // idem HardFault_Handler: trampa de depuracion
}

void SVC_Handler(void)    {}   // llamada a supervisor (SVC) -- no se usa sin RTOS, cuerpo vacio
void DebugMon_Handler(void) {}   // monitor de depuracion -- no se usa, cuerpo vacio
void PendSV_Handler(void) {}   // interrupcion de cambio de contexto -- no se usa sin RTOS, cuerpo vacio

/* Sin esta, HAL_Delay y los timeouts internos del HAL se traban */
void SysTick_Handler(void)   // interrupcion del SysTick del nucleo, configurada a 1ms por HAL_Init()
{
    HAL_IncTick();   // incrementa el contador de milisegundos que usan HAL_GetTick()/HAL_Delay() en todo el proyecto
}

/* ========================================================================== */
/* ===== PERIFERICOS DEL JOYSTICK ============================================ */
/* ========================================================================== */

/* El disparo periodico de la conversion ADC (cada 20ms) viene de TIM3 (ver
 * TIM3_ADCTrigger_Init en main.c); esta interrupcion solo atiende el "fin de
 * conversion" del propio ADC1 para que HAL_ADC_ConvCpltCallback (main.c)
 * pueda leer el canal recien convertido y encadenar el siguiente. No hay un
 * handler EXTI en este archivo: los pines de click (SW) de ambos joystick
 * fueron retirados fisicamente del montaje (ver board_pins.h), por lo que
 * el handler EXTI0_IRQHandler que antes los atendia se elimino junto con el
 * resto del codigo asociado. */
void ADC_IRQHandler(void)   // interrupcion de fin de conversion del ADC1 (los 4 canales del joystick 1 y 2)
{
    HAL_ADC_IRQHandler(&hadc1);   // delega en el HAL, que a su vez llama a HAL_ADC_ConvCpltCallback (definido en main.c)
}

/* ========================================================================== */
/* ===== BUZZER (TONO POR SOFTWARE) =========================================== */
/* ========================================================================== */

void TIM4_IRQHandler(void)   // interrupcion periodica de TIM4, usada para alternar PA6 y generar el tono del buzzer
{
    HAL_TIM_IRQHandler(&htim4);   // delega en el HAL, que a su vez llama a HAL_TIM_PeriodElapsedCallback (definido en main.c, ahi se alterna el pin)
}
