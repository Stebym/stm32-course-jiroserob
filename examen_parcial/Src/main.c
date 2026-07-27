/**
 ******************************************************************************
 * @file    : main.c
 * @author  : Jimmy Stebym Rosero Barrera
 * @brief   : Examen Parcial (25%) - Taller V (2.0), 1er Semestre 2026
 * Profesor: Nerio Andres Montoya Giraldo, PhD.
 * Nucleo-F411RE (STM32F411RETx). Sensor sorteado: Joystick analogico.
 * Display sorteado: OLED GME12864-41 (I2C, controlador SSD1315).
 *
 * ------------------------------------------------------------------------
 * QUE HACE ESTE PROGRAMA
 * ------------------------------------------------------------------------
 * Resuelve las 6 preguntas tecnicas del examen (la 7ma es la sustentacion
 * oral) usando una FSM de 5 estados (mas un easter egg opcional, ver abajo),
 * todo HAL puro sin CubeMX:
 *   1. LED de estado parpadeando a 250 ms exactos (TIM10, interrupcion).
 *   2. OLED GME12864-41 por I2C1, polling (el enunciado exige que I2C NO
 *      use interrupciones), mostrando hora RTC, joystick X/Y y estado FSM.
 *   3. USART2 115200 8N1, Rx por interrupcion, con 6 comandos verificables:
 *      'H'/'L'/'P' (fuente de MCO1), 'T' (ajustar hora RTC), 'F' (ajustar
 *      fecha RTC), 'R' (reporte).
 *   4. Joystick analogico (ADC1, 2 canales en modo Scan disparado cada
 *      20 ms por TIM3_TRGO, lectura por interrupcion, sin DMA).
 *   5. RTC interno con LSE (cristal 32.768 kHz de la board) + VBAT, para
 *      que la hora sobreviva al desconectar la alimentacion principal.
 *   6. SYSCLK a 100 MHz (PLL desde HSI) verificable en MCO1 (PA8): HSI,
 *      LSE o PLL segun el comando serial recibido.
 *
 * ------------------------------------------------------------------------
 * MAPA DE PINES
 * ------------------------------------------------------------------------
 * Pin   Funcion              Modo              Notas
 * PA0   SW Joystick (click)  EXTI falling      pull-up interno, sale de ESTADO_BIENVENIDA
 * PA1   ADC1_IN1  Joystick Y Analogico         VRy
 * PA2   USART2_TX            AF7               hacia PC (ST-Link VCP)
 * PA3   USART2_RX            AF7 + PullUp      evita flancos falsos en reposo
 * PA4   ADC1_IN4  Joystick X Analogico         VRx -- separado de PA1 (no adyacentes)
 * PA5   LED Blinky Nucleo    Salida PP         toggle cada 250 ms (TIM10)
 * PA8   MCO1                 AF0               HSI / LSE / PLL segun comando
 * PB8   I2C1_SCL  OLED       AF4 open-drain    GME12864-41
 * PB9   I2C1_SDA  OLED       AF4 open-drain    GME12864-41
 * PH1   LED espejo del blinky Salida PP        toggle junto con PA5 (micro tapado en la demo)
 * PC13  Boton azul B1 board  Entrada           no usado por ahora (libre)
 *
 * ------------------------------------------------------------------------
 * RELOJES
 * ------------------------------------------------------------------------
 * HSI 16 MHz -> PLL (M=8, N=200, P=4) -> SYSCLK = 100 MHz
 *   VCO_in  = 16 MHz / 8   = 2 MHz    (rango recomendado 1-2 MHz)
 *   VCO_out = 2 MHz  x 200 = 400 MHz  (rango valido 192-432 MHz)
 *   PLLCLK  = 400 MHz / 4  = 100 MHz
 * AHB  (HCLK)  = SYSCLK / 1 = 100 MHz
 * APB1 (PCLK1) = HCLK   / 2 =  50 MHz (limite APB1 = 50 MHz)
 * APB2 (PCLK2) = HCLK   / 1 = 100 MHz (limite APB2 = 100 MHz)
 * Flash latency = 3 wait states (obligatorio para 100 MHz a 2.7-3.6 V)
 * Con APB1 y APB2 en prescaler != 1 y ==1 respectivamente, TIMx alcanza
 * 100 MHz en ambos buses (regla: si el prescaler APB no es 1, el reloj del
 * timer se dobla respecto al PCLK correspondiente).
 *
 * ------------------------------------------------------------------------
 * LOS TIMERS
 * ------------------------------------------------------------------------
 *   TIM10 - blinky de estado. PSC=9999 (tick=100us) ARR=2499 -> periodo
 *           exacto de 250 ms. Interrupcion de update, alterna PA5.
 *   TIM3  - generador de disparo (TRGO) para el ADC1 cada 20 ms (50 Hz).
 *           Configurado como timer base con MasterOutputTrigger=UPDATE.
 *           No genera ninguna interrupcion propia ni senal visible; el
 *           F411 SI soporta TIM3_TRGO como fuente externa del ADC1 (a
 *           diferencia de TIM4, que no la tiene).
 *
 * ------------------------------------------------------------------------
 * ADC1 (joystick, canales 1 y 4 en modo Scan)
 * ------------------------------------------------------------------------
 * Resolucion 12 bits, Scan Mode con 2 conversiones (rank1=CH1=Y en PA1,
 * rank2=CH4=X en PA4 -- se separaron a pines NO adyacentes porque con
 * PA0/PA1 aparecia continuidad electrica entre los dos ejes en el
 * cableado fisico), disparadas por TIM3_TRGO, EOC individual por cada
 * canal (EOC_SINGLE_CONV, sin DMA). El callback HAL_ADC_ConvCpltCallback
 * usa un contador de rank para saber cual de los dos ejes llego; solo se
 * re-arma HAL_ADC_Start_IT() despues del ULTIMO canal de la secuencia
 * (rank2), igual que se rearma el ADC de un solo canal en Tarea 3, para
 * que el proximo TIM3_TRGO dispare la secuencia completa otra vez desde
 * el rank1 (Y).
 *
 * ------------------------------------------------------------------------
 * USART2 (5 comandos individuales, caracteres sueltos)
 * ------------------------------------------------------------------------
 *   'H'/'h' -> MCO1 = HSI  (16 MHz sin dividir)
 *   'L'/'l' -> MCO1 = LSE  (32.768 kHz sin dividir)
 *   'P'/'p' -> MCO1 = PLL  (100 MHz / 4 = 25 MHz, prescaler para que sea
 *              medible con seguridad en el osciloscopio del laboratorio)
 *   'T'/'t' -> entra a ESTADO_CONFIG_RTC: pide "HH:MM:SS" por teclado y
 *              actualiza el RTC.
 *   'R'/'r' -> imprime por consola un reporte con hora RTC, joystick X/Y
 *              y la fuente de reloj activa en MCO1.
 * La recepcion es por interrupcion (1 byte, re-armada en el callback); la
 * transmision es por polling.
 *
 * ------------------------------------------------------------------------
 * RTC INTERNO + LSE + VBAT
 * ------------------------------------------------------------------------
 * Fuente de reloj RTC = LSE (cristal 32.768 kHz soldado en la Nucleo).
 * AsynchPrediv=127, SynchPrediv=255 -> (127+1)x(255+1)=32768 -> 1 Hz exacto.
 * Para que el RTC seguir funcionando sin la alimentacion principal (USB),
 * el pin VBAT debe conectarse a una pila CR2032 de 3V: la region de
 * backup del micro (dominio RTC + registros BKP) queda energizada por esa
 * pila aunque se desconecte VDD. En software, se usa el registro de backup
 * RTC_BKP_DR0 como "bandera de primer arranque": solo se fija fecha/hora
 * por defecto la primera vez (o si la pila se agoto y se perdio el
 * backup); en resets posteriores el RTC ya viene corriendo solo y no se
 * pisa la hora real.
 *
 * ------------------------------------------------------------------------
 * LA FSM (5 estados)
 * ------------------------------------------------------------------------
 *   ESTADO_INICIALIZACION -> arranca relojes (100 MHz), RTC, ADC, OLED.
 *   ESTADO_BIENVENIDA      -> pantalla de arranque ("Presione click para
 *                             iniciar"); se queda aca, sin leer/mostrar
 *                             joystick ni hora, hasta el primer click del
 *                             boton SW del joystick (PA0, EXTI).
 *   ESTADO_MONITOR         -> refresca el OLED con hora/joystick/estado,
 *                             procesa comandos H/L/P/R de una, y salta a
 *                             CONFIG_RTC si llega 'T' o a CONFIG_FECHA si
 *                             llega 'F'.
 *   ESTADO_CONFIG_RTC      -> pausa el refresco de joystick en pantalla,
 *                             muestra un prompt y arma "HH:MM:SS" caracter
 *                             a caracter hasta un ENTER, corrige el RTC y
 *                             regresa a MONITOR (tambien ajustable con el
 *                             joystick).
 *   ESTADO_CONFIG_FECHA    -> igual que CONFIG_RTC pero para "DD/MM/YY".
 *
 * ------------------------------------------------------------------------
 * BOTON SW DEL JOYSTICK (click central, PA0, EXTI por interrupcion)
 * ------------------------------------------------------------------------
 * No lo pide la rubrica, se agrega como detalle extra: su unico trabajo es
 * sacar la FSM de ESTADO_BIENVENIDA hacia ESTADO_MONITOR con el primer
 * click. Fuera de ese estado no tiene efecto.
 * ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"
#include "oled_display.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * DEFINES
 * ----------------------------------------------------------------------- */

