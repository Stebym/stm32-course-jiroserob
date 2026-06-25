/*
 * main.c
 * Autor  : Jimmy Stebym Rosero Barrera
 * Tarea 3: Control de LED RGB con FSM, ADC, Encoder y USART
 * Placa  : Nucleo-F411RE | Reloj HSI interno 16 MHz
 *
 * -----------------------------------------------------------------------
 * MAPA DE PINES
 * -----------------------------------------------------------------------
 *  PA0  TIM2_CH1  Encoder fase A     AF1 + PullUp
 *  PA1  TIM2_CH2  Encoder fase B     AF1 + PullUp
 *  PA2  USART2_TX Serial al PC       AF7
 *  PA3  USART2_RX Serial del PC      AF7 + PullUp (obligatorio segun rubrica)
 *  PA4  ADC1_IN4  Potenciometro      Analogico
 *  PA5  LED Blinky Nucleo            Salida PP
 *  PA6  TIM3_CH1  PWM Rojo           AF2
 *  PA7  TIM3_CH2  PWM Verde          AF2
 *  PA8  MCO1 16MHz salida de reloj   AF0  <-- bono osciloscopio, esos puntos extra valen por esa tarea 1 :/
 *  PB0  TIM3_CH3  PWM Azul           AF2
 *  PH1  LED2 board auxiliar          Salida PP
 *
 * -----------------------------------------------------------------------
 * RESUMEN DEL SISTEMA
 * -----------------------------------------------------------------------
 *  ROJO  <- serial USART2  ( +  sube 1%,  -  baja 1%,  0  apaga,  1  al 100% )
 *  VERDE <- encoder TIM2   ( CW sube, CCW baja, sin interupciones )
 *  AZUL  <- potenciomerto  ( ADC1 disparado por TIM4 cada 20 ms )
 *
 *  FSM de 4 estados: LEER_ADC -> LEER_ENCODER -> VERIFICAR_SERIAL -> ESCRIBIR_PWM
 *  TIM10 maneja el blinky de forma independiente por interrupcion cada 500 ms
 *
 * -----------------------------------------------------------------------
 * TIMERS USADOS
 * -----------------------------------------------------------------------
 *  TIM2  encoder rotativo (32 bits, modo TI12, sin interrupcion)
 *  TIM3  PWM RGB 2 kHz, tres canales (CH1=rojo, CH2=verde, CH3=azul)
 *  TIM4  trigger del ADC cada 20 ms via evento CC4 (sin interrupcion propia)
 *  TIM10 blinky, desborda cada 500 ms y llama al callback por interrupcion
 */

// -----------------------------------------------------------------------
// 1. DIRECTIVAS DE PROCESAMIENTO
// -----------------------------------------------------------------------
#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"
#include <stdio.h>
#include <string.h>

// <<< MODIFICAR: sensibilidad del encoder >>>
// cuantos conteos de TIM2 equivalen al 100% de brillo verde
// en modo TI12: 1 detent fisico = 4 conteos del TIM2
// 120 conteos ~ 30 detents ~ 1.5 vueltas para llegar al maximo
// para mas vueltas: subir (ej: 400 = ~5 vueltas como el profe)
// para menos vueltas: bajar (ej: 80 = ~1 vuelta)
#define ENCODER_MAX  120U

// -----------------------------------------------------------------------
// 2. VARIABLES
// -----------------------------------------------------------------------
// handles del HAL (uno por periferico)
TIM_HandleTypeDef  htim2;   // encoder rotativo
TIM_HandleTypeDef  htim3;   // PWM de los 3 colores
TIM_HandleTypeDef  htim4;   // trigger del ADC cada 20 ms
TIM_HandleTypeDef  htim10;  // blinky 500 ms
ADC_HandleTypeDef  hadc1;   // potenciomerto en PA4
UART_HandleTypeDef huart2;  // serial 115200 bps

// lecturas crudas
uint32_t valor_adc = 0;         // resultado ADC [0, 4095]
uint8_t  rx_data[1];            // buffer de un byte para el serial

// duty cycles de cada color [0 = apagado, 999 = brillo maximo]
volatile uint32_t pwm_rojo  = 0;
volatile uint32_t pwm_verde = 0;
volatile uint32_t pwm_azul  = 0;

// flag que pone la ISR del USART cuando llega un byte
volatile uint8_t serial_nuevo = 0;

// conatdor de clics del usuario via serial (+ sube, - baja, 0 resetea)
volatile int32_t contador_clics = 0;

typedef enum {
    ESTADO_LEER_ADC = 0,
    ESTADO_LEER_ENCODER,
    ESTADO_VERIFICAR_SERIAL,
    ESTADO_ESCRIBIR_PWM
} FSM_Estado_t;

