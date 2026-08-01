/**
 ******************************************************************************
 * @file    main.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Prueba de sonido — buzzer en PA6, Nucleo-F411RE. Tipo sin confirmar.
 *
 * Entorno aislado solo para el buzzer, separado de practica_pantalla (que
 * antes asumio que este buzzer era ACTIVO). El usuario no esta seguro de que
 * modulo compro: puede ser activo (oscilador interno, un solo tono, suena
 * con una senal DC) o pasivo (sin oscilador, necesita una onda cuadrada de
 * 2-5kHz, "TAR-BUZZER-PAS" de su tienda dice explicitamente que NO suena con
 * DC). PA6 se maneja por TIM3_CH1 (PWM) para poder generar ambas cosas sin
 * recablear nada — el propio oido del usuario decide cual es, escuchando:
 *
 *   1) DC corto / DC largo / DC doble  -> si suenan (tono fijo, sin variar
 *      de altura), el buzzer es ACTIVO.
 *   2) Tono fijo 3kHz / sirena (barrido continuo) / canciones reales
 *      -> si estos SI sueltan notas distintas entre si (y el DC de arriba NO
 *      sono nada o solo un click), el buzzer es PASIVO.
 *
 * Canciones (confirman fuerte que el buzzer es pasivo: son codigo tipo
 * tone()/noTone() de Arduino, que solo suena en un buzzer pasivo):
 *   - Del PDF "Canciones y frecuencias.pdf" que el usuario subio a esta
 *     carpeta: Estrellita donde estas, Himno de la Alegria, Martinillo
 *     (Fray Santiago) y una etiquetada "Navidad" que en realidad es Jingle
 *     Bells (el PDF dejo el comentario viejo "// Martinillo" por error).
 *   - De github.com/daironln/BuzzerMelody (a su vez de
 *     github.com/robsoncouto/arduino-songs): Marcha Imperial (Star Wars) y
 *     el tema de Tetris — formato de duracion por codigo (4=negra,
 *     8=corchea..., negativo=con puntillo), convertido a ms con la misma
 *     formula que usa BuzzerMelody.cpp.
 *
 * Se reproducen SOLAS una tras otra (todo el arreglo PRUEBAS[], incluyendo
 * los tests de diagnostico DC/tono/sirena de arriba) — al terminar una,
 * arranca la siguiente despues de una pausa corta. B1 (PC13, flanco de
 * bajada) salta de inmediato a la siguiente sin esperar a que termine la
 * actual (util para saltarse una cancion larga). El click (SW, PA0) del
 * joystick PARA la cancion/prueba actual al toque (silencio inmediato); el
 * autoplay la deja seguir a la siguiente sola, como si hubiera terminado.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "board_pins.h"

TIM_HandleTypeDef htim3;

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_PWM_Init(void);

/* ========================================================================== */
/* === SALIDA PWM HACIA EL BUZZER (TIM3_CH1, PA6) ============================ */
/* ========================================================================== */
/* TIM3CLK = 16MHz (HSI sin PLL, APB1 prescaler=1 -> sin duplicar). Prescaler
 * fijo a 15 -> tick de 1MHz (1us), asi el periodo/frecuencia se calculan con
 * una simple division entera: ARR = 1000000/freq_hz - 1. */
#define BUZZER_TIM_TICK_HZ  1000000U

/* freq_hz == 0      -> silencio (CCR=0)
 * freq_hz == 0xFFFF -> senal DC pura (CCR > ARR, sin oscilar) -- prueba de activo
 * cualquier otro valor -> tono PWM 50% duty a esa frecuencia -- prueba de pasivo */
#define BUZZER_DC   0xFFFFu

static void Buzzer_SetSalida(uint16_t freq_hz) {
    if (freq_hz == 0) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    } else if (freq_hz == BUZZER_DC) {
        __HAL_TIM_SET_AUTORELOAD(&htim3, 999);            /* periodo irrelevante */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 1001); /* CCR > ARR -> nivel alto fijo */
    } else {
        uint32_t arr = (BUZZER_TIM_TICK_HZ / freq_hz) - 1;
        __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (arr + 1) / 2);  /* 50% duty */
    }
}