#define RTC_BKP_MAGIC   0x32F2U   /* marca de "el rtc ya fue inicializado" */

/* -----------------------------------------------------------------------
 * HANDLES HAL (manuales, sin CubeMX)
 * ----------------------------------------------------------------------- */

TIM_HandleTypeDef  htim10;  /* blinky 250 ms                 */
TIM_HandleTypeDef  htim3;   /* disparador del adc cada 20 ms  */
ADC_HandleTypeDef  hadc1;   /* joystick x/y                   */
UART_HandleTypeDef huart2;  /* consola de comandos            */
RTC_HandleTypeDef  hrtc;    /* reloj de tiempo real con lse    */

/* -----------------------------------------------------------------------
 * VARIABLES GLOBALES
 * ----------------------------------------------------------------------- */

/* joystick: lecturas de 12 bits (0-4095) ya filtradas (ver HAL_ADC_ConvCpltCallback) */
volatile uint16_t joystick_x = 2048;
volatile uint16_t joystick_y = 2048;
static uint8_t    adc_rank_actual = 0;   /* 0 = esperando x, 1 = esperando y */

/* filtro pasa-bajos exponencial (promedio movil): suaviza picos de una sola
 * muestra causados por ruido electrico o un punto sucio del potenciometro.
 * filtro += (crudo - filtro) / N -- N mas grande = mas suave pero mas lento
 * para responder a un movimiento real del joystick */
#define ADC_FILTRO_N  4
static int32_t filtro_adc_x = 2048;
static int32_t filtro_adc_y = 2048;

/* zona muerta: si la lectura (ya filtrada) esta a +-150 cuentas del centro
 * teorico (2048), se fuerza al centro exacto. Se aplica sobre joystick_x/
 * joystick_y directamente -- afecta por igual al reporte 'R', a los
 * numeros del OLED y al cursor, para que se vea perfectamente quieto en
 * reposo y solo cambie cuando el joystick se mueve de verdad */
#define JOY_CENTRO       2048
#define JOY_ZONA_MUERTA   150

/* recepcion serial de 1 byte por interrupcion */
uint8_t          rx_data[1];
volatile uint8_t serial_nuevo = 0;

/* buffer de edicion para el comando 'T' (ajustar hora) */
#define RTC_INPUT_MAXLEN 16
static char    rtc_input_buffer[RTC_INPUT_MAXLEN + 1];
static uint8_t rtc_input_len = 0;

/* fuente activa de mco1, solo para mostrar en pantalla/reporte */
typedef enum { MCO_FUENTE_HSI = 0, MCO_FUENTE_LSE, MCO_FUENTE_PLL } MCO_Fuente_t;
static MCO_Fuente_t mco_fuente_actual = MCO_FUENTE_HSI;

/* boton sw del joystick (click central, pa0, exti por flanco de bajada) */
#define SW_DEBOUNCE_MS      200U   /* ignora rebotes/reclicks mas rapidos que esto */
volatile uint8_t boton_sw_pulsado = 0;   /* la isr la levanta, el loop la consume */
static uint32_t  tick_ultimo_click = 0;  /* para el debounce dentro de la isr     */