FSM_Estado_t estado_fsm = ESTADO_LEER_ADC;

// -----------------------------------------------------------------------
// 3. HEADERS PRIVADOS (prototipos de funciones internas)
// -----------------------------------------------------------------------
void Inicializar_Hardware(void);
void GPIO_Init_Manual(void);
void TIM2_Encoder_Init(void);
void TIM3_PWM_Init(void);
void TIM4_Trigger_Init(void);
void TIM10_Blinky_Init(void);
void ADC1_Init_Manual(void);
void USART2_Init_Manual(void);
void MCO1_Config_Bono(void);

// -----------------------------------------------------------------------
// 4. LOGICA (main + implementacion de funciones)
// -----------------------------------------------------------------------
int main(void)
{
    HAL_Init();             // SysTick a 1 ms, flash latency, grupo NVIC
    Inicializar_Hardware(); // GPIO primero, luego timers, luego serial

    // arranque de perifericos en orden importatne:
    // los PWM deben estar activos antes de que el ADC empiece a disparar
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);       // rojo  PA6
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);       // verde PA7
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);       // azul  PB0

    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL); // fases A y B del encoder

    // TIM4 antes que el ADC para que el primer trigger no se pierda
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    HAL_ADC_Start(&hadc1);                          // ADC queda esperando el CC4 del TIM4

    HAL_TIM_Base_Start_IT(&htim10);                 // blinky por interrupcion cada 500 ms
    HAL_UART_Receive_IT(&huart2, rx_data, 1);       // recepcion del serial por interrupcion

    // FSM principal: corre para siempre
    while (1)
    {
        switch (estado_fsm)
        {
            // ===========================================================
            case ESTADO_LEER_ADC:
            // ===========================================================
                // el TIM4 CC4 dispara el ADC cada 20 ms
                // revisamos el flag EOC para no leer un dato viejo
                if (__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC))
                {
                    valor_adc = HAL_ADC_GetValue(&hadc1); // leer limpia el flag EOC

                    // escala 12 bits [0, 4095] -> PWM [0, 999]
                    // zona muerta: si el ADC lee menos de 50 lo tratamos como cero
                    // el potenciomerto fisico nunca baja exactamente a 0 V
                    if (valor_adc < 50U) {
                        pwm_azul = 0U;
                    } else {
                        pwm_azul = (valor_adc * 999UL) / 4095UL;
                    }
                }
                estado_fsm = ESTADO_LEER_ENCODER;
                break;

            // ===========================================================
            case ESTADO_LEER_ENCODER:
            // ===========================================================
                {
                    // TIM2 de 32 bits con ARR = 0xFFFFFFFF
                    // el cast a int32_t detecta giro CCW: cuando el conatdor baja
                    // de 0 queda en 0xFFFFFFFF que como int32_t es -1, lo clampeamos
                    // ENCODER_MAX esta definido arriba en la seccion de directivas

                    int32_t cnt = (int32_t)htim2.Instance->CNT;

                    if (cnt < 0) {
                        cnt = 0;
                        htim2.Instance->CNT = 0;
                    } else if ((uint32_t)cnt > ENCODER_MAX) {
                        cnt = (int32_t)ENCODER_MAX;
                        htim2.Instance->CNT = ENCODER_MAX;
                    }

                    // escala lineal [0, ENCODER_MAX] -> [0, 999]
                    pwm_verde = ((uint32_t)cnt * 999UL) / ENCODER_MAX;
                }
                estado_fsm = ESTADO_VERIFICAR_SERIAL;
                break;

            // ===========================================================
            case ESTADO_VERIFICAR_SERIAL:
            // ===========================================================
                if (serial_nuevo)
                {
                    serial_nuevo = 0;
                    char letra = (char)rx_data[0];

                    // <<< MODIFICAR: letras del serial y sus acciones >>>
                    // para cambiar una letra, modificar el caracter entre comillas
                    // para cambiar el paso, modificar el numero que se suma/resta
                    // actualmente: paso = 10 = 1% del rango [0, 999]
                    // el profe uso este mismo paso en el video de la demostracion

                    if (letra == '+') {
                        // <<< MODIFICAR: paso de subida (actualmente 10 = 1%) >>>
                        pwm_rojo = (pwm_rojo + 10U <= 999U) ? pwm_rojo + 10U : 999U;
                        contador_clics++;
                    }
                    else if (letra == '-') {
                        // <<< MODIFICAR: paso de bajada (actualmente 10 = 1%) >>>
                        pwm_rojo = (pwm_rojo >= 10U) ? pwm_rojo - 10U : 0U;
                        contador_clics--;
                    }
                    else if (letra == '0') {
                        // <<< MODIFICAR: accion libre 1 (actualmente apaga el LED) >>>
                        pwm_rojo = 0U;
                        contador_clics = 0;
                    }
                    else if (letra == '1') {
                        // <<< MODIFICAR: accion libre 2 (actualmente 100% de brillo) >>>
                        pwm_rojo = 999U;
                    }

                    // respuesta serial: el profe puede ver el estado en tiempo real
                    char buffer_tx[100];
                    sprintf(buffer_tx,
                            "Clics: %ld | Rojo: %lu | Verde: %lu | Azul: %lu\r\n",
                            contador_clics, pwm_rojo, pwm_verde, pwm_azul);
                    HAL_UART_Transmit(&huart2, (uint8_t*)buffer_tx, strlen(buffer_tx), HAL_MAX_DELAY);
                }
                estado_fsm = ESTADO_ESCRIBIR_PWM;
                break;

            // ===========================================================
            case ESTADO_ESCRIBIR_PWM:
            // ===========================================================
                // escribimos los tres CCR del TIM3 con los valores calculados
                // el hardware actualiza el duty cycle al inicio del proximo periodo
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_rojo);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm_verde);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm_azul);
                estado_fsm = ESTADO_LEER_ADC;
                break;

            default:
                estado_fsm = ESTADO_LEER_ADC;
                break;
        }
    }
}