/* ========================================================================== */
/* === SECUENCIADOR NO BLOQUEANTE DE PASOS (freq_hz, duracion_ms) ============ */
/* ========================================================================== */

typedef struct {
    uint16_t freq_hz;   /* 0=silencio, 0xFFFF=DC, otro=tono en Hz */
    uint16_t dur_ms;
} PasoSonido_t;

/* Apunta directo al arreglo const de la cancion/prueba (sin copiar) -- las
 * canciones reales (Himno, Estrellita) tienen decenas de notas, copiarlas a
 * un buffer de tamano fijo obligaria a un limite artificial. */
static const PasoSonido_t *patron_pasos = 0;
static uint16_t patron_n    = 0;
static uint16_t patron_pos  = 0;
static uint32_t patron_tick = 0;

static void Patron_Iniciar(const PasoSonido_t *pasos, uint16_t n) {
    patron_pasos = pasos;
    patron_n     = n;
    patron_pos   = 0;
    patron_tick  = HAL_GetTick();
    Buzzer_SetSalida(patron_pasos[0].freq_hz);
}

static uint8_t Patron_Sonando(void) {
    return patron_pos < patron_n;
}

/* llamar una vez por vuelta del loop principal, sin importar el estado */
static void Patron_Actualizar(void) {
    if (patron_pos >= patron_n) return;
    if (HAL_GetTick() - patron_tick < patron_pasos[patron_pos].dur_ms) return;

    patron_pos++;
    patron_tick = HAL_GetTick();
    if (patron_pos >= patron_n) {
        Buzzer_SetSalida(0);
    } else {
        Buzzer_SetSalida(patron_pasos[patron_pos].freq_hz);
    }
}

/* ========================================================================== */
/* === SIRENA — BARRIDO CONTINUO DE FRECUENCIA (solo audible si es pasivo) === */
/* ========================================================================== */
#define SIRENA_F_MIN     400U
#define SIRENA_F_MAX    3000U
#define SIRENA_PASO_MS    15U   /* cada cuanto sube/baja el tono */
#define SIRENA_PASO_HZ   40U    /* cuanto sube/baja por paso     */
#define SIRENA_DUR_MS  1800U    /* duracion total del efecto     */

static uint8_t  sirena_activa = 0;
static uint16_t sirena_freq   = SIRENA_F_MIN;
static int8_t   sirena_dir    = 1;
static uint32_t sirena_tick_paso = 0;
static uint32_t sirena_tick_inicio = 0;

static void Sirena_Iniciar(void) {
    sirena_activa      = 1;
    sirena_freq         = SIRENA_F_MIN;
    sirena_dir          = 1;
    sirena_tick_paso    = HAL_GetTick();
    sirena_tick_inicio  = HAL_GetTick();
    Buzzer_SetSalida(sirena_freq);
}

static uint8_t Sirena_Sonando(void) {
    return sirena_activa;
}

static void Sirena_Actualizar(void) {
    if (!sirena_activa) return;
    uint32_t ahora = HAL_GetTick();

    if (ahora - sirena_tick_inicio >= SIRENA_DUR_MS) {
        sirena_activa = 0;
        Buzzer_SetSalida(0);
        return;
    }
    if (ahora - sirena_tick_paso < SIRENA_PASO_MS) return;
    sirena_tick_paso = ahora;

    int32_t nueva = (int32_t)sirena_freq + sirena_dir * (int32_t)SIRENA_PASO_HZ;
    if (nueva >= (int32_t)SIRENA_F_MAX) { nueva = SIRENA_F_MAX; sirena_dir = -1; }
    if (nueva <= (int32_t)SIRENA_F_MIN) { nueva = SIRENA_F_MIN; sirena_dir = 1; }
    sirena_freq = (uint16_t)nueva;
    Buzzer_SetSalida(sirena_freq);
}

/* ========================================================================== */
/* === LISTA DE PRUEBAS DE SONIDO ============================================ */
/* ========================================================================== */

/* --- Grupo 1: senal DC -- si suenan (tono fijo, sin variar), es ACTIVO --- */
static const PasoSonido_t PRUEBA_DC_CORTO[] = { { BUZZER_DC, 90 } };
static const PasoSonido_t PRUEBA_DC_LARGO[] = { { BUZZER_DC, 400 } };
static const PasoSonido_t PRUEBA_DC_DOBLE[] = {
    { BUZZER_DC, 70 }, { 0, 60 }, { BUZZER_DC, 70 }
};