/* comando 'R' en modo continuo: solo transmite un reporte nuevo cuando el
 * joystick realmente cambio (chequeado cada 300 ms) -- evita inundar la
 * consola con lineas identicas mientras esta quieto (zona muerta) */
static uint8_t  reporte_continuo = 0;
static uint32_t tick_ultimo_reporte_continuo = 0;
static uint16_t ultimo_reporte_x = 0xFFFF; /* valor imposible: fuerza el primer envio */
static uint16_t ultimo_reporte_y = 0xFFFF;

/* ajuste de hora con el joystick dentro de ESTADO_CONFIG_RTC:
 * eje Y (arriba/abajo) mueve minutos, eje X (izq/der) mueve horas.
 * cada paso es "por flanco" -- hay que volver al centro antes de que
 * cuente el siguiente paso, para no incrementar sin control */
#define JOY_CENTRO_MIN   1700U
#define JOY_CENTRO_MAX   2400U
#define JOY_UMBRAL_ALTO  3200U
#define JOY_UMBRAL_BAJO   900U
static uint8_t joy_listo_paso_min  = 1;
static uint8_t joy_listo_paso_hora = 1;
/* mismos umbrales/logica de "por flanco" reutilizados en ESTADO_CONFIG_FECHA:
 * joy_listo_paso_hora -> eje X -> dia, joy_listo_paso_min -> eje Y -> mes */

/* estados de la maquina de estados finita (fsm) */
typedef enum {
    ESTADO_INICIALIZACION = 0,
    ESTADO_BIENVENIDA,
    ESTADO_MONITOR,
    ESTADO_CONFIG_RTC,
    ESTADO_CONFIG_FECHA
} FSM_Estado_t;

FSM_Estado_t estado_fsm = ESTADO_INICIALIZACION;

/* -----------------------------------------------------------------------
 * PROTOTIPOS
 * ----------------------------------------------------------------------- */

static void SystemClock_Config(void);
static void GPIO_Init_Manual(void);
static void TIM10_Blinky_Init(void);
static void TIM3_ADCTrigger_Init(void);
static void ADC1_Joystick_Init(void);
static void UART2_Init_Manual(void);
static void RTC_Init_Manual(void);
static void MCO1_SetSource(MCO_Fuente_t fuente);
static const char *MCO_FuenteTexto(MCO_Fuente_t fuente);
static void Procesar_Comando(char c);
static void Enviar_Reporte(void);
static void Enviar_Menu_Ayuda(void);
static void OLED_ActualizarMonitor(void);
static void OLED_ActualizarConfigRTC(void);
static void OLED_ActualizarConfigFecha(void);
static void OLED_MostrarBienvenida(void);
static void RTC_AplicarEntradaUsuario(void);
static void RTC_AjustarConJoystick(void);
static void RTC_AplicarFechaTecleada(void);
static void RTC_AjustarFechaConJoystick(void);
static void Inicializar_Hardware(void);

/* -----------------------------------------------------------------------
 * LOGICA PRINCIPAL
 * ----------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();

    /* la fsm arranca en INICIALIZACION: monta todo el hardware una sola vez */
    Inicializar_Hardware();
    estado_fsm = ESTADO_BIENVENIDA;
    OLED_MostrarBienvenida(); /* pantalla estatica, se dibuja una sola vez */

    uint32_t tick_ultima_pantalla = 0;
    uint32_t tick_menu_previo     = 0;

    while (1)
    {
        switch (estado_fsm)
        {
            case ESTADO_INICIALIZACION:
                /* no deberia volver a caer aca; por seguridad, reinicializa */
                Inicializar_Hardware();
                estado_fsm = ESTADO_BIENVENIDA;
                OLED_MostrarBienvenida();
                break;

            case ESTADO_BIENVENIDA:
                /* se queda aca, sin tocar el oled de nuevo, hasta el primer click */
                if (boton_sw_pulsado)
                {
                    boton_sw_pulsado = 0;
                    estado_fsm = ESTADO_MONITOR;
                    tick_ultima_pantalla = 0; /* fuerza el primer refresco del monitor de una */
                    tick_menu_previo     = HAL_GetTick(); /* arranca el conteo de los 30s aqui */
                }
                break;

            case ESTADO_MONITOR:
                if (serial_nuevo)
                {
                    serial_nuevo = 0;
                    Procesar_Comando((char)rx_data[0]);
                }

                /* recordatorio periodico del menu de ayuda, cada 30 s */
                if ((HAL_GetTick() - tick_menu_previo) >= 30000U)
                {
                    Enviar_Menu_Ayuda();
                    tick_menu_previo = HAL_GetTick();
                }

                /* modo 'R' continuo: chequea cada 300 ms, pero SOLO transmite
                 * si el joystick cambio de verdad desde el ultimo envio --
                 * evita inundar la consola con lineas identicas en reposo */
                if (reporte_continuo && ((HAL_GetTick() - tick_ultimo_reporte_continuo) >= 300U))
                {
                    if (joystick_x != ultimo_reporte_x || joystick_y != ultimo_reporte_y)
                    {
                        Enviar_Reporte();
                        ultimo_reporte_x = joystick_x;
                        ultimo_reporte_y = joystick_y;
                    }
                    tick_ultimo_reporte_continuo = HAL_GetTick();
                }

                /* refresco del oled cada ~150 ms (evita saturar el bus i2c) */
                if ((HAL_GetTick() - tick_ultima_pantalla) >= 150U)
                {
                    OLED_ActualizarMonitor();
                    tick_ultima_pantalla = HAL_GetTick();
                }
                break;

            case ESTADO_CONFIG_RTC:
                if (serial_nuevo)
                {
                    serial_nuevo = 0;
                    char c = (char)rx_data[0];

                    if (c == '\r' || c == '\n')
                    {
                        RTC_AplicarEntradaUsuario();
                        rtc_input_len = 0;
                        rtc_input_buffer[0] = '\0';
                        estado_fsm = ESTADO_MONITOR;
                    }
                    else if ((c == 0x08 || c == 0x7F) && rtc_input_len > 0)
                    {
                        /* backspace: borra el ultimo caracter tecleado */
                        rtc_input_len--;
                        rtc_input_buffer[rtc_input_len] = '\0';
                    }
                    else if (rtc_input_len < RTC_INPUT_MAXLEN)
                    {
                        rtc_input_buffer[rtc_input_len++] = c;
                        rtc_input_buffer[rtc_input_len] = '\0';
                    }

                    OLED_ActualizarConfigRTC();
                }

                /* click del joystick: confirma/sale de config_rtc (alternativa al ENTER) */
                if (boton_sw_pulsado)
                {
                    boton_sw_pulsado = 0;
                    rtc_input_len = 0;
                    rtc_input_buffer[0] = '\0';
                    estado_fsm = ESTADO_MONITOR;
                }

                /* ajuste de hora con el joystick: X=minutos, Y=horas, por flanco
                 * (el modulo fisico queda rotado en la protoboard: el eje que
                 * se siente como "izquierda/derecha" es electricamente
                 * joystick_y, y el que se siente "arriba/abajo" es
                 * joystick_x -- mismo motivo por el que el cursor del oled
                 * necesita el intercambio px<-joystick_y / py<-joystick_x) */
                RTC_AjustarConJoystick();
                break;

            case ESTADO_CONFIG_FECHA:
                if (serial_nuevo)
                {
                    serial_nuevo = 0;
                    char c = (char)rx_data[0];

                    if (c == '\r' || c == '\n')
                    {
                        RTC_AplicarFechaTecleada();
                        rtc_input_len = 0;
                        rtc_input_buffer[0] = '\0';
                        estado_fsm = ESTADO_MONITOR;
                    }
                    else if ((c == 0x08 || c == 0x7F) && rtc_input_len > 0)
                    {
                        rtc_input_len--;
                        rtc_input_buffer[rtc_input_len] = '\0';
                    }
                    else if (rtc_input_len < RTC_INPUT_MAXLEN)
                    {
                        rtc_input_buffer[rtc_input_len++] = c;
                        rtc_input_buffer[rtc_input_len] = '\0';
                    }

                    OLED_ActualizarConfigFecha();
                }

                /* click del joystick: confirma/sale de config_fecha (alternativa al ENTER) */
                if (boton_sw_pulsado)
                {
                    boton_sw_pulsado = 0;
                    rtc_input_len = 0;
                    rtc_input_buffer[0] = '\0';
                    estado_fsm = ESTADO_MONITOR;
                }

                /* ajuste de fecha con el joystick: X=mes, Y=dia, por flanco
                 * (mismo motivo de rotacion fisica que en config_rtc) */
                RTC_AjustarFechaConJoystick();
                break;

            default:
                estado_fsm = ESTADO_MONITOR;
                break;
        }
    }
}

