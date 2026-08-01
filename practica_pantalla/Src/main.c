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
static GameState_t gs;

/* === JOYSTICK — misma configuracion/filtro que examen_parcial ============= */
/* lecturas de 12 bits (0-4095) ya filtradas (ver HAL_ADC_ConvCpltCallback) */
volatile uint16_t joystick_x = 2048;
volatile uint16_t joystick_y = 2048;
static uint8_t    adc_rank_actual = 0;   /* 0=y1(rank1) 1=x1(rank2) 2=y2(rank3) 3=x2(rank4) */

#define ADC_FILTRO_N     2   /* bajado de 4 -- con 4 el filtro tardaba ~100ms
    en alcanzar un movimiento brusco del joystick (se sentia lento el cursor
    y hasta la deteccion de direccion); con 2 responde casi al doble de
    rapido. La zona muerta (JOY_ZONA_MUERTA) sigue fijando el reposo exacto
    en 2048, asi que no se reintroduce temblor visual al quedarse quieto. */
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

#define JOY_CENTRO       2048
#define JOY_ZONA_MUERTA  150

/* umbral de inclinacion para detectar una direccion del joystick durante
 * el juego real (ver Joystick_LeerDireccion/Joystick2_LeerDireccion) */
#define JOY_UMBRAL_ALTO  3200U
#define JOY_UMBRAL_BAJO   900U

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

