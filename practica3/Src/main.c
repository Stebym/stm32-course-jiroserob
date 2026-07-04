/**
 * ******************************************************************************
 * @file    : main.c
 * @author  : Jimmy Stebym Rosero Barrera
 * @brief   : Tarea 3 - Control de LED RGB con FSM, ADC, Encoder y UART asincrono
 * Nucleo-F411RE | Reloj HSI interno 16 MHz
 *
 * Pin   Funcion              Modo
 * PA0   TIM2_CH1  Encoder A  AF1 + PullUp
 * PA1   TIM2_CH2  Encoder B  AF1 + PullUp
 * PA2   UART2_TX  Serial PC  AF7
 * PA3   UART2_RX  Serial PC  AF7 + PullUp obligatorio segun rubrica
 * PA4   ADC1_IN4  Pot        Analogico
 * PA5   LED Blinky Nucleo    Salida PP
 * PA6   TIM3_CH1  PWM Rojo   AF2
 * PA7   TIM3_CH2  PWM Verde  AF2
 * PA8   MCO1 16MHz           AF0 bono osciloscopio puntos extra
 * PB0   TIM3_CH3  PWM Azul   AF2
 * PH1   LED2 board auxiliar  Salida PP
 * ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"
#include <stdio.h>
#include <string.h>

// sensibilidad del encoder conteos de TIM2 para maximo brillo
// 396 = 99 clics mecanicos x4 cuentas por clic (encoder en cuadratura)
// asi cada clic mueve el pwm 999/99 = 1.0% ~ 33 mV, igual a la resolucion
// que el profe mostro en el video de la tarea


#define ENCODER_MAX  396U

// tiempo minimo entre transmisiones del reporte (ms); sin esto, girar el pote
// despacio inunda la terminal con una linea nueva cada 20 ms
#define REPORTE_INTERVALO_MIN_MS  150U


// -----------------------------------------------------------------------
// VARIABLES GLOBALES
// -----------------------------------------------------------------------



// handles de la hal manuales de los perifericos


TIM_HandleTypeDef  htim2;   // encoder rotativo
TIM_HandleTypeDef  htim3;   // pwm de los 3 colores
TIM_HandleTypeDef  htim4;   // trigger del adc cada 20 ms
TIM_HandleTypeDef  htim10;  // blinky de 500 ms
ADC_HandleTypeDef  hadc1;   // potenciomtero en PA4
UART_HandleTypeDef huart2;  // handle asincrono UART solicitado por el profe sin la S

// lecturas crudas de los sensores
uint32_t valor_adc = 0;
uint8_t  rx_data[1];            // buffer de un byte para la interupcion

// duty cycles rango valido entre 0 apagado y 999 maximo brillo
volatile uint32_t pwm_rojo  = 0;
volatile uint32_t pwm_verde = 0;
volatile uint32_t pwm_azul  = 0;

// variables espejo para cheqear cambios en tiempo real y transmitir de una
uint32_t pwm_rojo_ant  = 0;
uint32_t pwm_verde_ant = 0;
uint32_t pwm_azul_ant  = 0;
int32_t  clics_encoder_ant = 0;    // para detectar cambios en los clics de la perilla

// direccion del ultimo giro REAL del potenciometro (por encima del ruido), se
// mantiene aunque el pote este quieto, igual que el bit DIR del encoder en hardware
// 0 = aun no se ha movido, 1 = CW/derecha (sube), -1 = CCW/izquierda (baja)
volatile int8_t pot_direccion = 0;

// marca de tiempo del ultimo reporte enviado, para limitar la frecuencia de
// transmision (ver REPORTE_INTERVALO_MIN_MS)
uint32_t tick_ultimo_reporte = 0;

// flag que levanta la ISR cuando llega un byte por usart
volatile uint8_t serial_nuevo = 0;

// conatdor de clicks fisicos del encoder rotativo para la terminal
volatile int32_t clics_encoder = 0;

// stados de la maquina finita fsm
typedef enum {
    ESTADO_LEER_ADC = 0,
    ESTADO_LEER_ENCODER,
    ESTADO_VERIFICAR_SERIAL,
    ESTADO_ESCRIBIR_PWM
} FSM_Estado_t;

FSM_Estado_t estado_fsm = ESTADO_LEER_ADC;


// -----------------------------------------------------------------------
// PROTOTIPOS
// -----------------------------------------------------------------------



void Inicializar_Hardware(void);
void GPIO_Init_Manual(void);
void TIM2_Encoder_Init(void);
void TIM3_PWM_Init(void);
void TIM4_Trigger_Init(void);
void TIM10_Blinky_Init(void);
void ADC1_Init_Manual(void);
void UART2_Init_Manual(void);
void MCO1_Config_Bono(void);
void Enviar_Menu_Bienvenida(void);

// -----------------------------------------------------------------------
// LOGICA PRINCIPAL
// -----------------------------------------------------------------------


int main(void)
{
    // iniciamos el core tick a 1 ms latency flash nvic
    HAL_Init();

    // configuramos el hardware a mano sin ayudas graficas
    Inicializar_Hardware();

    // prender los modulos pwm antes del disparo automatico del adc
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);       // rojo PA6
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);       // verde PA7
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);       // azul PB0

    // fases del conatdor del encoder
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);

    // tim4 pwm canal 4 genera flancos para el adc cada 20 ms
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    HAL_ADC_Start_IT(&hadc1); // armamos con interrupcion para disparar el callback

    // arrancar interupciones del blinky y el receptor serial
    HAL_TIM_Base_Start_IT(&htim10);
    HAL_UART_Receive_IT(&huart2, rx_data, 1);

    // el banner sale una vez al arrancar, pero como la terminal casi nunca esta
    // conectada tan rapido, cualquier tecla no reconocida en el switch de abajo
    // lo vuelve a mandar (ver ESTADO_VERIFICAR_SERIAL) presionando ENTER alcanza
    Enviar_Menu_Bienvenida();
    uint32_t tick_menu_previo = HAL_GetTick(); // arranca el conteo de los 10 s justo aqui

    // la fsm corre para siempre aca adentro
    while (1)
    {
        switch (estado_fsm)
        {
            case ESTADO_LEER_ADC:
                // la conversion del pote la maneja HAL_ADC_ConvCpltCallback por interrupcion
                // la fsm solo avanza al siguiente estado
                estado_fsm = ESTADO_LEER_ENCODER;
                break;

            case ESTADO_LEER_ENCODER:
                {
                    // tim2 es de 32 bits cast a int32_t ayuda a detectar retroceso
                    int32_t cnt = (int32_t)htim2.Instance->CNT;

                    if (cnt < 0) {
                        cnt = 0;
                        htim2.Instance->CNT = 0;
                    } else if ((uint32_t)cnt > ENCODER_MAX) {
                        cnt = (int32_t)ENCODER_MAX;
                        htim2.Instance->CNT = ENCODER_MAX;
                    }

                    // mapeo del conatdor al pwm verde
                    pwm_verde = ((uint32_t)cnt * 999UL) / ENCODER_MAX;

                    // calculamos los clicks fisicos reales dividiendo por 4 los pasos
                    clics_encoder = cnt / 4;
                }
                estado_fsm = ESTADO_VERIFICAR_SERIAL;
                break;

            case ESTADO_VERIFICAR_SERIAL:
                // la isr levanta el flag cuando cae una letra en el receptor
                if (serial_nuevo)
                {
                    serial_nuevo = 0;
                    char letra = (char)rx_data[0];

                    if (letra == '+') {
                        pwm_rojo = (pwm_rojo + 10U <= 999U) ? pwm_rojo + 10U : 999U;
                    }
                    else if (letra == '-') {
                        pwm_rojo = (pwm_rojo >= 10U) ? pwm_rojo - 10U : 0U;
                    }
                    else if (letra == '0') {
                        pwm_rojo = 0U;
                    }
                    else if (letra == '1') {
                        // salta directo al 100%; el conteo de clics (ver mas abajo,
                        // se deriva siempre de pwm_rojo) va a mostrar el nivel 100
                        // de una, para poder bajar con '-' de forma consistente
                        pwm_rojo = 999U;
                    }
                    else if ((letra == 'm') || (letra == 'M')) {
                        // quinta tecla de cortesia: manda el rojo a la mitad exacta
                        pwm_rojo = 500U;
                    }
                    else {
                        // tecla no reconocida (ENTER, '?', etc): reimprime el
                        // menu de bienvenida, asi el usuario no depende de que
                        // el banner haya llegado justo antes de abrir la terminal
                        Enviar_Menu_Bienvenida();
                    }
                }
                estado_fsm = ESTADO_ESCRIBIR_PWM;
                break;

            case ESTADO_ESCRIBIR_PWM:
                // inyeccion de datos a las salidas fisicas del led rgb
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_rojo);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm_verde);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm_azul);

                // diferencia con signo para saber hacia donde se movio el pote, y su
                // valor absoluto para el umbral de ruido de siempre
                int32_t diff_azul_signo = (int32_t)pwm_azul - (int32_t)pwm_azul_ant;
                int32_t diff_azul = (diff_azul_signo < 0) ? -diff_azul_signo : diff_azul_signo;

                // la direccion del pote solo se actualiza si el movimiento fue real
                // (mismo umbral de 4 cuentas que ya se usaba para filtrar ruido), y se
                // queda guardada aunque el pote no se mueva en los ciclos siguientes
                if (diff_azul > 4) {
                    pot_direccion = (diff_azul_signo > 0) ? 1 : -1;
                }

                // TRANSMISION AUTOMATICA REACTIVA
                // se dispara si cambia el pwm rojo, el encoder, o el azul (pote),
                // pero nunca mas seguido que REPORTE_INTERVALO_MIN_MS: sin este
                // limite, girar el pote despacio manda un bloque nuevo cada 20 ms
                // (un scroll ilegible en la terminal)
                uint8_t hay_cambio = (clics_encoder != clics_encoder_ant) ||
                                     (pwm_rojo != pwm_rojo_ant) || (diff_azul > 4);
                uint32_t ahora = HAL_GetTick();

                if (hay_cambio && ((ahora - tick_ultimo_reporte) >= REPORTE_INTERVALO_MIN_MS))
                {
                    // direccion real leida del registro del timer, no hay que inferirla comparando cuentas
                    const char *dir_encoder = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2) ? "CCW" : "CW";

                    // direccion del pote inferida en software (ver arriba); "--" mientras no se haya
                    // movido todavia desde el arranque
                    const char *dir_pot = (pot_direccion > 0) ? "CW" :
                                          (pot_direccion < 0) ? "CCW" : "--";

                    // conversion de la lectura cruda del pote a milivoltios reales (escala 3.3 V)
                    uint32_t adc_mv = (valor_adc * 3300UL) / 4095UL;

                    // clics rojo SIEMPRE calculados desde el pwm real (redondeado a la
                    // unidad de 10 mas cercana), nunca se desincroniza: si el usuario dio
                    // '1' esto ya muestra "100 clics" (el equivalente a haber llegado a 999
                    // a punta de '+'), listo para bajar con '-' de forma consistente
                    uint32_t clics_rojo = (pwm_rojo + 5U) / 10U;

                    // un renglon por canal, con encabezado; hace scroll normal (sin
                    // ANSI), pero el limite de REPORTE_INTERVALO_MIN_MS de arriba ya
                    // evita que se dispare un bloque nuevo en cada muestra del ADC
                    char buffer_tx[260];
                    snprintf(buffer_tx, sizeof(buffer_tx),
                            "=== ESTADO EN VIVO ===\r\n"
                            "  Rojo  [UART]    : %3lu clics       -> PWM = %3lu (%3lu%%)\r\n"
                            "  Verde [Encoder] : %-3s  %2ld clics  -> PWM = %3lu (%3lu%%)\r\n"
                            "  Azul  [ADC]     : %-3s %4lu raw = %4lu mV -> PWM = %3lu (%3lu%%)\r\n\r\n",
                            clics_rojo, pwm_rojo, (pwm_rojo * 100UL) / 999UL,
                            dir_encoder, clics_encoder, pwm_verde, (pwm_verde * 100UL) / 999UL,
                            dir_pot, valor_adc, adc_mv, pwm_azul, (pwm_azul * 100UL) / 999UL);

                    // transmite de inmediato por polling asincrono
                    HAL_UART_Transmit(&huart2, (uint8_t*)buffer_tx, strlen(buffer_tx), 100);

                    // guardamos el stado actual en el espejo
                    pwm_rojo_ant       = pwm_rojo;
                    pwm_verde_ant      = pwm_verde;
                    pwm_azul_ant       = pwm_azul;
                    clics_encoder_ant  = clics_encoder;
                    tick_ultimo_reporte = ahora;
                }

                estado_fsm = ESTADO_LEER_ADC;
                break;

            default:
                estado_fsm = ESTADO_LEER_ADC;
                break;
        }

        // RECORDATORIO PERIODICO: reimprime el menu cada 30 s por si el usuario
        // se le olvidaron las teclas; HAL_GetTick() ya corre solo por el SysTick
        if ((HAL_GetTick() - tick_menu_previo) >= 30000U)
        {
            Enviar_Menu_Bienvenida();
            tick_menu_previo = HAL_GetTick();
        }
    }
}

// -----------------------------------------------------------------------
// RUTEACIÓN DE PINES MANUAL
// -----------------------------------------------------------------------


void GPIO_Init_Manual(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    // PH1 pin de salida digital para el led de la board auxiliar
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    // PA5 led indicador de blinky del micro nucleo
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA6 y PA7 salidas pwm del timer 3 canales 1 y 2 en modo af2
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PB0 canal 3 del timer 3 modo af2
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // PA0 y PA1 canales del encoder tim2 modo af1 con pullup critico
    GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA4 pata del potenciomtero en modo analogico
    GPIO_InitStruct.Pin  = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA2 pin de transmision af7 usart2
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA3: USART2_RX desde el PC, pullup obligatorio segun la rubrica del pdf
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA8 pin especial mco1 funcion af0 para sacar los 16 mhz al osciloscopo
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// -----------------------------------------------------------------------
// INICIALISACION DE TIMER ENCODER TIM2
// -----------------------------------------------------------------------


void TIM2_Encoder_Init(void)
{
    TIM_Encoder_InitTypeDef sConfig = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance           = TIM2;
    htim2.Init.Prescaler     = 0;
    htim2.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim2.Init.Period        = 0xFFFFFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    // cuadratura completa en ambos flancos TI12
    sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;

    // filtros para qe los ruidos mecanicos de la perilla no sumen de mas
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
// INICIALISACION DE TIMER PWM RGB TIM3 (2 kHz)
// -----------------------------------------------------------------------



void TIM3_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 8 - 1;    // divide reloj a 2 mhz
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 1000 - 1; // f = 2 mhz dividido 1000 igual a 2 khz exactos
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim3);

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);
}

// -----------------------------------------------------------------------
// INICIALISACION DE TIMER DISPARADOR TIM4 (20 ms)
// -----------------------------------------------------------------------



void TIM4_Trigger_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    __HAL_RCC_TIM4_CLK_ENABLE();

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 16000 - 1; // tick de 1 ms
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 20 - 1;    // periodo de 20 ms es decir 50 hz
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim4);

    // modo pwm1 genera un flanco de subida limpio cada 20 ms para despertar el modulo analogo
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 9U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4);
}

// -----------------------------------------------------------------------
// INICIALISACION DE TIMER BLINKY TIM10 (500 ms)
// -----------------------------------------------------------------------



void TIM10_Blinky_Init(void)
{
    __HAL_RCC_TIM10_CLK_ENABLE();

    htim10.Instance               = TIM10;
    htim10.Init.Prescaler         = 16000 - 1;
    htim10.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim10.Init.Period            = 500 - 1;   // desborda cada medio segundo
    htim10.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim10);

    // prioridad alta para qe no se trabe por culpa del serial
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

// -----------------------------------------------------------------------
// INICIALISACION DEL MODULO ANALOGO ADC1
// -----------------------------------------------------------------------


void ADC1_Init_Manual(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;  // conversion asincrona solo cuando el tim4 avise
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T4_CC4; // amarrado al canal 4 del tim4
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    // habilitamos la interrupcion del adc para que dispare el callback
    HAL_NVIC_SetPriority(ADC_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    sConfig.Channel      = ADC_CHANNEL_4;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

// -----------------------------------------------------------------------
// INICIALISACION DEL PUERTO SERIAL ASINCRONO UART2
// -----------------------------------------------------------------------
void UART2_Init_Manual(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2; // hardware fisico
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2); // configurado como uart asincrono puro sin reloj para qe no se pegue

    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

// -----------------------------------------------------------------------
// EXPORTACION DE RELOJ MCO1 (BONO EXTRA)
// -----------------------------------------------------------------------
void MCO1_Config_Bono(void)
{
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1);
}

// -----------------------------------------------------------------------
// MENU DE BIENVENIDA POR USART2
// -----------------------------------------------------------------------
void Enviar_Menu_Bienvenida(void)
{
    static const char msg_bienvenida[] =
            "\r\n"
            "==========================================================\r\n"
            "         SISTEMA DE CONTROL INTERACTIVO TAREA 3           \r\n"
            "            Estudiante: Jimmy Stebym Rosero Barrera       \r\n"
            "==========================================================\r\n"
            " Control LED RGB (PWM 2kHz, 0-999 ticks)\r\n"
            " [COMANDOS DEL TECLADO]\r\n"
            "   ROJO  [serial]   : '+'=+1  '-'=-1  '0'=off  '1'=100%  'm'=50%\r\n"
            "   VERDE [encoder]  : CW(der)=+1  CCW(izq)=-1               \r\n"
            "   AZUL  [ADC]      : pote -> duty automatico, indica CW/CCW\r\n"
            "   (cualquier otra tecla reimprime este menu)              \r\n"
            " Este menu se reenvia solo cada 30 s como recordatorio\r\n"
            "==========================================================\r\n\r\n";

    HAL_UART_Transmit(&huart2, (uint8_t*)msg_bienvenida, strlen(msg_bienvenida), HAL_MAX_DELAY);
}

// -----------------------------------------------------------------------
// LLAMADO DE HARDWARE CENTRAL
// -----------------------------------------------------------------------
void Inicializar_Hardware(void)
{
    GPIO_Init_Manual();
    MCO1_Config_Bono();
    TIM2_Encoder_Init();
    TIM3_PWM_Init();
    TIM4_Trigger_Init();
    TIM10_Blinky_Init();
    ADC1_Init_Manual();
    UART2_Init_Manual(); // inicialisador uart asincrono enlazado
}

// -----------------------------------------------------------------------
// CALLBACKS DE INTERRUPCIONES DE LA HAL
// -----------------------------------------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1); // prende led espejo de la board auxiliar
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        serial_nuevo = 1;
        HAL_UART_Receive_IT(&huart2, rx_data, 1);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        valor_adc = HAL_ADC_GetValue(&hadc1);

        // escala de 12 bits 4095 a rango pwm de 1000 pasos
        // zona muerta de 100 para eliminar ruido de piso
        if (valor_adc < 100U) {
            pwm_azul = 0U;
        } else {
            pwm_azul = (valor_adc * 999UL) / 4095UL;
        }

        // re-armamos para la siguiente disparo del tim4
        HAL_ADC_Start_IT(&hadc1);
    }
}