/* -----------------------------------------------------------------------
 * INICIALIZACION CENTRAL DE HARDWARE
 * ----------------------------------------------------------------------- */
void Inicializar_Hardware(void)
{
    SystemClock_Config();     /* sysclk a 100 mhz via pll                */
    GPIO_Init_Manual();
    MCO1_SetSource(MCO_FUENTE_HSI); /* arranca mostrando hsi por defecto  */

    RTC_Init_Manual();        /* rtc con lse + vbat                      */

    UART2_Init_Manual();      /* consola de comandos, 115200 8n1 (antes del
                                * oled: OLED_ScanAddress() necesita transmitir) */

    OLED_I2C1_Init();         /* i2c1 polling, sin interrupciones        */
    OLED_ScanAddress(&huart2); /* detecta 0x78 vs 0x7A del jumper del modulo */
    SSD1306_Init();
    SSD1306_Fill(0);
    SSD1306_WriteString(0, 0, "Examen Parcial TallerV");
    SSD1306_UpdateScreen();

    TIM3_ADCTrigger_Init();   /* trigger del adc cada 20 ms              */
    ADC1_Joystick_Init();     /* joystick x/y en modo scan               */

    TIM10_Blinky_Init();      /* blinky de estado a 250 ms               */

    /* arranques de perifericos con interrupcion / disparo por hardware */
    HAL_TIM_Base_Start(&htim3);         /* solo genera el trgo, sin isr propia */
    HAL_ADC_Start_IT(&hadc1);
    HAL_TIM_Base_Start_IT(&htim10);
    HAL_UART_Receive_IT(&huart2, rx_data, 1);

    Enviar_Menu_Ayuda(); /* banner de comandos disponibles, una vez al arrancar */
}

/* -----------------------------------------------------------------------
 * SYSCLK A 100 MHZ (PLL DESDE HSI)
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 8;    /* vco_in  = 16MHz/8   = 2MHz   */
    RCC_OscInitStruct.PLL.PLLN            = 200;  /* vco_out = 2MHz*200  = 400MHz */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV4; /* sysclk = 400/4 = 100MHz */
    RCC_OscInitStruct.PLL.PLLQ            = 4;    /* usb/sdio, no se usa aqui, valor valido */
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                        RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;  /* hclk  = 100 mhz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;    /* pclk1 =  50 mhz (limite apb1) */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;    /* pclk2 = 100 mhz (limite apb2) */

    /* 3 wait states: obligatorios en la f411 para 100 mhz a 2.7-3.6 v */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
}

/* -----------------------------------------------------------------------
 * GPIO
 * ----------------------------------------------------------------------- */
static void GPIO_Init_Manual(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* pa5: led de estado (blinky) */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ph1: led espejo del blinky en una placa auxiliar (mismo patron que
     * Tarea 3) -- no lo pide la rubrica, pero el micro va a quedar tapado
     * y este led sí queda visible para la sustentacion */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    /* pa1/pa4: entradas analogicas del joystick (pa1=vry, pa4=vrx). Se separaron
     * a pines NO vecinos (antes eran pa0/pa1, adyacentes en el header) porque
     * con pa0/pa1 aparecia continuidad electrica entre los dos ejes en el
     * cableado fisico -- pa1/pa4 no comparten header ni posicion adyacente,
     * asi que un roce accidental entre los dos cables es mucho mas dificil */
    GPIO_InitStruct.Pin  = GPIO_PIN_1 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* pa2: usart2 tx, af7 */
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* pa3: usart2 rx, af7, pullup obligatorio para evitar flancos falsos */
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* pa8: salida de reloj mco1 */
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* pa0: click (sw) del joystick, entrada con interrupcion por flanco de
     * bajada (activo en bajo) -- sin resistencia externa, se resuelve solo
     * con el pull-up interno del micro (GPIO_PULLUP). Antes vivia en pc0;
     * se movio a pa0 (libre, ya que vrx/vry se movieron a pa1/pa4) */
    GPIO_InitStruct.Pin   = GPIO_PIN_0;
    GPIO_InitStruct.Mode  = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* -----------------------------------------------------------------------
 * TIM10: BLINKY DE ESTADO A 250 MS
 * ----------------------------------------------------------------------- */
static void TIM10_Blinky_Init(void)
{
    __HAL_RCC_TIM10_CLK_ENABLE();

    htim10.Instance               = TIM10;
    htim10.Init.Prescaler         = 9999;   /* 100mhz/10000 = 10khz -> tick de 100us */
    htim10.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim10.Init.Period            = 2499;   /* 2500 ticks x 100us = 250 ms exactos   */
    htim10.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim10);

    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

/* -----------------------------------------------------------------------
 * TIM3: DISPARADOR (TRGO) DEL ADC1 CADA 20 MS
 * ----------------------------------------------------------------------- */
static void TIM3_ADCTrigger_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 9999;  /* 100mhz/10000 = 10khz -> tick de 100us */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 199;   /* 200 ticks x 100us = 20 ms exactos     */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim3);

    /* trgo en el evento de update: es lo que el adc1 usa como disparo externo */
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
}

