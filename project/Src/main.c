/**
 ******************************************************************************
 * @file    main.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Practica de diseño visual para Beat Clash — ILI9341 320x240.
 *
 * Origen del proyecto: naturalmente `practica_pantalla/` partio como banco de
 * pruebas de renderizado (usando el `renderer.c` real del proyecto principal,
 * copiado sin cambios, con un `GameState_t` de ejemplo) para ajustar colores,
 * tamaños y layout sin la complejidad del resto de perifericos. Con el tiempo
 * el alcance crecio: hoy este archivo contiene la logica REAL y completa de
 * dos de los tres modos de juego (Simon+Joystick y Simon con botones arcade,
 * ambos a 2 jugadores cara a cara), no solo la maqueta visual original.
 *
 * Navegacion del recorrido de pantallas: B1 (PC13) o el eje Y de cualquiera
 * de los 2 joystick (PA1=VRy1 / PC0=VRy2, arriba/abajo) avanzan/retroceden
 * entre pantallas — configuracion ADC heredada y probada en examen_parcial
 * (TIM3_TRGO cada 20ms, filtro EMA, zona muerta). Las animaciones (parpadeo
 * del splash, notas cayendo) siguen vivas mientras se espera.
 *
 * B1 es, desde el 2026-07-30, el UNICO boton de "click" fisico del sistema:
 * los pines SW de ambos joystick se retiraron para ahorrar cableado (ver
 * board_pins.h), asi que B1 paso a confirmar tambien los menus (cuantos
 * jugadores, que modo) y a lanzar el conteo 3-2-1-GO (ver el bloque
 * "if (avanzar)" en main()). El reintento tras perder en cualquier modo ya
 * NO depende de ningun boton: en Simon+Joystick se dispara moviendo el
 * propio stick (ver SJ_GAMEOVER en SimonJoy_Actualizar/SimonJoy2_Actualizar-
 * Jugador) y en el modo de botones arcade, presionando cualquiera de los 4
 * botones propios (ver Botones_ActualizarJugador) — el mismo patron "cualquier
 * entrada tuya reintenta" se aplico a los tres modos por consistencia.
 *
 * El modo Guitar Hero real sincronizado con canciones se quito (2026-07-28)
 * para bajar el peso del codigo mientras el foco estaba en sacar adelante
 * los joystick y los botones arcade -- solo queda GuitarHero_IntentarGolpe(),
 * el boton de prueba original dentro del recorrido de diseño (DEMO_JUGANDO).
 * Con Simon+Joystick y Simon+Botones ya jugables a 2 jugadores en hardware
 * real, el siguiente foco del proyecto es reconstruir este modo por completo.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "board_pins.h"
#include "game_state.h"
#include "ili9341.h"
#include "renderer.h"
#include <string.h>
#include <stdio.h>

SPI_HandleTypeDef hspi1;
ADC_HandleTypeDef hadc1;   /* joystick x/y */
TIM_HandleTypeDef htim3;   /* disparador del adc cada 20 ms */
TIM_HandleTypeDef htim4;   /* alterna pa6 por interrupcion para el tono del buzzer */
UART_HandleTypeDef huart2; /* consola de depuracion por el VCP del ST-Link (ver MX_USART2_UART_Init) */
static GameState_t gs;

/* === JOYSTICK — misma configuracion/filtro que examen_parcial ============= */
/* lecturas de 12 bits (0-4095) ya filtradas (ver HAL_ADC_ConvCpltCallback) */
volatile uint16_t joystick_x = 2048;
volatile uint16_t joystick_y = 2048;
static uint8_t    adc_rank_actual = 0;   /* 0=y1(rank1) 1=x1(rank2) 2=y2(rank3) 3=x2(rank4) */

#define ADC_FILTRO_N     4   /* vuelto a 4 (2026-07-31, pedido explicito del
    usuario) -- mismo valor que examen_parcial, mas suave que el 2 que se
    habia puesto antes; con el centro ahora calibrado en el arranque (ver
    Joystick_Calibrar) ya no hace falta sacrificar suavidad por velocidad de
    deteccion de direccion. */
static int32_t filtro_adc_x = 2048;
static int32_t filtro_adc_y = 2048;

/* === JOYSTICK 2 — segundo jugador (2026-07-29, ver CABLEADO_JOYSTICK2.txt) =
 * Mismo filtro EMA + zona muerta que el joystick 1, leido en los ranks 3/4
 * del mismo escaneo del ADC1 (ver ADC1_Joystick_Init/HAL_ADC_ConvCpltCallback).
 * En un inicio (2026-07-29) solo se leia y se mostraba un 2do cursor en
 * pantalla para verificar el cableado durante el montaje fisico de la caja;
 * ese cursor de depuracion ya se retiro (2026-07-30) porque quedo obsoleto
 * en cuanto existio la logica real de juego a 2 jugadores (ver seccion
 * "SIMON + JOYSTICK — 2 JUGADORES CARA A CARA" mas abajo). */
volatile uint16_t joystick2_x = 2048;
volatile uint16_t joystick2_y = 2048;
static int32_t filtro_adc_x2 = 2048;
static int32_t filtro_adc_y2 = 2048;

#define JOY_ZONA_MUERTA  150

/* CALIBRACION DE CENTRO (2026-07-31): pedido de diagnostico -- por consola
 * se vio que el joystick fisico de este montaje NO descansa en 2048 (mitad
 * de escala teorica), sino mucho mas arriba (~2950-3150 en J1, ~2985-3070
 * en J2). Un centro fijo en 2048 hacia que un eje casi nunca alcanzara el
 * umbral "bajo" (tendria que recorrer casi toda la escala) y el otro
 * disparara con cualquier ruido (ya estaba a unas pocas cuentas del umbral
 * "alto"). Por eso el centro de cada eje se MIDE al arrancar (ver
 * Joystick_Calibrar en main(), corre una vez tras HAL_ADC_Start_IT) y los
 * umbrales de direccion se calculan relativos a ese centro medido, no a un
 * valor fijo -- ver JOY_UMBRAL_DESVIO y Joy_Umbrales(). */
static uint16_t centro_j1x = 2048, centro_j1y = 2048;
static uint16_t centro_j2x = 2048, centro_j2y = 2048;

/* Bajado de 600 a 280 (2026-07-31, confirmado con datos reales de consola):
 * el recorrido fisico de este joystick NO es simetrico alrededor del centro
 * medido -- un lado llega a unas ~2900 cuentas de distancia, pero el otro
 * lado (el "corto") solo recorre ~340-350 cuentas antes de chocar contra el
 * tope mecanico. Con el umbral en 600, ese lado corto NUNCA alcanzaba el
 * umbral -- esa direccion quedaba practicamente inalcanzable. 280 deja
 * margen de sobra bajo el peor caso medido (~340) sin ser tan chico como
 * para disparar con ruido normal del ADC. */
#define JOY_UMBRAL_DESVIO 280U   /* cuentas de distancia al centro MEDIDO para contar como "direccion" */

/* Calcula el rango [bajo,alto] alrededor de un centro medido, con clamp a
 * los limites reales del ADC de 12 bits (evita underflow/overflow si el
 * centro medido queda muy cerca de 0 o 4095). */
static void Joy_Umbrales(uint16_t centro, uint16_t *bajo, uint16_t *alto) {
    int32_t b = (int32_t)centro - (int32_t)JOY_UMBRAL_DESVIO;
    int32_t a = (int32_t)centro + (int32_t)JOY_UMBRAL_DESVIO;
    *bajo = (uint16_t)(b < 0 ? 0 : b);
    *alto = (uint16_t)(a > 4095 ? 4095 : a);
}

/* === BOTONES ARCADE — 2 jugadores x 4 colores (2026-07-29) =================
 * Tablas indexadas [jugador][color], color: 0=ROJO 1=VERDE 2=AZUL 3=AMARILLO
 * (ver board_pins.h para el cableado fisico completo). */
typedef struct { GPIO_TypeDef *port; uint16_t pin; } PinRef_t;

static const PinRef_t BTN_SW[2][4] = {
    { { BTN1_ROJO_SW_PORT, BTN1_ROJO_SW_PIN }, { BTN1_VERDE_SW_PORT, BTN1_VERDE_SW_PIN },
      { BTN1_AZUL_SW_PORT, BTN1_AZUL_SW_PIN }, { BTN1_AMARILLO_SW_PORT, BTN1_AMARILLO_SW_PIN } },
    { { BTN2_ROJO_SW_PORT, BTN2_ROJO_SW_PIN }, { BTN2_VERDE_SW_PORT, BTN2_VERDE_SW_PIN },
      { BTN2_AZUL_SW_PORT, BTN2_AZUL_SW_PIN }, { BTN2_AMARILLO_SW_PORT, BTN2_AMARILLO_SW_PIN } }
};

static const PinRef_t BTN_LED[2][4] = {
    { { BTN1_ROJO_LED_PORT, BTN1_ROJO_LED_PIN }, { BTN1_VERDE_LED_PORT, BTN1_VERDE_LED_PIN },
      { BTN1_AZUL_LED_PORT, BTN1_AZUL_LED_PIN }, { BTN1_AMARILLO_LED_PORT, BTN1_AMARILLO_LED_PIN } },
    { { BTN2_ROJO_LED_PORT, BTN2_ROJO_LED_PIN }, { BTN2_VERDE_LED_PORT, BTN2_VERDE_LED_PIN },
      { BTN2_AZUL_LED_PORT, BTN2_AZUL_LED_PIN }, { BTN2_AMARILLO_LED_PORT, BTN2_AMARILLO_LED_PIN } }
};

static GPIO_PinState btn_prev[2][4] = {
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }
};

/* Debounce real por tiempo (2026-07-31, pedido explicito del usuario: nada
 * de cambios de pantalla/modo "de la nada"). Antes, Botones_LeerColor y
 * Boton_B1_Flanco solo comparaban el nivel actual contra el del tick
 * anterior (~33ms) sin exigir que se mantenga estable -- un rebote
 * mecanico o un pico de ruido electrico de un solo tick ya contaba como
 * flanco real. Ahora se exige que el nuevo nivel se mantenga estable
 * BTN_DEBOUNCE_MS antes de aceptarlo como cambio de verdad. */
#define BTN_DEBOUNCE_MS 30U
static GPIO_PinState btn_estable[2][4] = {
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }
};
static uint32_t btn_tick_cambio[2][4];

static void Boton_LED(uint8_t p, uint8_t c, uint8_t on) {
    HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Secuencia de encendido tipo "tira LED" (2026-07-31, pedido explicito del
 * usuario) -- un LED prendido a la vez, recorriendo los 8 botones (J1 y
 * despues J2) en bucle, MIENTRAS se ve el splash. Ya no es una pasada unica
 * y bloqueante antes de arrancar: se actualiza sola, un paso por vuelta del
 * loop principal (ver BotonesChase_Actualizar, llamada solo si
 * screen==DEMO_SPLASH), y se apaga del todo al salir del splash
 * (BotonesChase_Detener) -- no se toca el "heartbeat" en pantalla
 * (draw_heartbeat en renderer.c), sigue igual. */
#define CHASE_LED_PASO_MS 180U
static uint32_t chase_led_tick   = 0;
static uint8_t  chase_led_idx    = 0;
static uint8_t  chase_led_activo = 0;

static void BotonesChase_Iniciar(void) {
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
    chase_led_idx    = 0;
    chase_led_tick   = HAL_GetTick();
    chase_led_activo = 1;
    Boton_LED(0, 0, 1);
}

static void BotonesChase_Actualizar(void) {
    if (!chase_led_activo) return;
    uint32_t ahora = HAL_GetTick();
    if (ahora - chase_led_tick < CHASE_LED_PASO_MS) return;
    chase_led_tick = ahora;

    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 0);
    chase_led_idx = (uint8_t)((chase_led_idx + 1) % 8);
    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 1);
}

static void BotonesChase_Detener(void) {
    if (!chase_led_activo) return;
    chase_led_activo = 0;
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
}

/* Nombres por consola (2026-07-31, pedido explicito del usuario: una forma
 * de saber SEGURO que boton/direccion se detecto, sin depender de describirlo
 * por chat -- alcanza con copiar y pegar la consola UART). Puestos en las
 * funciones de MAS BAJO NIVEL (Botones_LeerColor/Joystick*_LeerDireccion)
 * para que salgan SIEMPRE que se detecta un flanco real, sin importar desde
 * donde se llamen (menu, juego real, Guitar Hero) y sin duplicar el log ni
 * robarle el flanco a nadie -- estas funciones ya eran el unico punto por
 * donde pasa la lectura real del hardware. */
static const char *const COLOR_NOMBRE[4] = { "ROJO", "VERDE", "AZUL", "AMARILLO" };
static const char *const DIR_NOMBRE[4]   = { "ARRIBA", "ABAJO", "IZQUIERDA", "DERECHA" };

/* primer color con flanco de presion (activo BAJO, pull-up interno) del
 * jugador p, o 0xFF si ninguno se presiono este tick -- con debounce real
 * (ver BTN_DEBOUNCE_MS). */
static uint8_t Botones_LeerColor(uint8_t p) {
    for (uint8_t c = 0; c < 4; c++) {
        GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_SW[p][c].port, BTN_SW[p][c].pin);

        if (cur != btn_estable[p][c]) {
            btn_estable[p][c]    = cur;
            btn_tick_cambio[p][c] = HAL_GetTick();
            continue;   /* posible rebote -- todavia no cuenta, seguir con el siguiente color */
        }
        if (HAL_GetTick() - btn_tick_cambio[p][c] < BTN_DEBOUNCE_MS) continue;  /* aun no esta estable */

        uint8_t flanco = (btn_prev[p][c] == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
        btn_prev[p][c] = cur;
        if (flanco) {
            printf("[INPUT] boton J%u = %s\r\n", (unsigned)(p + 1), COLOR_NOMBRE[c]);
            return c;
        }
    }
    return 0xFF;
}

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void TIM3_ADCTrigger_Init(void);
static void ADC1_Joystick_Init(void);
static void MX_TIM4_Buzzer_Init(void);
static void MX_USART2_UART_Init(void);

/* ========================================================================== */
/* === BUZZER — TONO POR SOFTWARE (TIM4 + GPIO PA6) — TIPO SIN CONFIRMAR ===== */
/* ========================================================================== */
/* El usuario no esta seguro si el buzzer es activo (un solo tono, suena con
 * DC) o pasivo (necesita onda cuadrada, ver prueba_de_sonido/ para la prueba
 * dedicada). Por eso, igual que alli, se maneja con frecuencia variable: si
 * resulta activo, sonara igual (el activo ignora la frecuencia externa y usa
 * su propio tono fijo); si resulta pasivo, ahora SI se escucharan tonos y
 * canciones reales en vez de solo clics.
 *
 * El STM32F411 no tiene TIM13/14 y el unico timer con canal en PA6 (TIM3_CH1)
 * ya esta ocupado por el disparador del ADC del joystick -- por eso el tono
 * NO es PWM de hardware: TIM4 (libre) genera una interrupcion periodica que
 * alterna PA6 por software (HAL_GPIO_TogglePin en TIM4_IRQHandler), dos
 * flancos = un ciclo completo de la onda cuadrada.
 *
 * TIM4CLK = 16MHz (HSI sin PLL). Prescaler=15 -> tick de 1MHz, asi
 * ARR = 1000000/(2*freq_hz) - 1 alterna el pin a la frecuencia exacta. */