// -----------------------------------------------------------------------
// CONFIGURACION DE GPIO
// todos los pines en una sola funcion para tener el mapa en un solo lugar
// -----------------------------------------------------------------------
void GPIO_Init_Manual(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    // PH1: LED2 de la board auxiliar
    // normalmente es OSC_OUT pero como usamos HSI el cristal no esta conectado
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    // PA5: LED verde de la placa Nucleo (blinky)
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA6 y PA7: TIM3_CH1 (rojo) y TIM3_CH2 (verde), funcion alterna AF2
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PB0: TIM3_CH3 (azul), mismo AF2 pero en puerto B
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // PA0 y PA1: encoder fases A y B, AF1
    // el pullup es necesario para evitar que los pines floten entre pulsos
    // sin el, el TIM2 cuenta flancos falsos y el conatdor se dispara solo
    GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA4: entrada analogica del potenciometro
    // modo ANALOG desconecta el buffer digital para que el ADC lea bien
    GPIO_InitStruct.Pin  = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA2: USART2_TX hacia el PC, velocidad alta para 115200 bps
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA3: USART2_RX desde el PC, pullup obligatorio segun la rubrica del pdf
    // sin pullup el pin flota y el USART genera bytes fantasma
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA8: MCO1, saca el reloj HSI de 16 MHz en el pin
    // el profe lo chequea con el osci para verificar la freucencia del HSI
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// -----------------------------------------------------------------------
// TIM2: encoder rotativo en modo cuadratura TI12
// el hardware cuenta los flancos de ambas fases sin usar interupciones
// -----------------------------------------------------------------------
void TIM2_Encoder_Init(void)
{
    TIM_Encoder_InitTypeDef sConfig = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();

    // ARR = 0xFFFFFFFF: periodo maximo del TIM2 de 32 bits
    // cuando el usuario gira CCW y el contador baja de 0 queda en 0xFFFFFFFF
    // al castearlo a int32_t ese valor es -1, lo detectamos y clampeamos a 0
    htim2.Instance           = TIM2;
    htim2.Init.Prescaler     = 0;
    htim2.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim2.Init.Period        = 0xFFFFFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    // TI12: cuenta en los flancos de ambas fases A y B del encoder
    // esto da 4 conteos por cada detent fisico (mayor resolucion)
    sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;

    // filtro maximo (IC1Filter=15) para atrapar rebotes del encoder mecanico
    sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter    = 15;

    sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter    = 15;

    HAL_TIM_Encoder_Init(&htim2, &sConfig);
}

// -----------------------------------------------------------------------
// TIM3: PWM de 3 canales para el LED RGB
//
// <<< MODIFICAR: frecuencia del PWM >>>
// calculo para 2 kHz (dentro del rango 1-5 kHz de la rubrica):
//   F = 16 MHz / (PSC+1) / (ARR+1)
//   F = 16 MHz / 8 / 1000 = 2000 Hz
// para cambiar la freucencia, ajustar PSC y ARR con esta formula
// trampa comun: PSC=15 da 1 kHz NO 2 kHz, el profe lo chequea con el osci
// -----------------------------------------------------------------------
void TIM3_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 8 - 1;    // 16 MHz / 8 = 2 MHz de conteo
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 1000 - 1; // 2 MHz / 1000 = 2 kHz exactos
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim3);

    // PWM1: salida ALTA mientras CNT < CCR, BAJA cuando CNT >= CCR
    // duty = CCR / 1000 -> CCR=500 es 50% de brillo
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;                  // empieza apagado
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1); // PA6 rojo
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2); // PA7 verde
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3); // PB0 azul
}