/* -----------------------------------------------------------------------
 * ADC1: JOYSTICK X/Y (CH1=PA1=Y, CH4=PA4=X) EN MODO SCAN
 * ----------------------------------------------------------------------- */
static void ADC1_Joystick_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ENABLE;   /* 2 canales: x y y */
    hadc1.Init.ContinuousConvMode    = DISABLE;  /* solo convierte cuando tim3 avisa */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 2;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV; /* interrupcion por cada canal */
    HAL_ADC_Init(&hadc1);

    HAL_NVIC_SetPriority(ADC_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    /* rank 1: eje y (pa1, canal 1) */
    sConfig.Channel      = ADC_CHANNEL_1;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES; /* maximo posible: le da tiempo de
        sobra al capacitor interno de muestreo para "olvidar" el voltaje del canal
        anterior antes de convertir el siguiente -- evita crosstalk entre canales.
        Con 2 canales x 480 ciclos a 25MHz de reloj adc, la secuencia completa dura
        ~40us, insignificante frente a los 20ms entre disparos del TIM3 */
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* rank 2: eje x (pa4, canal 4) */
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank    = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* -----------------------------------------------------------------------
 * USART2: CONSOLA DE COMANDOS
 * ----------------------------------------------------------------------- */
static void UART2_Init_Manual(void)
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

    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* -----------------------------------------------------------------------
 * RTC: LSE + VBAT, CON DETECCION DE PRIMER ARRANQUE
 * ----------------------------------------------------------------------- */
static void RTC_Init_Manual(void)
{
    RCC_OscInitTypeDef        RCC_OscInitStruct = {0};
    RCC_PeriphCLKInitTypeDef  PeriphClkInit = {0};

    HAL_PWR_EnableBkUpAccess(); /* obligatorio para tocar el dominio de backup (rtc/bkp) */

    /* enciende el cristal externo lse de 32.768 khz de la board */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.LSEState       = RCC_LSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* el rtc toma su reloj del lse, no del lsi ni del hse dividido */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_RTC_ENABLE();

    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127; /* (127+1)x(255+1) = 32768 -> reloj interno a 1 hz */
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    HAL_RTC_Init(&hrtc);

    /* si el registro de backup no tiene la marca, es la primera vez que
     * este rtc arranca (o se le agoto la pila de respaldo): se pone una
     * fecha/hora de arranque por defecto. en resets posteriores (con vbat
     * puesto) el rtc ya viene corriendo solo y esto se salta, para no
     * pisar la hora real con el valor por defecto. */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != RTC_BKP_MAGIC)
    {
        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};

        sTime.Hours   = 0;
        sTime.Minutes = 0;
        sTime.Seconds = 0;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Month   = RTC_MONTH_JANUARY;
        sDate.Date    = 1;
        sDate.Year    = 26; /* 2026 */
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_BKP_MAGIC);
    }
}

/* -----------------------------------------------------------------------
 * MCO1: CAMBIO DE FUENTE DE RELOJ VISIBLE EN PA8
 * ----------------------------------------------------------------------- */
static void MCO1_SetSource(MCO_Fuente_t fuente)
{
    switch (fuente)
    {
        case MCO_FUENTE_HSI:
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1); /* 16 MHz */
            break;
        case MCO_FUENTE_LSE:
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_LSE, RCC_MCODIV_1); /* 32.768 kHz */
            break;
        case MCO_FUENTE_PLL:
            /* pll = 100 mhz; se divide entre 4 (25 mhz) para verlo limpio y
             * seguro en el osciloscopio del laboratorio */
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_4);
            break;
    }
    mco_fuente_actual = fuente;
}

static const char *MCO_FuenteTexto(MCO_Fuente_t fuente)
{
    switch (fuente)
    {
        case MCO_FUENTE_HSI: return "HSI 16MHz";
        case MCO_FUENTE_LSE: return "LSE 32.768kHz";
        case MCO_FUENTE_PLL: return "PLL 100MHz/4";
        default:             return "?";
    }
}

/* -----------------------------------------------------------------------
 * COMANDOS DE LA CONSOLA SERIAL (6 comandos individuales verificables)
 * ----------------------------------------------------------------------- */