/* --- Grupo 2: tono PWM variable -- si suenan notas distintas, es PASIVO --- */
static const PasoSonido_t PRUEBA_TONO_FIJO[] = { { 3000, 400 } };

/* ========================================================================== */
/* === GRUPO 3: CANCIONES REALES ============================================= */
/* ========================================================================== */
/* Transcritas del PDF que el usuario subio a esta carpeta ("Canciones y
 * frecuencias.pdf" — sketches de Arduino con tone()/noTone(), que solo tienen
 * sentido con un buzzer PASIVO: confirma la sospecha de la nota anterior.
 * Frecuencias tomadas de la tabla de notas del mismo PDF (temperamento igual). */

/* Octava 4 (Estrellita, Martinillo en el PDF) */
#define DO4   262
#define RE4   294
#define MI4   330
#define FA4   349
#define SOL4  392
#define LA4   440
#define SI4   494

/* Octava 5 (Himno de la Alegria, Navidad/Jingle Bells en el PDF) */
#define DO5   523
#define RE5   587
#define MI5   659
#define FA5   698
#define SOL5  784
#define LA5   880
#define SI5   988

/* Duraciones: negra=220ms a un tempo vivo mas facil de escuchar en pruebas
 * cortas que el original del PDF (que usaba valores confusos/muy lentos).
 * GAP: silencio breve tras cada nota para separarla de la siguiente (incluso
 * si es la misma altura repetida, como "Do Do" en Estrellita). */
#define NEGRA    220
#define CORCHEA  110
#define BLANCA   440
#define GAP_MS    20

/* N(frecuencia, duracion) -> nota + su silencio de separacion. Se usa dentro
 * de un inicializador de arreglo, por eso expande a DOS entradas. */
#define N(f, d)  { (f), (d) }, { 0, GAP_MS }