#define BUZZER_TIM_TICK_HZ 1000000U
#define BUZZER_TONO_HZ     4000U   /* subido de 2000 (2026-07-31, pedido explicito del
    usuario de mas volumen) -- sin datos de un barrido propio (no lo escucho),
    4000Hz es una apuesta razonable: la mayoria de piezos chicos como este
    resuenan (suenan mas fuerte) en el rango ~2.7-4kHz, no en 2000Hz. Si no
    se nota mas fuerte, correr Buzzer_BarridoDiagnostico (ver mas abajo,
    BUZZER_DIAGNOSTICO_BARRIDO) para medir la frecuencia real de resonancia
    de ESTE buzzer especifico. */

typedef struct {
    uint16_t freq_hz;   /* 0=silencio, otro=tono en Hz */
    uint16_t dur_ms;
} PasoSonido_t;

static void Buzzer_SetSalida(uint16_t freq_hz) {
    if (freq_hz == 0) {
        HAL_TIM_Base_Stop_IT(&htim4);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    } else {
        uint32_t arr = (BUZZER_TIM_TICK_HZ / (2U * freq_hz)) - 1U;
        HAL_TIM_Base_Stop_IT(&htim4);
        __HAL_TIM_SET_COUNTER(&htim4, 0);
        __HAL_TIM_SET_AUTORELOAD(&htim4, arr);
        HAL_TIM_Base_Start_IT(&htim4);
    }
}

/* Apunta directo al arreglo const (sin copiar) -- las canciones tienen
 * decenas de notas, un buffer de tamaño fijo ya no tiene sentido. */
static const PasoSonido_t *buzzer_pasos     = 0;
static uint16_t            buzzer_pasos_n   = 0;
static uint16_t            buzzer_pos       = 0;
static uint32_t            buzzer_tick_paso = 0;

static void Buzzer_Patron(const PasoSonido_t *pasos, uint16_t n) {
    buzzer_pasos     = pasos;
    buzzer_pasos_n   = n;
    buzzer_pos       = 0;
    buzzer_tick_paso = HAL_GetTick();
    Buzzer_SetSalida(buzzer_pasos[0].freq_hz);
}

static void Buzzer_Beep(uint16_t duracion_ms) {
    static PasoSonido_t un_paso;
    un_paso.freq_hz = BUZZER_TONO_HZ;
    un_paso.dur_ms  = duracion_ms;
    Buzzer_Patron(&un_paso, 1);
}

/* Diagnostico de volumen (2026-07-31, pedido explicito del usuario): un
 * buzzer piezo suena mas fuerte cerca de su frecuencia de resonancia
 * propia, no necesariamente a BUZZER_TONO_HZ (2000Hz, elegido a ojo). Corre
 * UNA vez al arrancar, antes del splash -- bloqueante (HAL_Delay), no pasa
 * nada porque todavia no hay juego corriendo. Escuchar por el parlante y
 * anotar por consola (UART, ver MX_USART2_UART_Init) cual frecuencia sono
 * mas fuerte, despues fijar BUZZER_TONO_HZ a ese valor y poner
 * BUZZER_DIAGNOSTICO_BARRIDO en 0 para que deje de correr en cada arranque. */
#define BUZZER_DIAGNOSTICO_BARRIDO 0   /* apagado (2026-07-31) -- el usuario pidio
    subir el volumen ya, sin hacer la prueba de escucha; se deja la funcion
    lista por si mas adelante se quiere medir la resonancia real. */
#if BUZZER_DIAGNOSTICO_BARRIDO
static void Buzzer_BarridoDiagnostico(void) {
    printf("\r\n[BUZZER] barrido de frecuencias -- escuchar y anotar cual suena mas fuerte\r\n");
    for (uint16_t f = 200; f <= 5000; f = (uint16_t)(f + 200)) {
        printf("[BUZZER] %u Hz\r\n", f);
        Buzzer_SetSalida(f);
        HAL_Delay(400);
        Buzzer_SetSalida(0);
        HAL_Delay(120);
    }
    printf("[BUZZER] fin del barrido\r\n\r\n");
}
#endif

/* llamar una vez por vuelta del loop principal, sin importar el estado */
/* Musica de fondo (2026-07-31, pedido explicito del usuario) -- "segundo
 * canal" logico separado del de arriba (que pasa a ser el de SFX en primer
 * plano: beeps de acierto/fallo/ronda, jingle de bienvenida, preview de
 * DEMO_MENU_CANCIONES). El buzzer sigue siendo mono (una sola nota real a
 * la vez) -- mientras un SFX esta sonando, la musica de fondo queda
 * "congelada" en su paso actual (no avanza su reloj) y Buzzer_Actualizar
 * retoma esa MISMA nota apenas el SFX termina, en vez de perderse un paso
 * o reiniciar la cancion. Ver Buzzer_Fondo_Iniciar/Detener mas abajo (una
 * vez declaradas CANCIONES_DATA/LEN). */
static const PasoSonido_t *buzzer_fondo_pasos     = 0;
static uint16_t            buzzer_fondo_pasos_n   = 0;
static uint16_t            buzzer_fondo_pos       = 0;
static uint32_t            buzzer_fondo_tick_paso = 0;
static uint8_t             buzzer_fondo_activo    = 0;

static void Buzzer_Actualizar(void) {
    if (buzzer_pos < buzzer_pasos_n) {
        /* SFX en primer plano en curso */
        if (HAL_GetTick() - buzzer_tick_paso < buzzer_pasos[buzzer_pos].dur_ms) return;
        buzzer_pos++;
        buzzer_tick_paso = HAL_GetTick();
        if (buzzer_pos < buzzer_pasos_n) {
            Buzzer_SetSalida(buzzer_pasos[buzzer_pos].freq_hz);
            return;
        }
        /* el SFX termino recien en este mismo tick -- retomar la musica de
         * fondo YA (sin esperar a que termine su paso actual, que quedo
         * congelado mientras sonaba el SFX encima). */
        Buzzer_SetSalida(buzzer_fondo_activo ? buzzer_fondo_pasos[buzzer_fondo_pos].freq_hz : 0);
        return;
    }

    /* sin SFX en curso: avanzar la musica de fondo normalmente (en loop) */
    if (!buzzer_fondo_activo) return;
    if (HAL_GetTick() - buzzer_fondo_tick_paso < buzzer_fondo_pasos[buzzer_fondo_pos].dur_ms) return;

    buzzer_fondo_pos++;
    if (buzzer_fondo_pos >= buzzer_fondo_pasos_n) buzzer_fondo_pos = 0;   /* loop */
    buzzer_fondo_tick_paso = HAL_GetTick();
    Buzzer_SetSalida(buzzer_fondo_pasos[buzzer_fondo_pos].freq_hz);
}

/* Jingle de bienvenida: arpegio ascendente Do-Mi-Sol-Do (3 octavas) —
 * composicion propia del usuario para Beat Clash, ver
 * prueba_de_sonido/sonidos/beat_clash_welcome_jingle.ino */
static const PasoSonido_t BEEP_BIENVENIDA[] = {
    { 262, 120 }, { 0, 40 }, { 330, 120 }, { 0, 40 }, { 392, 120 }, { 0, 40 },
    { 523, 180 }, { 0, 40 }, { 659, 180 }, { 0, 40 }, { 784, 250 }, { 0, 40 },
    { 1047, 400 }
};
#define BEEP_BIENVENIDA_N (sizeof(BEEP_BIENVENIDA) / sizeof(BEEP_BIENVENIDA[0]))

/* ========================================================================== */
/* === LISTA DE CANCIONES (pantalla "reproductor", ver DEMO_MENU_CANCIONES) == */
/* ========================================================================== */
/* Todas de dominio publico (compositores fallecidos hace mucho o folclor
 * tradicional) transcritas en prueba_de_sonido/, mas la propia composicion
 * del usuario -- ver esa memoria para detalle de fuentes/frecuencias. */

#define DO4   262
#define RE4   294
#define MI4   330
#define FA4   349
#define SOL4  392
#define LA4   440

#define DO5   523
#define RE5   587
#define MI5   659
#define FA5   698
#define SOL5  784

#define NEGRA    220
#define CORCHEA  110
#define BLANCA   440
#define GAP_MS    20
#define N(f, d)  { (f), (d) }, { 0, GAP_MS }

static const PasoSonido_t CANCION_ESTRELLITA[] = {   /* Estrellita donde estas */
    N(DO4, NEGRA),  N(DO4, NEGRA),  N(SOL4, NEGRA), N(SOL4, NEGRA),
    N(LA4, NEGRA),  N(LA4, NEGRA),  N(SOL4, BLANCA),
    N(FA4, NEGRA),  N(FA4, NEGRA),  N(MI4, NEGRA),  N(MI4, NEGRA),
    N(RE4, NEGRA),  N(RE4, NEGRA),  N(DO4, BLANCA),
    N(SOL4, NEGRA), N(SOL4, NEGRA), N(FA4, NEGRA),  N(FA4, NEGRA),
    N(MI4, NEGRA),  N(MI4, NEGRA),  N(RE4, BLANCA),
    N(SOL4, NEGRA), N(SOL4, NEGRA), N(FA4, NEGRA),  N(FA4, NEGRA),
    N(MI4, NEGRA),  N(MI4, NEGRA),  N(RE4, BLANCA),
    N(DO4, NEGRA),  N(DO4, NEGRA),  N(SOL4, NEGRA), N(SOL4, NEGRA),
    N(LA4, NEGRA),  N(LA4, NEGRA),  N(SOL4, BLANCA),
    N(FA4, NEGRA),  N(FA4, NEGRA),  N(MI4, NEGRA),  N(MI4, NEGRA),
    N(RE4, NEGRA),  N(RE4, NEGRA),  N(DO4, BLANCA),
};

static const PasoSonido_t CANCION_HIMNO[] = {   /* Himno de la Alegria, Beethoven */
    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(MI5, NEGRA), N(0, CORCHEA), N(RE5, CORCHEA), N(RE5, BLANCA),

    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(RE5, NEGRA), N(0, CORCHEA), N(DO5, CORCHEA), N(DO5, BLANCA),

    N(RE5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA), N(DO5, NEGRA),
    N(RE5, NEGRA), N(MI5, CORCHEA), N(FA5, CORCHEA), N(MI5, NEGRA), N(DO5, NEGRA),
    N(RE5, NEGRA), N(MI5, CORCHEA), N(FA5, CORCHEA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(RE5, NEGRA), N(SOL5, BLANCA),

    N(MI5, NEGRA), N(MI5, NEGRA), N(FA5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, NEGRA), N(FA5, NEGRA), N(MI5, NEGRA), N(RE5, NEGRA),
    N(DO5, NEGRA), N(DO5, NEGRA), N(RE5, NEGRA), N(MI5, NEGRA),
    N(RE5, NEGRA), N(0, CORCHEA), N(DO5, CORCHEA), N(DO5, BLANCA),
};

static const PasoSonido_t CANCION_MARTINILLO[] = {   /* Fray Santiago / Frere Jacques */
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
};

static const PasoSonido_t CANCION_NAVIDAD[] = {   /* Jingle Bells, tradicional 1857 */
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(SOL5, CORCHEA), N(DO5, CORCHEA), N(RE5, CORCHEA), N(MI5, BLANCA),
    N(FA5, CORCHEA), N(FA5, CORCHEA), N(FA5, NEGRA), N(FA5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(RE5, CORCHEA), N(RE5, CORCHEA), N(MI5, CORCHEA), N(RE5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, CORCHEA), N(SOL5, CORCHEA), N(FA5, CORCHEA), N(RE5, CORCHEA), N(DO5, NEGRA),
};

/* Tetris (Korobeiniki, folclor ruso s.XIX), tempo=144 -- codigo de duracion
 * (4=negra,8=corchea,negativo=con puntillo), formula igual a BuzzerMelody.cpp */
#define TD(tempo, div) ((div) > 0 ? (240000 / (tempo)) / (div) \
                                  : ((240000 / (tempo)) / (-(div)) * 3) / 2)
#define NT(freq, tempo, div) N((freq), TD((tempo), (div)))
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880

static const PasoSonido_t CANCION_TETRIS[] = {
    NT(NOTE_E5,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_C5,144,8), NT(NOTE_B4,144,8),
    NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,-4), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_A4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8),

    NT(NOTE_D5,144,-4), NT(NOTE_F5,144,8), NT(NOTE_A5,144,4), NT(NOTE_G5,144,8), NT(NOTE_F5,144,8),
    NT(NOTE_E5,144,-4), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,4), NT(0,144,4),

    NT(NOTE_E5,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_C5,144,8), NT(NOTE_B4,144,8),
    NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,-4), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,8), NT(NOTE_A4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8),

    NT(NOTE_D5,144,-4), NT(NOTE_F5,144,8), NT(NOTE_A5,144,4), NT(NOTE_G5,144,8), NT(NOTE_F5,144,8),
    NT(NOTE_E5,144,-4), NT(NOTE_C5,144,8), NT(NOTE_E5,144,4), NT(NOTE_D5,144,8), NT(NOTE_C5,144,8),
    NT(NOTE_B4,144,4), NT(NOTE_B4,144,8), NT(NOTE_C5,144,8), NT(NOTE_D5,144,4), NT(NOTE_E5,144,4),
    NT(NOTE_C5,144,4), NT(NOTE_A4,144,4), NT(NOTE_A4,144,4), NT(0,144,4),

    NT(NOTE_E5,144,2), NT(NOTE_C5,144,2),
    NT(NOTE_D5,144,2), NT(NOTE_B4,144,2),
    NT(NOTE_C5,144,2), NT(NOTE_A4,144,2),
    NT(NOTE_GS4,144,2), NT(NOTE_B4,144,4), NT(0,144,8),
    NT(NOTE_E5,144,2), NT(NOTE_C5,144,2),
    NT(NOTE_D5,144,2), NT(NOTE_B4,144,2),
    NT(NOTE_C5,144,4), NT(NOTE_E5,144,4), NT(NOTE_A5,144,2),
    NT(NOTE_GS5,144,2),
};
#undef NT
#undef TD
#undef N

typedef enum { CANCION_BIENVENIDA_IDX = 0, CANCION_ESTRELLITA_IDX, CANCION_HIMNO_IDX,
               CANCION_MARTINILLO_IDX, CANCION_NAVIDAD_IDX, CANCION_TETRIS_IDX,
               CANCIONES_N } CancionIdx_t;

/* Los nombres para mostrar en pantalla viven en renderer.c (RLC_NOMBRE) --
 * el ORDEN debe coincidir exactamente con este arreglo. */
static const PasoSonido_t *const CANCIONES_DATA[CANCIONES_N] = {
    BEEP_BIENVENIDA, CANCION_ESTRELLITA, CANCION_HIMNO, CANCION_MARTINILLO, CANCION_NAVIDAD, CANCION_TETRIS
};
static const uint16_t CANCIONES_LEN[CANCIONES_N] = {
    BEEP_BIENVENIDA_N,
    (uint16_t)(sizeof(CANCION_ESTRELLITA)  / sizeof(CANCION_ESTRELLITA[0])),
    (uint16_t)(sizeof(CANCION_HIMNO)       / sizeof(CANCION_HIMNO[0])),
    (uint16_t)(sizeof(CANCION_MARTINILLO)  / sizeof(CANCION_MARTINILLO[0])),
    (uint16_t)(sizeof(CANCION_NAVIDAD)     / sizeof(CANCION_NAVIDAD[0])),
    (uint16_t)(sizeof(CANCION_TETRIS)      / sizeof(CANCION_TETRIS[0])),
};