static void Procesar_Comando(char c)
{
    char msg[40];

    switch (c)
    {
        case 'H': case 'h':
            MCO1_SetSource(MCO_FUENTE_HSI);
            snprintf(msg, sizeof(msg), "-> MCO1: %s\r\n", MCO_FuenteTexto(mco_fuente_actual));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
            break;
        case 'L': case 'l':
            MCO1_SetSource(MCO_FUENTE_LSE);
            snprintf(msg, sizeof(msg), "-> MCO1: %s\r\n", MCO_FuenteTexto(mco_fuente_actual));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
            break;
        case 'P': case 'p':
            MCO1_SetSource(MCO_FUENTE_PLL);
            snprintf(msg, sizeof(msg), "-> MCO1: %s\r\n", MCO_FuenteTexto(mco_fuente_actual));
            HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
            break;
        case 'T': case 't':
            rtc_input_len = 0;
            rtc_input_buffer[0] = '\0';
            joy_listo_paso_min  = 1;
            joy_listo_paso_hora = 1;
            estado_fsm = ESTADO_CONFIG_RTC;
            OLED_ActualizarConfigRTC();
            HAL_UART_Transmit(&huart2, (uint8_t *)
                "-> Escriba HH:MM:SS y ENTER, o mueva el joystick\r\n"
                "   (X=minutos, Y=horas, click=salir)\r\n", 88, 100);
            break;
        case 'F': case 'f':
            rtc_input_len = 0;
            rtc_input_buffer[0] = '\0';
            joy_listo_paso_min  = 1;
            joy_listo_paso_hora = 1;
            estado_fsm = ESTADO_CONFIG_FECHA;
            OLED_ActualizarConfigFecha();
            HAL_UART_Transmit(&huart2, (uint8_t *)
                "-> Escriba DD/MM/YY y ENTER, o mueva el joystick\r\n"
                "   (X=mes, Y=dia, click=salir)\r\n", 82, 100);
            break;
        case 'R': case 'r':
            reporte_continuo = !reporte_continuo;
            if (reporte_continuo)
            {
                HAL_UART_Transmit(&huart2, (uint8_t *)
                    "-> Reporte CONTINUO activado (manda 'R' de nuevo para apagarlo)\r\n", 67, 100);
                tick_ultimo_reporte_continuo = 0; /* fuerza el chequeo inmediato */
                ultimo_reporte_x = 0xFFFF; /* fuerza que el primer chequeo cuente como "cambio" */
                ultimo_reporte_y = 0xFFFF;
            }
            else
            {
                HAL_UART_Transmit(&huart2, (uint8_t *)"-> Reporte continuo apagado\r\n", 30, 100);
            }
            break;
        default:
            /* tecla no reconocida: reimprime el menu de ayuda */
            Enviar_Menu_Ayuda();
            break;
    }
}

/* -----------------------------------------------------------------------
 * MENU DE AYUDA POR CONSOLA (mismo patron que Tarea 3)
 * ----------------------------------------------------------------------- */
static void Enviar_Menu_Ayuda(void)
{
    static const char msg_ayuda[] =
            "\r\n"
            "==========================================================\r\n"
            "         EXAMEN PARCIAL TALLER V - CONSOLA DE COMANDOS    \r\n"
            "            Estudiante: Jimmy Stebym Rosero Barrera       \r\n"
            "==========================================================\r\n"
            "  H / h  -> MCO1 = HSI  (16 MHz) en PA8\r\n"
            "  L / l  -> MCO1 = LSE  (32.768 kHz) en PA8\r\n"
            "  P / p  -> MCO1 = PLL  (100 MHz / 4 = 25 MHz) en PA8\r\n"
            "  T / t  -> ajustar hora del RTC: escriba HH:MM:SS+ENTER,\r\n"
            "            o use el joystick (X=minutos, Y=horas,\r\n"
            "            click=salir)\r\n"
            "  F / f  -> ajustar fecha del RTC: escriba DD/MM/YY+ENTER,\r\n"
            "            o use el joystick (X=mes, Y=dia,\r\n"
            "            click=salir)\r\n"
            "  R / r  -> alterna reporte del sistema: ON = continuo\r\n"
            "            cada 300ms, R de nuevo lo apaga\r\n"
            "  (cualquier otra tecla reimprime este menu)\r\n"
            "Este menu se reenvia solo cada 30 s como recordatorio\r\n"
            "==========================================================\r\n\r\n";

    HAL_UART_Transmit(&huart2, (uint8_t *)msg_ayuda, strlen(msg_ayuda), HAL_MAX_DELAY);
}

/* -----------------------------------------------------------------------
 * REPORTE POR CONSOLA (comando 'R')
 * ----------------------------------------------------------------------- */
static void Enviar_Reporte(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    char buffer_tx[220];

    /* hay que leer siempre time y date juntos para desbloquear el shadow register */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    snprintf(buffer_tx, sizeof(buffer_tx),
             "\r\n=== REPORTE DEL SISTEMA ===\r\n"
             "  Fecha/Hora RTC : 20%02u-%02u-%02u  %02u:%02u:%02u\r\n"
             "  Joystick       : X=%4u  Y=%4u\r\n"
             "  MCO1 (PA8)     : %s\r\n"
             "  SYSCLK         : %lu Hz\r\n"
             "  Estado FSM     : %s\r\n\r\n",
             sDate.Year, sDate.Month, sDate.Date,
             sTime.Hours, sTime.Minutes, sTime.Seconds,
             joystick_x, joystick_y,
             MCO_FuenteTexto(mco_fuente_actual),
             (unsigned long)HAL_RCC_GetSysClockFreq(),
             (estado_fsm == ESTADO_MONITOR) ? "MONITOR" :
             (estado_fsm == ESTADO_CONFIG_RTC) ? "CONFIG_RTC" : "CONFIG_FECHA");

    HAL_UART_Transmit(&huart2, (uint8_t *)buffer_tx, strlen(buffer_tx), 100);
}

/* -----------------------------------------------------------------------
 * APLICA LA HORA TECLEADA POR EL USUARIO (comando 'T')
 * Formato esperado: "HH:MM:SS" (tambien acepta "HH MM SS" separado por
 * espacios). Si el formato no calza, se ignora y no se toca el rtc.
 * ----------------------------------------------------------------------- */
static void RTC_AplicarEntradaUsuario(void)
{
    int hh = -1, mm = -1, ss = -1;

    if (sscanf(rtc_input_buffer, "%d:%d:%d", &hh, &mm, &ss) != 3)
    {
        sscanf(rtc_input_buffer, "%d %d %d", &hh, &mm, &ss);
    }

    if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59)
    {
        RTC_TimeTypeDef sTime = {0};
        sTime.Hours   = (uint8_t)hh;
        sTime.Minutes = (uint8_t)mm;
        sTime.Seconds = (uint8_t)ss;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    }
}

/* -----------------------------------------------------------------------
 * APLICA LA FECHA TECLEADA POR EL USUARIO (comando 'F')
 * Formato esperado: "DD/MM/YY" (tambien acepta "DD MM YY" separado por
 * espacios). Si el formato o el rango no calzan, se avisa por UART en vez
 * de fallar en silencio (antes no avisaba nada, y un formato en el orden
 * equivocado -- ej. MM/DD/YY -- se ignoraba sin explicacion).
 * El dia de la semana no se recalcula (no lo usa ninguna pantalla ni el
 * reporte), se deja fijo igual que en la inicializacion por defecto.
 * ----------------------------------------------------------------------- */