// -----------------------------------------------------------------------
// TIM4: genera el trigger del ADC cada 20 ms via evento CC4
// sin interrupcion propia del TIM4, el ADC se dispara solo con CC4
//
// calculo: 16 MHz / 16000 / 20 = 50 Hz -> T = 20 ms (minimo rubrica)
// -----------------------------------------------------------------------
void TIM4_Trigger_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    __HAL_RCC_TIM4_CLK_ENABLE();

    // nota: el HAL V1.28.0 no define ADC_EXTERNALTRIGCONV_T4_TRGO en el F411
    // en el F411 solo el CC4 del TIM4 esta conectado al ADC como trigger externo
    // por eso usamos el canal 4 en modo PWM1 para generar el evento CC4
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 16000 - 1; // 16 MHz / 16000 = 1 kHz de tick
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 20 - 1;    // 1 kHz / 20 = 50 Hz -> T = 20 ms
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim4);

    // PWM1 con Pulse=9: flanco de subida al inicio de cada periodo (cada 20 ms)
    // ese flanco dispara el ADC via ADC_EXTERNALTRIGCONV_T4_CC4
    // TOGGLE daba un flanco cada 40 ms (alternaba subida/bajada), por eso PWM1
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 9U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4);
}

// -----------------------------------------------------------------------
// TIM10: blinky independiente, desborda cada 500 ms
//
// calculo: 16 MHz / 16000 / 500 = 2 Hz -> toggle cada 500 ms
// -----------------------------------------------------------------------
void TIM10_Blinky_Init(void)
{
    __HAL_RCC_TIM10_CLK_ENABLE();

    htim10.Instance               = TIM10;
    htim10.Init.Prescaler         = 16000 - 1; // 1 kHz de tick (1 ms por tick)
    htim10.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim10.Init.Period            = 500 - 1;   // 500 ticks = 500 ms
    htim10.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim10);

    // prioridad 1: mas urgente que el serial (prioridad 2)
    // si el serial se traba, el blinky sigue funcionando y el profe sabe que el sistema vive
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

// -----------------------------------------------------------------------
// ADC1: potenciometro en PA4, 12 bits, disparado por TIM4 CC4 cada 20 ms
// -----------------------------------------------------------------------
void ADC1_Init_Manual(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4; // reloj ADC = 4 MHz
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;        // [0, 4095]
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;  // una sola conversion por trigger
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T4_CC4; // trigger TIM4 cada 20 ms
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    sConfig.Channel      = ADC_CHANNEL_4;           // PA4 potenciometro
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; // rapido pero estable
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

// -----------------------------------------------------------------------
// USART2: serial bidireccional 115200 bps
// TX por polling (bloqueante), RX por interrupcion (obligatorio rubrica)
// -----------------------------------------------------------------------
void USART2_Init_Manual(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);

    // prioridad 2: menor que el blinky para que el LED nunca se detenga
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

// -----------------------------------------------------------------------
// MCO1: saca el reloj HSI de 16 MHz por PA8 para el osciloscopio
// el bono del osciloscopio, esos puntos extra valen por esa tarea 1
// PA8 ya quedo configurado como AF0 en GPIO_Init_Manual
// -----------------------------------------------------------------------
void MCO1_Config_Bono(void)
{
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1);
}

// -----------------------------------------------------------------------
// INICIALIZACION GENERAL
// GPIO primero siempre: los pines deben estar listos antes que los timers
// -----------------------------------------------------------------------
void Inicializar_Hardware(void)
{
    GPIO_Init_Manual();
    MCO1_Config_Bono();    // el bono del osciloscopio, esos puntos extra valen por esa tarea 1
    TIM2_Encoder_Init();
    TIM3_PWM_Init();
    TIM4_Trigger_Init();
    TIM10_Blinky_Init();
    ADC1_Init_Manual();
    USART2_Init_Manual();
}

// -----------------------------------------------------------------------
// CALLBACKS DE INTERRUPCIONES
// -----------------------------------------------------------------------

// llamado por el HAL cuando el TIM10 desborda (cada 500 ms)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); // LED de la Nucleo
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1); // LED2 de la board auxiliar
    }
}

// llamado por el HAL cuando llega un byte por el USART2
// solo ponemos el flag; el procesamiento lo hace la FSM en el main
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        serial_nuevo = 1;
        // re-armamos la recepcion para no perder el proximo byte
        HAL_UART_Receive_IT(&huart2, rx_data, 1);
    }
}