/* Arranca la musica de fondo con la cancion `cancion_idx` (loop continuo,
 * ver Buzzer_Actualizar para como convive con los SFX en primer plano). */
static void Buzzer_Fondo_Iniciar(uint8_t cancion_idx) {
    buzzer_fondo_pasos     = CANCIONES_DATA[cancion_idx];
    buzzer_fondo_pasos_n   = CANCIONES_LEN[cancion_idx];
    buzzer_fondo_pos       = 0;
    buzzer_fondo_tick_paso = HAL_GetTick();
    buzzer_fondo_activo    = 1;
    if (buzzer_pos >= buzzer_pasos_n) Buzzer_SetSalida(buzzer_fondo_pasos[0].freq_hz);
}

static void Buzzer_Fondo_Detener(void) {
    buzzer_fondo_activo = 0;
    if (buzzer_pos >= buzzer_pasos_n) Buzzer_SetSalida(0);
}

/* ========================================================================== */
/* === RECORRIDO DE PANTALLAS DE DISEÑO ====================================== */
/* ========================================================================== */

typedef enum {
    DEMO_SPLASH = 0,
    DEMO_JUGADORES_1,       /* seleccion de jugadores, cursor en "1 JUGADOR"   */
    DEMO_JUGADORES_2,       /* seleccion de jugadores, cursor en "2 JUGADORES" */
    DEMO_INICIALES,         /* jugador(es) eligen su nombre de 3 letras        */
    DEMO_MODO_SIMON,        /* seleccion de modo, cursor en "SIMON"            */
    DEMO_MODO_SIMONJOY,     /* seleccion de modo, cursor en "SIM+JOY"          */
    DEMO_MODO_GUITAR,       /* seleccion de modo, cursor en "GT HERO"          */
    DEMO_PREVIEW_SIMON,     /* vista previa del modo Simon Clasico             */
    DEMO_PREVIEW_SIMONJOY,  /* vista previa del modo Simon + Joystick          */
    DEMO_CONTEO_3,
    DEMO_CONTEO_2,
    DEMO_CONTEO_1,
    DEMO_CONTEO_GO,
    DEMO_JUGANDO,           /* modo Guitar Hero (ya construido)                */
    DEMO_RESULTADO,
    DEMO_MENU_CANCIONES,    /* lista de canciones tipo "reproductor"           */
    DEMO_COUNT
} DemoScreen_t;

/* Nombres para el log por consola (ver MX_USART2_UART_Init) -- el orden
 * debe coincidir exactamente con DemoScreen_t. */
static const char *const DEMO_NOMBRE[DEMO_COUNT] = {
    "SPLASH", "JUGADORES_1", "JUGADORES_2", "INICIALES", "MODO_SIMON", "MODO_SIMONJOY",
    "MODO_GUITAR", "PREVIEW_SIMON", "PREVIEW_SIMONJOY", "CONTEO_3", "CONTEO_2",
    "CONTEO_1", "CONTEO_GO", "JUGANDO", "RESULTADO", "MENU_CANCIONES"
};

/* Indice seleccionado en DEMO_MENU_CANCIONES (persiste entre visitas) */
static uint8_t cancion_cursor = 0;

/* Iniciales de 3 letras por jugador (2026-07-31, pedido explicito del
 * usuario) -- se piden en DEMO_INICIALES, entre JUGADORES y MODO. Se
 * muestran en vez del generico "J1"/"J2" en las etiquetas de cada modo (ver
 * Nombre_Jugador(), usada desde renderer.c). */
static char    nombre_jugadores[2][4] = { "AAA", "AAA" };
static uint8_t nombre_jugador_actual  = 0;   /* 0 o 1 -- quien esta escribiendo */
static uint8_t nombre_pos_actual      = 0;   /* 0,1,2 -- que letra             */

/* Mitigacion de sintoma (2026-07-31): "nombre_jugadores[1]" se corrompe a
 * bytes invalidos en algun momento entre DEMO_INICIALES (donde se ve bien)
 * y el juego real de jugador 2 -- se investigo dos veces (razonamiento
 * propio + una exploracion de codigo fresca) sin encontrar el camino de
 * escritura culpable; todo acceso a ese arreglo esta comprobadamente
 * acotado. Sin un watchpoint de hardware (SWD/OpenOCD) no hay forma de
 * seguir por lectura de codigo sola. Mientras tanto, esto evita que la
 * pantalla vuelva a mostrar "???": cualquier byte fuera del rango
 * imprimible que soporta el font (32-90) se reemplaza por '-' letra por
 * letra, en vez de propagar la corrupcion a la pantalla. */
const char *Nombre_Jugador(uint8_t jugador) {
    static char seguro[2][4];
    const char *n = nombre_jugadores[jugador];
    for (uint8_t i = 0; i < 3; i++) {
        char c = n[i];
        seguro[jugador][i] = (c >= 32 && c <= 90) ? c : '-';
    }
    seguro[jugador][3] = '\0';
    return seguro[jugador];
}

/* Paso "encendido" (0-3, o 0xFF=ninguno) para animar las vistas previas de
 * Simon Clasico / Simon+Joystick, cambia cada 500ms mientras se muestran. */
static uint8_t Demo_PasoSimon(void) {
    uint32_t t = (HAL_GetTick() / 500) % 5;
    return (t < 4) ? (uint8_t)t : 0xFF;
}

/* Entra a la pantalla `screen`: prepara el GameState_t de ejemplo y dibuja
 * el primer cuadro. Renderer_Update() se encarga de animar lo que siga. */
static void Demo_Enter(uint8_t screen) {
    printf("[SCREEN] -> %s\r\n", DEMO_NOMBRE[screen]);
    memset(&gs, 0, sizeof(gs));

    /* Simon Clasico usa layout "cocktail" en retrato (240x320); el resto
     * del recorrido sigue en paisaje (320x240) como hasta ahora. */
    ILI9341_SetPortrait(screen == DEMO_PREVIEW_SIMON);

    switch (screen) {
    case DEMO_SPLASH:
        gs.estado = ESTADO_SPLASH;
        Buzzer_Patron(BEEP_BIENVENIDA, (uint8_t)BEEP_BIENVENIDA_N);
        break;

    case DEMO_JUGADORES_1:
    case DEMO_JUGADORES_2:
        Renderer_DrawSeleccionJugadores((uint8_t)(screen - DEMO_JUGADORES_1));
        break;

    case DEMO_INICIALES:
        nombre_jugador_actual = 0;
        nombre_pos_actual     = 0;
        strcpy(nombre_jugadores[0], "AAA");
        strcpy(nombre_jugadores[1], "AAA");
        Renderer_DrawNombre(0, nombre_jugadores[0], 0);
        break;

    case DEMO_MODO_SIMON:
    case DEMO_MODO_SIMONJOY:
    case DEMO_MODO_GUITAR:
        Renderer_DrawSeleccionModo((uint8_t)(screen - DEMO_MODO_SIMON));
        break;

    case DEMO_PREVIEW_SIMON:
        Renderer_DrawModoSimonClasico(0xFF, 0xFF);
        break;

    case DEMO_PREVIEW_SIMONJOY:
        /* misma pantalla de solo-flechas que usa el juego real
         * (Renderer_DrawModoSimonJoystick1P) -- antes se dibujaba con la
         * version de 2 jugadores con texto (ARRIBA/ABAJO/IZQ/DER), que no
         * coincidia con lo que se ve al arrancar la partida de verdad. */
        Renderer_DrawModoSimonJoystick1P(0xFF);
        break;

    case DEMO_CONTEO_3:
    case DEMO_CONTEO_2:
    case DEMO_CONTEO_1:
    case DEMO_CONTEO_GO:
        Renderer_DrawConteo((uint8_t)(DEMO_CONTEO_GO - screen));
        break;

    case DEMO_JUGANDO:
        /* puntaje/combo arrancan en 0 (antes eran numeros de adorno fijos)
         * porque ahora SI se juega de verdad, presionando B1 (ver
         * GuitarHero_IntentarGolpe) -- el click del SW del joystick que se
         * usaba antes para esto ya no existe (retirado 2026-07-30) */
        gs.estado       = ESTADO_JUGANDO;
        gs.nota_speed   = NOTE_SPEED_L2;
        gs.j[0].puntaje = 0;
        gs.j[0].combo   = 0;
        gs.j[1].puntaje = 0;
        gs.j[1].combo   = 0;
        gs.notas[0] = (Nota_t){ .x_rel = -NOTE_W, .carril = 1, .jugador = 0, .activa = 1 };
        gs.notas[1] = (Nota_t){ .x_rel = -NOTE_W, .carril = 2, .jugador = 1, .activa = 1 };
        break;

    case DEMO_RESULTADO:
        gs.estado       = ESTADO_RESULTADO;
        gs.j[0].puntaje = 1250;
        gs.j[0].combo   = 8;
        gs.j[1].puntaje = 980;
        gs.j[1].combo   = 5;
        break;

    case DEMO_MENU_CANCIONES:
        Renderer_DrawListaCanciones(cancion_cursor);
        break;
    }
}

/* ========================================================================== */
/* === GOLPEO DE NOTAS (GUITAR HERO) — B1 COMO BOTON UNICO =================== */
/* ========================================================================== */
/* Implementado (2026-07-27) cuando todavia no habia botones arcade cableados
 * ni un boton de click dedicado a esto: B1 (antes el click del SW del
 * joystick, retirado 2026-07-30, ver comentario de main.c arriba) hace de
 * "boton unico" -- golpea la nota activa mas cercana al centro de la zona de
 * presion, sin importar el carril/jugador (no se puede elegir carril con un
 * solo boton). Solo funciona dentro del recorrido de diseño (DEMO_JUGANDO, 2
 * notas fijas que se respawnean solas) -- el modo real sincronizado con la
 * cancion se quito (2026-07-28) por pedido del usuario para bajar el peso
 * del codigo mientras se enfocaba en sacar adelante joystick y botones
 * arcade; con eso ya listo, el siguiente foco del proyecto es reconstruir
 * este modo por completo (ver carpeta guitar_hero/ANALISIS_REFERENCIA.md
 * para las ideas de diseño ya evaluadas: chart de notas ligado al tiempo
 * real de una melodia en vez del spawn periodico actual). */
static void GuitarHero_IntentarGolpe(void) {
    int16_t pz_centro = (int16_t)(PRESS_ZONE_W / 2);
    int16_t mejor_dist = 0x7FFF;
    int8_t  mejor_i    = -1;

    for (uint8_t i = 0; i < MAX_NOTES; i++) {
        Nota_t *n = &gs.notas[i];
        if (!n->activa) continue;
        int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);
        int16_t dist = (int16_t)((centro_nota > pz_centro) ? (centro_nota - pz_centro) : (pz_centro - centro_nota));
        if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }
    }

    if (mejor_i < 0 || mejor_dist > (int16_t)HIT_OK) {
        printf("[GUITARHERO] MISS (nada en rango)\r\n");
        return;   /* nada cerca: fallo silencioso */
    }

    Nota_t          *n = &gs.notas[mejor_i];
    EstadoJugador_t *j = &gs.j[n->jugador];
    const char      *calidad;

    if      (mejor_dist <= (int16_t)HIT_PERFECT) { j->puntaje = (uint16_t)(j->puntaje + SCORE_PERFECT); calidad = "PERFECT"; }
    else if (mejor_dist <= (int16_t)HIT_GOOD)    { j->puntaje = (uint16_t)(j->puntaje + SCORE_GOOD);    calidad = "GOOD"; }
    else                                          { j->puntaje = (uint16_t)(j->puntaje + SCORE_OK);     calidad = "OK"; }
    j->combo++;
    printf("[GUITARHERO] jugador=%u dist=%d %s puntaje=%u combo=%u\r\n",
           n->jugador, mejor_dist, calidad, j->puntaje, j->combo);

    n->x_rel = -NOTE_W;   /* respawn inmediato para poder seguir probando */
}

/* Detecta el flanco de presion de B1 (activo BAJO, pull-up externo en la
 * Nucleo), con debounce real (BTN_DEBOUNCE_MS) -- B1 es el que saca de un
 * juego real o confirma menus, asi que un falso flanco aca es justo lo que
 * el usuario reporto como "se cambia de pantalla solo, sin tocar nada". */
static uint8_t Boton_B1_Flanco(void) {
    static GPIO_PinState prev        = GPIO_PIN_SET;
    static GPIO_PinState estable      = GPIO_PIN_SET;
    static uint32_t       tick_cambio = 0;
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);

    if (cur != estable) {
        estable     = cur;
        tick_cambio = HAL_GetTick();
        return 0;   /* posible rebote -- todavia no cuenta */
    }
    if (HAL_GetTick() - tick_cambio < BTN_DEBOUNCE_MS) return 0;  /* aun no esta estable */

    uint8_t flanco = (prev == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
    prev = cur;
    return flanco;
}

/* Navegacion del recorrido (menus, lista de canciones, etc): pedido
 * explicito del usuario (2026-07-30) de que mover pantallas ya NO se haga
 * con el joystick, sino presionando cualquiera de los 8 botones arcade --
 * el joystick queda reservado solo para jugar (direccion en JOYS). Revisa
 * los 8 botones (ambos jugadores) por flanco de presion; cualquiera de
 * ellos cuenta como "avanzar" (no hay boton dedicado a "retroceder": con
 * el recorrido siendo ciclico, presionando varias veces se alcanza
 * cualquier pantalla). Debe llamarse SOLO fuera de un juego real (ver el
 * guard en el loop principal) para no robarle el flanco de presion a
 * Botones_ActualizarJugador durante una partida de BOTONES. */
static uint8_t BotonesNavegacion_Presionado(void) {
    uint8_t presionado = 0;
    for (uint8_t p = 0; p < 2; p++) {
        if (Botones_LeerColor(p) != 0xFF) presionado = 1;
    }
    return presionado;
}

/* Gesto de salida "de emergencia": ROJO+AMARILLO del MISMO jugador
 * mantenidos 3s -- pedido explicito del usuario (2026-07-31) como
 * alternativa a B1 para salir de un juego real sin soltar su lado de la
 * mesa. Lee el nivel crudo de los pines (no el flanco de Botones_LeerColor,
 * que solo detecta un color a la vez y se consume solo con el primer
 * flanco) asi que no interfiere con la deteccion de color normal del juego. */
#define COMBO_SALIR_MS 3000U
static uint32_t combo_salir_tick[2] = { 0, 0 };