static void RTC_AplicarFechaTecleada(void)
{
    int dd = -1, mm = -1, yy = -1;
    char msg[96];

    if (sscanf(rtc_input_buffer, "%d/%d/%d", &dd, &mm, &yy) != 3)
    {
        sscanf(rtc_input_buffer, "%d %d %d", &dd, &mm, &yy);
    }

    if (dd >= 1 && dd <= 31 && mm >= 1 && mm <= 12 && yy >= 0 && yy <= 99)
    {
        RTC_DateTypeDef sDate = {0};
        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Date    = (uint8_t)dd;
        sDate.Month   = (uint8_t)mm;
        sDate.Year    = (uint8_t)yy;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        snprintf(msg, sizeof(msg), "-> Fecha actualizada: %02d/%02d/20%02d\r\n", dd, mm, yy);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
    }
    else
    {
        snprintf(msg, sizeof(msg), "-> Fecha invalida (\"%s\"). Use DD/MM/YY, ej: 27/07/26\r\n", rtc_input_buffer);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
    }
}

/*
 * RTC_AjustarConJoystick
 * Alternativa al teclado dentro de ESTADO_CONFIG_RTC: empujar el joystick
 * en el eje Y (arriba/abajo) sube/baja los minutos: en el eje X
 * (izquierda/derecha) sube/baja las horas. Cada empujon cuenta como UN
 * paso -- hay que soltar el joystick de vuelta al centro antes de que se
 * cuente el siguiente, para no disparar decenas de pasos por segundo
 * mientras el ADC sigue muestreando cada 20 ms.
 */
static void RTC_AjustarConJoystick(void)
{
    int8_t delta_horas = 0, delta_minutos = 0;

    /* eje y: minutos */
    if (joystick_y >= JOY_UMBRAL_ALTO && joy_listo_paso_min) {
        delta_minutos = 1;
        joy_listo_paso_min = 0;
    } else if (joystick_y <= JOY_UMBRAL_BAJO && joy_listo_paso_min) {
        delta_minutos = -1;
        joy_listo_paso_min = 0;
    } else if (joystick_y >= JOY_CENTRO_MIN && joystick_y <= JOY_CENTRO_MAX) {
        joy_listo_paso_min = 1; /* de vuelta al centro: re-arma el siguiente paso */
    }

    /* eje x: horas */
    if (joystick_x >= JOY_UMBRAL_ALTO && joy_listo_paso_hora) {
        delta_horas = 1;
        joy_listo_paso_hora = 0;
    } else if (joystick_x <= JOY_UMBRAL_BAJO && joy_listo_paso_hora) {
        delta_horas = -1;
        joy_listo_paso_hora = 0;
    } else if (joystick_x >= JOY_CENTRO_MIN && joystick_x <= JOY_CENTRO_MAX) {
        joy_listo_paso_hora = 1;
    }

    if (delta_horas == 0 && delta_minutos == 0) {
        return; /* nada que ajustar todavia */
    }

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* desbloquea el shadow register */

    int16_t nueva_hora = (int16_t)sTime.Hours + delta_horas;
    int16_t nuevo_min  = (int16_t)sTime.Minutes + delta_minutos;

    if (nueva_hora < 0)  nueva_hora = 23;
    if (nueva_hora > 23) nueva_hora = 0;
    if (nuevo_min < 0)   nuevo_min = 59;
    if (nuevo_min > 59)  nuevo_min = 0;

    sTime.Hours   = (uint8_t)nueva_hora;
    sTime.Minutes = (uint8_t)nuevo_min;
    sTime.Seconds = 0; /* cada ajuste manual resincroniza los segundos a 0 */
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    OLED_ActualizarConfigRTC(); /* feedback inmediato del nuevo valor */
}

/*
 * RTC_AjustarFechaConJoystick
 * Alternativa al teclado dentro de ESTADO_CONFIG_FECHA: mismo patron "por
 * flanco" que RTC_AjustarConJoystick, pero eje X mueve el dia y eje Y
 * mueve el mes. Reutiliza joy_listo_paso_hora/joy_listo_paso_min porque
 * nunca esta activo al mismo tiempo que ESTADO_CONFIG_RTC (se re-arman
 * al entrar a cada estado).
 */
static void RTC_AjustarFechaConJoystick(void)
{
    int8_t delta_dia = 0, delta_mes = 0;

    /* eje x: dia */
    if (joystick_x >= JOY_UMBRAL_ALTO && joy_listo_paso_hora) {
        delta_dia = 1;
        joy_listo_paso_hora = 0;
    } else if (joystick_x <= JOY_UMBRAL_BAJO && joy_listo_paso_hora) {
        delta_dia = -1;
        joy_listo_paso_hora = 0;
    } else if (joystick_x >= JOY_CENTRO_MIN && joystick_x <= JOY_CENTRO_MAX) {
        joy_listo_paso_hora = 1;
    }

    /* eje y: mes */
    if (joystick_y >= JOY_UMBRAL_ALTO && joy_listo_paso_min) {
        delta_mes = 1;
        joy_listo_paso_min = 0;
    } else if (joystick_y <= JOY_UMBRAL_BAJO && joy_listo_paso_min) {
        delta_mes = -1;
        joy_listo_paso_min = 0;
    } else if (joystick_y >= JOY_CENTRO_MIN && joystick_y <= JOY_CENTRO_MAX) {
        joy_listo_paso_min = 1;
    }

    if (delta_dia == 0 && delta_mes == 0) {
        return; /* nada que ajustar todavia */
    }

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* desbloquea el shadow register */

    int16_t nuevo_dia = (int16_t)sDate.Date + delta_dia;
    int16_t nuevo_mes = (int16_t)sDate.Month + delta_mes;

    if (nuevo_dia < 1)  nuevo_dia = 31;
    if (nuevo_dia > 31) nuevo_dia = 1;
    if (nuevo_mes < 1)  nuevo_mes = 12;
    if (nuevo_mes > 12) nuevo_mes = 1;

    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Date    = (uint8_t)nuevo_dia;
    sDate.Month   = (uint8_t)nuevo_mes;
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    OLED_ActualizarConfigFecha(); /* feedback inmediato del nuevo valor */
}

/* -----------------------------------------------------------------------
 * PANTALLAS DEL OLED
 * ----------------------------------------------------------------------- */
