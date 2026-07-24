/**
 ******************************************************************************
 * @file    stm32f4xx_hal_conf.h
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Configuracion del HAL para el Examen Parcial Taller V.
 *          Solo se habilitan los modulos que realmente usa el proyecto:
 *          GPIO, RCC, TIM, ADC, UART, I2C, RTC, DMA, CORTEX, FLASH, PWR, EXTI.
 *          El resto esta comentado para reducir el tiempo de compilacion.
 ******************************************************************************
 */

#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* ============= MODULOS HAL HABILITADOS PARA ESTE PROYECTO ================ */
/* ========================================================================== */

#define HAL_MODULE_ENABLED          /* nucleo del hal, siempre requerido      */
#define HAL_ADC_MODULE_ENABLED      /* potenciometro PA4                      */
#define HAL_CORTEX_MODULE_ENABLED   /* nvic y systick                         */
#define HAL_DMA_MODULE_ENABLED      /* requerido internamente por adc y uart  */
#define HAL_EXTI_MODULE_ENABLED     /* requerido internamente por gpio        */
#define HAL_FLASH_MODULE_ENABLED    /* requerido por hal_init (flash latency) */
#define HAL_GPIO_MODULE_ENABLED     /* todos los pines del proyecto           */
#define HAL_I2C_MODULE_ENABLED      /* oled gme12864-41 (i2c1, pb8/pb9)       */
#define HAL_PWR_MODULE_ENABLED      /* requerido por hal_init                 */
#define HAL_RCC_MODULE_ENABLED      /* relojes, pll a 100mhz y mco1           */
#define HAL_RTC_MODULE_ENABLED      /* rtc interno con lse + vbat             */
#define HAL_TIM_MODULE_ENABLED      /* blinky 250ms y trigger del adc         */
#define HAL_UART_MODULE_ENABLED     /* usart2 comunicacion serial             */

/* Modulos NO usados en este proyecto — comentados para compilacion rapida   */
/* #define HAL_CAN_MODULE_ENABLED    */
/* #define HAL_CRC_MODULE_ENABLED    */
/* #define HAL_CRYP_MODULE_ENABLED   */
/* #define HAL_DAC_MODULE_ENABLED    */
/* #define HAL_DCMI_MODULE_ENABLED   */
/* #define HAL_DMA2D_MODULE_ENABLED  */
/* #define HAL_ETH_MODULE_ENABLED    */
/* #define HAL_HASH_MODULE_ENABLED   */
/* #define HAL_HCD_MODULE_ENABLED    */
/* #define HAL_I2C_MODULE_ENABLED    */  /* ya esta arriba */
/* #define HAL_I2S_MODULE_ENABLED    */
/* #define HAL_IRDA_MODULE_ENABLED   */
/* #define HAL_IWDG_MODULE_ENABLED   */
/* #define HAL_LPTIM_MODULE_ENABLED  */
/* #define HAL_LTDC_MODULE_ENABLED   */
/* #define HAL_MMC_MODULE_ENABLED    */
/* #define HAL_NAND_MODULE_ENABLED   */
/* #define HAL_NOR_MODULE_ENABLED    */
/* #define HAL_PCD_MODULE_ENABLED    */
/* #define HAL_PCCARD_MODULE_ENABLED */
/* #define HAL_QSPI_MODULE_ENABLED   */
/* #define HAL_RNG_MODULE_ENABLED    */
/* #define HAL_RTC_MODULE_ENABLED    */  /* ya esta arriba */
/* #define HAL_SAI_MODULE_ENABLED    */
/* #define HAL_SD_MODULE_ENABLED     */
/* #define HAL_SDRAM_MODULE_ENABLED  */
/* #define HAL_SMARTCARD_MODULE_ENABLED */
/* #define HAL_SMBUS_MODULE_ENABLED  */
/* #define HAL_SPI_MODULE_ENABLED    */
/* #define HAL_SRAM_MODULE_ENABLED   */
/* #define HAL_TIM_MODULE_ENABLED    */  /* ya esta arriba */
/* #define HAL_USART_MODULE_ENABLED  */
/* #define HAL_WWDG_MODULE_ENABLED   */

/* ========================================================================== */
/* ============= VALORES DE OSCILADORES ==================================== */
/* ========================================================================== */

/* HSI interno a 16 MHz — es el reloj del sistema en este proyecto          */
#if !defined(HSI_VALUE)
  #define HSI_VALUE    16000000U
#endif

/* HSE externo — la nucleo-f411re tiene cristal de 8 MHz en el ST-LINK      */
/* no lo usamos pero lo declaramos por si el hal lo referencia internamente  */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    8000000U
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE    32000U
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE    32768U
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000U
#endif

#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE    12288000U
#endif

/* ========================================================================== */
/* ============= CONFIGURACION DEL SISTEMA ================================== */
/* ========================================================================== */

#define VDD_VALUE                 3300U   /* tension de alimentacion en mV   */
#define TICK_INT_PRIORITY         0x0FU   /* prioridad del systick tick       */
#define USE_RTOS                  0U
#define PREFETCH_ENABLE           1U
#define INSTRUCTION_CACHE_ENABLE  1U
#define DATA_CACHE_ENABLE         1U

/* Callbacks por registro: desactivados (usamos los callbacks globales HAL)  */
#define USE_HAL_ADC_REGISTER_CALLBACKS   0U
#define USE_HAL_TIM_REGISTER_CALLBACKS   0U
#define USE_HAL_UART_REGISTER_CALLBACKS  0U

/* ========================================================================== */
/* ============= INCLUDES DE LOS MODULOS HABILITADOS ======================= */
/* ========================================================================== */

#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32f4xx_hal_exti.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f4xx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32f4xx_hal_cortex.h"
#endif

#ifdef HAL_ADC_MODULE_ENABLED
  #include "stm32f4xx_hal_adc.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32f4xx_hal_flash.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
  #include "stm32f4xx_hal_i2c.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32f4xx_hal_pwr.h"
#endif

#ifdef HAL_RTC_MODULE_ENABLED
  #include "stm32f4xx_hal_rtc.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32f4xx_hal_tim.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32f4xx_hal_uart.h"
#endif

/* ========================================================================== */
/* ============= MACRO DE ASSERT (desactivado en produccion) ================ */
/* ========================================================================== */

/* Descomentar para activar las verificaciones de parametros del HAL:        */
/* #define USE_FULL_ASSERT  1U                                                */

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_HAL_CONF_H */