static uint8_t ComboSalir_Detectado(void) {
    uint8_t disparado = 0;
    for (uint8_t p = 0; p < 2; p++) {
        uint8_t rojo_amarillo = (HAL_GPIO_ReadPin(BTN_SW[p][0].port, BTN_SW[p][0].pin) == GPIO_PIN_RESET) &&
                                 (HAL_GPIO_ReadPin(BTN_SW[p][3].port, BTN_SW[p][3].pin) == GPIO_PIN_RESET);
        if (rojo_amarillo) {
            if (combo_salir_tick[p] == 0) {
                combo_salir_tick[p] = HAL_GetTick();
            } else if (HAL_GetTick() - combo_salir_tick[p] >= COMBO_SALIR_MS) {
                printf("[COMBO] P%u rojo+amarillo 3s -> salir\r\n", p);
                disparado = 1;
                combo_salir_tick[p] = 0;   /* rearma para la proxima vez */
            }
        } else {
            combo_salir_tick[p] = 0;
        }
    }
    return disparado;
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — LOGICA REAL (1 JUGADOR, con el joystick ya cableado) */
/* ========================================================================== */
/* Secuencia tipo "Simon Dice" pero con direcciones del joystick en vez de
 * colores: se muestra la secuencia parpadeando en el D-pad de
 * Renderer_DrawModoSimonJoystick1P(), el jugador la repite inclinando el
 * joystick, y si acierta se le agrega un paso mas (dificultad adaptativa:
 * empieza lenta y se acelera sola, sin niveles fijos). Direcciones:
 * 0=ARRIBA(joystick_y alto) 1=ABAJO(joystick_y bajo) 2=IZQ(joystick_x bajo)
 * 3=DER(joystick_x alto) — si al probar en hardware sale invertido, se
 * ajustan aca los signos, sin tocar el resto de la logica. */

typedef enum {
    SJ_MOSTRANDO,   /* reproduciendo la secuencia (parpadeo on/off)          */
    SJ_ESPERANDO,   /* esperando que el jugador repita paso por paso         */
    SJ_ACIERTO,     /* pausa corta de "bien" antes de mostrar la siguiente   */
    SJ_GAMEOVER     /* fallo: pantalla de resultado, espera mover el stick
                       (reintentar) o B1 (salir) -- ver comentario arriba   */
} SimonJoyFase_t;

#define SJ_MAX_LONGITUD      64
#define SJ_PAUSA_ACIERTO_MS  550U  /* subido de 400 -- el usuario sintio que
    la siguiente ronda empezaba "apenas ganamos", sin dejar descansar */

/* Velocidad como multiplicador: ronda 1 = x0.10 (lento), sube +0.02 por
 * ronda hasta un tope de x0.40 (ronda ~16), ahi se queda. intervalo =
 * REF_MS / velocidad -- a mas multiplicador, menos intervalo (mas rapido).
 * REF_MS=100 da: x0.10->1000ms (inicio), x0.40->250ms (tope, cada mitad
 * on/off dura 125ms -- suficiente para distinguir 2 repeticiones seguidas
 * del mismo lado; los topes anteriores x1.50->67ms y x0.75->133ms
 * resultaron demasiado rapidos para eso). */
#define SJ_VELOCIDAD_INICIAL  0.10f
#define SJ_VELOCIDAD_PASO     0.02f
#define SJ_VELOCIDAD_MAX      0.40f
#define SJ_INTERVALO_REF_MS   100U

static uint8_t        sj_secuencia[SJ_MAX_LONGITUD];
static uint8_t        sj_longitud;       /* pasos en la secuencia actual      */
static uint8_t        sj_paso_mostrar;   /* indice que se esta parpadeando    */
static uint8_t        sj_paso_esperado;  /* indice que se espera del jugador  */
static uint8_t        sj_mostrando_on;   /* sub-fase del parpadeo (on/off)    */
static uint8_t        sj_paso_dibujado = 0xFF; /* ultimo paso pintado en el D-pad
    (0xFF=ninguno) -- para actualizar solo el boton que cambio en vez de
    redibujar toda la pantalla (evita el parpadeo de un FillScreen completo) */
static uint8_t        sj_mejor_racha;    /* mejor racha de esta sesion        */
static SimonJoyFase_t sj_fase;
static uint32_t       sj_tick_fase;
static uint32_t       sj_seed = 1;
static uint8_t        en_juego_real = 0; /* 1 = SimonJoy real, no el recorrido*/
static uint8_t         jugadores_seleccionados = 1; /* 1 o 2, elegido en el menu */
static uint8_t         joy_listo_dir = 1;

/* conteo regresivo automatico (3,2,1,GO) al confirmar un modo jugable desde
 * el menu real -- a diferencia del recorrido de diseño (que espera B1 para
 * cada pantalla), aca cada numero avanza solo, como en un juego de verdad.
 * modo_confirmado: que arrancar al llegar a GO (BOTONES/SIMONJOY/GUITAR) --
 * antes era un booleano (modo_confirmado_es_joys) que solo distinguia 2
 * modos; se volvio un tri-estado (2026-07-31) al reactivar Guitar Hero como
 * tercer modo real seleccionable desde el menu (ver GuitarHero2_IniciarAmbos/
 * GuitarHero_IniciarSolo mas abajo). */
typedef enum { MODO_SEL_BOTONES = 0, MODO_SEL_SIMONJOY, MODO_SEL_GUITAR } ModoSeleccion_t;
static ModoSeleccion_t modo_confirmado = MODO_SEL_BOTONES;
static uint8_t  conteo_auto = 0;
static uint32_t conteo_tick = 0;
#define CONTEO_PASO_MS 700U

/* generador pseudoaleatorio simple (LCG), suficiente para elegir 1 de 4 direcciones */
static uint8_t SJ_Random4(void) {
    sj_seed = sj_seed * 1103515245u + 12345u;
    return (uint8_t)((sj_seed >> 16) & 0x3u);
}

/* siguiente direccion de la secuencia, sin permitir una 3ra repeticion
 * seguida (verificado: el LCG de arriba solo puede repetir 3-6 veces
 * seguidas cada ~20 tiradas, se siente injusto en un juego de memoria).
 * Si las 2 anteriores ya son iguales entre si, se fuerza que la nueva sea
 * distinta -- eligiendo uniforme entre las otras 3 direcciones. */
static uint8_t SJ_SiguienteDireccion(void) {
    uint8_t nuevo = SJ_Random4();
    if (sj_longitud >= 2 &&
        sj_secuencia[sj_longitud - 1] == sj_secuencia[sj_longitud - 2] &&
        nuevo == sj_secuencia[sj_longitud - 1]) {
        nuevo = (uint8_t)((nuevo + 1u + (SJ_Random4() % 3u)) & 0x3u);
    }
    return nuevo;
}

static uint16_t SimonJoy_IntervaloActual(void) {
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj_longitud - 1) * SJ_VELOCIDAD_PASO;
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);
}

/* direccion del joystick por flanco: hay que salir de la zona de disparo en
 * AMBOS ejes antes de que cuente el siguiente movimiento -- pero SIN exigir
 * el centro exacto (1700-2400), porque un eje con un corrimiento normal de
 * fabrica puede no volver nunca a esa banda tan angosta y deja el juego
 * "pegado" despues del primer acierto (bug real reportado en hardware). */
static uint8_t Joystick_LeerDireccion(void) {
    uint16_t bajo_y, alto_y, bajo_x, alto_x;
    Joy_Umbrales(centro_j1y, &bajo_y, &alto_y);
    Joy_Umbrales(centro_j1x, &bajo_x, &alto_x);

    if (!joy_listo_dir) {
        if (joystick_y < alto_y && joystick_y > bajo_y &&
            joystick_x < alto_x && joystick_x > bajo_x) {
            joy_listo_dir = 1;
        }
        return 0xFF;
    }
    /* EJES CRUZADOS (2026-07-31, confirmado contra examen_parcial, que usa
     * este mismo modulo de joystick fisico -- ver su comentario en
     * RTC_AjustarConJoystick/OLED_ActualizarMonitor): lo que se SIENTE como
     * arriba/abajo es electricamente joystick_x, y lo que se siente como
     * izquierda/derecha es joystick_y -- al reves de como estaban leídos
     * los canales aca (el nombre de la variable "joystick_y"/"_x" viene del
     * pin VRy/VRx del modulo, no de la sensacion fisica de movimiento). */
    /* Elegir el eje DOMINANTE, no el primero de la cadena (2026-07-31,
     * confirmado en hardware real): un empuje impreciso hacia un lado corre
     * un poco el otro eje tambien -- si ese corrimiento chico alcanza a
     * cruzar SU umbral, con una cadena de if fija siempre ganaba el mismo
     * eje sin importar cual desvio era el mas fuerte de verdad (ej.
     * "derecha" se leia como "abajo"). Ahora gana el eje con mayor
     * desviacion absoluta del centro. */
    int32_t dev_vert  = (int32_t)joystick_x - (int32_t)centro_j1x;  /* + = ABAJO  */
    int32_t dev_horiz = (int32_t)joystick_y - (int32_t)centro_j1y;  /* + = DERECHA */
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;

    uint8_t dir = 0xFF;
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {
        dir = (dev_vert > 0) ? 1 : 0;    /* ABAJO : ARRIBA */
        joy_listo_dir = 0;
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {
        dir = (dev_horiz > 0) ? 3 : 2;   /* DER : IZQ */
        joy_listo_dir = 0;
    }
    if (dir != 0xFF) printf("[INPUT] joystick J1 = %s\r\n", DIR_NOMBRE[dir]);
    return dir;
}

static void SimonJoy_DibujarGameOver(void) {
    char linea[32];
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_DrawString(70, 80, "GAME OVER", COLOR_RED, COLOR_BLACK, 3);
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)(sj_longitud - 1));
    ILI9341_DrawString(100, 140, linea, COLOR_WHITE, COLOR_BLACK, 2);
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)sj_mejor_racha);
    ILI9341_DrawString(100, 165, linea, COLOR_YELLOW, COLOR_BLACK, 2);
    ILI9341_DrawString(35, 210, "mueve=reintentar  B1=salir", COLOR_GRAY, COLOR_BLACK, 1);
}

/* Numero de ronda en vivo, arriba a la derecha del encabezado -- antes solo
 * se veia la racha al perder ("GAME OVER"), pedido explicito del usuario
 * verla mientras juega. Redibuja solo esa esquina, no todo el encabezado. */
static void SimonJoy_MostrarRacha(uint8_t racha) {
    char buf[12];
    snprintf(buf, sizeof(buf), "RONDA:%2u", racha);
    ILI9341_FillRect(LCD_W - 80, 0, 80, 26, COLOR_DARKGRAY);
    ILI9341_DrawString(LCD_W - 74, 9, buf, COLOR_YELLOW, COLOR_DARKGRAY, 1);
}

static void SimonJoy_Iniciar(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    sj_seed ^= HAL_GetTick();
    if (sj_seed == 0) sj_seed = 1;

    sj_longitud        = 1;
    sj_secuencia[0]     = SJ_Random4();
    sj_paso_mostrar     = 0;
    sj_mostrando_on     = 1;
    sj_fase             = SJ_MOSTRANDO;
    sj_tick_fase        = HAL_GetTick();
    joy_listo_dir       = 1;

    /* pantalla completa para 1 jugador (sin dividir), girada 180 hacia el
     * lado de "jugador 2" -- ya en landscape (heredado del recorrido de
     * menus), asi que solo hace falta el flip, sin tocar SetPortrait. */
    ILI9341_SetFlip180(1);
    Renderer_DrawModoSimonJoystick1P(sj_secuencia[0]);
    sj_paso_dibujado    = sj_secuencia[0];
    Renderer_ResetCursorJoystick();
    SimonJoy_MostrarRacha(sj_longitud);
}

/* Cambia cual boton del D-pad esta encendido (0xFF=ninguno) -- solo llama a
 * la actualizacion incremental (sin FillScreen) y solo si de verdad cambio,
 * para no repintar cuando el joystick se mantiene quieto en la misma
 * direccion (evita el parpadeo que reportó el usuario). */
static void SimonJoy_MostrarPaso(uint8_t nuevo) {
    if (nuevo == sj_paso_dibujado) return;
    Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, nuevo);
    sj_paso_dibujado = nuevo;
    if (nuevo < 4) Buzzer_Beep(90);  /* beep corto cada vez que se prende una flecha */
}

/* Confirmacion visual de una entrada del jugador durante SJ_ESPERANDO, con
 * un apagado-y-prendido REAL (no instantaneo) -- bug real reportado: si dos
 * entradas seguidas eran la misma direccion (ej. DERECHA DERECHA), la
 * flecha se quedaba encendida sin parpadear entre una y otra (MostrarPaso
 * no redibuja si el color no cambia), pareciendo que la 2da no se habia
 * registrado. Apaga ya mismo y programa el prendido SJ_FLASH_INPUT_MS
 * despues (revisado en SimonJoy_Actualizar), asi el apagon sí se alcanza a
 * ver aunque la direccion se repita. */
#define SJ_FLASH_INPUT_MS 70U
static uint8_t  sj_flash_pendiente = 0xFF;  /* 0xFF = nada pendiente */
static uint32_t sj_flash_tick      = 0;

static void SimonJoy_ConfirmarInput(uint8_t dir) {
    if (sj_paso_dibujado != 0xFF) {
        Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, 0xFF);
        sj_paso_dibujado = 0xFF;
    }
    sj_flash_pendiente = dir;
    sj_flash_tick      = HAL_GetTick();
    Buzzer_Beep(90);
}


/* tick no bloqueante del minijuego: se llama una vez por vuelta del loop
 * principal mientras en_juego_real este activo. Nunca usa HAL_Delay largo,
 * asi que el filtro/deadzone del joystick sigue actualizandose por interrupcion. */