static void OLED_ActualizarMonitor(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    char linea[32];

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* desbloquea el shadow register */

    SSD1306_Fill(0);

    snprintf(linea, sizeof(linea), "%02u:%02u:%02u  20%02u-%02u-%02u",
             sTime.Hours, sTime.Minutes, sTime.Seconds,
             sDate.Year, sDate.Month, sDate.Date);
    SSD1306_WriteString(0, 0, linea);

    snprintf(linea, sizeof(linea), "Joy X:%4u", joystick_x);
    SSD1306_WriteString(0, 16, linea);
    snprintf(linea, sizeof(linea), "Joy Y:%4u", joystick_y);
    SSD1306_WriteString(0, 26, linea);

    SSD1306_WriteString(0, 42, "Estado: MONITOR");

    snprintf(linea, sizeof(linea), "MCO1: %s", MCO_FuenteTexto(mco_fuente_actual));
    SSD1306_WriteString(0, 54, linea);

    /* cuadro subido hasta el techo (y=0) -- misma columna en x que antes,
     * solo cambia donde arranca en y. cursor "+" se mueve dentro segun la
     * posicion real del joystick. Nota: px se alimenta de joystick_y y py
     * de joystick_x (con el rango de py invertido) -- esto rota la lectura
     * 90 grados en sentido antihorario para que coincida con la
     * orientacion fisica real del modulo montado en la protoboard
     * (confirmado en hardware) */
    SSD1306_DrawEmptyRect(90, 6, 37, 47);

    /* joystick_x/joystick_y ya vienen filtrados y con zona muerta aplicada
     * desde HAL_ADC_ConvCpltCallback -- se ven quietos en reposo y solo
     * cambian con un movimiento real, tanto aqui como en Joy X/Joy Y y en
     * el reporte 'R' */
    uint8_t px = (uint8_t)Map(joystick_y, 0, 4095, 92, 124);
    uint8_t py = (uint8_t)Map(joystick_x, 0, 4095, 51, 8);
    SSD1306_DrawCross(px, py);

    SSD1306_UpdateScreen();
}

static void OLED_ActualizarConfigRTC(void)
{
    char linea[24];
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* desbloquea el shadow register */

    SSD1306_Fill(0);
    SSD1306_WriteString(0, 0, "CONFIG RTC");

    snprintf(linea, sizeof(linea), "Actual: %02u:%02u:%02u", sTime.Hours, sTime.Minutes, sTime.Seconds);
    SSD1306_WriteString(0, 12, linea);

    SSD1306_WriteString(0, 24, "Joystick: X=min Y=hr");
    SSD1306_WriteString(0, 34, "Teclado: HH:MM:SS+ENT");
    SSD1306_WriteString(0, 44, "Click = salir");

    snprintf(linea, sizeof(linea), "> %s", rtc_input_buffer);
    SSD1306_WriteString(0, 54, linea);

    SSD1306_UpdateScreen();
}

static void OLED_ActualizarConfigFecha(void)
{
    char linea[24];
    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* desbloquea el shadow register */

    SSD1306_Fill(0);
    SSD1306_WriteString(0, 0, "CONFIG FECHA");

    snprintf(linea, sizeof(linea), "Actual: %02u/%02u/20%02u", sDate.Date, sDate.Month, sDate.Year);
    SSD1306_WriteString(0, 12, linea);

    SSD1306_WriteString(0, 24, "Joystick: X=mes Y=dia");
    SSD1306_WriteString(0, 34, "Teclado: DD/MM/YY+ENT");
    SSD1306_WriteString(0, 44, "Click = salir");

    snprintf(linea, sizeof(linea), "> %s", rtc_input_buffer);
    SSD1306_WriteString(0, 54, linea);

    SSD1306_UpdateScreen();
}

/* pantalla de bienvenida disparada por el click del joystick (pa0/exti0) */
static void OLED_MostrarBienvenida(void)
{
    SSD1306_Fill(0);
    SSD1306_WriteString(10, 8, "Examen Parcial V");
    SSD1306_WriteString(10, 20, "Jimmy Rosero B.");
    SSD1306_WriteString(10, 36, "Presiona Joys!");
    SSD1306_DrawEmptyRect(4, 4, 119, 55);
    SSD1306_UpdateScreen();
}

/* -----------------------------------------------------------------------
 * CALLBACKS DE INTERRUPCIONES DE LA HAL
 * ----------------------------------------------------------------------- */

/* tim10 desborda cada 250 ms -> alterna el led de estado (pa5) y su espejo (ph1) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
    }
}

/* llega un byte nuevo por usart2 -> levanta la bandera y rearma la escucha */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        serial_nuevo = 1;
        HAL_UART_Receive_IT(&huart2, rx_data, 1);
    }
}

/* tim3_trgo dispara una conversion (cada 20 ms), el adc entrega rank1 y luego rank2 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        uint16_t valor = (uint16_t)HAL_ADC_GetValue(&hadc1);

        if (adc_rank_actual == 0)
        {
            /* primer canal de la secuencia: rank1 = ch1 = pa1 = vry */
            filtro_adc_y += ((int32_t)valor - filtro_adc_y) / ADC_FILTRO_N;
            if (filtro_adc_y > JOY_CENTRO - JOY_ZONA_MUERTA && filtro_adc_y < JOY_CENTRO + JOY_ZONA_MUERTA) {
                joystick_y = JOY_CENTRO; /* dentro de la zona muerta: se ve quieto */
            } else {
                joystick_y = (uint16_t)filtro_adc_y;
            }
            adc_rank_actual = 1;
        }
        else
        {
            /* ultimo canal de la secuencia: rank2 = ch4 = pa4 = vrx. se rearma
             * aca para que el proximo tim3_trgo dispare la secuencia completa
             * otra vez desde el rank1, igual que se hace con un solo canal en
             * Tarea 3 */
            filtro_adc_x += ((int32_t)valor - filtro_adc_x) / ADC_FILTRO_N;
            if (filtro_adc_x > JOY_CENTRO - JOY_ZONA_MUERTA && filtro_adc_x < JOY_CENTRO + JOY_ZONA_MUERTA) {
                joystick_x = JOY_CENTRO;
            } else {
                joystick_x = (uint16_t)filtro_adc_x;
            }
            adc_rank_actual = 0;
            HAL_ADC_Start_IT(&hadc1);
        }
    }
}

/* click del joystick (pa0, flanco de bajada) -> levanta la bandera que saca
 * a la fsm de ESTADO_BIENVENIDA hacia ESTADO_MONITOR. debounce simple por
 * software (ignora un segundo flanco si llego muy rapido detras del
 * anterior). */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        uint32_t ahora = HAL_GetTick();
        if ((ahora - tick_ultimo_click) >= SW_DEBOUNCE_MS)
        {
            boton_sw_pulsado = 1;
            tick_ultimo_click = ahora;
        }
    }
}