static void Boton_LED(uint8_t p, uint8_t c, uint8_t on) {
    HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* primer color con flanco de presion (activo BAJO, pull-up interno) del
 * jugador p, o 0xFF si ninguno se presiono este tick */
static uint8_t Botones_LeerColor(uint8_t p) {
    for (uint8_t c = 0; c < 4; c++) {
        GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_SW[p][c].port, BTN_SW[p][c].pin);
        uint8_t flanco = (btn_prev[p][c] == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
        btn_prev[p][c] = cur;
        if (flanco) return c;
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
#define BUZZER_TONO_HZ     2000U   /* tono unico para los beeps de UI (sin tono variable) */

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

/* llamar una vez por vuelta del loop principal, sin importar el estado */
static void Buzzer_Actualizar(void) {
    if (buzzer_pos >= buzzer_pasos_n) return;
    if (HAL_GetTick() - buzzer_tick_paso < buzzer_pasos[buzzer_pos].dur_ms) return;

    buzzer_pos++;
    buzzer_tick_paso = HAL_GetTick();
    if (buzzer_pos >= buzzer_pasos_n) {
        Buzzer_SetSalida(0);
    } else {
        Buzzer_SetSalida(buzzer_pasos[buzzer_pos].freq_hz);
    }
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

/* ========================================================================== */
/* === RECORRIDO DE PANTALLAS DE DISEÑO ====================================== */
/* ========================================================================== */

typedef enum {
    DEMO_SPLASH = 0,
    DEMO_JUGADORES_1,       /* seleccion de jugadores, cursor en "1 JUGADOR"   */
    DEMO_JUGADORES_2,       /* seleccion de jugadores, cursor en "2 JUGADORES" */
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

/* Indice seleccionado en DEMO_MENU_CANCIONES (persiste entre visitas) */
static uint8_t cancion_cursor = 0;

/* Paso "encendido" (0-3, o 0xFF=ninguno) para animar las vistas previas de
 * Simon Clasico / Simon+Joystick, cambia cada 500ms mientras se muestran. */
static uint8_t Demo_PasoSimon(void) {
    uint32_t t = (HAL_GetTick() / 500) % 5;
    return (t < 4) ? (uint8_t)t : 0xFF;
}

/* Entra a la pantalla `screen`: prepara el GameState_t de ejemplo y dibuja
 * el primer cuadro. Renderer_Update() se encarga de animar lo que siga. */
static void Demo_Enter(uint8_t screen) {
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

    if (mejor_i < 0 || mejor_dist > (int16_t)HIT_OK) return;   /* nada cerca: fallo silencioso */

    Nota_t          *n = &gs.notas[mejor_i];
    EstadoJugador_t *j = &gs.j[n->jugador];

    if      (mejor_dist <= (int16_t)HIT_PERFECT) j->puntaje = (uint16_t)(j->puntaje + SCORE_PERFECT);
    else if (mejor_dist <= (int16_t)HIT_GOOD)    j->puntaje = (uint16_t)(j->puntaje + SCORE_GOOD);
    else                                          j->puntaje = (uint16_t)(j->puntaje + SCORE_OK);
    j->combo++;

    n->x_rel = -NOTE_W;   /* respawn inmediato para poder seguir probando */
}

/* Detecta el flanco de presion de B1 (activo BAJO, pull-up externo en la Nucleo) */
static uint8_t Boton_B1_Flanco(void) {
    static GPIO_PinState prev = GPIO_PIN_SET;
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);
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
 * modo_confirmado_es_joys: 0=BOTONES (Simon Clasico) 1=JOYS (Simon+Joystick)
 * -- decide que arrancar al llegar a GO. Se agrego (2026-07-29) para poder
 * reutilizar el mismo conteo compartido entre los 2 modos jugables sin
 * reintroducir la variable `conteo_destino` que enrutaba hacia Guitar Hero
 * antes de que ese modo se quitara (2026-07-28). */
static uint8_t  modo_confirmado_es_joys = 0;
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
    if (!joy_listo_dir) {
        if (joystick_y < JOY_UMBRAL_ALTO && joystick_y > JOY_UMBRAL_BAJO &&
            joystick_x < JOY_UMBRAL_ALTO && joystick_x > JOY_UMBRAL_BAJO) {
            joy_listo_dir = 1;
        }
        return 0xFF;
    }
    /* eje arriba/abajo invertido en el montaje actual (confirmado en
     * hardware real) -- joystick_y alto = fisicamente ABAJO, joystick_y
     * bajo = fisicamente ARRIBA. Izquierda/derecha si estan bien. */
    if      (joystick_y >= JOY_UMBRAL_ALTO) { joy_listo_dir = 0; return 1; } /* ABAJO  */
    else if (joystick_y <= JOY_UMBRAL_BAJO) { joy_listo_dir = 0; return 0; } /* ARRIBA */
    else if (joystick_x <= JOY_UMBRAL_BAJO) { joy_listo_dir = 0; return 2; } /* IZQ    */
    else if (joystick_x >= JOY_UMBRAL_ALTO) { joy_listo_dir = 0; return 3; } /* DER    */
    return 0xFF;
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
    sj_seed ^= HAL_GetTick();
    if (sj_seed == 0) sj_seed = 1;

    sj_longitud        = 1;
    sj_secuencia[0]     = SJ_Random4();
    sj_paso_mostrar     = 0;
    sj_mostrando_on     = 1;
    sj_fase             = SJ_MOSTRANDO;
    sj_tick_fase        = HAL_GetTick();
    joy_listo_dir       = 1;

    /* pantalla completa para 1 jugador (sin dividir) -- unico redibujo
     * completo: al arrancar */
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

        if (dir != sj_secuencia[sj_paso_esperado]) {
            if ((uint8_t)(sj_longitud - 1) > sj_mejor_racha) sj_mejor_racha = (uint8_t)(sj_longitud - 1);
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
        if (Joystick_LeerDireccion() != 0xFF) SimonJoy_Iniciar();
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

    if (!sj2_joy_listo_dir[p]) {
        if (jy < JOY_UMBRAL_ALTO && jy > JOY_UMBRAL_BAJO &&
            jx < JOY_UMBRAL_ALTO && jx > JOY_UMBRAL_BAJO) {
            sj2_joy_listo_dir[p] = 1;
        }
        return 0xFF;
    }
    if      (jy >= JOY_UMBRAL_ALTO) { sj2_joy_listo_dir[p] = 0; return 1; } /* ABAJO  */
    else if (jy <= JOY_UMBRAL_BAJO) { sj2_joy_listo_dir[p] = 0; return 0; } /* ARRIBA */
    else if (jx <= JOY_UMBRAL_BAJO) { sj2_joy_listo_dir[p] = 0; return 2; } /* IZQ    */
    else if (jx >= JOY_UMBRAL_ALTO) { sj2_joy_listo_dir[p] = 0; return 3; } /* DER    */
    return 0xFF;
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
}

/* dibuja ambas mitades desde cero (arranque de una partida nueva) */
static void SimonJoy2_IniciarAmbos(void) {
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
}

/* tick no bloqueante de UN jugador -- se llama 2 veces por vuelta del loop
 * principal (una por jugador) mientras en_juego_real_2p este activo. */
static void SimonJoy2_ActualizarJugador(uint8_t p) {
    uint32_t ahora = HAL_GetTick();

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

        if (dir != sj2_secuencia[p][sj2_paso_esperado[p]]) {
            if ((uint8_t)(sj2_longitud[p] - 1) > sj2_mejor_racha[p]) sj2_mejor_racha[p] = (uint8_t)(sj2_longitud[p] - 1);
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
        if (Joystick2_LeerDireccion(p) != 0xFF) SimonJoy2_ReiniciarJugador(p);
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
static uint8_t        en_juego_real_botones = 0;   /* 1 = modo BOTONES 2 jugadores real */

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

    Renderer_DrawModoSimonClasicoJugador(p, btn_secuencia[p][0]);
    Boton_LED(p, btn_secuencia[p][0], 1);
    btn_paso_dibujado[p] = btn_secuencia[p][0];
    Renderer_ActualizarRachaBotones(p, btn_longitud[p]);
}

static void Botones_IniciarAmbos(void) {
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

        Botones_ConfirmarInput(p, color);

        if (color != btn_secuencia[p][btn_paso_esperado[p]]) {
            if ((uint8_t)(btn_longitud[p] - 1) > btn_mejor_racha[p]) btn_mejor_racha[p] = (uint8_t)(btn_longitud[p] - 1);
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

    case SJ_GAMEOVER:
        /* en un arcade real no hay "click" separado -- cualquier boton
         * propio reintenta */
        if (Botones_LeerColor(p) != 0xFF) Botones_ReiniciarJugador(p);
        break;
    }
}

static void Botones_Actualizar(void) {
    Botones_ActualizarJugador(0);
    Botones_ActualizarJugador(1);
}

/* ========================================================================== */
/* === MAIN =================================================================== */
/* ========================================================================== */

int main(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    TIM3_ADCTrigger_Init();
    ADC1_Joystick_Init();
    MX_TIM4_Buzzer_Init();

    ILI9341_Init();

    HAL_TIM_Base_Start(&htim3);
    HAL_ADC_Start_IT(&hadc1);

    uint8_t  screen        = DEMO_SPLASH;
    uint8_t  last_paso_sim = 0xFF;   /* fuerza el primer dibujo de las vistas previas */
    Demo_Enter(screen);

    while (1) {
        Buzzer_Actualizar();
        uint8_t avanzar   = Boton_B1_Flanco();
        /* paso_joy: pese al nombre (heredado de cuando esto se leia con el
         * joystick), ya NO viene del joystick -- pedido explicito del
         * usuario (2026-07-30) de navegar el recorrido solo con los botones
         * arcade, dejando el joystick exclusivamente para jugar. Ver
         * BotonesNavegacion_Presionado. Se deja fuera de un juego real para
         * no robarle el flanco de presion a la logica de juego real. */
        int8_t paso_joy = (en_juego_real || en_juego_real_2p || en_juego_real_botones)
                         ? 0
                         : (BotonesNavegacion_Presionado() ? +1 : 0);

        /* B1: unico boton "de click" que queda (los SW de ambos joystick se
         * retiraron 2026-07-30 para ahorrar espacio). En las pantallas de
         * confirmacion hace el salto especial (en vez del avance generico
         * del recorrido, ver mas abajo); dentro de un juego real solo sale
         * (el reintento tras game over ya es automatico moviendo el stick o
         * cualquier boton propio, ver SJ_GAMEOVER / Botones_ActualizarJugador) */
        if (avanzar) {
            if (en_juego_real || en_juego_real_2p || en_juego_real_botones) {
                /* nada que hacer aca -- salir se maneja mas abajo, por modo */
            } else if (screen == DEMO_PREVIEW_SIMONJOY) {
                en_juego_real = 1;
                SimonJoy_Iniciar();
                avanzar = 0;
            } else if (screen == DEMO_JUGANDO) {
                GuitarHero_IntentarGolpe();
                avanzar = 0;
            } else if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {
                /* confirma jugadores y pasa a elegir modo */
                jugadores_seleccionados = (screen == DEMO_JUGADORES_2) ? 2 : 1;
                screen = DEMO_MODO_SIMON;
                Demo_Enter(screen);
                avanzar = 0;
            } else if (screen == DEMO_MODO_SIMON) {
                /* confirma "Modo 1: Botones" (Simon Clasico, 2 jugadores con
                 * sus propios 4 botones) -- arranca el conteo */
                modo_confirmado_es_joys = 0;
                screen = DEMO_CONTEO_3;
                Demo_Enter(screen);
                Buzzer_Beep(100);
                conteo_auto = 1;
                conteo_tick = HAL_GetTick();
                avanzar = 0;
            } else if (screen == DEMO_MODO_SIMONJOY) {
                /* confirma "Modo 2: Simon+Joystick" -- arranca el conteo;
                 * el destino (1 o 2 jugadores) se decide al llegar a GO */
                modo_confirmado_es_joys = 1;
                screen = DEMO_CONTEO_3;
                Demo_Enter(screen);
                Buzzer_Beep(100);
                conteo_auto = 1;
                conteo_tick = HAL_GetTick();
                avanzar = 0;
            }
            /* DEMO_MENU_CANCIONES ya NO tiene caso especial aqui (ver mas
             * abajo, bloque "paso_joy" de la lista): antes "reproducir" y
             * "salir" eran 2 botones fisicos distintos (click=reproduce,
             * B1=sale). Al retirar el SW de ambos joystick (2026-07-30) solo
             * queda B1, asi que se reasigno: reproducir ahora es automatico
             * al mover el cursor, y B1 vuelve a significar SIEMPRE "salir"
             * (cae al avance generico de mas abajo, igual que cualquier otra
             * pantalla sin caso especial). */
        }

        if (conteo_auto) {
            if (HAL_GetTick() - conteo_tick >= CONTEO_PASO_MS) {
                conteo_tick = HAL_GetTick();
                if (screen == DEMO_CONTEO_GO) {
                    conteo_auto = 0;
                    if (modo_confirmado_es_joys) {
                        screen = DEMO_PREVIEW_SIMONJOY;  /* a donde vuelve al salir del juego */
                        if (jugadores_seleccionados == 2) {
                            en_juego_real_2p = 1;
                            SimonJoy2_IniciarAmbos();
                        } else {
                            en_juego_real = 1;
                            SimonJoy_Iniciar();
                        }
                    } else {
                        screen = DEMO_PREVIEW_SIMON;  /* a donde vuelve al salir del juego */
                        if (jugadores_seleccionados == 2) {
                            en_juego_real_botones = 1;
                            Botones_IniciarAmbos();
                        } else {
                            /* BOTONES a 1 jugador: sin logica real todavia
                             * (solo el recorrido animado) */
                            Demo_Enter(screen);
                        }
                    }
                } else {
                    screen++;
                    Demo_Enter(screen);
                    if (screen == DEMO_CONTEO_GO) {
                        /* fanfarria corta de arranque: sol4->do5 sostenido */
                        static const PasoSonido_t BEEP_GO[3] = {
                            { SOL4, 90 }, { 0, 20 }, { DO5, 220 }
                        };
                        Buzzer_Patron(BEEP_GO, 3);
                    } else {
                        Buzzer_Beep(100);  /* beep corto para "2" y "1" */
                    }
                }
            }
            HAL_Delay(GAME_TICK_MS);
            continue;
        }

        if (en_juego_real) {
            if (avanzar) {
                /* B1 sale del juego real y vuelve a la vista previa */
                en_juego_real = 0;
                Demo_Enter(screen);
            } else {
                SimonJoy_Actualizar();
            }
            HAL_Delay(GAME_TICK_MS);
            continue;
        }

        if (en_juego_real_2p) {
            if (avanzar) {
                /* B1 sale del juego real (ambos lados) y vuelve al recorrido */
                en_juego_real_2p = 0;
                ILI9341_SetPortrait(0);
                Demo_Enter(screen);
            } else {
                SimonJoy2_Actualizar();
            }
            HAL_Delay(GAME_TICK_MS);
            continue;
        }

        if (en_juego_real_botones) {
            if (avanzar) {
                /* B1 sale del juego real (ambos lados) y vuelve al recorrido */
                en_juego_real_botones = 0;
                for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
                Demo_Enter(screen);
            } else {
                Botones_Actualizar();
            }
            HAL_Delay(GAME_TICK_MS);
            continue;
        }

        switch (screen) {

        /* Pantallas animadas por el dispatcher real del juego (renderer.c) */
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

        /* Vistas previas de Simon Clasico / Simon+Joystick: redibujan solo
         * cuando cambia el paso "encendido" (cada 500ms, ver Demo_PasoSimon) */
        case DEMO_PREVIEW_SIMON:
        case DEMO_PREVIEW_SIMONJOY: {
            uint8_t paso = Demo_PasoSimon();
            if (paso != last_paso_sim) {
                if (screen == DEMO_PREVIEW_SIMON) {
                    Renderer_DrawModoSimonClasico(paso, paso);
                } else {
                    /* incremental: solo la flecha que cambio, sin FillScreen completo
                     * (misma funcion que usa el juego real en SimonJoy_MostrarPaso) */
                    Renderer_UpdateModoSimonJoystick1P(last_paso_sim, paso);
                }
                last_paso_sim = paso;
            }
            break;
        }

        default:
            /* Jugadores / Modo / Conteo: pantallas estaticas, nada que animar */
            break;
        }

        if (screen == DEMO_MENU_CANCIONES && paso_joy && !avanzar) {
            /* dentro de la lista, un boton arcade solo mueve el cursor -- no
             * cambia de pantalla (B1 es lo unico que sale de aqui). La
             * cancion seleccionada se reproduce sola al llegar a ella (ya
             * no hay click de SW para pedirlo bajo demanda, ver el
             * comentario grande junto a "if (avanzar)" mas arriba). */
            uint8_t cancion_cursor_ant = cancion_cursor;
            cancion_cursor = (uint8_t)((cancion_cursor + CANCIONES_N + paso_joy) % CANCIONES_N);
            Renderer_UpdateListaCanciones(cancion_cursor_ant, cancion_cursor);
            Buzzer_Patron(CANCIONES_DATA[cancion_cursor], CANCIONES_LEN[cancion_cursor]);
        } else if (avanzar || paso_joy) {
            uint8_t screen_ant = screen;
            /* solo avanza -- ya no existe "retroceder" (era el joystick
             * hacia arriba, retirado de la navegacion). El recorrido es
             * ciclico, asi que cualquier pantalla se alcanza presionando
             * B1 o un boton arcade las veces que haga falta. */
            screen = (uint8_t)((screen + 1) % DEMO_COUNT);
            last_paso_sim = 0xFF;

            /* si el movimiento se queda DENTRO de la misma pantalla de
             * seleccion (jugadores o modo), solo mueve el cursor -- sin
             * FillScreen -- en vez de redibujar todo con Demo_Enter() */
            uint8_t en_jugadores_ant = (screen_ant == DEMO_JUGADORES_1 || screen_ant == DEMO_JUGADORES_2);
            uint8_t en_jugadores_nuevo = (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2);
            uint8_t en_modo_ant = (screen_ant >= DEMO_MODO_SIMON && screen_ant <= DEMO_MODO_GUITAR);
            uint8_t en_modo_nuevo = (screen >= DEMO_MODO_SIMON && screen <= DEMO_MODO_GUITAR);

            if (en_jugadores_ant && en_jugadores_nuevo) {
                Renderer_UpdateSeleccionJugadores((uint8_t)(screen_ant - DEMO_JUGADORES_1),
                                                   (uint8_t)(screen - DEMO_JUGADORES_1));
            } else if (en_modo_ant && en_modo_nuevo) {
                Renderer_UpdateSeleccionModo((uint8_t)(screen_ant - DEMO_MODO_SIMON),
                                              (uint8_t)(screen - DEMO_MODO_SIMON));
            } else {
                Demo_Enter(screen);
            }
        }

        HAL_Delay(RENDER_TICK_MS);   /* ~30fps + debounce simple de B1 */
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
/* En el proyecto original de practica_pantalla/ esta funcion solo inicializaba
 * los 3 pines de control del display (CS/DC/RST); crecio junto con el resto
 * del archivo a medida que se fueron agregando perifericos reales (B1,
 * joystick 1 y 2, los 8 botones arcade con sus LED, el buzzer). */

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

    /* pa1/pa4: entradas analogicas del joystick (pa1=vry, pa4=vrx) — misma
     * config que examen_parcial, pines separados para evitar continuidad
     * electrica entre ejes en el cableado */
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
     * hacia ULN2003A) -- ver BTN_SW/BTN_LED y board_pins.h */
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

    /* buzzer en pa6 -- salida digital normal, TIM4 la alterna por interrupcion
     * para generar el tono (ver MX_TIM4_Buzzer_Init) */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &g);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

/* -----------------------------------------------------------------------
 * TIM4: INTERRUPCION PERIODICA QUE ALTERNA PA6 POR SOFTWARE (TONO DEL BUZZER)
 * NOTA: no se usa TIM3_CH1 (el unico canal de hardware disponible en PA6)
 * porque TIM3 ya esta ocupado como disparador del ADC del joystick cada
 * 20ms, y el STM32F411 no tiene TIM13/14 para usar otro canal en ese pin.
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
/* === CALLBACKS DEL JOYSTICK ================================================ */
/* ========================================================================== */

/* tick de tim4: alterna pa6 -- dos flancos = un ciclo de la onda cuadrada
 * del tono actual (ver Buzzer_SetSalida) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    }
}

/* fin de conversion adc: filtro exponencial (N=2) + zona muerta, igual que
 * examen_parcial, ahora para 4 canales (2 joysticks). rank1=y1(pa1)
 * rank2=x1(pa4) rank3=y2(pc0) rank4=x2(pc1); se rearma tras el ultimo canal
 * para que el proximo tim3_trgo dispare la secuencia completa otra vez. */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        uint16_t valor = (uint16_t)HAL_ADC_GetValue(&hadc1);

        switch (adc_rank_actual) {
        case 0:
            filtro_adc_y += ((int32_t)valor - filtro_adc_y) / ADC_FILTRO_N;
            joystick_y = (filtro_adc_y > JOY_CENTRO - JOY_ZONA_MUERTA &&
                          filtro_adc_y < JOY_CENTRO + JOY_ZONA_MUERTA)
                       ? JOY_CENTRO : (uint16_t)filtro_adc_y;
            adc_rank_actual = 1;
            break;
        case 1:
            filtro_adc_x += ((int32_t)valor - filtro_adc_x) / ADC_FILTRO_N;
            joystick_x = (filtro_adc_x > JOY_CENTRO - JOY_ZONA_MUERTA &&
                          filtro_adc_x < JOY_CENTRO + JOY_ZONA_MUERTA)
                       ? JOY_CENTRO : (uint16_t)filtro_adc_x;
            adc_rank_actual = 2;
            break;
        case 2:
            filtro_adc_y2 += ((int32_t)valor - filtro_adc_y2) / ADC_FILTRO_N;
            joystick2_y = (filtro_adc_y2 > JOY_CENTRO - JOY_ZONA_MUERTA &&
                           filtro_adc_y2 < JOY_CENTRO + JOY_ZONA_MUERTA)
                        ? JOY_CENTRO : (uint16_t)filtro_adc_y2;
            adc_rank_actual = 3;
            break;
        default:
            filtro_adc_x2 += ((int32_t)valor - filtro_adc_x2) / ADC_FILTRO_N;
            joystick2_x = (filtro_adc_x2 > JOY_CENTRO - JOY_ZONA_MUERTA &&
                           filtro_adc_x2 < JOY_CENTRO + JOY_ZONA_MUERTA)
                        ? JOY_CENTRO : (uint16_t)filtro_adc_x2;
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