static void SimonJoy_Actualizar(void) {
    uint32_t ahora = HAL_GetTick();

    if (sj_fase != SJ_GAMEOVER) {
        Renderer_ActualizarCursorJoystick(joystick_x, joystick_y, joy_listo_dir);
    }

    /* completa el parpadeo de SimonJoy_ConfirmarInput -- prende la flecha
     * ya pasado el tiempo minimo de "apagada" */
    if (sj_flash_pendiente != 0xFF && (ahora - sj_flash_tick) >= SJ_FLASH_INPUT_MS) {
        SimonJoy_MostrarPaso(sj_flash_pendiente);
        sj_flash_pendiente = 0xFF;
    }

    switch (sj_fase) {
    case SJ_MOSTRANDO: {
        uint16_t medio = (uint16_t)(SimonJoy_IntervaloActual() / 2U);
        if ((ahora - sj_tick_fase) < medio) break;
        sj_tick_fase = ahora;

        if (sj_mostrando_on) {
            /* mitad "apagada" del parpadeo, mismo paso */
            SimonJoy_MostrarPaso(0xFF);
            sj_mostrando_on = 0;
        } else {
            sj_paso_mostrar++;
            if (sj_paso_mostrar >= sj_longitud) {
                sj_fase          = SJ_ESPERANDO;
                sj_paso_esperado = 0;
                joy_listo_dir    = 1;
            } else {
                SimonJoy_MostrarPaso(sj_secuencia[sj_paso_mostrar]);
                sj_mostrando_on = 1;
            }
        }
        break;
    }

    case SJ_ESPERANDO: {
        uint8_t dir = Joystick_LeerDireccion();
        if (dir == 0xFF) break;

        SimonJoy_ConfirmarInput(dir);  /* parpadeo real, aunque se repita la misma direccion */
        printf("[SIMONJOY 1P] dir=%u esperado=%u %s\r\n", dir, sj_secuencia[sj_paso_esperado],
               (dir == sj_secuencia[sj_paso_esperado]) ? "OK" : "FALLO");

        if (dir != sj_secuencia[sj_paso_esperado]) {
            if ((uint8_t)(sj_longitud - 1) > sj_mejor_racha) sj_mejor_racha = (uint8_t)(sj_longitud - 1);
            printf("[SIMONJOY 1P] GAME OVER racha=%u mejor=%u\r\n", (unsigned)(sj_longitud - 1), sj_mejor_racha);
            sj_fase      = SJ_GAMEOVER;
            sj_tick_fase = ahora;
            sj_flash_pendiente = 0xFF;  /* cancela: game over dibuja otra pantalla encima */
            Buzzer_Beep(350);  /* beep largo de error (extiende el corto que ya sonaba) */
            SimonJoy_DibujarGameOver();
            break;
        }

        sj_paso_esperado++;
        if (sj_paso_esperado >= sj_longitud) {
            if (sj_longitud > sj_mejor_racha) sj_mejor_racha = sj_longitud;
            if (sj_longitud < SJ_MAX_LONGITUD) {
                sj_secuencia[sj_longitud] = SJ_SiguienteDireccion();
                sj_longitud++;
                SimonJoy_MostrarRacha(sj_longitud);
            }
            sj_fase      = SJ_ACIERTO;
            sj_tick_fase = ahora;
            sj_flash_pendiente = 0xFF;   /* cancela cualquier prendido pendiente */
            SimonJoy_MostrarPaso(0xFF);  /* apaga el ultimo boton -- si no, se
                queda encendido durante la pausa y el primer paso de la
                siguiente repeticion no se ve como un flanco nuevo */
            {
                static const PasoSonido_t BEEP_RONDA[3] = {
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };  /* 2 pitidos cortos */
                Buzzer_Patron(BEEP_RONDA, 3);
            }
        }
        break;
    }

    case SJ_ACIERTO:
        if ((ahora - sj_tick_fase) < SJ_PAUSA_ACIERTO_MS) break;
        sj_paso_mostrar = 0;
        sj_mostrando_on = 1;
        sj_tick_fase    = ahora;
        sj_fase         = SJ_MOSTRANDO;
        SimonJoy_MostrarPaso(sj_secuencia[0]);
        break;

    case SJ_GAMEOVER:
        /* sin SW fisico (retirado 2026-07-30): mover el stick a cualquier
         * direccion reintenta (mismo patron que "cualquier boton" en
         * BOTONES); B1 (avanzar) sigue saliendo, manejado en el loop
         * principal */
        if (Joystick_LeerDireccion() != 0xFF) { printf("[SIMONJOY 1P] retry\r\n"); SimonJoy_Iniciar(); }
        break;
    }
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — 2 JUGADORES CARA A CARA (2026-07-29) =============== */
/* ========================================================================== */
/* Misma mecanica que la version 1 jugador de arriba, pero duplicada por
 * jugador (arrays[2], indice 0=J1 abajo/normal, 1=J2 arriba/rotado 180) --
 * cada uno juega su PROPIA secuencia independiente en su propia mitad de la
 * pantalla en retrato (ver Renderer_DrawModoSimonJoystick2P/Cockpit_* en
 * renderer.c). Se activa al confirmar "JOYS" con "2 JUGADORES" seleccionado.
 *
 * OJO cableado: la direccion arriba/abajo del joystick 1 salio invertida en
 * el montaje real (ver Joystick_LeerDireccion). El joystick 2 puede salir
 * invertido DIFERENTE -- ademas de su propio montaje, esta fisicamente al
 * otro lado de la mesa, asi que "empujar hacia el jugador 2" bien podria
 * leerse al reves de como lo hace el joystick 1. Probar en hardware real y
 * ajustar los signos de Joystick2_LeerDireccion si hace falta. */

static uint8_t        sj2_secuencia[2][SJ_MAX_LONGITUD];
static uint8_t        sj2_longitud[2];
static uint8_t        sj2_paso_mostrar[2];
static uint8_t        sj2_paso_esperado[2];
static uint8_t        sj2_mostrando_on[2];
static uint8_t        sj2_paso_dibujado[2] = { 0xFF, 0xFF };
static uint8_t        sj2_mejor_racha[2];
static SimonJoyFase_t sj2_fase[2];
static uint32_t       sj2_tick_fase[2];
static uint32_t       sj2_seed[2] = { 1, 7 };   /* semillas distintas -- secuencias distintas entre jugadores */
static uint8_t        sj2_joy_listo_dir[2];
static uint8_t        sj2_flash_pendiente[2] = { 0xFF, 0xFF };
static uint32_t       sj2_flash_tick[2];
static uint8_t        en_juego_real_2p = 0;   /* 1 = modo 2 jugadores real, no el recorrido */

static uint8_t SJ2_Random4(uint8_t p) {
    sj2_seed[p] = sj2_seed[p] * 1103515245u + 12345u;
    return (uint8_t)((sj2_seed[p] >> 16) & 0x3u);
}

/* mismo anti-repeticion que SJ_SiguienteDireccion, por jugador */
static uint8_t SJ2_SiguienteDireccion(uint8_t p) {
    uint8_t nuevo = SJ2_Random4(p);
    if (sj2_longitud[p] >= 2 &&
        sj2_secuencia[p][sj2_longitud[p] - 1] == sj2_secuencia[p][sj2_longitud[p] - 2] &&
        nuevo == sj2_secuencia[p][sj2_longitud[p] - 1]) {
        nuevo = (uint8_t)((nuevo + 1u + (SJ2_Random4(p) % 3u)) & 0x3u);
    }
    return nuevo;
}

static uint16_t SimonJoy2_IntervaloActual(uint8_t p) {
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj2_longitud[p] - 1) * SJ_VELOCIDAD_PASO;
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);
}

/* p=0 -> joystick 1 (pa1/pa4), p=1 -> joystick 2 (pc0/pc1) -- misma logica de
 * flanco por zona de disparo que Joystick_LeerDireccion */
static uint8_t Joystick2_LeerDireccion(uint8_t p) {
    uint16_t jx = (p == 0) ? joystick_x : joystick2_x;
    uint16_t jy = (p == 0) ? joystick_y : joystick2_y;
    uint16_t bajo_y, alto_y, bajo_x, alto_x;
    Joy_Umbrales((p == 0) ? centro_j1y : centro_j2y, &bajo_y, &alto_y);
    Joy_Umbrales((p == 0) ? centro_j1x : centro_j2x, &bajo_x, &alto_x);

    if (!sj2_joy_listo_dir[p]) {
        if (jy < alto_y && jy > bajo_y &&
            jx < alto_x && jx > bajo_x) {
            sj2_joy_listo_dir[p] = 1;
        }
        return 0xFF;
    }
    /* EJES CRUZADOS -- ver comentario en Joystick_LeerDireccion arriba,
     * mismo modulo/pines, mismo cruce electrico X<->Y. Eje DOMINANTE (no el
     * primero de la cadena) -- mismo motivo que alla: un empuje impreciso
     * puede cruzar el umbral de los 2 ejes, y debe ganar el que este mas
     * lejos de su centro de verdad. */
    uint16_t centro_x = (p == 0) ? centro_j1x : centro_j2x;
    uint16_t centro_y = (p == 0) ? centro_j1y : centro_j2y;
    int32_t dev_vert  = (int32_t)jx - (int32_t)centro_x;  /* + = ABAJO  */
    int32_t dev_horiz = (int32_t)jy - (int32_t)centro_y;  /* + = DERECHA */
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;

    uint8_t dir = 0xFF;
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {
        dir = (dev_vert > 0) ? 1 : 0;    /* ABAJO : ARRIBA */
        sj2_joy_listo_dir[p] = 0;
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {
        dir = (dev_horiz > 0) ? 3 : 2;   /* DER : IZQ */
        sj2_joy_listo_dir[p] = 0;
    }
    if (dir != 0xFF) printf("[INPUT] joystick J%u = %s\r\n", (unsigned)(p + 1), DIR_NOMBRE[dir]);
    return dir;
}

static void SimonJoy2_MostrarPaso(uint8_t p, uint8_t nuevo) {
    if (nuevo == sj2_paso_dibujado[p]) return;
    Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], nuevo);
    sj2_paso_dibujado[p] = nuevo;
    if (nuevo < 4) Buzzer_Beep(90);
}

static void SimonJoy2_ConfirmarInput(uint8_t p, uint8_t dir) {
    if (sj2_paso_dibujado[p] != 0xFF) {
        Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], 0xFF);
        sj2_paso_dibujado[p] = 0xFF;
    }
    sj2_flash_pendiente[p] = dir;
    sj2_flash_tick[p]      = HAL_GetTick();
    Buzzer_Beep(90);
}

/* arranca (o reinicia) SOLO el jugador p -- no toca la pantalla del otro */
static void SimonJoy2_ReiniciarJugador(uint8_t p) {
    sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 3u);
    if (sj2_seed[p] == 0) sj2_seed[p] = 1;

    sj2_longitud[p]        = 1;
    sj2_secuencia[p][0]    = SJ2_Random4(p);
    sj2_paso_mostrar[p]    = 0;
    sj2_mostrando_on[p]    = 1;
    sj2_fase[p]            = SJ_MOSTRANDO;
    sj2_tick_fase[p]       = HAL_GetTick();
    sj2_joy_listo_dir[p]   = 1;
    sj2_flash_pendiente[p] = 0xFF;

    Renderer_DrawModoSimonJoystick2PJugador(p, sj2_secuencia[p][0]);
    sj2_paso_dibujado[p] = sj2_secuencia[p][0];
    Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);
    Renderer_ResetCursorJoystick2P(p);
}

/* dibuja ambas mitades desde cero (arranque de una partida nueva) */
static void SimonJoy2_IniciarAmbos(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    for (uint8_t p = 0; p < 2; p++) {
        sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 1u);
        if (sj2_seed[p] == 0) sj2_seed[p] = 1;

        sj2_longitud[p]        = 1;
        sj2_secuencia[p][0]    = SJ2_Random4(p);
        sj2_paso_mostrar[p]    = 0;
        sj2_mostrando_on[p]    = 1;
        sj2_fase[p]            = SJ_MOSTRANDO;
        sj2_tick_fase[p]       = HAL_GetTick();
        sj2_joy_listo_dir[p]   = 1;
        sj2_flash_pendiente[p] = 0xFF;
        sj2_paso_dibujado[p]   = sj2_secuencia[p][0];
    }
    Renderer_DrawModoSimonJoystick2P(sj2_secuencia[0][0], sj2_secuencia[1][0]);
    Renderer_ActualizarRachaJoystick2P(0, sj2_longitud[0]);
    Renderer_ActualizarRachaJoystick2P(1, sj2_longitud[1]);
    Renderer_ResetCursorJoystick2P(0);
    Renderer_ResetCursorJoystick2P(1);
}

/* tick no bloqueante de UN jugador -- se llama 2 veces por vuelta del loop
 * principal (una por jugador) mientras en_juego_real_2p este activo. */
static void SimonJoy2_ActualizarJugador(uint8_t p) {
    uint32_t ahora = HAL_GetTick();

    if (sj2_fase[p] != SJ_GAMEOVER) {
        uint16_t jx = (p == 0) ? joystick_x : joystick2_x;
        uint16_t jy = (p == 0) ? joystick_y : joystick2_y;
        Renderer_ActualizarCursorJoystick2P(p, jx, jy, sj2_joy_listo_dir[p]);
    }

    if (sj2_flash_pendiente[p] != 0xFF && (ahora - sj2_flash_tick[p]) >= SJ_FLASH_INPUT_MS) {
        SimonJoy2_MostrarPaso(p, sj2_flash_pendiente[p]);
        sj2_flash_pendiente[p] = 0xFF;
    }

    switch (sj2_fase[p]) {
    case SJ_MOSTRANDO: {
        uint16_t medio = (uint16_t)(SimonJoy2_IntervaloActual(p) / 2U);
        if ((ahora - sj2_tick_fase[p]) < medio) break;
        sj2_tick_fase[p] = ahora;

        if (sj2_mostrando_on[p]) {
            SimonJoy2_MostrarPaso(p, 0xFF);
            sj2_mostrando_on[p] = 0;
        } else {
            sj2_paso_mostrar[p]++;
            if (sj2_paso_mostrar[p] >= sj2_longitud[p]) {
                sj2_fase[p]          = SJ_ESPERANDO;
                sj2_paso_esperado[p] = 0;
                sj2_joy_listo_dir[p] = 1;
            } else {
                SimonJoy2_MostrarPaso(p, sj2_secuencia[p][sj2_paso_mostrar[p]]);
                sj2_mostrando_on[p] = 1;
            }
        }
        break;
    }

    case SJ_ESPERANDO: {
        uint8_t dir = Joystick2_LeerDireccion(p);
        if (dir == 0xFF) break;

        SimonJoy2_ConfirmarInput(p, dir);
        printf("[SIMONJOY 2P] P%u dir=%u esperado=%u %s\r\n", p, dir, sj2_secuencia[p][sj2_paso_esperado[p]],
               (dir == sj2_secuencia[p][sj2_paso_esperado[p]]) ? "OK" : "FALLO");

        if (dir != sj2_secuencia[p][sj2_paso_esperado[p]]) {
            if ((uint8_t)(sj2_longitud[p] - 1) > sj2_mejor_racha[p]) sj2_mejor_racha[p] = (uint8_t)(sj2_longitud[p] - 1);
            printf("[SIMONJOY 2P] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);
            sj2_fase[p]            = SJ_GAMEOVER;
            sj2_tick_fase[p]       = ahora;
            sj2_flash_pendiente[p] = 0xFF;
            Buzzer_Beep(350);
            Renderer_DibujarGameOverJoystick2P(p, (uint16_t)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);
            break;
        }

        sj2_paso_esperado[p]++;
        if (sj2_paso_esperado[p] >= sj2_longitud[p]) {
            if (sj2_longitud[p] > sj2_mejor_racha[p]) sj2_mejor_racha[p] = sj2_longitud[p];
            if (sj2_longitud[p] < SJ_MAX_LONGITUD) {
                sj2_secuencia[p][sj2_longitud[p]] = SJ2_SiguienteDireccion(p);
                sj2_longitud[p]++;
                Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);
            }
            sj2_fase[p]            = SJ_ACIERTO;
            sj2_tick_fase[p]       = ahora;
            sj2_flash_pendiente[p] = 0xFF;
            SimonJoy2_MostrarPaso(p, 0xFF);
            {
                static const PasoSonido_t BEEP_RONDA[3] = {
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };
                Buzzer_Patron(BEEP_RONDA, 3);
            }
        }
        break;
    }

    case SJ_ACIERTO:
        if ((ahora - sj2_tick_fase[p]) < SJ_PAUSA_ACIERTO_MS) break;
        sj2_paso_mostrar[p] = 0;
        sj2_mostrando_on[p] = 1;
        sj2_tick_fase[p]    = ahora;
        sj2_fase[p]         = SJ_MOSTRANDO;
        SimonJoy2_MostrarPaso(p, sj2_secuencia[p][0]);
        break;

    case SJ_GAMEOVER:
        /* sin SW fisico (retirado 2026-07-30): mover SU PROPIO stick a
         * cualquier direccion reintenta SOLO ese jugador; B1 sigue
         * saliendo de ambos lados (manejado en el loop principal) */
        if (Joystick2_LeerDireccion(p) != 0xFF) { printf("[SIMONJOY 2P] P%u retry\r\n", p); SimonJoy2_ReiniciarJugador(p); }
        break;
    }
}

static void SimonJoy2_Actualizar(void) {
    SimonJoy2_ActualizarJugador(0);
    SimonJoy2_ActualizarJugador(1);
}

/* ========================================================================== */
/* === BOTONES ARCADE — 2 JUGADORES, CADA UNO CON SUS 4 BOTONES (2026-07-29) = */
/* ========================================================================== */
/* Misma mecanica y misma cara-a-cara (portrait, cockpit) que Simon+Joystick
 * 2P de arriba, pero el "color" lo entrega un boton fisico real (con su
 * propio LED, ver Boton_LED/Botones_LeerColor) en vez del joystick. Cada
 * jugador juega su PROPIA secuencia independiente en su propia mitad. */