/* --- Estrellita donde estas (Twinkle Twinkle) --- */
static const PasoSonido_t CANCION_ESTRELLITA[] = {
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

/* --- Himno de la Alegria (Oda a la Alegria, Beethoven) --- */
static const PasoSonido_t CANCION_HIMNO[] = {
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

/* --- Martinillo (Fray Santiago / Frere Jacques) --- */
static const PasoSonido_t CANCION_MARTINILLO[] = {
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(DO4, NEGRA), N(RE4, NEGRA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(MI4, NEGRA), N(FA4, NEGRA), N(SOL4, BLANCA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(SOL4, CORCHEA), N(LA4, CORCHEA), N(SOL4, CORCHEA), N(FA4, CORCHEA), N(MI4, NEGRA), N(DO4, NEGRA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
    N(RE4, NEGRA), N(SOL4, NEGRA), N(DO4, BLANCA),
};

/* --- "Navidad" (Jingle Bells, etiquetado "Martinillo" por error en el PDF) --- */
static const PasoSonido_t CANCION_NAVIDAD[] = {
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(SOL5, CORCHEA), N(DO5, CORCHEA), N(RE5, CORCHEA), N(MI5, BLANCA),
    N(FA5, CORCHEA), N(FA5, CORCHEA), N(FA5, NEGRA), N(FA5, CORCHEA), N(MI5, CORCHEA), N(MI5, NEGRA),
    N(MI5, CORCHEA), N(RE5, CORCHEA), N(RE5, CORCHEA), N(MI5, CORCHEA), N(RE5, NEGRA), N(SOL5, NEGRA),
    N(SOL5, CORCHEA), N(SOL5, CORCHEA), N(FA5, CORCHEA), N(RE5, CORCHEA), N(DO5, NEGRA),
};

/* ========================================================================== */
/* === BONUS: 2 CANCIONES DE LOS REPOS QUE EL USUARIO SEÑALO ================= */
/* ========================================================================== */
/* github.com/daironln/BuzzerMelody (ejemplos starwars/tetris, a su vez
 * tomados de github.com/robsoncouto/arduino-songs) usan un formato de
 * duracion por codigo en vez de ms: 4=negra, 8=corchea, 16=semicorchea...,
 * y NEGATIVO=con puntillo (le suma la mitad de su propia duracion). Formula
 * exacta copiada de BuzzerMelody.cpp (playMelody()): duracion_ms =
 * (240000/tempo)/codigo, y si el codigo es negativo, esa duracion x1.5.
 * NT(nota,tempo,codigo) reutiliza el macro N(freq,dur) de arriba para
 * mantener el mismo motor/formato de reproduccion que las 4 canciones del PDF. */
#define D(tempo, div) ((div) > 0 ? (240000 / (tempo)) / (div) \
                                  : ((240000 / (tempo)) / (-(div)) * 3) / 2)
#define NT(freq, tempo, div) N((freq), D((tempo), (div)))

#define NOTE_GS4  415
#define NOTE_A4   440
#define NOTE_AS4  466
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_G5   784
#define NOTE_GS5  831
#define NOTE_A5   880
#define NOTE_AS5  932
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_DS6 1245
#define NOTE_F6  1397

/* --- Tetris (tema principal / Korobeiniki), tempo=144 --- */
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

/* --- Jingle de bienvenida propio (arpegio Do-Mi-Sol, 3 octavas) --- */
/* Composicion propia del usuario para su proyecto Beat Clash/Simon Guitar
 * Hero (ver sonidos/beat_clash_welcome_jingle.ino) -- no es una melodia de
 * terceros, solo un arpegio generico de acorde mayor. */
static const PasoSonido_t CANCION_BEATCLASH[] = {
    N(DO4, 120), N(MI4, 120), N(SOL4, 120),
    N(DO5, 180), N(MI5, 180), N(SOL5, 250), N(NOTE_C6, 400),
};

#undef NT
#undef D
#undef N

typedef enum { MODO_PATRON, MODO_SIRENA } ModoPrueba_t;
typedef struct {
    ModoPrueba_t        modo;
    const PasoSonido_t *pasos;   /* solo si modo == MODO_PATRON */
    uint16_t             n;
} Prueba_t;

#define P(arr) { MODO_PATRON, arr, (uint16_t)(sizeof(arr) / sizeof(arr[0])) }
static const Prueba_t PRUEBAS[] = {
    P(PRUEBA_DC_CORTO),
    P(PRUEBA_DC_LARGO),
    P(PRUEBA_DC_DOBLE),
    P(PRUEBA_TONO_FIJO),
    { MODO_SIRENA, 0, 0 },   /* barrido continuo -- solo audible si es pasivo */
    P(CANCION_ESTRELLITA),
    P(CANCION_HIMNO),
    P(CANCION_MARTINILLO),
    P(CANCION_NAVIDAD),
    P(CANCION_TETRIS),
    P(CANCION_BEATCLASH),
};
#define PRUEBAS_N (sizeof(PRUEBAS) / sizeof(PRUEBAS[0]))
#undef P

static void Prueba_Reproducir(uint8_t i) {
    if (PRUEBAS[i].modo == MODO_SIRENA) {
        Sirena_Iniciar();
    } else {
        Patron_Iniciar(PRUEBAS[i].pasos, PRUEBAS[i].n);
    }
}

static uint8_t Prueba_Sonando(void) {
    return Patron_Sonando() || Sirena_Sonando();
}

/* Corta lo que este sonando ya mismo (patron o sirena), sin avanzar todavia
 * a la siguiente prueba -- el loop principal la deja avanzar sola despues
 * de la pausa normal de autoplay, como si hubiera terminado por su cuenta. */
static void Prueba_Detener(void) {
    patron_pos    = patron_n;   /* marca el patron como terminado */
    sirena_activa = 0;
    Buzzer_SetSalida(0);
}

/* Pausa entre el final de una prueba y el arranque automatico de la siguiente */
#define AUTOPLAY_PAUSA_MS 600U

/* Detecta el flanco de presion de B1 (activo BAJO, pull-up externo en la Nucleo) */
static uint8_t Boton_B1_Flanco(void) {
    static GPIO_PinState prev = GPIO_PIN_SET;
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);
    uint8_t flanco = (prev == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
    prev = cur;
    return flanco;
}

/* Detecta el flanco del click (SW) del joystick -- para la cancion actual */
static uint8_t Joystick_SW_Flanco(void) {
    static GPIO_PinState prev = GPIO_PIN_SET;
    GPIO_PinState cur = HAL_GPIO_ReadPin(JOY_SW_PORT, JOY_SW_PIN);
    uint8_t flanco = (prev == GPIO_PIN_SET && cur == GPIO_PIN_RESET);
    prev = cur;
    return flanco;
}

/* ========================================================================== */
/* === MAIN =================================================================== */
/* ========================================================================== */

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM3_PWM_Init();
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    uint8_t  prueba_actual = 0;
    uint8_t  esperando_siguiente = 0;   /* pausa corta entre pruebas antes de autoreproducir */
    uint32_t tick_fin = 0;
    Prueba_Reproducir(prueba_actual);

    while (1) {
        Patron_Actualizar();
        Sirena_Actualizar();

        /* B1: salta de inmediato a la siguiente prueba, aunque la actual
         * siga sonando (util para saltarse una cancion larga) */
        if (Boton_B1_Flanco()) {
            prueba_actual        = (uint8_t)((prueba_actual + 1) % PRUEBAS_N);
            esperando_siguiente  = 0;
            Prueba_Reproducir(prueba_actual);
        }
        /* click (SW) del joystick: para la cancion/prueba actual ya mismo,
         * sin saltar todavia -- el autoplay de abajo la deja pasar a la
         * siguiente sola tras la pausa normal, como si hubiera terminado */
        if (Joystick_SW_Flanco()) {
            Prueba_Detener();
        }

        /* autoreproduccion: al terminar una prueba sola, arranca la
         * siguiente despues de una pausa corta (pedido explicito del
         * usuario: "reproducir una a la vez cada vez que termine la otra") */
        else if (!Prueba_Sonando()) {
            if (!esperando_siguiente) {
                esperando_siguiente = 1;
                tick_fin            = HAL_GetTick();
            } else if (HAL_GetTick() - tick_fin >= AUTOPLAY_PAUSA_MS) {
                esperando_siguiente = 0;
                prueba_actual       = (uint8_t)((prueba_actual + 1) % PRUEBAS_N);
                Prueba_Reproducir(prueba_actual);
            }
        }

        HAL_Delay(5);
    }
}

/* ========================================================================== */
/* === RELOJ DEL SISTEMA ===================================================== */
/* ========================================================================== */

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSI 16MHz sin PLL — de sobra para GPIO/PWM simple */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;      /* APB1  = 16MHz (TIM3CLK = 16MHz, prescaler=1 -> sin duplicar) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* APB2  = 16MHz */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

/* ========================================================================== */
/* === GPIO — B1 y click del joystick (ENTRADA). PA6 lo configura            */
/* === MX_TIM3_PWM_Init como AF ============================================= */
/* ========================================================================== */

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();   /* PA0 = click del joystick */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* B1 (PC13) — pull-up externo R30=4k7 en la Nucleo, no usar PULLUP sw */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    g.Pin  = BTN_USER_PIN;
    HAL_GPIO_Init(BTN_USER_PORT, &g);

    /* PA0: click (SW) del joystick, activo bajo, pull-up interno (el modulo
     * no trae resistencia propia) */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin  = JOY_SW_PIN;
    HAL_GPIO_Init(JOY_SW_PORT, &g);
}

/* ========================================================================== */
/* === TIM3 CH1 (PA6) EN MODO PWM — SALIDA HACIA EL BUZZER =================== */
/* ========================================================================== */

static void MX_TIM3_PWM_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = BUZZER_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = BUZZER_TIM_AF;
    HAL_GPIO_Init(BUZZER_PORT, &g);

    htim3.Instance           = TIM3;
    htim3.Init.Prescaler     = 15;    /* 16MHz/16 = 1MHz -> tick de 1us */
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim3.Init.Period        = 999;   /* arranca en silencio a 1kHz, se ajusta en runtime */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim3);

    TIM_OC_InitTypeDef ocConfig = {0};
    ocConfig.OCMode     = TIM_OCMODE_PWM1;
    ocConfig.Pulse      = 0;          /* arranca en silencio */
    ocConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    ocConfig.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &ocConfig, TIM_CHANNEL_1);
}

/* ========================================================================== */
/* === MANEJO DE ERRORES ===================================================== */
/* ========================================================================== */

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