static uint8_t        btn_secuencia[2][SJ_MAX_LONGITUD];
static uint8_t        btn_longitud[2];
static uint8_t        btn_paso_mostrar[2];
static uint8_t        btn_paso_esperado[2];
static uint8_t        btn_mostrando_on[2];
static uint8_t        btn_paso_dibujado[2] = { 0xFF, 0xFF };
static uint8_t        btn_mejor_racha[2];
static SimonJoyFase_t btn_fase[2];
static uint32_t       btn_tick_fase[2];
static uint32_t       btn_seed[2] = { 3, 11 };   /* semillas distintas de las de SJ2 */
static uint8_t        btn_flash_pendiente[2] = { 0xFF, 0xFF };
static uint32_t       btn_flash_tick[2];
static uint8_t        en_juego_real_botones = 0;   /* 1 = modo BOTONES real (1 o 2 jugadores) */
static uint8_t        btn_modo_1p = 0;              /* 1 = solo jugador 1 activo (el otro lado queda en blanco) */
static uint8_t        en_juego_real_guitar = 0;    /* 1 = modo GUITAR HERO real (1 o 2 jugadores) */
static uint8_t        guitar_modo_1p = 0;           /* 1 = solo jugador 1 activo (el otro lado queda en blanco) */

/* El joystick frena solo el ritmo de entrada: Joystick_LeerDireccion() exige
 * salir de la zona muerta en los 2 ejes antes de contar el siguiente
 * movimiento, asi que el jugador NO puede encadenar aciertos mas rapido que
 * lo que tarda en volver el stick al centro. El boton arcade no tiene ese
 * freno fisico (soltar+tocar es casi instantaneo), asi que en la practica se
 * completaban rondas mucho mas rapido y se llegaba antes a longitudes altas
 * (donde Botones_IntervaloActual ya esta en el tope de velocidad) -- pedido
 * del usuario (2026-07-30) de que el ritmo se sienta igual que en Joystick.
 * BTN_REARME_MIN_MS es un cooldown minimo entre 2 entradas aceptadas por
 * jugador, para emular ese mismo freno de forma artificial. */
#define BTN_REARME_MIN_MS 250U
static uint32_t       btn_ultimo_input_tick[2];

/* Cooldown minimo en GAME OVER antes de aceptar el reintento (2026-07-31,
 * pedido explicito del usuario: "dejalo que se refresque un poco") -- sin
 * esto, un boton que rebota justo al perder (o el mismo dedo todavia
 * apoyado) podia reiniciar la ronda antes de que se alcance a ver la
 * pantalla de GAME OVER. */
#define BTN_GAMEOVER_COOLDOWN_MS 1000U

static uint8_t Botones_Random4(uint8_t p) {
    btn_seed[p] = btn_seed[p] * 1103515245u + 12345u;
    return (uint8_t)((btn_seed[p] >> 16) & 0x3u);
}

static uint8_t Botones_SiguienteColor(uint8_t p) {
    uint8_t nuevo = Botones_Random4(p);
    if (btn_longitud[p] >= 2 &&
        btn_secuencia[p][btn_longitud[p] - 1] == btn_secuencia[p][btn_longitud[p] - 2] &&
        nuevo == btn_secuencia[p][btn_longitud[p] - 1]) {
        nuevo = (uint8_t)((nuevo + 1u + (Botones_Random4(p) % 3u)) & 0x3u);
    }
    return nuevo;
}

static uint16_t Botones_IntervaloActual(uint8_t p) {
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(btn_longitud[p] - 1) * SJ_VELOCIDAD_PASO;
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);
}

static void Botones_MostrarColor(uint8_t p, uint8_t nuevo) {
    if (nuevo == btn_paso_dibujado[p]) return;
    if (btn_paso_dibujado[p] < 4) Boton_LED(p, btn_paso_dibujado[p], 0);
    if (nuevo < 4) Boton_LED(p, nuevo, 1);
    Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], nuevo);
    btn_paso_dibujado[p] = nuevo;
    if (nuevo < 4) Buzzer_Beep(90);
}

/* apaga-y-prende real (no instantaneo) para que 2 presiones seguidas del
 * mismo color se vean como 2 flancos distintos -- mismo bug/fix que
 * SimonJoy2_ConfirmarInput, aca aplicado al LED fisico ademas de la pantalla */
static void Botones_ConfirmarInput(uint8_t p, uint8_t color) {
    if (btn_paso_dibujado[p] != 0xFF) {
        Boton_LED(p, btn_paso_dibujado[p], 0);
        Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], 0xFF);
        btn_paso_dibujado[p] = 0xFF;
    }
    btn_flash_pendiente[p] = color;
    btn_flash_tick[p]      = HAL_GetTick();
    Buzzer_Beep(90);
}

static void Botones_ReiniciarJugador(uint8_t p) {
    btn_seed[p] ^= (HAL_GetTick() + p * 977u + 5u);
    if (btn_seed[p] == 0) btn_seed[p] = 1;

    btn_longitud[p]        = 1;
    btn_secuencia[p][0]    = Botones_Random4(p);
    btn_paso_mostrar[p]    = 0;
    btn_mostrando_on[p]    = 1;
    btn_fase[p]            = SJ_MOSTRANDO;
    btn_tick_fase[p]       = HAL_GetTick();
    btn_flash_pendiente[p] = 0xFF;
    btn_ultimo_input_tick[p] = HAL_GetTick();

    Renderer_DrawModoSimonClasicoJugador(p, btn_secuencia[p][0]);
    Boton_LED(p, btn_secuencia[p][0], 1);
    btn_paso_dibujado[p] = btn_secuencia[p][0];
    Renderer_ActualizarRachaBotones(p, btn_longitud[p]);
}

static void Botones_IniciarAmbos(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    for (uint8_t p = 0; p < 2; p++) {
        btn_seed[p] ^= (HAL_GetTick() + p * 977u + 2u);
        if (btn_seed[p] == 0) btn_seed[p] = 1;

        btn_longitud[p]        = 1;
        btn_secuencia[p][0]    = Botones_Random4(p);
        btn_paso_mostrar[p]    = 0;
        btn_mostrando_on[p]    = 1;
        btn_fase[p]            = SJ_MOSTRANDO;
        btn_tick_fase[p]       = HAL_GetTick();
        btn_flash_pendiente[p] = 0xFF;
        btn_paso_dibujado[p]   = 0xFF;
        btn_ultimo_input_tick[p] = HAL_GetTick();
        for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
    }
    Renderer_DrawModoSimonClasico(btn_secuencia[0][0], btn_secuencia[1][0]);
    Boton_LED(0, btn_secuencia[0][0], 1);
    Boton_LED(1, btn_secuencia[1][0], 1);
    btn_paso_dibujado[0] = btn_secuencia[0][0];
    btn_paso_dibujado[1] = btn_secuencia[1][0];
    Renderer_ActualizarRachaBotones(0, btn_longitud[0]);
    Renderer_ActualizarRachaBotones(1, btn_longitud[1]);
}

/* Arranca BOTONES a 1 solo jugador (jugador 0) -- el lado del jugador 2 se
 * deja en negro, apagado, y Botones_Actualizar() nunca llama a
 * Botones_ActualizarJugador(1) mientras btn_modo_1p este activo (ver mas
 * abajo). Reutiliza Botones_ReiniciarJugador(0), que ya hacia exactamente
 * esto para el reintento tras un game over -- solo faltaba un punto de
 * entrada para usarla como arranque inicial de una partida nueva. */
static void Botones_IniciarSolo(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    /* SIN flip de hardware (2026-07-31, corregido tras probar en hardware
     * real): el swap de Cockpit_FillRect/DrawString/Punto (que arreglo el
     * cruce de J1/J2 en el modo 2 jugadores) ya deja el jugador 0 orientado
     * hacia el lado contrario -- sumarle ademas el flip de hardware giraba
     * la pantalla otros 180 de mas. */
    ILI9341_FillScreen(COLOR_BLACK);
    for (uint8_t c = 0; c < 4; c++) { Boton_LED(0, c, 0); Boton_LED(1, c, 0); }
    Botones_ReiniciarJugador(0);
}

static void Botones_ActualizarJugador(uint8_t p) {
    uint32_t ahora = HAL_GetTick();

    if (btn_flash_pendiente[p] != 0xFF && (ahora - btn_flash_tick[p]) >= SJ_FLASH_INPUT_MS) {
        Botones_MostrarColor(p, btn_flash_pendiente[p]);
        btn_flash_pendiente[p] = 0xFF;
    }

    switch (btn_fase[p]) {
    case SJ_MOSTRANDO: {
        uint16_t medio = (uint16_t)(Botones_IntervaloActual(p) / 2U);
        if ((ahora - btn_tick_fase[p]) < medio) break;
        btn_tick_fase[p] = ahora;

        if (btn_mostrando_on[p]) {
            Botones_MostrarColor(p, 0xFF);
            btn_mostrando_on[p] = 0;
        } else {
            btn_paso_mostrar[p]++;
            if (btn_paso_mostrar[p] >= btn_longitud[p]) {
                btn_fase[p]          = SJ_ESPERANDO;
                btn_paso_esperado[p] = 0;
            } else {
                Botones_MostrarColor(p, btn_secuencia[p][btn_paso_mostrar[p]]);
                btn_mostrando_on[p] = 1;
            }
        }
        break;
    }

    case SJ_ESPERANDO: {
        uint8_t color = Botones_LeerColor(p);
        if (color == 0xFF) break;
        if ((ahora - btn_ultimo_input_tick[p]) < BTN_REARME_MIN_MS) break;  /* cooldown: ver BTN_REARME_MIN_MS */
        btn_ultimo_input_tick[p] = ahora;

        Botones_ConfirmarInput(p, color);
        printf("[BOTONES] P%u color=%u esperado=%u %s\r\n", p, color, btn_secuencia[p][btn_paso_esperado[p]],
               (color == btn_secuencia[p][btn_paso_esperado[p]]) ? "OK" : "FALLO");

        if (color != btn_secuencia[p][btn_paso_esperado[p]]) {
            if ((uint8_t)(btn_longitud[p] - 1) > btn_mejor_racha[p]) btn_mejor_racha[p] = (uint8_t)(btn_longitud[p] - 1);
            printf("[BOTONES] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            btn_fase[p]            = SJ_GAMEOVER;
            btn_tick_fase[p]       = ahora;
            btn_flash_pendiente[p] = 0xFF;
            for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
            Buzzer_Beep(350);
            Renderer_DibujarGameOverBotones(p, (uint16_t)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            break;
        }

        btn_paso_esperado[p]++;
        if (btn_paso_esperado[p] >= btn_longitud[p]) {
            if (btn_longitud[p] > btn_mejor_racha[p]) btn_mejor_racha[p] = btn_longitud[p];
            if (btn_longitud[p] < SJ_MAX_LONGITUD) {
                btn_secuencia[p][btn_longitud[p]] = Botones_SiguienteColor(p);
                btn_longitud[p]++;
                Renderer_ActualizarRachaBotones(p, btn_longitud[p]);
            }
            btn_fase[p]            = SJ_ACIERTO;
            btn_tick_fase[p]       = ahora;
            btn_flash_pendiente[p] = 0xFF;
            Botones_MostrarColor(p, 0xFF);
            {
                static const PasoSonido_t BEEP_RONDA[3] = {
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };
                Buzzer_Patron(BEEP_RONDA, 3);
            }
        }
        break;
    }

    case SJ_ACIERTO:
        if ((ahora - btn_tick_fase[p]) < SJ_PAUSA_ACIERTO_MS) break;
        btn_paso_mostrar[p] = 0;
        btn_mostrando_on[p] = 1;
        btn_tick_fase[p]    = ahora;
        btn_fase[p]         = SJ_MOSTRANDO;
        Botones_MostrarColor(p, btn_secuencia[p][0]);
        break;

    case SJ_GAMEOVER: {
        /* en un arcade real no hay "click" separado -- cualquier boton
         * propio reintenta, pero solo despues de BTN_GAMEOVER_COOLDOWN_MS
         * (siempre hay que llamar a Botones_LeerColor, aunque el cooldown
         * no haya pasado, para que su debounce interno no se desincronice). */
        uint8_t color = Botones_LeerColor(p);
        if (color != 0xFF && (ahora - btn_tick_fase[p]) >= BTN_GAMEOVER_COOLDOWN_MS) {
            printf("[BOTONES] P%u retry\r\n", p);
            Botones_ReiniciarJugador(p);
        }
        break;
    }
    }
}

static void Botones_Actualizar(void) {
    Botones_ActualizarJugador(0);
    if (!btn_modo_1p) Botones_ActualizarJugador(1);
}

/* ========================================================================== */
/* === GUITAR HERO — CARA A CARA (RETRATO, COCKPIT) ========================== */
/* ========================================================================== */
/* Notas reales ligadas a un spawn periodico independiente por jugador -- a
 * diferencia de la maqueta original de DEMO_JUGANDO/GuitarHero_IntentarGolpe
 * (2 notas fijas, B1 como boton unico sin importar color/carril), aca cada
 * jugador tiene su propia mitad de gs.notas[MAX_NOTES] (0-7 = J1, 8-15 = J2,
 * ver el comentario de MAX_NOTES en game_state.h) y su propio color debe
 * coincidir con el carril de la nota para contar como golpe -- usa los
 * mismos 4 botones arcade que Simon Clasico (Botones_LeerColor). Layout
 * visual (carril angosto + zona de golpe circular) en las funciones
 * Renderer_DrawModoGuitarHero.../Renderer_GH_... (renderer.c), inspirado en el
 * video_box/video_ellipse del repo FPGA de referencia (ver
 * guitar_hero/ANALISIS_REFERENCIA.md). Cada jugador termina su ronda a los
 * NOTES_PER_GAME notas -- ahi se dibuja "RONDA COMPLETA" SOLO en su mitad y
 * cualquiera de sus propios 4 botones arranca una ronda nueva, sin tocar la
 * partida en curso del otro (mismo patron "cualquier entrada tuya reintenta"
 * que Simon Clasico/Simon+Joystick). */

#define GH_NOTA_BASE(p)     ((uint8_t)((p) * (MAX_NOTES / 2)))
#define GH_NOTAS_POR_JUG    (MAX_NOTES / 2)
#define GH_SPAWN_MS         SPAWN_INTERVAL_L2

static uint32_t gh_seed[2]      = { 5, 13 };
static uint8_t  gh_terminado[2] = { 0, 0 };

static uint8_t GH_Random4(uint8_t p) {
    gh_seed[p] = gh_seed[p] * 1103515245u + 12345u;
    return (uint8_t)((gh_seed[p] >> 16) & 0x3u);
}

/* arranca (o reinicia) SOLO el jugador p -- no toca la mitad del otro */
static void GuitarHero_ReiniciarJugador(uint8_t p) {
    gh_seed[p] ^= (HAL_GetTick() + p * 977u + 9u);
    if (gh_seed[p] == 0) gh_seed[p] = 1;

    gs.j[p].puntaje           = 0;
    gs.j[p].combo             = 0;
    gs.j[p].notas_spawneadas  = 0;
    gs.j[p].tick_ultimo_spawn = HAL_GetTick();
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) gs.notas[GH_NOTA_BASE(p) + i].activa = 0;
    gh_terminado[p] = 0;

    Renderer_DrawModoGuitarHeroJugador(p);
    Renderer_GH_ActualizarPuntaje(p, 0, 0);
}

/* dibuja ambas mitades desde cero (arranque de una partida nueva a 2 jugadores) */
static void GuitarHero2_IniciarAmbos(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    gs.nota_speed = NOTE_SPEED_L2;
    memset(gs.notas, 0, sizeof(gs.notas));
    Renderer_DrawModoGuitarHero2P();
    GuitarHero_ReiniciarJugador(0);
    GuitarHero_ReiniciarJugador(1);
}

/* Arranca GUITAR HERO a 1 solo jugador (jugador 0) -- el lado del jugador 2
 * se deja en negro, apagado, mismo patron que Botones_IniciarSolo. SIN flip
 * de hardware (2026-07-31, mismo motivo que Botones_IniciarSolo): el swap
 * de Cockpit_FillRect/DrawString/Punto ya orienta al jugador 0 hacia el
 * lado contrario, un flip de hardware encima giraba 180 de mas. */
static void GuitarHero_IniciarSolo(void) {
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    ILI9341_FillScreen(COLOR_BLACK);
    gs.nota_speed = NOTE_SPEED_L2;
    memset(gs.notas, 0, sizeof(gs.notas));
    GuitarHero_ReiniciarJugador(0);
}

static void GuitarHero_ActualizarJugador(uint8_t p) {
    if (gh_terminado[p]) {
        if (Botones_LeerColor(p) != 0xFF) {
            printf("[GUITARHERO] P%u nueva ronda\r\n", p);
            GuitarHero_ReiniciarJugador(p);
        }
        return;
    }

    uint32_t ahora = HAL_GetTick();
    uint8_t  base  = GH_NOTA_BASE(p);

    /* Spawn periodico -- una nota nueva cada GH_SPAWN_MS mientras queden
     * cupos en la ronda (NOTES_PER_GAME) y un slot libre en la mitad de p. */
    if (gs.j[p].notas_spawneadas < NOTES_PER_GAME &&
        (ahora - gs.j[p].tick_ultimo_spawn) >= GH_SPAWN_MS) {
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {
            Nota_t *n = &gs.notas[base + i];
            if (n->activa) continue;
            n->carril  = GH_Random4(p);
            n->jugador = p;
            n->x_rel   = COCKPIT_ZONE_W;
            n->x_prev  = n->x_rel;
            n->activa  = 1;
            gs.j[p].notas_spawneadas++;
            gs.j[p].tick_ultimo_spawn = ahora;
            break;
        }
    }

    /* Input: el color propio del jugador caza la nota mas cercana de ESE
     * carril (no la mas cercana de cualquier color, a diferencia de la
     * maqueta original de 1 solo boton) */
    uint8_t color = Botones_LeerColor(p);
    if (color != 0xFF) {
        int16_t mejor_dist = 0x7FFF;
        int8_t  mejor_i    = -1;
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {
            Nota_t *n = &gs.notas[base + i];
            if (!n->activa || n->carril != color) continue;
            int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);
            int16_t dist = (int16_t)((centro_nota > (int16_t)GH_ZONA_CX) ? (centro_nota - (int16_t)GH_ZONA_CX) : ((int16_t)GH_ZONA_CX - centro_nota));
            if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }
        }
        if (mejor_i >= 0 && mejor_dist <= (int16_t)HIT_OK) {
            Nota_t     *n = &gs.notas[base + mejor_i];
            const char *calidad;
            if      (mejor_dist <= (int16_t)HIT_PERFECT) { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_PERFECT); calidad = "PERFECT"; }
            else if (mejor_dist <= (int16_t)HIT_GOOD)    { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_GOOD);    calidad = "GOOD"; }
            else                                          { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_OK);     calidad = "OK"; }
            gs.j[p].combo++;
            n->activa = 0;
            Buzzer_Beep(80);
            printf("[GUITARHERO] P%u color=%u dist=%d %s puntaje=%u combo=%u\r\n",
                   p, color, mejor_dist, calidad, gs.j[p].puntaje, gs.j[p].combo);
        } else {
            gs.j[p].combo = 0;   /* boton sin nota propia en rango -- corta combo */
        }
    }

    /* Movimiento + render delta de las notas activas de este jugador --
     * viajan de derecha a izquierda (nacen lejos, se acercan a la zona de
     * golpe), al reves de la maqueta original DEMO_JUGANDO. */
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {
        Nota_t *n = &gs.notas[base + i];
        if (!n->activa) continue;
        n->x_prev = n->x_rel;
        n->x_rel  = (int16_t)(n->x_rel - gs.nota_speed);
        Renderer_GH_EraseNotaTrail(p, n, gs.nota_speed);
        if (n->x_rel < -(int16_t)NOTE_W) {
            n->activa     = 0;
            gs.j[p].combo = 0;   /* nota perdida sin presionar -- corta combo */
        } else {
            Renderer_GH_DrawNota(p, n);
        }
    }

    Renderer_GH_ActualizarPuntaje(p, gs.j[p].puntaje, gs.j[p].combo);

    /* Fin de ronda: se agotaron los spawns y no queda ninguna nota viva */
    if (gs.j[p].notas_spawneadas >= NOTES_PER_GAME) {
        uint8_t queda_activa = 0;
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) if (gs.notas[base + i].activa) { queda_activa = 1; break; }
        if (!queda_activa) {
            gh_terminado[p] = 1;
            printf("[GUITARHERO] P%u ronda completa puntaje=%u\r\n", p, gs.j[p].puntaje);
            Renderer_GH_DibujarFin(p, gs.j[p].puntaje);
        }
    }
}

static void GuitarHero_Actualizar(void) {
    GuitarHero_ActualizarJugador(0);
    if (!guitar_modo_1p) GuitarHero_ActualizarJugador(1);
}

/* ========================================================================== */
/* === MAIN (CON NUEVA NAVEGACIÓN DE MENÚS) ================================== */
/* ========================================================================== */

/* --------------------------------------------------------------------------
 * NUEVAS FUNCIONES PARA NAVEGACIÓN CON JOYSTICK SIN EXIGIR CENTRADO EXACTO
 * -------------------------------------------------------------------------- */
#define MENU_JOY_COOLDOWN_MS 300U
#define MENU_MODO_COOLDOWN_MS 250U

/* Mueve el cursor SOLO con el joystick (cualquiera de los 2) -- el boton
 * arcade queda reservado exclusivamente para CONFIRMAR (ver "confirmar_boton"
 * en main()). Antes tambien se usaba BotonesNavegacion_Presionado() aca para
 * mover el cursor, lo que dejaba el boton sin ningun uso libre para confirmar
 * -- de ahi que el menu de modo (y el de jugadores) nunca terminaran de
 * confirmar nada con B1 muerto: el cursor se movia, pero jamas se llegaba a
 * "avanzar". */
static void MenuJugadores_Procesar(uint8_t *screen) {
    static uint32_t ultimo_mov_joy = 0;
    uint32_t ahora = HAL_GetTick();

    uint16_t j1x_b, j1x_a, j1y_b, j1y_a, j2x_b, j2x_a, j2y_b, j2y_a;
    Joy_Umbrales(centro_j1x, &j1x_b, &j1x_a);
    Joy_Umbrales(centro_j1y, &j1y_b, &j1y_a);
    Joy_Umbrales(centro_j2x, &j2x_b, &j2x_a);
    Joy_Umbrales(centro_j2y, &j2y_b, &j2y_a);

    uint8_t joy_movido = 0;
    if ((ahora - ultimo_mov_joy) >= MENU_JOY_COOLDOWN_MS) {
        if (joystick_x < j1x_b || joystick_x > j1x_a || joystick_y < j1y_b || joystick_y > j1y_a ||
            joystick2_x < j2x_b || joystick2_x > j2x_a || joystick2_y < j2y_b || joystick2_y > j2y_a) {
            joy_movido = 1;
            ultimo_mov_joy = ahora;
        }
    }

    if (joy_movido) {
        uint8_t screen_ant = *screen;
        *screen = (*screen == DEMO_JUGADORES_1) ? DEMO_JUGADORES_2 : DEMO_JUGADORES_1;
        Renderer_UpdateSeleccionJugadores((uint8_t)(screen_ant - DEMO_JUGADORES_1),
                                          (uint8_t)(*screen - DEMO_JUGADORES_1));
        printf("[MENU] jugadores -> %u\r\n", (unsigned)(*screen - DEMO_JUGADORES_1));
    }
}

static void MenuModos_Procesar(uint8_t *screen) {
    static uint32_t ultimo_mov_modo = 0;
    uint32_t ahora = HAL_GetTick();

    uint16_t j1x_b, j1x_a, j1y_b, j1y_a, j2x_b, j2x_a, j2y_b, j2y_a;
    Joy_Umbrales(centro_j1x, &j1x_b, &j1x_a);
    Joy_Umbrales(centro_j1y, &j1y_b, &j1y_a);
    Joy_Umbrales(centro_j2x, &j2x_b, &j2x_a);
    Joy_Umbrales(centro_j2y, &j2y_b, &j2y_a);

    uint8_t joy_movido = 0;
    if ((ahora - ultimo_mov_modo) >= MENU_MODO_COOLDOWN_MS) {
        if (joystick_x < j1x_b || joystick_x > j1x_a || joystick_y < j1y_b || joystick_y > j1y_a ||
            joystick2_x < j2x_b || joystick2_x > j2x_a || joystick2_y < j2y_b || joystick2_y > j2y_a) {
            joy_movido = 1;
            ultimo_mov_modo = ahora;
        }
    }

    if (joy_movido) {
        uint8_t screen_ant = *screen;
        if (*screen == DEMO_MODO_GUITAR)
            *screen = DEMO_MODO_SIMON;
        else
            (*screen)++;
        Renderer_UpdateSeleccionModo((uint8_t)(screen_ant - DEMO_MODO_SIMON),
                                     (uint8_t)(*screen - DEMO_MODO_SIMON));
        printf("[MENU] modo -> %s\r\n", DEMO_NOMBRE[*screen]);
    }
}

/* Ciclar A-Z la letra actual de nombre_jugadores[nombre_jugador_actual]
 * [nombre_pos_actual] con el joystick del jugador que esta escribiendo
 * (arriba=siguiente letra, abajo=letra anterior) -- confirmar (avanzar de
 * posicion o de jugador) se maneja en la confirmacion de menus de main(),
 * no aca. */
#define INICIALES_JOY_COOLDOWN_MS 180U
static void MenuIniciales_Procesar(void) {
    static uint32_t ultimo_mov = 0;
    uint32_t ahora = HAL_GetTick();
    if ((ahora - ultimo_mov) < INICIALES_JOY_COOLDOWN_MS) return;

    /* EJES CRUZADOS -- arriba/abajo se lee del canal electrico "x", igual
     * que en Joystick_LeerDireccion (ver comentario ahi). */
    uint16_t jx = (nombre_jugador_actual == 0) ? joystick_x : joystick2_x;
    uint16_t bajo_y, alto_y;
    Joy_Umbrales((nombre_jugador_actual == 0) ? centro_j1x : centro_j2x, &bajo_y, &alto_y);

    char *c = &nombre_jugadores[nombre_jugador_actual][nombre_pos_actual];
    if (jx >= alto_y) {
        *c = (char)((*c >= 'Z') ? 'A' : (char)(*c + 1));
        ultimo_mov = ahora;
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);
    } else if (jx <= bajo_y) {
        *c = (char)((*c <= 'A') ? 'Z' : (char)(*c - 1));
        ultimo_mov = ahora;
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    TIM3_ADCTrigger_Init();
    ADC1_Joystick_Init();
    MX_TIM4_Buzzer_Init();
    MX_USART2_UART_Init();

    BotonesChase_Iniciar();
    ILI9341_Init();

    HAL_TIM_Base_Start(&htim3);
    HAL_ADC_Start_IT(&hadc1);

    printf("\r\n=== Beat Clash boot OK (PA2/PA3 @ 115200 8N1) ===\r\n");

    /* Indicador visual de calibración */
    ILI9341_FillScreen(COLOR_BLACK);
    ILI9341_DrawString(20, 100, "Calibrando joysticks...", COLOR_WHITE, COLOR_BLACK, 2);
    ILI9341_DrawString(20, 130, "No tocar los sticks", COLOR_YELLOW, COLOR_BLACK, 1);
    HAL_Delay(300);

    centro_j1x = joystick_x;
    centro_j1y = joystick_y;
    centro_j2x = joystick2_x;
    centro_j2y = joystick2_y;
    printf("[CALIB] centro j1=(%u,%u) j2=(%u,%u)\r\n", centro_j1x, centro_j1y, centro_j2x, centro_j2y);

#if BUZZER_DIAGNOSTICO_BARRIDO
    Buzzer_BarridoDiagnostico();
#endif

    uint8_t  screen        = DEMO_SPLASH;
    uint8_t  last_paso_sim = 0xFF;   /* fuerza el primer dibujo de las vistas previas */
    Demo_Enter(screen);

    uint32_t last_frame_tick = 0;   /* para sincronización no bloqueante */

    while (1) {
        /* ---- sincronización de frame (no bloqueante) ---- */
        if (HAL_GetTick() - last_frame_tick < RENDER_TICK_MS) continue;
        last_frame_tick = HAL_GetTick();

        /* ---- diagnóstico ADC (solo cuando no hay partida) ---- */
        if (!en_juego_real && !en_juego_real_2p && !en_juego_real_botones && !en_juego_real_guitar) {
            static uint32_t dbg_tick = 0;
            if (HAL_GetTick() - dbg_tick >= 500) {
                dbg_tick = HAL_GetTick();
                printf("[ADC] j1=(%u,%u) j2=(%u,%u)\r\n", joystick_x, joystick_y, joystick2_x, joystick2_y);
            }
        }

        /* Combo de salida ROJO+AMARILLO (máxima prioridad) */
        if (ComboSalir_Detectado()) {
            en_juego_real         = 0;
            en_juego_real_2p      = 0;
            en_juego_real_botones = 0;
            en_juego_real_guitar  = 0;
            conteo_auto           = 0;
            Buzzer_Fondo_Detener();
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
            ILI9341_SetPortrait(0);
            ILI9341_SetFlip180(0);
            screen = DEMO_SPLASH;
            Demo_Enter(screen);
            BotonesChase_Iniciar();
            continue;
        }

        Buzzer_Actualizar();
        if (screen == DEMO_SPLASH) BotonesChase_Actualizar();
        uint8_t avanzar = Boton_B1_Flanco();

        /* Confirmacion de los menus de JUGADORES/MODO con CUALQUIER boton
         * arcade (B1 fisicamente inaccesible, ver comentario de main.c
         * arriba) -- calculado ANTES de mover el cursor (MenuXxx_Procesar,
         * abajo) para que un boton confirme lo que se estaba mostrando este
         * tick, no lo que el cursor recien paso a mostrar. Se computa una
         * sola vez (BotonesNavegacion_Presionado consume el flanco) y solo
         * aplica en estas 2 pantallas -- el resto de las confirmaciones de
         * abajo (salir de un juego real, arrancar preview) siguen atadas
         * solo a B1 por ahora, sin cambios de comportamiento ahi. */
        uint8_t en_pantalla_modo = (screen == DEMO_MODO_SIMON || screen == DEMO_MODO_SIMONJOY || screen == DEMO_MODO_GUITAR);
        uint8_t confirmar_boton = (screen == DEMO_SPLASH || screen == DEMO_JUGADORES_1 ||
                                    screen == DEMO_JUGADORES_2 || screen == DEMO_INICIALES || en_pantalla_modo)
                                 ? BotonesNavegacion_Presionado() : 0;

        /* ------------------------------------------------------------------
         * MENÚS DE SELECCIÓN (con joystick y botones, sin B1 para mover)
         * ------------------------------------------------------------------ */
        if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {
            MenuJugadores_Procesar(&screen);
        }
        else if (screen == DEMO_INICIALES) {
            MenuIniciales_Procesar();
        }
        else if (en_pantalla_modo) {
            MenuModos_Procesar(&screen);
        }

        /* ------------------------------------------------------------------
         * CONFIRMACIÓN CON B1 (o navegación en lista de canciones)
         * ------------------------------------------------------------------ */
        if (avanzar || confirmar_boton) {
            if (en_juego_real || en_juego_real_2p || en_juego_real_botones || en_juego_real_guitar) {
                /* B1 durante un juego real: salir al recorrido */
                en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;
                Buzzer_Fondo_Detener();
                for (uint8_t p = 0; p < 2; p++)
                    for (uint8_t c = 0; c < 4; c++)
                        Boton_LED(p, c, 0);
                ILI9341_SetFlip180(0);
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_SPLASH) {
                /* splash -> menu de jugadores: se habia quedado sin ninguna
                 * forma de avanzar (ni B1 -- muerto -- ni boton arcade) tras
                 * la reescritura de la navegacion de menus; sin esto la
                 * consola queda trabada en el splash para siempre. */
                BotonesChase_Detener();
                screen = DEMO_JUGADORES_1;
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {
                jugadores_seleccionados = (screen == DEMO_JUGADORES_2) ? 2 : 1;
                printf("[MENU] jugadores = %u\r\n", jugadores_seleccionados);
                screen = DEMO_INICIALES;
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_INICIALES) {
                printf("[INICIALES] J%u letra %u = %c confirmada\r\n",
                       (unsigned)(nombre_jugador_actual + 1), (unsigned)(nombre_pos_actual + 1),
                       nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);
                Renderer_ConfirmarNombreLetra(nombre_pos_actual, nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);

                if (nombre_pos_actual < 2) {
                    nombre_pos_actual++;
                    Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual,
                                                nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]);
                } else if (jugadores_seleccionados == 2 && nombre_jugador_actual == 0) {
                    nombre_jugador_actual = 1;
                    nombre_pos_actual     = 0;
                    Renderer_DrawNombre(1, nombre_jugadores[1], 0);
                } else {
                    printf("[INICIALES] J1=%s J2=%s\r\n", nombre_jugadores[0], nombre_jugadores[1]);
                    screen = DEMO_MODO_SIMON;
                    Demo_Enter(screen);
                }
                avanzar = 0;
            }
            else if (en_pantalla_modo) {
                modo_confirmado = (screen == DEMO_MODO_SIMONJOY) ? MODO_SEL_SIMONJOY :
                                   (screen == DEMO_MODO_GUITAR)  ? MODO_SEL_GUITAR  : MODO_SEL_BOTONES;
                printf("[MENU] modo = %s\r\n",
                       modo_confirmado == MODO_SEL_SIMONJOY ? "SIMONJOY" :
                       modo_confirmado == MODO_SEL_GUITAR   ? "GUITAR"   : "BOTONES");
                screen = DEMO_CONTEO_3;
                Demo_Enter(screen);
                Buzzer_Beep(100);
                conteo_auto = 1;
                conteo_tick = HAL_GetTick();
                avanzar = 0;
            }
            else if (screen == DEMO_PREVIEW_SIMONJOY) {
                en_juego_real = 1;
                SimonJoy_Iniciar();
                avanzar = 0;
            }
            else if (screen == DEMO_PREVIEW_SIMON) {
                en_juego_real_botones = 1;
                btn_modo_1p = (jugadores_seleccionados == 1);
                if (jugadores_seleccionados == 2) Botones_IniciarAmbos();
                else Botones_IniciarSolo();
                avanzar = 0;
            }
            else if (screen == DEMO_JUGANDO) {
                GuitarHero_IntentarGolpe();
                avanzar = 0;
            }
        }

        /* ------------------------------------------------------------------
         * CONTEO AUTOMÁTICO 3-2-1-GO
         * ------------------------------------------------------------------ */
        if (conteo_auto) {
            if (HAL_GetTick() - conteo_tick >= CONTEO_PASO_MS) {
                conteo_tick = HAL_GetTick();
                if (screen == DEMO_CONTEO_GO) {
                    conteo_auto = 0;
                    printf("[GO] modo=%s jugadores=%u\r\n",
                           modo_confirmado == MODO_SEL_SIMONJOY ? "SIMONJOY" :
                           modo_confirmado == MODO_SEL_GUITAR   ? "GUITAR"   : "BOTONES",
                           jugadores_seleccionados);
                    switch (modo_confirmado) {
                    case MODO_SEL_SIMONJOY:
                        screen = DEMO_PREVIEW_SIMONJOY;
                        if (jugadores_seleccionados == 2) {
                            en_juego_real_2p = 1;
                            SimonJoy2_IniciarAmbos();
                        } else {
                            en_juego_real = 1;
                            SimonJoy_Iniciar();
                        }
                        break;
                    case MODO_SEL_GUITAR:
                        screen = DEMO_JUGANDO;
                        en_juego_real_guitar = 1;
                        guitar_modo_1p = (jugadores_seleccionados != 2);
                        if (jugadores_seleccionados == 2) {
                            GuitarHero2_IniciarAmbos();
                        } else {
                            GuitarHero_IniciarSolo();
                        }
                        break;
                    default: /* MODO_SEL_BOTONES */
                        screen = DEMO_PREVIEW_SIMON;
                        en_juego_real_botones = 1;
                        if (jugadores_seleccionados == 2) {
                            btn_modo_1p = 0;
                            Botones_IniciarAmbos();
                        } else {
                            btn_modo_1p = 1;
                            Botones_IniciarSolo();
                        }
                        break;
                    }
                } else {
                    screen++;
                    Demo_Enter(screen);
                    if (screen == DEMO_CONTEO_GO) {
                        static const PasoSonido_t BEEP_GO[3] = {
                            { SOL4, 90 }, { 0, 20 }, { DO5, 220 }
                        };
                        Buzzer_Patron(BEEP_GO, 3);
                    } else {
                        Buzzer_Beep(100);
                    }
                }
            }
            continue;
        }

        /* ------------------------------------------------------------------
         * JUEGOS ACTIVOS
         * ------------------------------------------------------------------ */
        if (en_juego_real) {
            SimonJoy_Actualizar();
            continue;
        }
        if (en_juego_real_2p) {
            SimonJoy2_Actualizar();
            continue;
        }
        if (en_juego_real_botones) {
            Botones_Actualizar();
            continue;
        }
        if (en_juego_real_guitar) {
            GuitarHero_Actualizar();
            continue;
        }

        /* ------------------------------------------------------------------
         * RECORRIDO DE DISEÑO (PANTALLAS ESTÁTICAS)
         * ------------------------------------------------------------------ */
        switch (screen) {

        case DEMO_SPLASH:
        case DEMO_JUGANDO:
        case DEMO_RESULTADO:
            if (screen == DEMO_JUGANDO) {
                for (uint8_t i = 0; i < 2; i++) {
                    Nota_t *n = &gs.notas[i];
                    n->x_prev = n->x_rel;
                    n->x_rel  = (int16_t)(n->x_rel + gs.nota_speed);
                    if (n->x_rel > (int16_t)PLAYER_W) n->x_rel = -NOTE_W;
                }
            }
            Renderer_Update(&gs);
            break;

        case DEMO_PREVIEW_SIMON:
        case DEMO_PREVIEW_SIMONJOY: {
            uint8_t paso = Demo_PasoSimon();
            if (paso != last_paso_sim) {
                if (screen == DEMO_PREVIEW_SIMON)
                    Renderer_DrawModoSimonClasico(paso, paso);
                else
                    Renderer_UpdateModoSimonJoystick1P(last_paso_sim, paso);
                last_paso_sim = paso;
            }
            break;
        }

        case DEMO_MENU_CANCIONES:
            if (BotonesNavegacion_Presionado()) {
                uint8_t ant = cancion_cursor;
                cancion_cursor = (cancion_cursor + 1) % CANCIONES_N;
                Renderer_UpdateListaCanciones(ant, cancion_cursor);
                Buzzer_Patron(CANCIONES_DATA[cancion_cursor], CANCIONES_LEN[cancion_cursor]);
            }
            break;

        default:
            break;
        }
    }
}

/* ========================================================================== */
/* === RELOJ DEL SISTEMA ===================================================== */
/* ========================================================================== */

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSI 16MHz sin PLL — suficiente para SPI@8MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;      /* APB1  = 16MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* APB2  = 16MHz (SPI1) */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

/* ========================================================================== */
/* === GPIO — PANTALLA, JOYSTICKS, BOTONES ARCADE Y BUZZER =================== */
/* ========================================================================== */

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = LCD_RST_PIN;
    HAL_GPIO_Init(LCD_RST_PORT, &g);
    LCD_RST_HIGH();

    g.Pin = LCD_CS_PIN;
    HAL_GPIO_Init(LCD_CS_PORT, &g);
    LCD_CS_HIGH();

    g.Pin = LCD_DC_PIN;
    HAL_GPIO_Init(LCD_DC_PORT, &g);

    /* B1 (PC13) — pull-up externo R30=4k7 en la Nucleo, no usar PULLUP sw */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    g.Pin  = BTN_USER_PIN;
    HAL_GPIO_Init(BTN_USER_PORT, &g);

    /* pa1/pa4: entradas analogicas del joystick (pa1=vry, pa4=vrx) */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = GPIO_PIN_1 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &g);

    /* pc0/pc1: entradas analogicas del joystick 2 (pc0=vry2, pc1=vrx2) */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOC, &g);

    /* botones arcade: 8 switches (entrada, pull-up interno) + 8 LED (salida
     * hacia ULN2003A) */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    for (uint8_t p = 0; p < 2; p++) {
        for (uint8_t c = 0; c < 4; c++) {
            g.Pin = BTN_SW[p][c].pin;
            HAL_GPIO_Init(BTN_SW[p][c].port, &g);
        }
    }

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    for (uint8_t p = 0; p < 2; p++) {
        for (uint8_t c = 0; c < 4; c++) {
            g.Pin = BTN_LED[p][c].pin;
            HAL_GPIO_Init(BTN_LED[p][c].port, &g);
            HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, GPIO_PIN_RESET);
        }
    }

    /* buzzer en pa6 */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &g);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

/* -----------------------------------------------------------------------
 * TIM4: INTERRUPCION PERIODICA QUE ALTERNA PA6 POR SOFTWARE
 * ----------------------------------------------------------------------- */
static void MX_TIM4_Buzzer_Init(void) {
    __HAL_RCC_TIM4_CLK_ENABLE();

    htim4.Instance           = TIM4;
    htim4.Init.Prescaler     = 15;    /* 16MHz/16 = 1MHz -> tick de 1us */
    htim4.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim4.Init.Period        = 999;   /* arranca detenido, se ajusta en runtime */
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim4);

    HAL_NVIC_SetPriority(TIM4_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

/* -----------------------------------------------------------------------
 * TIM3: DISPARADOR (TRGO) DEL ADC1 CADA 20 MS
 * ----------------------------------------------------------------------- */
static void TIM3_ADCTrigger_Init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance           = TIM3;
    htim3.Init.Prescaler     = 1599;  /* 16mhz/1600 = 10khz -> tick de 100us */
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim3.Init.Period        = 199;   /* 200 ticks x 100us = 20 ms exactos  */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim3);

    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
}

/* -----------------------------------------------------------------------
 * ADC1: JOYSTICK 1 (CH1=PA1=Y1, CH4=PA4=X1) + JOYSTICK 2 (CH10=PC0=Y2,
 * CH11=PC1=X2) EN MODO SCAN, 4 CANALES
 * ----------------------------------------------------------------------- */
static void ADC1_Joystick_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 4;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    HAL_NVIC_SetPriority(ADC_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    sConfig.Channel      = ADC_CHANNEL_1;   /* rank 1: joy1 eje y (pa1) */
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_4;        /* rank 2: joy1 eje x (pa4) */
    sConfig.Rank    = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_10;       /* rank 3: joy2 eje y (pc0) */
    sConfig.Rank    = 3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_11;       /* rank 4: joy2 eje x (pc1) */
    sConfig.Rank    = 4;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* ========================================================================== */
/* === SPI1 — BUS HACIA EL ILI9341 =========================================== */
/* ========================================================================== */

static void MX_SPI1_Init(void) {
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL=0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA=0 → SPI Mode 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  /* 8MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 10;
    HAL_SPI_Init(&hspi1);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_5 | GPIO_PIN_7;  /* PA5=SCK, PA7=MOSI */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* ========================================================================== */
/* === CONSOLA DE DEPURACION — USART2 POR EL VCP DEL ST-LINK (2026-07-31) ==== */
/* ========================================================================== */

static void MX_USART2_UART_Init(void) {
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;  /* PA2=TX, PA3=RX -- VCP ST-Link */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ========================================================================== */
/* === CALLBACKS DEL JOYSTICK ================================================ */
/* ========================================================================== */

/* tick de tim4: alterna pa6 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        uint16_t valor = (uint16_t)HAL_ADC_GetValue(&hadc1);

        switch (adc_rank_actual) {
        case 0:
            filtro_adc_y += ((int32_t)valor - filtro_adc_y) / ADC_FILTRO_N;
            joystick_y = ((int32_t)filtro_adc_y > (int32_t)centro_j1y - (int32_t)JOY_ZONA_MUERTA &&
                          (int32_t)filtro_adc_y < (int32_t)centro_j1y + (int32_t)JOY_ZONA_MUERTA)
                       ? centro_j1y : (uint16_t)filtro_adc_y;
            adc_rank_actual = 1;
            break;
        case 1:
            filtro_adc_x += ((int32_t)valor - filtro_adc_x) / ADC_FILTRO_N;
            joystick_x = ((int32_t)filtro_adc_x > (int32_t)centro_j1x - (int32_t)JOY_ZONA_MUERTA &&
                          (int32_t)filtro_adc_x < (int32_t)centro_j1x + (int32_t)JOY_ZONA_MUERTA)
                       ? centro_j1x : (uint16_t)filtro_adc_x;
            adc_rank_actual = 2;
            break;
        case 2:
            filtro_adc_y2 += ((int32_t)valor - filtro_adc_y2) / ADC_FILTRO_N;
            joystick2_y = ((int32_t)filtro_adc_y2 > (int32_t)centro_j2y - (int32_t)JOY_ZONA_MUERTA &&
                           (int32_t)filtro_adc_y2 < (int32_t)centro_j2y + (int32_t)JOY_ZONA_MUERTA)
                        ? centro_j2y : (uint16_t)filtro_adc_y2;
            adc_rank_actual = 3;
            break;
        default:
            filtro_adc_x2 += ((int32_t)valor - filtro_adc_x2) / ADC_FILTRO_N;
            joystick2_x = ((int32_t)filtro_adc_x2 > (int32_t)centro_j2x - (int32_t)JOY_ZONA_MUERTA &&
                           (int32_t)filtro_adc_x2 < (int32_t)centro_j2x + (int32_t)JOY_ZONA_MUERTA)
                        ? centro_j2x : (uint16_t)filtro_adc_x2;
            adc_rank_actual = 0;
            HAL_ADC_Start_IT(&hadc1);
            break;
        }
    }
}

/* ========================================================================== */
/* === MANEJO DE ERRORES ===================================================== */
/* ========================================================================== */

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
