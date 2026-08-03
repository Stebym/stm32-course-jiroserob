/**
 ******************************************************************************
 * @file    main.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Logica de aplicacion de Beat Clash, consola de juego arcade tipo
 *          "Simon Dice / Guitar Hero" para 2 jugadores, sobre pantalla
 *          ILI9341 de 320x240 pixeles.
 *
 * Este archivo contiene la logica completa de aplicacion sobre la capa de
 * HAL de STM32Cube: inicializacion de perifericos, la maquina de estados de
 * pantallas del recorrido, y la implementacion de dos de los tres modos de
 * juego (Simon + Joystick y Simon con botones arcade, ambos jugables a 1 o
 * 2 jugadores en configuracion cara a cara). El renderizado (dibujo sobre
 * la pantalla) esta separado en renderer.c; este archivo se concentra en el
 * estado del juego y la lectura de entradas (botones, joystick).
 *
 * Navegacion del recorrido de pantallas: el boton B1 (PC13) o el eje
 * vertical de cualquiera de los 2 joystick (PA1=VRy1 / PC0=VRy2) avanzan
 * entre pantallas. La lectura de joystick usa un disparo periodico del ADC
 * por TIM3 (TRGO cada 20 ms) combinado con un filtro EMA (media movil
 * exponencial) y una zona muerta central para evitar falsos positivos por
 * ruido. Las animaciones (parpadeo del splash, notas cayendo) permanecen
 * activas mientras el sistema espera una entrada del usuario.
 *
 * B1 es el unico boton de tipo "click" del sistema, ya que los pines SW de
 * ambos joystick fueron retirados fisicamente para simplificar el cableado
 * (ver board_pins.h). Por esta razon, B1 confirma los menus (cantidad de
 * jugadores, modo de juego) y da inicio al conteo regresivo 3-2-1-GO (ver
 * el bloque "if (avanzar)" dentro de main()). El reintento tras perder una
 * partida no depende de ningun boton dedicado: en Simon + Joystick se
 * dispara moviendo el propio joystick (ver el estado SJ_GAMEOVER en
 * SimonJoy_Actualizar / SimonJoy2_ActualizarJugador), y en el modo de
 * botones arcade, presionando cualquiera de los 4 botones propios del
 * jugador (ver Botones_ActualizarJugador). Este patron de "cualquier
 * entrada del propio jugador reintenta la partida" se aplica a los tres
 * modos de juego por consistencia de interfaz.
 *
 * El modo Guitar Hero con sincronizacion real de canciones fue removido de
 * una version anterior para reducir la complejidad del codigo mientras el
 * desarrollo se concentraba en completar los modos de joystick y botones
 * arcade; de esa version solo permanece GuitarHero_IntentarGolpe(), el
 * boton de prueba original dentro del recorrido de diseno (DEMO_JUGANDO).
 * Con Simon + Joystick y Simon + Botones ya jugables a 2 jugadores sobre
 * hardware real, la reconstruccion completa de Guitar Hero es el siguiente
 * objetivo de desarrollo del proyecto.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"          // trae todo el HAL de ST (GPIO, SPI, ADC, TIM, UART, HAL_Delay/HAL_GetTick, etc.)
#include "board_pins.h"             // todos los #define de pines fisicos (BTN1_*, BTN2_*, pantalla, joystick, buzzer)
#include "game_state.h"             // constantes de juego (tamaños, velocidades, puntajes) y struct GameState_t
#include "ili9341.h"                // prototipos del driver de la pantalla (ILI9341_*)
#include "renderer.h"               // prototipos de la capa de dibujo (Renderer_*)
#include <string.h>                 // strcpy/strcmp/memset usados mas abajo
#include <stdio.h>                  // printf/snprintf (printf sale por USART2, ver __io_putchar mas abajo)

SPI_HandleTypeDef hspi1;             // handle HAL del periferico SPI1, usado por la pantalla (SCK=PA5,MOSI=PA7,CS=PB6,DC=PC7)
ADC_HandleTypeDef hadc1;   /* joystick x/y */                          // handle HAL del ADC1, escanea los 4 canales de los 2 joystick en secuencia
TIM_HandleTypeDef htim3;   /* disparador del adc cada 20 ms */         // TIM3 en modo TRGO: cada 20ms dispara una conversion nueva del ADC1
TIM_HandleTypeDef htim4;   /* alterna pa6 por interrupcion para el tono del buzzer */  // TIM4 genera la interrupcion periodica que alterna PA6 (buzzer)
UART_HandleTypeDef huart2; /* consola de depuracion por el VCP del ST-Link (ver MX_USART2_UART_Init) */  // USART2, 115200 8N1, retargeteado a printf
static GameState_t gs;               // estado "global" de la partida en curso (puntajes, notas, etc.) — se resetea en cada Demo_Enter()

/* === JOYSTICK 1 =========================================================== */
/* Las lecturas se almacenan ya filtradas, en la escala nativa de 12 bits
 * (0-4095) del ADC (ver HAL_ADC_ConvCpltCallback para el filtrado). */
volatile uint16_t joystick_x = 2048;  // ultima lectura filtrada (EMA) del canal VRx de J1 (PA4/ADC1_IN4); volatile porque se escribe desde la rutina de interrupcion del ADC
volatile uint16_t joystick_y = 2048;  // ultima lectura filtrada (EMA) del canal VRy de J1 (PA1/ADC1_IN1); volatile porque se escribe desde la rutina de interrupcion del ADC
static uint8_t    adc_rank_actual = 0;   /* 0=y1(rank1) 1=x1(rank2) 2=y2(rank3) 3=x2(rank4) */   // indice de turno dentro del ciclo de 4 canales del ADC; modificarlo manualmente descuadraria el orden de lectura

/* Orden del filtro EMA (media movil exponencial) aplicado a cada eje del
 * ADC: un valor mayor suaviza mas la lectura mas a costa de mayor latencia
 * de respuesta. Con el centro de cada eje calibrado en el arranque (ver
 * Joystick_Calibrar), no es necesario sacrificar suavidad de la señal a
 * cambio de velocidad de deteccion de direccion. */
#define ADC_FILTRO_N     4
static int32_t filtro_adc_x = 2048;   // acumulador del filtro EMA para el canal X de J1
static int32_t filtro_adc_y = 2048;   // acumulador del filtro EMA para el canal Y de J1

/* === JOYSTICK 2 — segundo jugador (ver CABLEADO_JOYSTICK2.txt) ============
 * Utiliza el mismo esquema de filtro EMA y zona muerta que el joystick 1,
 * leido en los rangos 3 y 4 del mismo barrido del ADC1 (ver
 * ADC1_Joystick_Init / HAL_ADC_ConvCpltCallback). */
volatile uint16_t joystick2_x = 2048;  // ultima lectura filtrada del canal VRx2 de J2 (PC1/ADC1_IN11)
volatile uint16_t joystick2_y = 2048;  // ultima lectura filtrada del canal VRy2 de J2 (PC0/ADC1_IN10)

static uint8_t btn_modo_1p = 0;     /* 1 = solo el jugador logico 1 esta activo (el otro lado de la pantalla queda en blanco) */
static uint8_t guitar_modo_1p = 0;  /* 1 = solo el jugador logico 1 esta activo (el otro lado de la pantalla queda en blanco) */
static int32_t filtro_adc_x2 = 2048;   // acumulador EMA del canal X de J2
static int32_t filtro_adc_y2 = 2048;   // acumulador EMA del canal Y de J2

#define JOY_ZONA_MUERTA  150   // ancho de la banda "muerta" alrededor del centro donde se ignora el movimiento del filtro EMA (ver HAL_ADC_ConvCpltCallback); aumentar este valor hace el joystick menos sensible cerca del centro, disminuirlo lo hace mas propenso a ruido en reposo

/* CALIBRACION DE CENTRO: el joystick fisico de este montaje no descansa en
 * el valor 2048 (mitad teorica de la escala de 12 bits del ADC), sino en un
 * valor propio de cada unidad y cada eje, determinado por tolerancias
 * mecanicas y de fabricacion del potenciometro. Un centro fijo en 2048
 * haria que un eje casi nunca alcance el umbral de un lado (al tener que
 * recorrer casi toda la escala) mientras el otro lado dispara con
 * cualquier ruido (al estar ya cerca del umbral). Por ello, el centro de
 * cada eje se mide una vez al arrancar (ver Joystick_Calibrar en main(),
 * ejecutada tras HAL_ADC_Start_IT) y todos los umbrales de direccion se
 * calculan en relacion a ese centro medido, no a un valor fijo (ver
 * JOY_UMBRAL_DESVIO y Joy_Umbrales()). */
static uint16_t centro_j1x = 2048, centro_j1y = 2048;   // centro real medido de J1 al arrancar (ver calibracion en main()); inicia en 2048 (mitad teorica) hasta completar la medicion
static uint16_t centro_j2x = 2048, centro_j2y = 2048;   // idem para J2

/* El recorrido fisico de este joystick no es simetrico respecto al centro
 * medido: un lado de cada eje alcanza una distancia considerablemente mayor
 * que el lado opuesto antes de llegar al tope mecanico. Un umbral de
 * deteccion demasiado alto puede volver inalcanzable la direccion
 * correspondiente al lado con menor recorrido; el valor elegido deja
 * margen suficiente bajo el peor caso medido sin ser tan pequeño como para
 * disparar con el ruido normal del ADC en reposo. */
#define JOY_UMBRAL_DESVIO 280U   /* distancia (en cuentas del ADC) al centro medido necesaria para contar como una direccion valida */  // aumentar este valor exige un empuje mas fuerte del joystick para registrar una direccion (menor sensibilidad); disminuirlo dispara con empujes pequeños (mayor sensibilidad, mas riesgo de falsos positivos)

/* Rango de desviacion (en cuentas del ADC, a cada lado del centro medido)
 * que se considera "empuje a fondo" para el cursor visual "X" del modo de 2
 * jugadores (ver Joy_CursorEscala). Un poco mas generoso que
 * JOY_UMBRAL_DESVIO para dejar margen visible incluso antes de que la
 * direccion se registre como valida. */
#define JOY_CURSOR_RANGO 350U

/* Convierte una lectura cruda del ADC (0-4095) y el centro calibrado de ESE
 * eje en un valor sintetico tambien 0-4095, pero centrado en 2048 (reposo)
 * y que llega a 0/4095 con un empuje de JOY_CURSOR_RANGO cuentas. Sin esto,
 * el cursor "X" del modo 2 jugadores (que mapea su entrada directamente
 * sobre la escala completa 0-4095 del ADC, ver Renderer_ActualizarCursor-
 * Joystick2P) queda descentrado en reposo -- el centro real de este
 * joystick esta lejos de 2048 (ver la calibracion mas arriba) -- y un
 * empuje real, al ser una fraccion chica de la escala completa, apenas lo
 * mueve unos pocos pixeles dentro de su caja, dando la impresion de que se
 * mueve para cualquier lado en vez de en linea recta hacia la direccion
 * empujada. */
static uint16_t Joy_CursorEscala(uint16_t valor, uint16_t centro) {
    int32_t dev = (int32_t)valor - (int32_t)centro;   // desviacion con signo respecto al centro medido de este eje
    int32_t s   = 2048 + (dev * 2048) / (int32_t)JOY_CURSOR_RANGO;   // reescala esa desviacion para que +-JOY_CURSOR_RANGO caiga en 0/4095
    if (s < 0) s = 0;         // clamp por si el empuje supera el rango esperado
    if (s > 4095) s = 4095;
    return (uint16_t)s;
}

/* Calcula el rango [bajo,alto] alrededor de un centro medido, con clamp a
 * los limites reales del ADC de 12 bits (evita underflow/overflow si el
 * centro medido queda muy cerca de 0 o 4095). */
static void Joy_Umbrales(uint16_t centro, uint16_t *bajo, uint16_t *alto) {   // recibe el centro medido de UN eje y devuelve por puntero el rango [bajo,alto] "sin direccion" alrededor de ese centro
    int32_t b = (int32_t)centro - (int32_t)JOY_UMBRAL_DESVIO;   // limite inferior del rango, en 32 bits con signo para poder detectar si se fue negativo
    int32_t a = (int32_t)centro + (int32_t)JOY_UMBRAL_DESVIO;   // limite superior del rango, en 32 bits con signo para poder detectar si se paso de 4095
    *bajo = (uint16_t)(b < 0 ? 0 : b);      // si el centro esta muy cerca de 0, recorta a 0 en vez de dar la vuelta (underflow de un uint16_t)
    *alto = (uint16_t)(a > 4095 ? 4095 : a);   // si el centro esta muy cerca de 4095 (max de un ADC de 12 bits), recorta a 4095
}

/* === BOTONES ARCADE — 2 jugadores x 4 colores ==============================
 * Tablas indexadas por [jugador][color], donde color sigue el orden fijo
 * 0=ROJO 1=VERDE 2=AZUL 3=AMARILLO (ver board_pins.h para el cableado
 * fisico completo de cada pin). */
typedef struct { GPIO_TypeDef *port; uint16_t pin; } PinRef_t;   // par (puerto GPIO, numero de pin), usado para guardar pines en tablas indexables por [jugador][color]

/* Correspondencia entre jugador logico y hardware fisico: el jugador
 * logico 0 (mostrado en pantalla como "jugador 1") se controla con el
 * conjunto de pines fisico BTN2_*, y el jugador logico 1 con BTN1_* -- esta
 * es la unica tabla que requiere modificarse si el cableado fisico de los
 * botones cambia, ya que Botones_LeerColor / Boton_LED / ComboSalir_Detec-
 * tado y el resto de funciones indexan siempre por jugador logico, nunca
 * por el nombre BTN1/BTN2 directamente. Ver tambien JugadorFisico() mas
 * abajo para la correspondencia equivalente en el joystick, que al ser
 * variables globales sueltas (no una tabla) requiere invertir el indice en
 * vez de reordenar los datos. */
static const PinRef_t BTN_SW[2][4] = {              // pines de lectura (switch) de los botones arcade, indexados [jugador logico][color 0-3]
    { { BTN2_ROJO_SW_PORT, BTN2_ROJO_SW_PIN }, { BTN2_VERDE_SW_PORT, BTN2_VERDE_SW_PIN },      // fila [0]: jugador logico 0, hardware fisico BTN2_* (ver comentario de correspondencia logico/fisico arriba)
      { BTN2_AZUL_SW_PORT, BTN2_AZUL_SW_PIN }, { BTN2_AMARILLO_SW_PORT, BTN2_AMARILLO_SW_PIN } },  // columnas 2 y 3 de la fila [0]: azul y amarillo, hardware BTN2
    { { BTN1_ROJO_SW_PORT, BTN1_ROJO_SW_PIN }, { BTN1_VERDE_SW_PORT, BTN1_VERDE_SW_PIN },      // fila [1]: jugador logico 1, hardware fisico BTN1_*
      { BTN1_AZUL_SW_PORT, BTN1_AZUL_SW_PIN }, { BTN1_AMARILLO_SW_PORT, BTN1_AMARILLO_SW_PIN } }   // columnas 2 y 3 de la fila [1]: azul y amarillo, hardware BTN1
};

static const PinRef_t BTN_LED[2][4] = {              // pines de escritura (LED, a traves del driver ULN2003A) de los botones arcade, misma indexacion [jugador logico][color]
    { { BTN2_ROJO_LED_PORT, BTN2_ROJO_LED_PIN }, { BTN2_VERDE_LED_PORT, BTN2_VERDE_LED_PIN },   // fila [0]: LEDs del conjunto fisico BTN2 (jugador logico 0)
      { BTN2_AZUL_LED_PORT, BTN2_AZUL_LED_PIN }, { BTN2_AMARILLO_LED_PORT, BTN2_AMARILLO_LED_PIN } },  // columnas azul y amarillo del conjunto BTN2
    { { BTN1_ROJO_LED_PORT, BTN1_ROJO_LED_PIN }, { BTN1_VERDE_LED_PORT, BTN1_VERDE_LED_PIN },   // fila [1]: LEDs del conjunto fisico BTN1 (jugador logico 1)
      { BTN1_AZUL_LED_PORT, BTN1_AZUL_LED_PIN }, { BTN1_AMARILLO_LED_PORT, BTN1_AMARILLO_LED_PIN } }   // columnas azul y amarillo del conjunto BTN1
};

/* Correspondencia logico/fisico equivalente a la de arriba, aplicada al
 * joystick: como las lecturas joystick_x/y (J1) y joystick2_x/y (J2) son
 * variables globales sueltas y no una tabla indexable, se invierte el
 * indice justo antes de elegir cual leer, en vez de reordenar los datos.
 * Debe usarse unicamente en los puntos que leen hardware crudo del
 * joystick; la capa de renderizado sigue recibiendo el jugador logico sin
 * pasar por esta funcion, de modo que la disposicion en pantalla no se ve
 * afectada. */
static inline uint8_t JugadorFisico(uint8_t jugador_logico) {   // convierte jugador logico (0/1, el que muestra la pantalla) en jugador fisico (que joystick de hardware leer)
    return (uint8_t)(1U - jugador_logico);   // invierte el indice (0->1, 1->0); si el cableado fisico se intercambiara de nuevo, esta es la unica linea a modificar para el joystick
}

static GPIO_PinState btn_prev[2][4] = {   // ultimo nivel leido (sin confirmar aun como estable) de cada boton, [jugador][color], usado por Botones_LeerColor para detectar flancos
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },   // jugador 0: inicia en SET (activo en bajo, es decir, no presionado, con resistencia de pull-up)
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }    // jugador 1: idem
};

/* Antirrebote (debounce) por tiempo: exige que el nuevo nivel de un pin se
 * mantenga estable durante BTN_DEBOUNCE_MS antes de aceptarlo como una
 * pulsacion real, en lugar de comparar unicamente contra el nivel leido en
 * el ciclo anterior del bucle principal. Esto evita que un rebote mecanico
 * del contacto o un pico de ruido electrico de un solo ciclo se interprete
 * como una pulsacion o cambio de pantalla valido. */
#define BTN_DEBOUNCE_MS 30U   // milisegundos que un nivel nuevo debe mantenerse estable antes de contarlo como cambio real; aumentar este valor hace la respuesta de los botones mas lenta pero mas inmune a rebotes, disminuirlo la hace mas rapida pero mas propensa a falsos flancos
static GPIO_PinState btn_estable[2][4] = {   // ultimo nivel ya confirmado (estable) de cada boton, [jugador][color]
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET },   // jugador 0, inicia sin ningun boton presionado
    { GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET }    // jugador 1, idem
};
static uint32_t btn_tick_cambio[2][4];   // tick (HAL_GetTick()) del ultimo cambio de nivel detectado por cada boton, usado para medir cuanto tiempo lleva estable

static void Boton_LED(uint8_t p, uint8_t c, uint8_t on) {   // enciende/apaga el LED del boton [jugador p][color c]
    if (btn_modo_1p || guitar_modo_1p) p = 0; // en modo de 1 jugador, los LEDs siempre corresponden al conjunto fisico del jugador logico 0, independientemente del jugador logico que se este actualizando
    HAL_GPIO_WritePin(BTN_LED[p][c].port, BTN_LED[p][c].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);   // el ULN2003A es un driver en configuracion "sink", pero desde el GPIO se maneja como una salida normal: SET enciende, RESET apaga
}

/* Animacion de encendido secuencial tipo "tira LED": mantiene un unico LED
 * encendido a la vez, recorriendo los 8 botones (los 4 de J1 y luego los 4
 * de J2) en bucle mientras se muestra la pantalla de bienvenida. La
 * animacion no bloquea la ejecucion: avanza un paso por cada vuelta del
 * bucle principal (ver BotonesChase_Actualizar, invocada solo mientras
 * screen == DEMO_SPLASH) y se apaga por completo al salir de la pantalla
 * de bienvenida (ver BotonesChase_Detener). */
#define CHASE_LED_PASO_MS 180U   // duracion en milisegundos de cada LED encendido antes de pasar al siguiente; disminuir este valor acelera la animacion, aumentarlo la hace mas lenta
static uint32_t chase_led_tick   = 0;   // tick del ultimo paso de la animacion
static uint8_t  chase_led_idx    = 0;   // indice 0-7 del LED actualmente prendido (0-3=J1 rojo/verde/azul/amarillo, 4-7=J2)
static uint8_t  chase_led_activo = 0;   // 1 mientras la animacion esta corriendo (solo durante DEMO_SPLASH)

static void BotonesChase_Iniciar(void) {   // arranca la animacion desde cero
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs primero, por si quedo alguno prendido de antes
    chase_led_idx    = 0;              // vuelve a empezar por el primer LED (J1 rojo)
    chase_led_tick   = HAL_GetTick();  // marca el instante de arranque para el primer paso de tiempo
    chase_led_activo = 1;              // habilita BotonesChase_Actualizar() a partir de ahora
    Boton_LED(0, 0, 1);                // prende el primer LED (jugador 0, color 0 = rojo)
}

static void BotonesChase_Actualizar(void) {   // avanza un paso la animacion si ya paso CHASE_LED_PASO_MS; se llama una vez por vuelta del loop principal
    if (!chase_led_activo) return;   // si la animacion esta apagada, no hace nada
    uint32_t ahora = HAL_GetTick();   // tick actual en milisegundos desde el arranque
    if (ahora - chase_led_tick < CHASE_LED_PASO_MS) return;   // todavia no toca avanzar de LED
    chase_led_tick = ahora;   // guarda el tick de este paso para medir el proximo intervalo

    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 0);   // apaga el LED actual (division entera da el jugador, el resto da el color)
    chase_led_idx = (uint8_t)((chase_led_idx + 1) % 8);   // avanza al siguiente indice, dando la vuelta de 7 a 0 (bucle infinito de 8 LEDs)
    Boton_LED(chase_led_idx / 4, chase_led_idx % 4, 1);   // prende el nuevo LED actual
}

static void BotonesChase_Detener(void) {   // detiene la animacion y apaga todos los LEDs (se invoca al salir de la pantalla de bienvenida)
    if (!chase_led_activo) return;   // si ya estaba detenida, no hace nada (evita apagar LEDs que otro modo ya este utilizando)
    chase_led_activo = 0;   // deshabilita BotonesChase_Actualizar()
    for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs
}

/* Registro por consola de cada entrada detectada: permite verificar, sin
 * depender de observar la pantalla fisica, exactamente que boton o
 * direccion de joystick fue reconocida por el sistema. Se ubica en las
 * funciones de mas bajo nivel (Botones_LeerColor / Joystick*_LeerDireccion)
 * para que el registro se genere siempre que se detecta un flanco real,
 * sin importar desde que contexto se invoque la funcion (menu, partida en
 * curso, Guitar Hero), evitando duplicar el registro o interferir con la
 * deteccion del flanco. */
static const char *const COLOR_NOMBRE[4] = { "ROJO", "VERDE", "AZUL", "AMARILLO" };   // nombres para el registro por consola, en el mismo orden que el indice de color 0-3
static const char *const DIR_NOMBRE[4]   = { "ARRIBA", "ABAJO", "IZQUIERDA", "DERECHA" };   // idem para las direcciones de joystick 0-3

/* Devuelve el primer color con flanco de presion (activo en bajo, con
 * resistencia de pull-up interna) del jugador p, o 0xFF si ninguno de sus
 * botones tuvo un flanco de presion estable en este ciclo (ver
 * BTN_DEBOUNCE_MS para el criterio de estabilidad). */
static uint8_t Botones_LeerColor(uint8_t p) {   // p = jugador logico (0 o 1); recorre sus 4 botones y devuelve el primer flanco de presion encontrado
	if (btn_modo_1p || guitar_modo_1p) p = 0; // en modo de 1 jugador, la lectura de hardware siempre corresponde al conjunto fisico del jugador logico 0
	for (uint8_t c = 0; c < 4; c++) {   // recorre los 4 colores en el orden fijo ROJO, VERDE, AZUL, AMARILLO; si dos botones se presionan en el mismo ciclo, tiene prioridad el de menor indice
        GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_SW[p][c].port, BTN_SW[p][c].pin);   // lee el nivel crudo actual del pin de ese boton

        if (cur != btn_estable[p][c]) {   // el nivel cambio respecto al ultimo estado ya confirmado como estable
            btn_estable[p][c]    = cur;          // guarda el nuevo nivel como "candidato" a estable
            btn_tick_cambio[p][c] = HAL_GetTick();   // reinicia el cronometro de estabilidad para este boton
            continue;   /* posible rebote -- todavia no cuenta, seguir con el siguiente color */
        }
        if (HAL_GetTick() - btn_tick_cambio[p][c] < BTN_DEBOUNCE_MS) continue;  /* aun no esta estable */

        uint8_t flanco = (btn_prev[p][c] == GPIO_PIN_SET && cur == GPIO_PIN_RESET);   // flanco de bajada real: antes NO presionado (SET), ahora SI presionado (RESET, activo bajo)
        btn_prev[p][c] = cur;   // actualiza el "anterior" para la proxima llamada, sin importar si hubo flanco o no
        if (flanco) {
            printf("[INPUT] boton J%u = %s\r\n", (unsigned)(p + 1), COLOR_NOMBRE[c]);   // log por consola: que jugador y que color se detecto (siempre que hay flanco real, sin excepcion)
            return c;   // devuelve el color presionado y CORTA el for -- no sigue buscando otros botones este mismo tick
        }
    }
    return 0xFF;   // ningun boton tuvo flanco de presion estable este tick
}

/* Lectura de NIVEL (no de flanco) del boton "color" del jugador p: 1 si
 * esta presionado AHORA, 0 si no. Usada por las notas sostenidas de Guitar
 * Hero (ver GH_SOSTENIDA_DURACION_MS), que necesitan saber "sigue
 * presionado" en cada tick mientras se mantiene, algo que Botones_LeerColor
 * no puede responder porque solo informa flancos. Se apoya en btn_prev, que
 * Botones_LeerColor ya deja actualizado con el ultimo nivel CONFIRMADO
 * (antirrebotado) de cada boton -- no vuelve a leer el pin crudo ni duplica
 * el antirrebote. */
static uint8_t Botones_ColorSostenido(uint8_t p, uint8_t color) {
    if (btn_modo_1p || guitar_modo_1p) p = 0;   // misma correspondencia logico/fisico que Botones_LeerColor
    return (btn_prev[p][color] == GPIO_PIN_RESET);   // activo en bajo
}

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);     // configura el reloj del micro (HSI/PLL) -- generado, ver definicion mas abajo
static void MX_GPIO_Init(void);           // configura todos los pines GPIO (entradas de botones/joystick, salidas de LED/pantalla/buzzer)
static void MX_SPI1_Init(void);           // configura el periferico SPI1 para la pantalla
static void TIM3_ADCTrigger_Init(void);   // configura TIM3 para disparar el ADC cada 20ms (TRGO)
static void ADC1_Joystick_Init(void);     // configura el ADC1 en modo escaneo de 4 canales (joystick 1 y 2)
static void MX_TIM4_Buzzer_Init(void);    // configura TIM4 para la interrupcion periodica del buzzer
static void MX_USART2_UART_Init(void);    // configura USART2 (consola de depuracion por el VCP del ST-Link)

/* ========================================================================== */
/* === BUZZER — TONO GENERADO POR SOFTWARE (TIM4 + GPIO PA6) ================= */
/* ========================================================================== */
/* El tipo de zumbador (buzzer) montado en el hardware no esta confirmado de
 * antemano: puede ser activo (emite un tono fijo con solo aplicar corriente
 * continua) o pasivo (requiere una onda cuadrada de la frecuencia deseada
 * para sonar; ver prueba_de_sonido/ para la prueba de caracterizacion
 * dedicada). Por ello se maneja siempre con frecuencia variable: si el
 * zumbador es activo, sonara de todas formas (ignora la frecuencia externa
 * y usa su propio tono fijo); si es pasivo, este esquema permite reproducir
 * tonos y melodias reales en lugar de simples clics.
 *
 * El STM32F411 no cuenta con los temporizadores TIM13/TIM14, y el unico
 * temporizador con un canal disponible en el pin PA6 (TIM3_CH1) ya esta
 * ocupado por el disparador del ADC del joystick. Por esta razon el tono no
 * se genera por PWM de hardware: TIM4 (libre) produce una interrupcion
 * periodica que alterna el pin PA6 por software (HAL_GPIO_TogglePin dentro
 * de TIM4_IRQHandler); dos flancos de esa alternancia equivalen a un ciclo
 * completo de la onda cuadrada resultante.
 *
 * La frecuencia del reloj de TIM4 (TIM4CLK) es 16 MHz, ya que el sistema
 * usa el oscilador interno HSI sin multiplicacion por PLL. Con un
 * prescaler de 15, el temporizador cuenta a 1 MHz, de modo que el valor de
 * auto-recarga ARR = 1000000 / (2 * freq_hz) - 1 produce la alternancia del
 * pin a la frecuencia deseada. */
#define BUZZER_TIM_TICK_HZ 1U   // frecuencia del tick de TIM4 tras aplicar el prescaler (1 MHz equivale a 1 tick por microsegundo); modificar este valor solo si cambia el prescaler configurado del temporizador
#define BUZZER_TONO_HZ     4U   /* frecuencia (en Hz) que usa Buzzer_Beep() para los
    efectos de sonido cortos. Se eligio 4000 Hz porque los zumbadores
    piezoelectricos pequeños, como el utilizado en este montaje, suelen
    tener su punto de mayor volumen (resonancia) en el rango aproximado de
    2.7 a 4 kHz, en lugar de frecuencias mas bajas como 2000 Hz. Para medir
    la frecuencia de resonancia real de una unidad especifica, puede
    habilitarse Buzzer_BarridoDiagnostico (ver BUZZER_DIAGNOSTICO_BARRIDO
    mas abajo). */   // cambiar este numero modifica el tono de todos los efectos de sonido cortos a la vez

typedef struct {
    uint16_t freq_hz;   /* 0=silencio, otro=tono en Hz */
    uint16_t dur_ms;    // duracion de este paso en milisegundos
} PasoSonido_t;   // un "paso" de una melodia: una frecuencia sonando (o silencio) durante dur_ms

static void Buzzer_SetSalida(uint16_t freq_hz) {   // arranca/detiene el tono del buzzer a la frecuencia dada (0 = silencio)
    if (freq_hz == 0) {
        HAL_TIM_Base_Stop_IT(&htim4);   // detiene la interrupcion periodica de TIM4 -- deja de alternar el pin
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);   // fuerza el pin del buzzer a nivel bajo para que quede en silencio real (no a medio ciclo)
    } else {
        uint32_t arr = (BUZZER_TIM_TICK_HZ / (2U * freq_hz)) - 1U;   // calcula el valor de auto-recarga (ARR) para que TIM4 desborde 2 veces por ciclo de la onda (2 flancos = 1 ciclo completo)
        HAL_TIM_Base_Stop_IT(&htim4);          // detiene el timer antes de reprogramarlo, para no dejarlo en un estado intermedio raro
        __HAL_TIM_SET_COUNTER(&htim4, 0);      // reinicia el contador del timer a 0
        __HAL_TIM_SET_AUTORELOAD(&htim4, arr);   // programa el nuevo periodo (ARR) calculado para la frecuencia pedida
        HAL_TIM_Base_Start_IT(&htim4);         // vuelve a arrancar el timer con interrupcion habilitada (la ISR alterna el pin, ver TIM4_IRQHandler en stm32f4xx_it.c)
    }
}

/* Apunta directamente al arreglo constante de la melodia en curso, sin
 * copiarlo: dado que las canciones pueden tener decenas de notas, resultaria
 * ineficiente mantener un buffer propio de tamaño fijo. */
static const PasoSonido_t *buzzer_pasos     = 0;   // puntero a la melodia/patron de SFX que esta sonando en primer plano ahora mismo (0 = ninguno)
static uint16_t            buzzer_pasos_n   = 0;   // cantidad de pasos de esa melodia
static uint16_t            buzzer_pos       = 0;   // indice del paso actual dentro de buzzer_pasos
static uint32_t            buzzer_tick_paso = 0;   // tick en el que arranco el paso actual, para saber cuando pasar al siguiente

static void Buzzer_Patron(const PasoSonido_t *pasos, uint16_t n) {   // arranca un patron de SFX en primer plano (interrumpe cualquier SFX anterior)
    buzzer_pasos     = pasos;   // guarda el puntero al arreglo de pasos (no copia, debe vivir mientras suena)
    buzzer_pasos_n   = n;       // cuantos pasos tiene
    buzzer_pos       = 0;       // arranca desde el primer paso
    buzzer_tick_paso = HAL_GetTick();   // marca el instante de arranque de este paso
    Buzzer_SetSalida(buzzer_pasos[0].freq_hz);   // suena YA el primer paso (freq_hz==0 => arranca en silencio)
}

static void Buzzer_Beep(uint16_t duracion_ms) {   // sonido corto de un solo tono fijo (BUZZER_TONO_HZ) por duracion_ms
    static PasoSonido_t un_paso;   // static: sobrevive despues de que la funcion retorna, porque Buzzer_Patron guarda el PUNTERO (no una copia)
    un_paso.freq_hz = BUZZER_TONO_HZ;   // siempre el mismo tono para todos los beeps cortos
    un_paso.dur_ms  = duracion_ms;      // duracion pedida por el llamador
    Buzzer_Patron(&un_paso, 1);         // lo reproduce como un patron de un solo paso
}

/* Herramienta de diagnostico de volumen: un zumbador piezoelectrico suena
 * con mayor intensidad cerca de su frecuencia de resonancia propia, que no
 * necesariamente coincide con BUZZER_TONO_HZ. Esta funcion, cuando se
 * habilita, se ejecuta una unica vez al arrancar, antes de mostrar la
 * pantalla de bienvenida; su implementacion es bloqueante (usa HAL_Delay),
 * lo cual es aceptable porque se ejecuta antes de que exista logica de
 * juego en curso. El procedimiento consiste en escuchar el zumbador y
 * registrar, a partir del texto impreso por consola (UART, ver
 * MX_USART2_UART_Init), la frecuencia con mayor volumen percibido, para
 * luego fijar BUZZER_TONO_HZ a ese valor. */
#define BUZZER_DIAGNOSTICO_BARRIDO 0   // establecer en 1 hace que Buzzer_BarridoDiagnostico() se ejecute una vez al arrancar, antes de la pantalla de bienvenida; se mantiene disponible para volver a caracterizar el zumbador si fuera necesario
#if BUZZER_DIAGNOSTICO_BARRIDO
static void Buzzer_BarridoDiagnostico(void) {   // reproduce un barrido de frecuencias para escuchar cual suena mas fuerte en ESTE buzzer especifico
    printf("\r\n[BUZZER] barrido de frecuencias -- escuchar y anotar cual suena mas fuerte\r\n");   // aviso por consola de que arranca el barrido
    for (uint16_t f = 200; f <= 5000; f = (uint16_t)(f + 200)) {   // recorre 200Hz a 5000Hz en pasos de 200Hz; cambiar estos numeros cambia el rango/resolucion del barrido
        printf("[BUZZER] %u Hz\r\n", f);   // imprime la frecuencia actual para poder anotarla junto a lo que se escucha
        Buzzer_SetSalida(f);   // suena esta frecuencia
        HAL_Delay(400);        // la mantiene sonando 400ms (bloqueante -- se acepta porque corre antes de que haya juego)
        Buzzer_SetSalida(0);   // apaga el tono
        HAL_Delay(120);        // pausa de silencio de 120ms antes de pasar a la siguiente frecuencia, para distinguir un paso del otro
    }
    printf("[BUZZER] fin del barrido\r\n\r\n");   // aviso de que termino todo el barrido
}
#endif

/* Debe invocarse una vez por cada vuelta del bucle principal, sin importar
 * el estado del sistema. */
/* Musica de fondo: constituye un segundo canal logico independiente del
 * canal de efectos de sonido en primer plano (pitidos de acierto/fallo/fin
 * de ronda, jingle de bienvenida, previsualizacion en DEMO_MENU_CANCIONES).
 * Dado que el zumbador es monofonico (reproduce una unica nota a la vez),
 * mientras un efecto de sonido esta sonando, la musica de fondo permanece
 * "congelada" en su paso actual (su cronometro no avanza), y Buzzer_Actua-
 * lizar retoma esa misma nota apenas finaliza el efecto de sonido, evitando
 * perder un paso o reiniciar la cancion desde el principio. Ver
 * Buzzer_Fondo_Iniciar / Buzzer_Fondo_Detener mas abajo, una vez declaradas
 * las tablas CANCIONES_DATA / CANCIONES_LEN. */
static const PasoSonido_t *buzzer_fondo_pasos     = 0;
static uint16_t            buzzer_fondo_pasos_n   = 0;
static uint16_t            buzzer_fondo_pos       = 0;
static uint32_t            buzzer_fondo_tick_paso = 0;
static uint8_t             buzzer_fondo_activo    = 0;

/* Definida mas abajo, junto a las tablas CANCIONES_DATA/CANCIONES_LEN (las
 * necesita para elegir la proxima cancion) -- se declara aca para que
 * Buzzer_Actualizar pueda invocarla sin reordenar todo el archivo. Rota el
 * repertorio de musica de fondo a una cancion aleatoria distinta cada vez
 * que la que esta sonando completa una vuelta entera, en vez de repetir
 * siempre la misma en loop (pedido explicito del usuario: "solo suena una
 * todo el tiempo"). */
static void Buzzer_Fondo_RotarSiTermino(void);

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
    if (!buzzer_fondo_activo) return;   // no hay musica de fondo activa, nada que hacer
    if (HAL_GetTick() - buzzer_fondo_tick_paso < buzzer_fondo_pasos[buzzer_fondo_pos].dur_ms) return;   // el paso actual todavia no termino su duracion

    buzzer_fondo_pos++;   // avanza al siguiente paso de la cancion
    if (buzzer_fondo_pos >= buzzer_fondo_pasos_n) {   // se paso del ultimo paso: en vez de repetir la misma cancion, rota a otra del repertorio
        Buzzer_Fondo_RotarSiTermino();   // elige una cancion nueva al azar (distinta de la actual) y deja buzzer_fondo_pos en 0
    }
    buzzer_fondo_tick_paso = HAL_GetTick();   // marca el instante de arranque del nuevo paso
    Buzzer_SetSalida(buzzer_fondo_pasos[buzzer_fondo_pos].freq_hz);   // suena la frecuencia de ese paso (0 = silencio, para las notas separadas por GAP_MS)
}

/* Jingle de bienvenida: arpegio ascendente Do-Mi-Sol-Do a lo largo de 3
 * octavas, composicion original para Beat Clash (ver
 * prueba_de_sonido/sonidos/beat_clash_welcome_jingle.ino). */
static const PasoSonido_t BEEP_BIENVENIDA[] = {   // melodia del jingle de bienvenida: pares {frecuencia_Hz, duracion_ms}; los {0,40} intermedios son silencios cortos entre notas para que se escuchen separadas
    { 262, 120 }, { 0, 40 }, { 330, 120 }, { 0, 40 }, { 392, 120 }, { 0, 40 },   // Do-Mi-Sol (arpegio ascendente), cada nota 120ms + 40ms de silencio
    { 523, 180 }, { 0, 40 }, { 659, 180 }, { 0, 40 }, { 784, 250 }, { 0, 40 },   // Do-Mi-Sol una octava arriba, notas mas largas (180-250ms) al acercarse al final
    { 1047, 400 }   // Do final, una octava mas arriba todavia, sostenido 400ms para cerrar el jingle
};
#define BEEP_BIENVENIDA_N (sizeof(BEEP_BIENVENIDA) / sizeof(BEEP_BIENVENIDA[0]))   // cantidad de pasos del arreglo, calculada automaticamente (no hay que actualizarla a mano si se agregan/quitan notas)

/* ========================================================================== */
/* === LISTA DE CANCIONES (pantalla "reproductor", ver DEMO_MENU_CANCIONES) == */
/* ========================================================================== */
/* Todas las melodias corresponden a obras de dominio publico (compositores
 * fallecidos hace mas de 70 años o folclor tradicional), transcritas en
 * prueba_de_sonido/; ver ese directorio para el detalle de fuentes y
 * frecuencias utilizadas. */

#define DO4   262   // frecuencia en Hz de la nota Do de la 4ta octava
#define RE4   294   // Re4
#define MI4   330   // Mi4
#define FA4   349   // Fa4
#define SOL4  392   // Sol4
#define LA4   440   // La4 (referencia estandar de afinacion, 440Hz)

#define DO5   523   // Do5, una octava arriba de DO4 (el doble de frecuencia)
#define RE5   587   // Re5
#define MI5   659   // Mi5
#define FA5   698   // Fa5
#define SOL5  784   // Sol5

#define NEGRA    220   // duracion en ms de una nota "negra" a este tempo fijo; cambiar este numero cambia el tempo de TODAS las canciones que usan NEGRA/CORCHEA/BLANCA
#define CORCHEA  110   // media negra (nota corta)
#define BLANCA   440   // el doble de una negra (nota larga)
#define GAP_MS    20    // silencio corto insertado por la macro N() entre nota y nota, para que no suenen "pegadas"
#define N(f, d)  { (f), (d) }, { 0, GAP_MS }   // macro de conveniencia: expande a DOS pasos, la nota (f,d) seguida de un silencio de GAP_MS -- asi cada llamado a N(...) en las canciones de abajo ya incluye su propio espacio

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

static uint8_t  buzzer_fondo_idx  = CANCION_TETRIS_IDX;   // indice (CancionIdx_t) de la cancion de fondo sonando ahora mismo -- se usa solo para elegir la siguiente al rotar el repertorio, ver Buzzer_Fondo_RotarSiTermino
static uint32_t buzzer_repertorio_seed = 2463534242u;      // semilla propia del LCG que elige la proxima cancion de fondo al azar (independiente de las semillas de secuencias de juego)

/* Arranca la musica de fondo con la cancion `cancion_idx` (loop continuo,
 * ver Buzzer_Actualizar para como convive con los SFX en primer plano). */
static void Buzzer_Fondo_Iniciar(uint8_t cancion_idx) {
    buzzer_fondo_idx       = cancion_idx;
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

/* Rota el repertorio de musica de fondo: elige al azar una cancion distinta
 * de la que acaba de terminar su vuelta (excluyendo BEEP_BIENVENIDA, indice
 * 0, que es el jingle corto de la pantalla de bienvenida y no una cancion de
 * fondo) y la deja lista desde su primer paso. Se invoca UNICAMENTE desde
 * Buzzer_Actualizar cuando buzzer_fondo_pos se pasa del final del arreglo, de
 * modo que la rotacion ocurre sola con el tiempo (cada vez que una cancion
 * completa su vuelta) sin tocar ningun punto de arranque de partida --
 * TODAS las partidas siguen empezando en CANCION_TETRIS_IDX como hasta
 * ahora, y a partir de ahi el repertorio se mezcla solo. */
static void Buzzer_Fondo_RotarSiTermino(void) {
    buzzer_repertorio_seed = buzzer_repertorio_seed * 1103515245u + 12345u;   // mismo LCG que el resto del proyecto, semilla propia
    uint8_t nuevo = (uint8_t)(1u + ((buzzer_repertorio_seed >> 16) % (CANCIONES_N - 1)));   // 1..CANCIONES_N-1: salta el indice 0 (jingle de bienvenida)
    if (nuevo == buzzer_fondo_idx) nuevo = (uint8_t)(1u + (nuevo % (CANCIONES_N - 1)));   // evita repetir la misma cancion 2 veces seguidas
    buzzer_fondo_idx      = nuevo;
    buzzer_fondo_pasos    = CANCIONES_DATA[nuevo];
    buzzer_fondo_pasos_n  = CANCIONES_LEN[nuevo];
    buzzer_fondo_pos      = 0;
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
    DEMO_CONTEO_3,          // primer numero del conteo regresivo 3-2-1-GO antes de empezar a jugar de verdad
    DEMO_CONTEO_2,          // segundo numero del conteo
    DEMO_CONTEO_1,          // tercer numero del conteo
    DEMO_CONTEO_GO,         // ultimo paso del conteo ("GO"), justo antes de arrancar el modo elegido
    DEMO_JUGANDO,           /* modo Guitar Hero (ya construido)                */
    DEMO_RESULTADO,         // pantalla de resultado/game over del recorrido de diseño (no la real de cada modo, ver *_DibujarGameOver)
    DEMO_MENU_CANCIONES,    /* lista de canciones tipo "reproductor"           */
    DEMO_COUNT              // cantidad total de pantallas -- SIEMPRE debe quedar ultima en el enum, se usa para dimensionar DEMO_NOMBRE[]
} DemoScreen_t;

/* Nombres para el log por consola (ver MX_USART2_UART_Init) -- el orden
 * debe coincidir exactamente con DemoScreen_t. */
static const char *const DEMO_NOMBRE[DEMO_COUNT] = {   // nombres de texto de cada pantalla, en el MISMO orden que DemoScreen_t -- usados solo para el log "[SCREEN] -> %s" por consola
    "SPLASH", "JUGADORES_1", "JUGADORES_2", "INICIALES", "MODO_SIMON", "MODO_SIMONJOY",
    "MODO_GUITAR", "PREVIEW_SIMON", "PREVIEW_SIMONJOY", "CONTEO_3", "CONTEO_2",
    "CONTEO_1", "CONTEO_GO", "JUGANDO", "RESULTADO", "MENU_CANCIONES"
};

/* Indice seleccionado en DEMO_MENU_CANCIONES (persiste entre visitas) */
static uint8_t cancion_cursor = CANCION_TETRIS_IDX;   // cancion actualmente resaltada en la lista; se almacena aca (y no en gs) para que el valor se conserve al entrar y salir del menu -- arranca en Tetris (la cancion de fondo historica) para que la primera visita a la lista no empiece resaltando el jingle de bienvenida

/* Iniciales de 3 letras por jugador: se solicitan en la pantalla
 * DEMO_INICIALES, entre la seleccion de cantidad de jugadores y la
 * seleccion de modo de juego. Se muestran en lugar de las etiquetas
 * genericas "J1"/"J2" en cada modo (ver Nombre_Jugador(), utilizada desde
 * renderer.c). */
static char    nombre_jugadores[2][4] = { "AAA", "AAA" };
static uint8_t nombre_jugador_actual  = 0;   /* 0 o 1 -- jugador que esta escribiendo su nombre actualmente */
static uint8_t nombre_pos_actual      = 0;   /* 0,1,2 -- posicion de la letra que se esta editando */

/* Indica si el jugador ya completo la escritura de su nombre en la sesion
 * actual. Es necesario un indicador explicito porque el valor por defecto
 * de nombre_jugadores ("AAA") nunca esta vacio: comprobar unicamente si el
 * primer caracter es distinto de '\0' resultaria en verdadero incluso
 * antes de que el jugador haya escrito nada, dejando la pantalla de
 * iniciales inalcanzable. */
static uint8_t nombre_confirmado[2] = { 0, 0 };

/* Medida preventiva frente a un caso observado de corrupcion de memoria: en
 * determinadas condiciones, el contenido de nombre_jugadores[1] puede
 * contener bytes fuera del rango esperado entre el momento en que se
 * confirma en DEMO_INICIALES y el uso posterior durante la partida del
 * jugador 2. Todos los accesos a este arreglo dentro del codigo estan
 * correctamente delimitados, por lo que identificar la causa exacta
 * requeriria depuracion asistida por hardware (punto de observacion de
 * memoria via SWD/OpenOCD). Como medida preventiva, esta funcion sustituye
 * cualquier byte fuera del rango imprimible soportado por la fuente
 * tipografica (32-90) por el caracter '-', evitando que la corrupcion se
 * propague visualmente a la pantalla. */
const char *Nombre_Jugador(uint8_t jugador) {   // devuelve el nombre "seguro" (sin bytes corruptos) del jugador logico 0/1, para mostrar en pantalla
    static char seguro[2][4];   // buffer de salida por jugador; static para que el puntero devuelto siga valido despues de retornar (el renderer lo usa enseguida)
    const char *n = nombre_jugadores[jugador];   // apunta al nombre "crudo" (posiblemente corrupto) de ese jugador
    for (uint8_t i = 0; i < 3; i++) {   // recorre las 3 letras del nombre
        char c = n[i];   // letra cruda actual
        seguro[jugador][i] = (c >= 32 && c <= 90) ? c : '-';   // si esta fuera del rango imprimible que soporta la fuente (32-90), la reemplaza por '-'; cambiar el rango cambiaria que caracteres se consideran "validos"
    }
    seguro[jugador][3] = '\0';   // cierra el string de 3 letras
    return seguro[jugador];   // devuelve el puntero al buffer seguro de ESE jugador
}

/* Paso "encendido" (0-3, o 0xFF=ninguno) para animar las vistas previas de
 * Simon Clasico / Simon+Joystick, cambia cada 500ms mientras se muestran. */
static uint8_t Demo_PasoSimon(void) {   // calcula, a partir del reloj del sistema, cual de los 4 pasos animar ahora en las vistas previas (sin estado propio -- se deriva del tiempo)
    uint32_t t = (HAL_GetTick() / 500) % 5;   // ciclo de 5 posiciones (0-4) que cambia cada 500ms; cambiar el 500 cambia la velocidad del parpadeo, cambiar el 5 cambia cuanto dura el "hueco" sin nada encendido
    return (t < 4) ? (uint8_t)t : 0xFF;   // valores 0-3 son un paso real encendido; el 5to valor (t==4) devuelve 0xFF (nada encendido), da una pausa visible antes de repetir
}

/* Entra a la pantalla `screen`: prepara el GameState_t de ejemplo y dibuja
 * el primer cuadro. Renderer_Update() se encarga de animar lo que siga. */
static void Demo_Enter(uint8_t screen) {   // punto unico de entrada a cualquier pantalla: loguea, resetea el estado de ejemplo, fija orientacion y dibuja el primer cuadro
    printf("[SCREEN] -> %s\r\n", DEMO_NOMBRE[screen]);   // log de consola de cada cambio de pantalla, para poder seguir el flujo sin ver la pantalla fisica
    memset(&gs, 0, sizeof(gs));   // limpia TODO el estado de juego de ejemplo antes de preparar la nueva pantalla, para no arrastrar datos viejos de la anterior

    /* Simon Clasico usa layout "cocktail" en retrato (240x320); el resto
     * del recorrido sigue en paisaje (320x240) como hasta ahora. */
    ILI9341_SetPortrait(screen == DEMO_PREVIEW_SIMON);   // solo la vista previa de Simon Clasico usa retrato aca; el juego REAL cambia a retrato aparte (ver Botones_IniciarAmbos/Solo)

    switch (screen) {   // arma el contenido especifico de cada pantalla segun cual sea
    case DEMO_SPLASH:
        gs.estado = ESTADO_SPLASH;   // marca el estado de ejemplo como splash (usado por el renderer de fondo, si lo consulta)
        Buzzer_Patron(BEEP_BIENVENIDA, (uint8_t)BEEP_BIENVENIDA_N);   // arranca el jingle de bienvenida cada vez que se entra al splash
        break;

    case DEMO_JUGADORES_1:
    case DEMO_JUGADORES_2:
        Renderer_DrawSeleccionJugadores((uint8_t)(screen - DEMO_JUGADORES_1));   // dibuja el menu de 1/2 jugadores con el cursor en la opcion correspondiente (resta el indice base para obtener 0 o 1)
        break;

    case DEMO_INICIALES:
        /* Unicamente se reinicia a "AAA" y se solicita letra por letra el
         * nombre de los jugadores que aun no lo hayan confirmado en la
         * sesion actual; si el jugador 1 ya confirmo su nombre previamente,
         * no se le vuelve a solicitar ni se borra su valor al entrar a esta
         * pantalla para pedirle el nombre al jugador 2. */
        nombre_jugador_actual = nombre_confirmado[0] ? 1 : 0;   // si el jugador 1 ya confirmo su nombre, se solicita directamente el del jugador 2; de lo contrario, se comienza por el jugador 1
        nombre_pos_actual     = 0;   // siempre se comienza por la primera letra del jugador correspondiente
        if (!nombre_confirmado[0]) strcpy(nombre_jugadores[0], "AAA");   // solo se reinicia a "AAA" si el jugador 1 aun no confirmo su nombre (de lo contrario, se conserva el nombre ya confirmado)
        if (!nombre_confirmado[1]) strcpy(nombre_jugadores[1], "AAA");   // idem para el jugador 2
        Renderer_DrawNombre(nombre_jugador_actual, nombre_jugadores[nombre_jugador_actual], 0);   // dibuja la pantalla de iniciales arrancando en la letra 0 (ultimo parametro) del jugador que corresponda
        break;

    case DEMO_MODO_SIMON:
    case DEMO_MODO_SIMONJOY:
    case DEMO_MODO_GUITAR:
        Renderer_DrawSeleccionModo((uint8_t)(screen - DEMO_MODO_SIMON));   // dibuja el menu de modos con el cursor en la opcion correspondiente (0=Simon,1=Sim+Joy,2=Guitar)
        break;

    case DEMO_PREVIEW_SIMON:
        Renderer_DrawModoSimonClasico(0xFF, 0xFF);   // dibuja el layout completo de Simon Clasico sin ningun boton "encendido" (0xFF=ninguno) para ambos jugadores, solo de adorno
        break;

    case DEMO_PREVIEW_SIMONJOY:
        /* misma pantalla de solo-flechas que usa el juego real
         * (Renderer_DrawModoSimonJoystick1P) -- antes se dibujaba con la
         * version de 2 jugadores con texto (ARRIBA/ABAJO/IZQ/DER), que no
         * coincidia con lo que se ve al arrancar la partida de verdad. */
        Renderer_DrawModoSimonJoystick1P(0xFF);   // D-pad completo sin ningun paso encendido, solo de adorno
        break;

    case DEMO_CONTEO_3:
    case DEMO_CONTEO_2:
    case DEMO_CONTEO_1:
    case DEMO_CONTEO_GO:
        Renderer_DrawConteo((uint8_t)(DEMO_CONTEO_GO - screen));   // convierte la pantalla actual en el numero a mostrar: DEMO_CONTEO_3->3, ..., DEMO_CONTEO_GO->0 (interpretado como "GO" por el renderer)
        break;

    case DEMO_JUGANDO:
        /* El puntaje y el combo inician en 0 porque esta pantalla permite
         * jugar de verdad presionando B1 (ver GuitarHero_IntentarGolpe). */
        gs.estado       = ESTADO_JUGANDO;   // marca el estado de ejemplo como "jugando" (lo consulta el renderer de Guitar Hero)
        gs.nota_speed   = NOTE_SPEED_L2;   // velocidad de caida de las notas del recorrido de diseño de Guitar Hero; cambiar esta constante (en game_state.h) cambia que tan rapido caen
        gs.j[0].puntaje = 0;   // puntaje inicial de jugador 0 en este recorrido de prueba
        gs.j[0].combo   = 0;   // combo inicial de jugador 0
        gs.j[1].puntaje = 0;   // puntaje inicial de jugador 1
        gs.j[1].combo   = 0;   // combo inicial de jugador 1
        gs.notas[0] = (Nota_t){ .x_rel = -NOTE_W, .carril = 1, .jugador = 0, .activa = 1 };   // primera nota de prueba: arranca justo fuera de pantalla a la izquierda (-NOTE_W), carril 1, del jugador 0, activa
        gs.notas[1] = (Nota_t){ .x_rel = -NOTE_W, .carril = 2, .jugador = 1, .activa = 1 };   // segunda nota de prueba, carril 2, del jugador 1
        break;

    case DEMO_RESULTADO:
        gs.estado       = ESTADO_RESULTADO;   // marca el estado de ejemplo como resultado (pantalla de adorno del recorrido, no la real de cada modo)
        gs.j[0].puntaje = 1250;   // puntaje de ADORNO fijo para mostrar como se veria la pantalla de resultado con datos
        gs.j[0].combo   = 8;      // combo de adorno de jugador 0
        gs.j[1].puntaje = 980;    // puntaje de adorno de jugador 1
        gs.j[1].combo   = 5;      // combo de adorno de jugador 1
        break;

    case DEMO_MENU_CANCIONES:
        Renderer_DrawListaCanciones(cancion_cursor);   // dibuja la lista de canciones con el cursor en la posicion recordada de la ultima visita
        break;
    }
}

/* ========================================================================== */
/* === GOLPEO DE NOTAS (GUITAR HERO) — B1 COMO BOTON UNICO =================== */
/* ========================================================================== */
/* Corresponde a la maqueta visual original del modo Guitar Hero, previa a
 * la existencia de los botones arcade cableados. El boton B1 actua como
 * "boton unico": golpea la nota activa mas cercana al centro de la zona de
 * presion, sin distinguir carril ni jugador, ya que un solo boton no
 * permite seleccionar un carril especifico. Esta logica solo se ejecuta
 * dentro del recorrido de diseño (pantalla DEMO_JUGANDO, con 2 notas fijas
 * que reaparecen automaticamente); el modo completo, con sincronizacion
 * real respecto a una melodia, constituye el siguiente objetivo de
 * desarrollo del proyecto (ver guitar_hero/ANALISIS_REFERENCIA.md para el
 * diseño propuesto: un mapa de notas ligado al tiempo real de una melodia,
 * en lugar de la generacion periodica actual). */
static void GuitarHero_IntentarGolpe(void) {   // busca la nota activa mas cercana al centro de la zona de golpe y la puntua (o falla si ninguna esta cerca)
    int16_t pz_centro = (int16_t)(PRESS_ZONE_W / 2);   // posicion X del centro de la zona de presion, en coordenadas locales del carril
    int16_t mejor_dist = 0x7FFF;   // arranca en el maximo posible de un int16_t, para que la primera nota comparada siempre sea "mejor" que este valor inicial
    int8_t  mejor_i    = -1;   // indice de la mejor nota encontrada hasta ahora; -1 = ninguna todavia

    for (uint8_t i = 0; i < MAX_NOTES; i++) {   // recorre TODAS las notas posibles del arreglo (activas o no)
        Nota_t *n = &gs.notas[i];   // puntero a la nota actual, para no repetir gs.notas[i] varias veces
        if (!n->activa) continue;   // ignora las notas que no estan en juego ahora mismo
        int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);   // posicion X del centro de la nota (su borde izquierdo x_rel mas la mitad de su ancho)
        int16_t dist = (int16_t)((centro_nota > pz_centro) ? (centro_nota - pz_centro) : (pz_centro - centro_nota));   // distancia absoluta entre el centro de la nota y el centro de la zona de presion (valor siempre positivo)
        if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }   // si esta nota esta mas cerca que la mejor encontrada hasta ahora, la reemplaza
    }

    if (mejor_i < 0 || mejor_dist > (int16_t)HIT_OK) {   // no habia ninguna nota activa, o la mas cercana esta mas lejos que la ventana de puntuacion "OK" (la mas ancha de las 3)
        printf("[GUITARHERO] MISS (nada en rango)\r\n");   // log de fallo por consola
        return;   /* nada cerca: fallo silencioso */
    }

    Nota_t          *n = &gs.notas[mejor_i];   // puntero a la nota que SI se va a puntuar
    EstadoJugador_t *j = &gs.j[n->jugador];   // puntero al estado (puntaje/combo) del jugador dueño de esa nota
    const char      *calidad;   // texto de la calidad del golpe, solo para el log por consola

    if      (mejor_dist <= (int16_t)HIT_PERFECT) { j->puntaje = (uint16_t)(j->puntaje + SCORE_PERFECT); calidad = "PERFECT"; }   // golpe dentro de la ventana mas angosta -> maximo puntaje
    else if (mejor_dist <= (int16_t)HIT_GOOD)    { j->puntaje = (uint16_t)(j->puntaje + SCORE_GOOD);    calidad = "GOOD"; }      // dentro de la ventana intermedia -> puntaje medio
    else                                          { j->puntaje = (uint16_t)(j->puntaje + SCORE_OK);     calidad = "OK"; }        // dentro de la ventana mas ancha (HIT_OK) -> puntaje minimo
    j->combo++;   // cualquier golpe (aunque sea "OK") suma combo -- solo un MISS total lo cortaria (no implementado en este recorrido de prueba)
    printf("[GUITARHERO] jugador=%u dist=%d %s puntaje=%u combo=%u\r\n",
           n->jugador, mejor_dist, calidad, j->puntaje, j->combo);   // log completo del golpe: jugador, distancia real, calidad, puntaje y combo resultantes

    n->x_rel = -NOTE_W;   /* respawn inmediato para poder seguir probando */
}

/* Detecta el flanco de presion de B1 (activo en bajo, con resistencia de
 * pull-up externa en la placa Nucleo), aplicando el mismo criterio de
 * antirrebote por tiempo (BTN_DEBOUNCE_MS). Dado que B1 confirma menus y
 * permite salir de una partida en curso, un flanco falso en esta deteccion
 * se traduciria en un cambio de pantalla no solicitado por el usuario. */
static uint8_t Boton_B1_Flanco(void) {   // detecta un flanco de presion (SET->RESET) de B1 con debounce real; devuelve 1 solo el tick en que se confirma el flanco
    static GPIO_PinState prev        = GPIO_PIN_SET;   // ultimo nivel YA CONFIRMADO en la llamada anterior (para comparar flanco); arranca en SET = no presionado
    static GPIO_PinState estable      = GPIO_PIN_SET;   // nivel "candidato" que se esta esperando confirmar como estable
    static uint32_t       tick_cambio = 0;   // tick del ultimo cambio de nivel detectado, para medir cuanto lleva estable
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);   // lee el nivel crudo actual de B1 (PC13)

    if (cur != estable) {   // el nivel leido cambio respecto al ultimo candidato
        estable     = cur;   // este nuevo nivel pasa a ser el candidato a estable
        tick_cambio = HAL_GetTick();   // reinicia el cronometro de estabilidad
        return 0;   /* posible rebote -- todavia no cuenta */
    }
    if (HAL_GetTick() - tick_cambio < BTN_DEBOUNCE_MS) return 0;  /* aun no esta estable */

    uint8_t flanco = (prev == GPIO_PIN_SET && cur == GPIO_PIN_RESET);   // flanco de bajada real: antes no presionado, ahora si (activo bajo)
    prev = cur;   // actualiza el "anterior" confirmado para la proxima llamada
    return flanco;
}

/* Navegacion del recorrido de pantallas (menus, lista de canciones, etc.):
 * el avance entre pantallas se realiza presionando cualquiera de los 8
 * botones arcade, manteniendo el joystick reservado exclusivamente para
 * jugar (deteccion de direccion en los modos JOYS). Esta funcion revisa los
 * 8 botones (de ambos jugadores) en busca de un flanco de presion;
 * cualquiera de ellos cuenta como una accion de "avanzar" (no existe un
 * boton dedicado a retroceder, ya que el recorrido es ciclico y presionar
 * varias veces permite alcanzar cualquier pantalla). Debe invocarse
 * unicamente fuera de una partida real (ver la condicion correspondiente
 * en el bucle principal) para no interceptar el flanco de presion que le
 * corresponde a Botones_ActualizarJugador durante una partida del modo de
 * botones. */
static uint8_t BotonesNavegacion_Presionado(void) {   // devuelve 1 si CUALQUIERA de los 8 botones arcade (ambos jugadores) tuvo un flanco de presion este tick
    uint8_t presionado = 0;   // acumulador de resultado; arranca en "no se presiono nada"
    for (uint8_t p = 0; p < 2; p++) {   // revisa ambos jugadores
        if (Botones_LeerColor(p) != 0xFF) presionado = 1;   // si ese jugador tuvo un flanco en cualquiera de sus 4 colores, cuenta como "se presiono algo" (nota: SIEMPRE llama a Botones_LeerColor de los 2, aunque el primero ya haya dado positivo, para no dejar flancos sin consumir)
    }
    return presionado;
}

/* Gesto de salida de emergencia: mantener presionados simultaneamente los
 * botones ROJO y AMARILLO del mismo jugador durante 3 segundos ofrece una
 * alternativa a B1 para abandonar una partida en curso sin que el jugador
 * deba desplazarse hasta la placa Nucleo. Esta funcion lee el nivel crudo
 * de los pines en lugar del flanco que entrega Botones_LeerColor (la cual
 * detecta un solo color a la vez y consume el flanco en el primer color
 * detectado), de modo que no interfiere con la deteccion normal de colores
 * del juego. */
#define COMBO_SALIR_MS 3000U   // tiempo (en milisegundos) que deben mantenerse presionados ROJO+AMARILLO para que se dispare el reinicio; disminuir este valor acelera la respuesta pero facilita activarlo sin intencion, aumentarlo lo hace mas seguro pero mas lento
static uint32_t combo_salir_tick[2] = { 0, 0 };   // tick en que comenzo el combo para cada jugador (0 = combo no esta en curso); se mantiene por separado para que cada jugador pueda salir sin depender del otro

static uint8_t ComboSalir_Detectado(void) {   // revisa si algun jugador lleva 3 segundos manteniendo presionados ROJO+AMARILLO; devuelve 1 en el ciclo en que se cumple la condicion
    uint8_t disparado = 0;   // resultado acumulado (contempla el caso, poco probable, de que ambos jugadores completen el gesto en el mismo ciclo)
    for (uint8_t p = 0; p < 2; p++) {   // revisa cada jugador por separado
        uint8_t target_p = p;
        if (btn_modo_1p || guitar_modo_1p) target_p = 0;   // en modo de 1 jugador, ambos jugadores logicos leen los botones fisicos del conjunto 0

        uint8_t rojo_amarillo = (HAL_GPIO_ReadPin(BTN_SW[target_p][0].port, BTN_SW[target_p][0].pin) == GPIO_PIN_RESET) &&
                                 (HAL_GPIO_ReadPin(BTN_SW[target_p][3].port, BTN_SW[target_p][3].pin) == GPIO_PIN_RESET);   // lee el NIVEL crudo (no el flanco) de rojo (color 0) y amarillo (color 3) de este jugador; 1 solo si AMBOS estan presionados ahora mismo
        if (rojo_amarillo) {   // los 2 botones estan presionados en este instante
            if (combo_salir_tick[p] == 0) {
                combo_salir_tick[p] = HAL_GetTick();   // primera vez que se detecta el combo: arranca el cronometro
            } else if (HAL_GetTick() - combo_salir_tick[p] >= COMBO_SALIR_MS) {
                printf("[COMBO] P%u rojo+amarillo 3s -> salir\r\n", p);   // log del disparo, con que jugador lo activo
                disparado = 1;   // marca que hay que resetear
                combo_salir_tick[p] = 0;   /* rearma para la proxima vez */
            }
        } else {
            combo_salir_tick[p] = 0;   // se solto alguno de los 2 botones antes de completar los 3s: cancela el cronometro de este jugador
        }
    }
    return disparado;
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — MODO DE 1 JUGADOR ================================== */
/* ========================================================================== */
/* Variante del juego "Simon Dice" que utiliza direcciones del joystick en
 * lugar de colores: la secuencia se reproduce parpadeando sobre el D-pad
 * dibujado por Renderer_DrawModoSimonJoystick1P(), el jugador la repite
 * inclinando el joystick, y cada acierto agrega un paso adicional a la
 * secuencia (dificultad adaptativa: comienza lenta y se acelera de forma
 * automatica, sin niveles fijos predefinidos). Las direcciones se codifican
 * como 0=ARRIBA, 1=ABAJO, 2=IZQUIERDA, 3=DERECHA. */

typedef enum {
    SJ_MOSTRANDO,   /* reproduciendo la secuencia (parpadeo on/off)          */
    SJ_ESPERANDO,   /* esperando que el jugador repita paso por paso         */
    SJ_ACIERTO,     /* pausa corta de "bien" antes de mostrar la siguiente   */
    SJ_GAMEOVER     /* fallo: pantalla de resultado, espera mover el stick
                       (reintentar) o B1 (salir) -- ver comentario arriba   */
} SimonJoyFase_t;

#define SJ_MAX_LONGITUD      64
#define SJ_PAUSA_ACIERTO_MS  550U  // duracion de la pausa de "acierto" antes de mostrar el siguiente paso; un valor demasiado bajo hace que la siguiente ronda comience de forma abrupta, sin dar tiempo de reaccion al jugador

/* La velocidad se modela como un multiplicador: en la ronda 1 vale x0.10
 * (lento) y aumenta +0.02 por cada ronda superada, hasta un tope de x0.40
 * (alcanzado alrededor de la ronda 16), donde se estabiliza. El intervalo
 * de tiempo real se calcula como REF_MS / velocidad, de modo que a mayor
 * multiplicador corresponde un intervalo menor (mas rapido). Con
 * REF_MS=100, el multiplicador inicial produce un intervalo de 1000 ms y
 * el tope produce 250 ms (cada mitad del parpadeo on/off dura 125 ms, lo
 * cual resulta suficiente para distinguir 2 repeticiones consecutivas de la
 * misma direccion; topes de velocidad mayores probados previamente
 * resultaron demasiado rapidos para esa distincion). */
#define SJ_VELOCIDAD_INICIAL  0.10f   // multiplicador de velocidad en la ronda 1 (mas chico = mas lento); ver formula en SimonJoy_IntervaloActual
#define SJ_VELOCIDAD_PASO     0.02f   // cuanto sube el multiplicador por cada ronda superada; subirlo hace que el juego se acelere mas rapido ronda a ronda
#define SJ_VELOCIDAD_MAX      0.40f   // tope maximo del multiplicador (se deja de acelerar despues de esto); subirlo sube el techo de velocidad maxima del juego
#define SJ_INTERVALO_REF_MS   100U   // milisegundos de referencia usados junto al multiplicador para calcular el intervalo real (ver SimonJoy_IntervaloActual)

static uint8_t        sj_secuencia[SJ_MAX_LONGITUD];   // secuencia completa de direcciones (0-3) que el jugador debe repetir, se va alargando de a 1
static uint8_t        sj_longitud;       /* pasos en la secuencia actual      */
static uint8_t        sj_paso_mostrar;   /* indice que se esta parpadeando    */
static uint8_t        sj_paso_esperado;  /* indice que se espera del jugador  */
static uint8_t        sj_mostrando_on;   /* sub-fase del parpadeo (on/off)    */
static uint8_t        sj_paso_dibujado = 0xFF; /* ultimo paso pintado en el D-pad
    (0xFF=ninguno) -- para actualizar solo el boton que cambio en vez de
    redibujar toda la pantalla (evita el parpadeo de un FillScreen completo) */
static uint8_t        sj_mejor_racha;    /* mejor racha de esta sesion        */
static SimonJoyFase_t sj_fase;   // fase actual de la maquina de estados (MOSTRANDO/ESPERANDO/ACIERTO/GAMEOVER)
static uint32_t       sj_tick_fase;   // tick en que arranco la fase actual, para medir cuanto lleva en ella
static uint32_t       sj_seed = 1;   // semilla del generador pseudoaleatorio LCG (ver SJ_Random4); nunca debe quedar en 0 o el LCG se "traba" repitiendo 0
static uint8_t        en_juego_real = 0; /* 1 = SimonJoy real, no el recorrido*/
static uint8_t         jugadores_seleccionados = 1; /* 1 o 2, elegido en el menu */
static uint8_t         joy_listo_dir = 1;   // 1 = el stick ya volvio al centro y esta listo para contar un nuevo movimiento (evita que un empuje sostenido cuente varias veces)

/* Conteo regresivo automatico (3, 2, 1, GO) al confirmar un modo jugable
 * desde el menu real: a diferencia del recorrido de diseño (que espera la
 * pulsacion de B1 en cada pantalla), en este caso cada numero avanza de
 * forma automatica, como en una partida real. La variable modo_confirmado
 * indica que modo de juego iniciar al llegar a "GO" entre las 3 opciones
 * disponibles (BOTONES, SIMONJOY, GUITAR); ver GuitarHero2_IniciarAmbos y
 * GuitarHero_IniciarSolo mas abajo. */
typedef enum { MODO_SEL_BOTONES = 0, MODO_SEL_SIMONJOY, MODO_SEL_GUITAR } ModoSeleccion_t;
static ModoSeleccion_t modo_confirmado = MODO_SEL_BOTONES;   // que modo real arrancar cuando el conteo automatico llegue a GO
static uint8_t  conteo_auto = 0;   // 1 mientras el conteo 3-2-1-GO esta avanzando solo (sin esperar B1)
static uint32_t conteo_tick = 0;   // tick en que se mostro el numero actual del conteo
#define CONTEO_PASO_MS 700U   // milisegundos que se muestra cada numero del conteo antes de pasar al siguiente; cambiar esto cambia el ritmo del "3, 2, 1, GO"

/* generador pseudoaleatorio simple (LCG), suficiente para elegir 1 de 4 direcciones */
static uint8_t SJ_Random4(void) {   // genera un numero pseudoaleatorio 0-3, avanzando el generador LCG (congruencial lineal) un paso
    sj_seed = sj_seed * 1103515245u + 12345u;   // formula estandar de un LCG de 32 bits (mismas constantes que usa glibc rand()); cambiar estos numeros cambia por completo la secuencia generada
    return (uint8_t)((sj_seed >> 16) & 0x3u);   // toma 2 bits "del medio" de la semilla (mas aleatorios que los bits bajos de un LCG) y los recorta a 0-3
}

/* siguiente direccion de la secuencia, sin permitir una 3ra repeticion
 * seguida (verificado: el LCG de arriba solo puede repetir 3-6 veces
 * seguidas cada ~20 tiradas, se siente injusto en un juego de memoria).
 * Si las 2 anteriores ya son iguales entre si, se fuerza que la nueva sea
 * distinta -- eligiendo uniforme entre las otras 3 direcciones. */
static uint8_t SJ_SiguienteDireccion(void) {   // elige la proxima direccion a agregar a la secuencia, evitando una 3ra repeticion seguida
    uint8_t nuevo = SJ_Random4();   // primer candidato aleatorio
    if (sj_longitud >= 2 &&
        sj_secuencia[sj_longitud - 1] == sj_secuencia[sj_longitud - 2] &&
        nuevo == sj_secuencia[sj_longitud - 1]) {   // las 2 direcciones anteriores ya son iguales entre si Y el candidato nuevo tambien coincide (seria una 3ra repeticion)
        nuevo = (uint8_t)((nuevo + 1u + (SJ_Random4() % 3u)) & 0x3u);   // fuerza un valor DISTINTO al repetido, eligiendo uniforme entre las otras 3 direcciones posibles
    }
    return nuevo;
}

static uint16_t SimonJoy_IntervaloActual(void) {   // calcula cuantos ms dura cada mitad (on u off) del parpadeo de la secuencia, segun la ronda actual
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj_longitud - 1) * SJ_VELOCIDAD_PASO;   // velocidad sube linealmente con la ronda (sj_longitud), arrancando en SJ_VELOCIDAD_INICIAL
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;   // no deja que la velocidad supere el tope maximo configurado
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);   // a mayor velocidad, menor intervalo (mas rapido) -- relacion inversa
}

/* Deteccion de direccion del joystick por flanco: exige que ambos ejes
 * salgan de la zona muerta y regresen a ella antes de contar el siguiente
 * movimiento, sin requerir que el valor vuelva al centro exacto, ya que un
 * eje con un desplazamiento propio de fabricacion podria no retornar nunca
 * a una banda demasiado angosta, dejando el juego sin responder despues
 * del primer acierto. */
static uint8_t Joystick_LeerDireccion(void) {   // devuelve la direccion (0-3) del joystick fisico usado en el modo de 1 jugador, detectada por flanco
    uint16_t bajo_y, alto_y, bajo_x, alto_x;
    Joy_Umbrales(centro_j2y, &bajo_y, &alto_y);   // rango de la zona muerta del eje Y de J2, relativo a su centro medido
    Joy_Umbrales(centro_j2x, &bajo_x, &alto_x);   // rango de la zona muerta del eje X de J2, relativo a su centro medido

    if (!joy_listo_dir) {
        if (joystick2_y < alto_y && joystick2_y > bajo_y && joystick2_x < alto_x && joystick2_x > bajo_x) {
            joy_listo_dir = 1;
        }
        return 0xFF;
    }

    /* Los ejes electricos del joystick fisico usado en el modo de 1 jugador
     * quedan invertidos respecto a la orientacion visual del montaje: el
     * canal que el ADC identifica como "Y" (joystick2_y, PC0) responde al
     * movimiento lateral del joystick, mientras que el canal "X"
     * (joystick2_x, PC1) responde al movimiento vertical. Por esta razon,
     * la direccion ARRIBA/ABAJO se calcula a partir del canal X, y la
     * direccion IZQUIERDA/DERECHA a partir del canal Y. */
    int32_t dev_vert  = (int32_t)joystick2_x - (int32_t)centro_j2x;  /*  + = ABAJO   */
    int32_t dev_horiz = (int32_t)joystick2_y - (int32_t)centro_j2y;  /*  + = IZQUIERDA */
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;

    uint8_t dir = 0xFF;
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {
        dir = (dev_vert > 0) ? 1 : 0;    /* ABAJO : ARRIBA  */
        joy_listo_dir = 0;
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {
        dir = (dev_horiz < 0) ? 3 : 2;   /* DERECHA cuando joystick2_y cae por debajo de su centro : IZQUIERDA */
        joy_listo_dir = 0;
    }
    if (dir != 0xFF) printf("[INPUT] joystick J2 Solo = %s\r\n", DIR_NOMBRE[dir]);
    return dir;
}

/* Misma mitigacion que Nombre_Jugador() de mas arriba (ver su comentario),
 * aplicada aca a los buffers "linea"/"buf" armados con snprintf antes de
 * dibujarlos en el modo de 1 jugador: reemplaza en el lugar cualquier byte
 * fuera del rango imprimible que soporta la fuente (32-90, mas minusculas
 * 97-122 -- ILI9341_DrawChar ya las normaliza a mayuscula antes de validar
 * el rango, asi que SON validas y no deben tocarse; una version anterior
 * las trataba como corruptas, rompiendo literales como "Racha: " en
 * pantalla) por '-'. Es un cambio puramente de visualizacion (no toca
 * timing ni logica de juego del modo de 1 jugador). */
static void Texto_Sanear(char *s) {
    for (; *s; s++) {
        char c = *s;
        if (c >= 'a' && c <= 'z') continue;   // minuscula legitima, no tocar
        if (c < 32 || c > 90) *s = '-';
    }
}

static void SimonJoy_DibujarGameOver(void) {   // dibuja la pantalla completa de "GAME OVER" del modo 1 jugador (racha actual y mejor racha de la sesion)
    char linea[32];   // buffer temporal para armar cada linea de texto con snprintf
    ILI9341_FillScreen(COLOR_BLACK);   // borra toda la pantalla a negro antes de dibujar el resultado
    ILI9341_DrawString(70, 80, "GAME OVER", COLOR_RED, COLOR_BLACK, 3);   // titulo grande (escala 3) en rojo
    snprintf(linea, sizeof(linea), "Racha: %u", (unsigned)(sj_longitud - 1));   // arma el texto de la racha de ESTA partida (sj_longitud-1 porque el ultimo paso agregado fue el que fallo)
    Texto_Sanear(linea);
    ILI9341_DrawString(100, 140, linea, COLOR_WHITE, COLOR_BLACK, 2);   // dibuja la racha de esta partida
    snprintf(linea, sizeof(linea), "Mejor: %u", (unsigned)sj_mejor_racha);   // arma el texto de la mejor racha de la sesion completa
    Texto_Sanear(linea);
    ILI9341_DrawString(100, 165, linea, COLOR_YELLOW, COLOR_BLACK, 2);   // dibuja la mejor racha
    ILI9341_DrawString(35, 210, "mueve=reintentar  B1=salir", COLOR_GRAY, COLOR_BLACK, 1);   // instrucciones de que hacer desde esta pantalla
}

/* Numero de ronda mostrado en tiempo real en la esquina superior derecha
 * del encabezado, para que el jugador pueda ver su progreso mientras juega
 * y no solo al perder la partida. Redibuja unicamente esa esquina, sin
 * afectar el resto del encabezado. */
static void SimonJoy_MostrarRacha(uint8_t racha) {   // redibuja solo el numero de ronda en la esquina superior derecha, sin tocar el resto de la pantalla
    char buf[12];   // buffer para el texto "RONDA:NN"
    snprintf(buf, sizeof(buf), "RONDA:%2u", racha);   // arma el texto con el numero de ronda actual (ancho fijo de 2 digitos)
    Texto_Sanear(buf);
    ILI9341_FillRect(LCD_W - 80, 0, 80, 26, COLOR_DARKGRAY);   // borra solo el recuadro de la esquina (80px de ancho, pegado al borde derecho) antes de escribir encima
    ILI9341_DrawString(LCD_W - 74, 9, buf, COLOR_YELLOW, COLOR_DARKGRAY, 1);   // dibuja el texto en amarillo sobre fondo gris oscuro
}

static void SimonJoy_Iniciar(void) {   // arranca una partida nueva de SimonJoy 1 jugador desde cero
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // arranca la musica de fondo (Tetris) en loop mientras se juega
    sj_seed ^= HAL_GetTick();   // mezcla la semilla con el tick actual para que cada partida tenga una secuencia distinta (no siempre la misma)
    if (sj_seed == 0) sj_seed = 1;   // evita que la semilla quede en 0 (el LCG se trabaria devolviendo siempre 0)

    sj_longitud        = 1;   // arranca con una secuencia de un solo paso
    sj_secuencia[0]     = SJ_Random4();   // primer paso aleatorio de la secuencia
    sj_paso_mostrar     = 0;   // arranca mostrando desde el paso 0
    sj_mostrando_on     = 1;   // arranca en la mitad "encendida" del parpadeo
    sj_fase             = SJ_MOSTRANDO;   // arranca en la fase de reproducir la secuencia
    sj_tick_fase        = HAL_GetTick();   // marca el instante de arranque de esta fase
    joy_listo_dir       = 1;   // habilita de entrada la deteccion de movimiento (no hace falta esperar a que "vuelva" al centro la primera vez)

    /* pantalla completa para 1 jugador (sin dividir), girada 180 hacia el
     * lado de "jugador 2" -- ya en landscape (heredado del recorrido de
     * menus), asi que solo hace falta el flip, sin tocar SetPortrait. */
    ILI9341_SetFlip180(0);   // gira la pantalla 180 grados en hardware para este modo
    Renderer_DrawModoSimonJoystick1P(sj_secuencia[0]);   // dibuja el D-pad completo con el primer paso ya "encendido"
    sj_paso_dibujado    = sj_secuencia[0];   // sincroniza que paso quedo pintado en pantalla, para que las actualizaciones incrementales sepan de donde partir
    Renderer_ResetCursorJoystick();   // vuelve el cursor en forma de "X" del centro del D-pad a su posicion neutral
    SimonJoy_MostrarRacha(sj_longitud);   // dibuja el numero de ronda inicial (1)
}

/* Cambia cual boton del D-pad esta encendido (0xFF=ninguno). Invoca
 * unicamente la actualizacion incremental (sin borrar toda la pantalla) y
 * solo cuando el estado realmente cambio, evitando repintar cuando el
 * joystick permanece quieto en la misma direccion, lo cual produciria un
 * parpadeo visual innecesario. */
static void SimonJoy_MostrarPaso(uint8_t nuevo) {   // cambia cual flecha del D-pad esta "encendida" (nuevo=0-3) o las apaga todas (nuevo=0xFF)
    if (nuevo == sj_paso_dibujado) return;   // ya esta pintado asi, no redibuja nada (evita parpadeo innecesario)
    Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, nuevo);   // redibuja solo las 2 flechas que cambiaron (la que se apaga y la que se prende), no toda la pantalla
    sj_paso_dibujado = nuevo;   // guarda el nuevo estado pintado
    if (nuevo < 4) Buzzer_Beep(90);  /* beep corto cada vez que se prende una flecha */
}

/* Confirmacion visual de una entrada del jugador durante la fase
 * SJ_ESPERANDO, mediante un ciclo real de apagado y encendido (no
 * instantaneo). Esto es necesario porque, si dos entradas consecutivas
 * corresponden a la misma direccion, SimonJoy_MostrarPaso no vuelve a
 * redibujar la flecha al no cambiar su estado, lo cual haria parecer que la
 * segunda entrada no fue registrada. Esta funcion apaga la flecha de
 * inmediato y programa su reencendido SJ_FLASH_INPUT_MS despues (verificado
 * en SimonJoy_Actualizar), garantizando que el apagon sea visible incluso
 * cuando la direccion se repite. */
#define SJ_FLASH_INPUT_MS 70U   // milisegundos que la flecha queda apagada antes de volver a prenderse al confirmar una entrada; subirlo hace el parpadeo mas notorio pero mas lento
static uint8_t  sj_flash_pendiente = 0xFF;  /* 0xFF = nada pendiente */
static uint32_t sj_flash_tick      = 0;   // tick en que se apago la flecha, para saber cuando volver a prenderla

static void SimonJoy_ConfirmarInput(uint8_t dir) {   // apaga YA la flecha actual y programa que se vuelva a prender en SJ_FLASH_INPUT_MS (para que el parpadeo se note aunque se repita la misma direccion)
    if (sj_paso_dibujado != 0xFF) {   // si habia algo pintado
        Renderer_UpdateModoSimonJoystick1P(sj_paso_dibujado, 0xFF);   // lo apaga ya mismo
        sj_paso_dibujado = 0xFF;   // marca que no hay nada pintado ahora
    }
    sj_flash_pendiente = dir;   // guarda que direccion hay que prender despues del apagon
    sj_flash_tick      = HAL_GetTick();   // marca el instante del apagon, para medir cuando cumplir SJ_FLASH_INPUT_MS
    Buzzer_Beep(90);   // beep corto de confirmacion de entrada, sin importar si fue acierto o fallo (el fallo agrega un beep largo aparte)
}


/* tick no bloqueante del minijuego: se llama una vez por vuelta del loop
 * principal mientras en_juego_real este activo. Nunca usa HAL_Delay largo,
 * asi que el filtro/deadzone del joystick sigue actualizandose por interrupcion. */
static void SimonJoy_Actualizar(void) {   // tick no bloqueante de la maquina de estados de SimonJoy 1 jugador; se llama una vez por vuelta del loop principal
    uint32_t ahora = HAL_GetTick();   // tick actual, usado para medir tiempos en todas las fases

    if (sj_fase != SJ_GAMEOVER) {
        Renderer_ActualizarCursorJoystick(joystick2_x, joystick2_y, joy_listo_dir);   // mueve el cursor "X" del centro del D-pad segun la posicion cruda actual del stick (menos en game over, que muestra otra pantalla)
    }

    /* completa el parpadeo de SimonJoy_ConfirmarInput -- prende la flecha
     * ya pasado el tiempo minimo de "apagada" */
    if (sj_flash_pendiente != 0xFF && (ahora - sj_flash_tick) >= SJ_FLASH_INPUT_MS) {   // hay un prendido pendiente Y ya paso el tiempo minimo de apagado
        SimonJoy_MostrarPaso(sj_flash_pendiente);   // prende la flecha correspondiente a la entrada confirmada
        sj_flash_pendiente = 0xFF;   // consume el pendiente, para no repetir el prendido en el proximo tick
    }

    switch (sj_fase) {   // logica especifica de cada fase del juego
    case SJ_MOSTRANDO: {   // reproduciendo la secuencia completa, un paso a la vez, parpadeando
        uint16_t medio = (uint16_t)(SimonJoy_IntervaloActual() / 2U);   // mitad del intervalo actual = duracion de cada mitad (on/off) del parpadeo
        if ((ahora - sj_tick_fase) < medio) break;   // todavia no paso suficiente tiempo para cambiar de sub-fase
        sj_tick_fase = ahora;   // marca el inicio de la nueva sub-fase

        if (sj_mostrando_on) {
            /* mitad "apagada" del parpadeo, mismo paso */
            SimonJoy_MostrarPaso(0xFF);   // apaga la flecha actual (sigue siendo el mismo paso, solo cambia a "apagado")
            sj_mostrando_on = 0;   // pasa a la sub-fase "apagada"
        } else {
            sj_paso_mostrar++;   // avanza al siguiente paso de la secuencia a mostrar
            if (sj_paso_mostrar >= sj_longitud) {   // ya se mostraron TODOS los pasos de la secuencia
                sj_fase          = SJ_ESPERANDO;   // pasa a esperar la respuesta del jugador
                sj_paso_esperado = 0;   // el jugador debe repetir empezando desde el primer paso
                joy_listo_dir    = 1;   // habilita la deteccion de movimiento para la primera entrada del jugador
            } else {
                SimonJoy_MostrarPaso(sj_secuencia[sj_paso_mostrar]);   // prende la flecha del siguiente paso de la secuencia
                sj_mostrando_on = 1;   // vuelve a la sub-fase "encendida"
            }
        }
        break;
    }

    case SJ_ESPERANDO: {   // esperando que el jugador repita la secuencia paso a paso con el joystick
        uint8_t dir = Joystick_LeerDireccion();   // intenta leer una direccion nueva del joystick este tick
        if (dir == 0xFF) break;   // todavia no hay una entrada nueva, sigue esperando

        SimonJoy_ConfirmarInput(dir);  /* parpadeo real, aunque se repita la misma direccion */
        printf("[SIMONJOY 1P] dir=%u esperado=%u %s\r\n", dir, sj_secuencia[sj_paso_esperado],
               (dir == sj_secuencia[sj_paso_esperado]) ? "OK" : "FALLO");   // log de la entrada: que se leyo, que se esperaba, y si fue acierto o fallo

        if (dir != sj_secuencia[sj_paso_esperado]) {   // la direccion leida NO coincide con la esperada -- fallo
            if ((uint8_t)(sj_longitud - 1) > sj_mejor_racha) sj_mejor_racha = (uint8_t)(sj_longitud - 1);   // si la racha de esta partida (pasos acertados antes de fallar) supera la mejor de la sesion, la actualiza
            printf("[SIMONJOY 1P] GAME OVER racha=%u mejor=%u\r\n", (unsigned)(sj_longitud - 1), sj_mejor_racha);   // log del game over con la racha final y la mejor historica
            sj_fase      = SJ_GAMEOVER;   // pasa a la fase de game over
            sj_tick_fase = ahora;   // marca el instante de entrada a game over
            sj_flash_pendiente = 0xFF;  /* cancela: game over dibuja otra pantalla encima */
            Buzzer_Beep(350);  /* beep largo de error (extiende el corto que ya sonaba) */
            SimonJoy_DibujarGameOver();   // dibuja la pantalla de resultado
            break;
        }

        sj_paso_esperado++;   // acerto este paso: avanza al siguiente paso esperado
        if (sj_paso_esperado >= sj_longitud) {   // el jugador ya repitio TODA la secuencia correctamente
            if (sj_longitud > sj_mejor_racha) sj_mejor_racha = sj_longitud;   // actualiza la mejor racha si corresponde
            if (sj_longitud < SJ_MAX_LONGITUD) {   // todavia hay espacio en el arreglo para un paso mas (limite de seguridad, dificilmente se alcanza jugando)
                sj_secuencia[sj_longitud] = SJ_SiguienteDireccion();   // agrega un paso nuevo aleatorio al final de la secuencia
                sj_longitud++;   // la secuencia crece en 1
                SimonJoy_MostrarRacha(sj_longitud);   // actualiza el numero de ronda en pantalla
            }
            sj_fase      = SJ_ACIERTO;   // pasa a la pausa corta de "bien hecho" antes de repetir la secuencia mas larga
            sj_tick_fase = ahora;   // marca el inicio de esa pausa
            sj_flash_pendiente = 0xFF;   /* cancela cualquier prendido pendiente */
            SimonJoy_MostrarPaso(0xFF);  /* apaga el ultimo boton -- si no, se
                queda encendido durante la pausa y el primer paso de la
                siguiente repeticion no se ve como un flanco nuevo */
            {
                static const PasoSonido_t BEEP_RONDA[3] = {
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };  /* 2 pitidos cortos */
                Buzzer_Patron(BEEP_RONDA, 3);   // suena el jingle corto de "ronda superada"
            }
        }
        break;
    }

    case SJ_ACIERTO:   // pausa corta despues de completar una ronda, antes de repetir la secuencia (ahora un paso mas larga)
        if ((ahora - sj_tick_fase) < SJ_PAUSA_ACIERTO_MS) break;   // todavia no paso el tiempo de pausa configurado
        sj_paso_mostrar = 0;   // reinicia el indice de reproduccion al primer paso
        sj_mostrando_on = 1;   // arranca de nuevo en la sub-fase "encendida"
        sj_tick_fase    = ahora;   // marca el inicio de la nueva reproduccion
        sj_fase         = SJ_MOSTRANDO;   // vuelve a la fase de mostrar la secuencia (ahora mas larga)
        SimonJoy_MostrarPaso(sj_secuencia[0]);   // prende de una vez la primera flecha de la nueva repeticion
        break;

    case SJ_GAMEOVER:
        /* Al no existir un pulsador dedicado en el joystick, mover el stick
         * en cualquier direccion reinicia la partida (mismo patron de
         * "cualquier entrada reintenta" utilizado en el modo de botones);
         * la salida mediante B1 se maneja por separado en el bucle
         * principal. */
        if (Joystick_LeerDireccion() != 0xFF) { printf("[SIMONJOY 1P] retry\r\n"); SimonJoy_Iniciar(); }   // cualquier movimiento del stick reinicia una partida nueva
        break;
    }
}

/* ========================================================================== */
/* === SIMON + JOYSTICK — 2 JUGADORES CARA A CARA ============================ */
/* ========================================================================== */
/* Aplica la misma mecanica que la version de 1 jugador descrita arriba,
 * pero duplicada por jugador mediante arreglos de tamaño 2 (indice 0 = J1,
 * en la mitad inferior de la pantalla; indice 1 = J2, en la mitad superior,
 * rotada 180 grados). Cada jugador juega su propia secuencia de forma
 * independiente en su mitad de la pantalla en orientacion retrato (ver
 * Renderer_DrawModoSimonJoystick2P y las funciones Cockpit_* en
 * renderer.c). Este modo se activa al confirmar el modo "JOYS" con la
 * opcion "2 JUGADORES" seleccionada.
 *
 * Nota sobre el cableado: dado que cada joystick fisico puede presentar un
 * desplazamiento de ejes propio de su montaje individual (ver el comentario
 * de Joystick_LeerDireccion para el caso del joystick usado en el modo de 1
 * jugador), los signos de deteccion de direccion de cada joystick en este
 * modo se calibran de forma independiente en Joystick2_LeerDireccion. */

static uint8_t        sj2_secuencia[2][SJ_MAX_LONGITUD];   // secuencia de direcciones de cada jugador, independiente entre si (primer indice = jugador logico)
static uint8_t        sj2_longitud[2];   // cuantos pasos tiene la secuencia de cada jugador
static uint8_t        sj2_paso_mostrar[2];   // indice del paso que se esta parpadeando, por jugador
static uint8_t        sj2_paso_esperado[2];   // indice del paso que se espera del jugador, por jugador
static uint8_t        sj2_mostrando_on[2];   // sub-fase del parpadeo (on/off), por jugador
static uint8_t        sj2_paso_dibujado[2] = { 0xFF, 0xFF };   // ultimo paso pintado en el D-pad de cada jugador (0xFF=ninguno)
static uint8_t        sj2_mejor_racha[2];   // mejor racha de la sesion, por jugador
static SimonJoyFase_t sj2_fase[2];   // fase actual de la maquina de estados de cada jugador (independientes entre si)
static uint32_t       sj2_tick_fase[2];   // tick de inicio de la fase actual, por jugador
static uint32_t       sj2_seed[2] = { 1, 7 };   /* semillas distintas -- secuencias distintas entre jugadores */
static uint8_t        sj2_joy_listo_dir[2];   // 1 = el stick de ese jugador ya volvio al centro y puede contar un nuevo movimiento
static uint8_t        sj2_flash_pendiente[2] = { 0xFF, 0xFF };   // direccion pendiente de prender tras el apagon de confirmacion, por jugador
static uint32_t       sj2_flash_tick[2];   // tick del apagon de confirmacion, por jugador
static uint8_t        en_juego_real_2p = 0;   /* 1 = modo 2 jugadores real, no el recorrido */

static uint8_t SJ2_Random4(uint8_t p) {   // igual que SJ_Random4 pero con la semilla PROPIA del jugador p, para que sus secuencias no coincidan con las del otro
    sj2_seed[p] = sj2_seed[p] * 1103515245u + 12345u;   // mismo LCG que la version 1 jugador, aplicado a la semilla de este jugador
    return (uint8_t)((sj2_seed[p] >> 16) & 0x3u);   // mismos bits "del medio" recortados a 0-3
}

/* mismo anti-repeticion que SJ_SiguienteDireccion, por jugador */
static uint8_t SJ2_SiguienteDireccion(uint8_t p) {   // elige el proximo paso de la secuencia del jugador p, evitando una 3ra repeticion seguida
    uint8_t nuevo = SJ2_Random4(p);   // candidato aleatorio inicial
    if (sj2_longitud[p] >= 2 &&
        sj2_secuencia[p][sj2_longitud[p] - 1] == sj2_secuencia[p][sj2_longitud[p] - 2] &&
        nuevo == sj2_secuencia[p][sj2_longitud[p] - 1]) {   // los 2 ultimos pasos de ESTE jugador ya son iguales y el candidato tambien coincide
        nuevo = (uint8_t)((nuevo + 1u + (SJ2_Random4(p) % 3u)) & 0x3u);   // fuerza un valor distinto, elegido uniforme entre las otras 3 direcciones
    }
    return nuevo;
}

static uint16_t SimonJoy2_IntervaloActual(uint8_t p) {   // misma formula que SimonJoy_IntervaloActual pero con la longitud de secuencia PROPIA del jugador p
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(sj2_longitud[p] - 1) * SJ_VELOCIDAD_PASO;   // velocidad de ESTE jugador, segun su propia ronda
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;   // mismo tope maximo compartido entre los 2 jugadores
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);   // intervalo resultante para ESTE jugador
}

/* Aplica la misma logica de deteccion de flanco por zona de disparo que
 * Joystick_LeerDireccion, pero recibe el jugador logico p (0 o 1) y lo
 * traduce al joystick fisico correspondiente mediante JugadorFisico(). */
static uint8_t Joystick2_LeerDireccion(uint8_t p) {   // igual que Joystick_LeerDireccion pero para el jugador logico p (0 o 1), traduciendo a que joystick fisico leer
    /* Unicamente la lectura de hardware pasa por la variable "hw" (ver
     * JugadorFisico() y el comentario de la tabla BTN_SW mas arriba); el
     * arreglo sj2_joy_listo_dir[] permanece indexado por el jugador logico
     * "p", ya que representa estado de juego y no de hardware. */
    uint8_t hw = JugadorFisico(p);   // traduce jugador logico a joystick fisico a leer (0=hardware J1, 1=hardware J2)
    uint16_t jx = (hw == 0) ? joystick_x : joystick2_x;   // lectura cruda del eje X del joystick fisico que le toca a este jugador
    uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;   // idem para el eje Y
    uint16_t bajo_y, alto_y, bajo_x, alto_x;   // rango "sin direccion" de cada eje
    Joy_Umbrales((hw == 0) ? centro_j1y : centro_j2y, &bajo_y, &alto_y);   // calcula el rango del eje Y usando el centro medido del joystick fisico correcto
    Joy_Umbrales((hw == 0) ? centro_j1x : centro_j2x, &bajo_x, &alto_x);   // idem para el eje X

    if (!sj2_joy_listo_dir[p]) {   // este jugador todavia no volvio al centro tras su ultimo movimiento
        if (jy < alto_y && jy > bajo_y &&
            jx < alto_x && jx > bajo_x) {   // ambos ejes ya estan dentro de la banda muerta
            sj2_joy_listo_dir[p] = 1;   // habilita el proximo movimiento de ESTE jugador
        }
        return 0xFF;
    }
    /* Se utiliza el eje dominante (no el primero evaluado): un empuje
     * impreciso del joystick puede superar el umbral de ambos ejes
     * simultaneamente, por lo que debe prevalecer el eje que se encuentre
     * mas alejado de su centro medido. */
    uint16_t centro_x = (hw == 0) ? centro_j1x : centro_j2x;   // centro medido del eje X del joystick fisico de este jugador
    uint16_t centro_y = (hw == 0) ? centro_j1y : centro_j2y;   // idem eje Y
    int32_t dev_x = (int32_t)jx - (int32_t)centro_x;   // desviacion del canal "X" del ADC respecto a su centro medido
    int32_t dev_y = (int32_t)jy - (int32_t)centro_y;   // desviacion del canal "Y" del ADC respecto a su centro medido

    /* El joystick fisico J2 (hw==1) tiene los ejes electricos cruzados
     * respecto a la orientacion visual del montaje -- la MISMA
     * caracteristica ya documentada y usada en Joystick_LeerDireccion (modo
     * de 1 jugador, que SIEMPRE lee este mismo joystick fisico, ver su
     * comentario mas arriba): el canal "X" controla el movimiento VERTICAL y
     * el canal "Y" el LATERAL. El fisico J1 (hw==0), confirmado correcto tal
     * cual esta, no tiene ese cruce: cada canal controla su eje "natural". */
    int32_t dev_vert  = (hw == 1) ? dev_x : dev_y;   // vertical: canal X si es el fisico J2 (cruzado), canal Y si es el fisico J1
    int32_t dev_horiz = (hw == 1) ? dev_y : dev_x;   // lateral: el canal que no se uso arriba
    int32_t abs_vert  = (dev_vert  < 0) ? -dev_vert  : dev_vert;   // magnitud vertical
    int32_t abs_horiz = (dev_horiz < 0) ? -dev_horiz : dev_horiz;   // magnitud horizontal

    /* Una version anterior invertia tambien el signo de ARRIBA/ABAJO a
     * proposito respecto a Joystick_LeerDireccion (1 jugador) -- se
     * confirmo con el usuario en hardware real que esa inversion vertical
     * estaba mal (empujar hacia ARRIBA se registraba como ABAJO en los 2
     * fisicos, ver el signo ya corregido mas abajo). IZQUIERDA/DERECHA
     * (dev_horiz) tambien comparte la misma polaridad entre los 2 fisicos
     * una vez identificado el canal correcto -- una version anterior asumia
     * que el fisico J1 necesitaba el signo opuesto al J2 por una supuesta
     * diferencia de cableado real, pero esa asuncion resulto falsa (ver el
     * comentario junto al signo mas abajo). */
    uint8_t dir = 0xFF;   // sin direccion detectada todavia
    if (abs_vert >= (int32_t)JOY_UMBRAL_DESVIO && abs_vert >= abs_horiz) {   // vertical supero el umbral y es el eje dominante
        /* CORREGIDO (confirmado con el usuario en hardware real, los 2
         * fisicos): la inversion vertical de abajo estaba al reves --
         * empujar el stick fisico hacia ARRIBA se registraba como ABAJO y
         * viceversa, en ambos jugadores, causando fallos/game over con el
         * stick visualmente apuntando a la direccion correcta. Misma
         * polaridad (sin invertir) que Joystick_LeerDireccion, el modo de 1
         * jugador que ya esta confirmado correcto. */
        dir = (dev_vert > 0) ? 1 : 0;    /* ABAJO cuando el eje vertical crece, ARRIBA cuando decrece */
        sj2_joy_listo_dir[p] = 0;   // bloquea nuevas detecciones para este jugador hasta que vuelva al centro
    } else if (abs_horiz >= (int32_t)JOY_UMBRAL_DESVIO) {   // horizontal supero su umbral
        /* CORREGIDO (2026-08-02, confirmado con el usuario en hardware real
         * con una prueba de las 4 direcciones una por una, comparando contra
         * el cursor "X" que ya se sabe correcto): la version anterior de
         * esta linea distinguia por "hw" con signos opuestos entre los 2
         * fisicos (comentario viejo: "el canal Y de J2 y el canal X de J1 no
         * comparten el mismo sentido fisico"), asumiendo un cableado
         * distinto entre ambos joystick. Esa asuncion resulto ser falsa en
         * el hardware real ya armado: para el fisico J1 (hw==0), empujar a
         * la DERECHA se registraba como IZQUIERDA y viceversa (verificado
         * con el cursor llegando al borde derecho de su caja mientras la
         * consola decia IZQUIERDA). Los 2 fisicos comparten la MISMA
         * polaridad una vez que dev_horiz ya identifica el canal correcto
         * (ver dev_horiz arriba) -- no hace falta distinguir por hw aca. */
        dir = (dev_horiz > 0) ? 2 : 3;   /* IZQUIERDA cuando crece, DERECHA cuando decrece -- misma polaridad para los 2 fisicos */
        sj2_joy_listo_dir[p] = 0;
    }
    if (dir != 0xFF) printf("[INPUT] joystick J%u = %s\r\n", (unsigned)(hw + 1), DIR_NOMBRE[dir]);   // log con el numero de joystick FISICO (hw+1), no el jugador logico
    return dir;
}

static void SimonJoy2_MostrarPaso(uint8_t p, uint8_t nuevo) {   // igual que SimonJoy_MostrarPaso pero para la mitad de pantalla del jugador p
    if (nuevo == sj2_paso_dibujado[p]) return;   // ya esta pintado asi, no redibuja
    Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], nuevo);   // redibuja solo las 2 flechas que cambiaron, en la mitad de este jugador
    sj2_paso_dibujado[p] = nuevo;   // guarda el nuevo estado pintado de este jugador
    if (nuevo < 4) Buzzer_Beep(90);   // beep corto al prender una flecha (compartido entre los 2 jugadores, el buzzer es mono)
}

static void SimonJoy2_ConfirmarInput(uint8_t p, uint8_t dir) {   // igual que SimonJoy_ConfirmarInput pero para el jugador p
    if (sj2_paso_dibujado[p] != 0xFF) {   // si este jugador tenia algo pintado
        Renderer_UpdateModoSimonJoystick2PPaso(p, sj2_paso_dibujado[p], 0xFF);   // lo apaga ya
        sj2_paso_dibujado[p] = 0xFF;
    }
    sj2_flash_pendiente[p] = dir;   // guarda que direccion prender despues del apagon, para este jugador
    sj2_flash_tick[p]      = HAL_GetTick();   // marca el instante del apagon de este jugador
    Buzzer_Beep(90);
}

/* Inicia (o reinicia) unicamente al jugador p, sin afectar la pantalla del otro jugador. */
static void SimonJoy2_ReiniciarJugador(uint8_t p) {   // reinicia SOLO al jugador p (tras perder) sin tocar la partida en curso del otro
    sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 3u);   // remezcla la semilla de este jugador con el tick actual (mas un offset por jugador, para que no coincidan si arrancan en el mismo tick)
    if (sj2_seed[p] == 0) sj2_seed[p] = 1;   // evita semilla en 0

    sj2_longitud[p]        = 1;   // secuencia nueva de 1 paso
    sj2_secuencia[p][0]    = SJ2_Random4(p);   // primer paso aleatorio
    sj2_paso_mostrar[p]    = 0;   // arranca mostrando desde el paso 0
    sj2_mostrando_on[p]    = 1;   // arranca en la mitad "encendida"
    sj2_fase[p]            = SJ_MOSTRANDO;   // arranca reproduciendo la secuencia
    sj2_tick_fase[p]       = HAL_GetTick();   // marca el inicio de esta fase
    sj2_joy_listo_dir[p]   = 1;   // habilita deteccion de movimiento de entrada
    sj2_flash_pendiente[p] = 0xFF;   // sin flash pendiente

    Renderer_DrawModoSimonJoystick2PJugador(p, sj2_secuencia[p][0]);   // redibuja SOLO la mitad de pantalla de este jugador
    sj2_paso_dibujado[p] = sj2_secuencia[p][0];   // sincroniza el paso pintado
    Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);   // actualiza el numero de ronda mostrado de este jugador
    Renderer_ResetCursorJoystick2P(p);   // vuelve el cursor del centro del D-pad de este jugador a neutral
}

/* Dibuja ambas mitades de la pantalla desde cero (arranque de una partida nueva). */
static void SimonJoy2_IniciarAmbos(void) {   // arranca una partida nueva de SimonJoy 2 jugadores, ambos lados desde cero
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);   // arranca la musica de fondo compartida
    ILI9341_SetPortrait(1);   // pasa la pantalla a orientacion retrato (layout cara a cara)
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    for (uint8_t p = 0; p < 2; p++) {   // inicializa el estado de ambos jugadores
        sj2_seed[p] ^= (HAL_GetTick() + p * 977u + 1u);   // remezcla la semilla de cada jugador (offset distinto al de ReiniciarJugador, para variar aun mas)
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
    Renderer_DrawModoSimonJoystick2P(sj2_secuencia[0][0], sj2_secuencia[1][0]);   // dibuja las 2 mitades completas de una vez, cada una con su primer paso encendido
    Renderer_ActualizarRachaJoystick2P(0, sj2_longitud[0]);   // numero de ronda inicial de jugador 0
    Renderer_ActualizarRachaJoystick2P(1, sj2_longitud[1]);   // numero de ronda inicial de jugador 1
    Renderer_ResetCursorJoystick2P(0);   // cursor neutral del D-pad de jugador 0
    Renderer_ResetCursorJoystick2P(1);   // cursor neutral del D-pad de jugador 1
}

/* Tick no bloqueante de un jugador; se invoca 2 veces por cada vuelta del
 * bucle principal (una por jugador) mientras en_juego_real_2p este activo. */
static void SimonJoy2_ActualizarJugador(uint8_t p) {   // igual que SimonJoy_Actualizar pero para un jugador (p); se llama 2 veces por ciclo, una por jugador (ver SimonJoy2_Actualizar)
    uint32_t ahora = HAL_GetTick();   // tick actual, usado en todas las fases

    if (sj2_fase[p] != SJ_GAMEOVER) {
        /* El primer parametro de Renderer_ActualizarCursorJoystick2P
         * corresponde siempre al jugador logico "p" (determina en que
         * mitad de la pantalla se dibuja); unicamente el joystick fisico
         * de origen (jx/jy) cambia segun la correspondencia logico/fisico
         * (ver JugadorFisico() mas arriba). */
        uint8_t hw = JugadorFisico(p);   // traduce a joystick fisico correspondiente
        uint16_t jx = (hw == 0) ? joystick_x : joystick2_x;   // lectura cruda del canal "X" de ese joystick fisico
        uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;   // lectura cruda del canal "Y"
        /* CORREGIDO (2026-08-02): Renderer_ActualizarCursorJoystick2P ya
         * aplica POR SI SOLA el mismo cruce de canales que usa el cursor del
         * modo de 1 jugador para el fisico J2 (px sale de joy_y, py sale de
         * joy_x -- ver su implementacion en renderer.c). La version anterior
         * de este bloque volvia a cruzar jx/jy ANTES de llamarla, aplicando
         * el cruce 2 veces: para hw==1 eso anulaba el cruce (dejaba el
         * cursor como si fuera un joystick sin cruzar) y para hw==0 metia un
         * cruce que ese fisico no tiene, dejando los ejes horizontal/vertical
         * de la "X" en pantalla intercambiados entre si en los 2 jugadores
         * (mover el stick arriba/abajo desplazaba la X a los lados y
         * viceversa) -- la logica real de acierto/fallo (Joystick2_LeerDir-
         * eccion) no pasa por aca y no se vio afectada, pero al guiarse
         * visualmente por esta X el jugador terminaba empujando el stick en
         * una direccion fisica distinta a la que el juego registraba. Ahora
         * se le pasan los canales crudos en el mismo orden que usa el modo
         * de 1 jugador para el fisico J2 (hw==1: jx,jy tal cual, igual que
         * Renderer_ActualizarCursorJoystick(joystick2_x, joystick2_y, ...));
         * para el fisico J1 (hw==0, sin cruce electrico real) se le pasan
         * intercambiados (jy,jx) para cancelar el cruce que la funcion ya
         * aplica internamente y dejarlo sin cruzar.
         *
         * Ademas (mismo dia, mismo problema visual reportado por el
         * usuario), jx/jy se reescalan con Joy_CursorEscala usando el
         * centro medido de CADA eje antes de pasarlos: sin esto el cursor
         * quedaba descentrado en reposo y un empuje real apenas lo corria
         * unos pixeles dentro de su caja (ver comentario de
         * Joy_CursorEscala), lo cual se percibia como que se movia para
         * cualquier lado en vez de en linea recta hacia donde se empujaba
         * el stick, aun con el cruce de ejes ya corregido. */
        uint16_t centro_x = (hw == 0) ? centro_j1x : centro_j2x;   // centro medido del canal "X" de ese fisico
        uint16_t centro_y = (hw == 0) ? centro_j1y : centro_j2y;   // idem canal "Y"
        uint16_t sx = Joy_CursorEscala(jx, centro_x);   // canal "X" reescalado y centrado
        uint16_t sy = Joy_CursorEscala(jy, centro_y);   // canal "Y" reescalado y centrado
        if (hw == 1) {
            Renderer_ActualizarCursorJoystick2P(p, sx, sy, sj2_joy_listo_dir[p]);
        } else {
            Renderer_ActualizarCursorJoystick2P(p, sy, sx, sj2_joy_listo_dir[p]);
        }
    }

    if (sj2_flash_pendiente[p] != 0xFF && (ahora - sj2_flash_tick[p]) >= SJ_FLASH_INPUT_MS) {   // hay un prendido pendiente de este jugador y ya paso el tiempo minimo
        SimonJoy2_MostrarPaso(p, sj2_flash_pendiente[p]);   // prende la flecha confirmada de este jugador
        sj2_flash_pendiente[p] = 0xFF;
    }

    switch (sj2_fase[p]) {   // logica de la fase actual de ESTE jugador (independiente del otro)
    case SJ_MOSTRANDO: {
        uint16_t medio = (uint16_t)(SimonJoy2_IntervaloActual(p) / 2U);   // mitad del intervalo actual de este jugador
        if ((ahora - sj2_tick_fase[p]) < medio) break;   // todavia no toca cambiar de sub-fase para este jugador
        sj2_tick_fase[p] = ahora;

        if (sj2_mostrando_on[p]) {
            SimonJoy2_MostrarPaso(p, 0xFF);   // apaga la flecha actual de este jugador
            sj2_mostrando_on[p] = 0;
        } else {
            sj2_paso_mostrar[p]++;   // avanza al siguiente paso de la secuencia de ESTE jugador
            if (sj2_paso_mostrar[p] >= sj2_longitud[p]) {   // ya mostro toda su secuencia
                sj2_fase[p]          = SJ_ESPERANDO;   // pasa a esperar la respuesta de este jugador
                sj2_paso_esperado[p] = 0;
                sj2_joy_listo_dir[p] = 1;
            } else {
                SimonJoy2_MostrarPaso(p, sj2_secuencia[p][sj2_paso_mostrar[p]]);   // prende el siguiente paso de SU secuencia
                sj2_mostrando_on[p] = 1;
            }
        }
        break;
    }

    case SJ_ESPERANDO: {
        uint8_t dir = Joystick2_LeerDireccion(p);   // intenta leer una direccion nueva del joystick fisico de ESTE jugador
        if (dir == 0xFF) break;   // todavia nada nuevo

        SimonJoy2_ConfirmarInput(p, dir);
        printf("[SIMONJOY 2P] P%u dir=%u esperado=%u %s\r\n", p, dir, sj2_secuencia[p][sj2_paso_esperado[p]],
               (dir == sj2_secuencia[p][sj2_paso_esperado[p]]) ? "OK" : "FALLO");   // log con el jugador logico, la direccion leida, la esperada y si acerto

        if (dir != sj2_secuencia[p][sj2_paso_esperado[p]]) {   // fallo de ESTE jugador (no afecta al otro)
            if ((uint8_t)(sj2_longitud[p] - 1) > sj2_mejor_racha[p]) sj2_mejor_racha[p] = (uint8_t)(sj2_longitud[p] - 1);   // actualiza la mejor racha de este jugador si corresponde
            printf("[SIMONJOY 2P] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);
            sj2_fase[p]            = SJ_GAMEOVER;   // SOLO este jugador pasa a game over, el otro sigue jugando su propia partida
            sj2_tick_fase[p]       = ahora;
            sj2_flash_pendiente[p] = 0xFF;
            Buzzer_Beep(350);
            Renderer_DibujarGameOverJoystick2P(p, (uint16_t)(sj2_longitud[p] - 1), sj2_mejor_racha[p]);   // dibuja el resultado SOLO en la mitad de este jugador
            break;
        }

        sj2_paso_esperado[p]++;   // acerto: avanza al siguiente paso esperado de este jugador
        if (sj2_paso_esperado[p] >= sj2_longitud[p]) {   // este jugador repitio TODA su secuencia
            if (sj2_longitud[p] > sj2_mejor_racha[p]) sj2_mejor_racha[p] = sj2_longitud[p];
            if (sj2_longitud[p] < SJ_MAX_LONGITUD) {
                sj2_secuencia[p][sj2_longitud[p]] = SJ2_SiguienteDireccion(p);   // agrega un paso nuevo a la secuencia de ESTE jugador
                sj2_longitud[p]++;
                Renderer_ActualizarRachaJoystick2P(p, sj2_longitud[p]);   // actualiza el numero de ronda en la mitad de este jugador
            }
            sj2_fase[p]            = SJ_ACIERTO;   // pausa corta antes de repetir, solo para este jugador
            sj2_tick_fase[p]       = ahora;
            sj2_flash_pendiente[p] = 0xFF;
            SimonJoy2_MostrarPaso(p, 0xFF);
            {
                static const PasoSonido_t BEEP_RONDA[3] = {
                    { BUZZER_TONO_HZ, 70 }, { 0, 60 }, { BUZZER_TONO_HZ, 70 }
                };
                Buzzer_Patron(BEEP_RONDA, 3);   // jingle corto de ronda superada (compartido, el buzzer es mono)
            }
        }
        break;
    }

    case SJ_ACIERTO:
        if ((ahora - sj2_tick_fase[p]) < SJ_PAUSA_ACIERTO_MS) break;   // todavia no paso la pausa de este jugador
        sj2_paso_mostrar[p] = 0;
        sj2_mostrando_on[p] = 1;
        sj2_tick_fase[p]    = ahora;
        sj2_fase[p]         = SJ_MOSTRANDO;
        SimonJoy2_MostrarPaso(p, sj2_secuencia[p][0]);   // prende de una vez el primer paso de la nueva repeticion de ESTE jugador
        break;

    case SJ_GAMEOVER:
        /* Al no existir un pulsador dedicado en el joystick, mover el
         * propio stick en cualquier direccion reinicia unicamente la
         * partida de ese jugador; la salida mediante B1 sigue disponible
         * para ambos lados y se maneja en el bucle principal. */
        if (Joystick2_LeerDireccion(p) != 0xFF) { printf("[SIMONJOY 2P] P%u retry\r\n", p); SimonJoy2_ReiniciarJugador(p); }   // solo el propio stick de este jugador lo reintenta, no afecta al otro
        break;
    }
}

static void SimonJoy2_Actualizar(void) {   // tick de la partida 2 jugadores: actualiza a cada jugador por separado, uno tras otro
    SimonJoy2_ActualizarJugador(0);   // procesa un tick de la logica del jugador 0
    SimonJoy2_ActualizarJugador(1);   // procesa un tick de la logica del jugador 1
}

/* ========================================================================== */
/* === BOTONES ARCADE — 2 JUGADORES, CADA UNO CON SUS 4 BOTONES ============== */
/* ========================================================================== */
/* Aplica la misma mecanica y el mismo layout cara a cara (retrato, cockpit)
 * que Simon + Joystick a 2 jugadores descrito arriba, con la diferencia de
 * que el color lo entrega un boton fisico real, con su propio LED (ver
 * Boton_LED y Botones_LeerColor), en lugar del joystick. Cada jugador juega
 * su propia secuencia de forma independiente en su propia mitad de la
 * pantalla. */

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
static uint8_t        en_juego_real_guitar = 0;    /* 1 = modo GUITAR HERO real (1 o 2 jugadores) */

/* El joystick limita de forma natural el ritmo de entrada, ya que
 * Joystick_LeerDireccion() exige que el stick salga de la zona muerta en
 * ambos ejes antes de contar el siguiente movimiento; el jugador no puede
 * encadenar aciertos mas rapido que el tiempo que tarda el stick en volver
 * al centro. El boton arcade no posee ese freno fisico (soltar y volver a
 * presionar es casi instantaneo), por lo que sin una limitacion equivalente
 * las rondas se completarian mucho mas rapido, alcanzando longitudes altas
 * (donde Botones_IntervaloActual ya esta en su tope de velocidad) de forma
 * prematura, y el ritmo de juego percibido diferiria notablemente entre
 * ambos modos. BTN_REARME_MIN_MS establece un tiempo de espera minimo entre
 * 2 entradas aceptadas de un mismo jugador, emulando artificialmente ese
 * freno. */
#define BTN_REARME_MIN_MS 250U
static uint32_t       btn_ultimo_input_tick[2];

/* Cooldown equivalente al de arriba, pero propio de Guitar Hero (arreglo
 * separado de btn_ultimo_input_tick para no compartir estado con Simon con
 * botones, aunque los 2 modos nunca corran al mismo tiempo). En Guitar Hero
 * el timing importa al milimetro -- hay que golpear notas que cruzan la
 * zona de golpe a velocidad constante, muchas veces una detras de otra
 * rapido -- asi que el freno es mucho mas corto que BTN_REARME_MIN_MS: solo
 * lo suficiente para no contar un rebote electrico del switch como 2
 * golpes distintos, sin notarse como demora para los dedos. */
#define GH_REARME_MIN_MS 50U
static uint32_t       gh_ultimo_input_tick[2];

/* Tiempo de espera minimo en la pantalla de GAME OVER antes de aceptar un
 * reintento: sin este margen, un boton que rebota justo al perder la
 * partida (o que el jugador aun mantiene presionado) podria reiniciar la
 * ronda antes de que la pantalla de resultado llegue a mostrarse. */
#define BTN_GAMEOVER_COOLDOWN_MS 1000U   // milisegundos de espera obligatoria en game over antes de aceptar un boton como reintento; bajarlo permite reintentar mas rapido pero mas riesgo de reiniciar sin querer

static uint8_t Botones_Random4(uint8_t p) {   // mismo LCG que SJ_Random4/SJ2_Random4 pero con la semilla propia de BOTONES del jugador p
    btn_seed[p] = btn_seed[p] * 1103515245u + 12345u;
    return (uint8_t)((btn_seed[p] >> 16) & 0x3u);
}

static uint8_t Botones_SiguienteColor(uint8_t p) {   // elige el proximo color de la secuencia del jugador p, evitando una 3ra repeticion seguida (mismo criterio que SJ_SiguienteDireccion)
    uint8_t nuevo = Botones_Random4(p);
    if (btn_longitud[p] >= 2 &&
        btn_secuencia[p][btn_longitud[p] - 1] == btn_secuencia[p][btn_longitud[p] - 2] &&
        nuevo == btn_secuencia[p][btn_longitud[p] - 1]) {
        nuevo = (uint8_t)((nuevo + 1u + (Botones_Random4(p) % 3u)) & 0x3u);
    }
    return nuevo;
}

static uint16_t Botones_IntervaloActual(uint8_t p) {   // misma formula de velocidad que SimonJoy, aplicada a la longitud de secuencia de BOTONES del jugador p
    float velocidad = SJ_VELOCIDAD_INICIAL + (float)(btn_longitud[p] - 1) * SJ_VELOCIDAD_PASO;
    if (velocidad > SJ_VELOCIDAD_MAX) velocidad = SJ_VELOCIDAD_MAX;
    return (uint16_t)((float)SJ_INTERVALO_REF_MS / velocidad);
}

static void Botones_MostrarColor(uint8_t p, uint8_t nuevo) {   // cambia cual boton esta "encendido" en pantalla Y en el LED fisico real (nuevo=0-3, o 0xFF=ninguno)
    if (nuevo == btn_paso_dibujado[p]) return;   // ya esta asi, no hace nada
    if (btn_paso_dibujado[p] < 4) Boton_LED(p, btn_paso_dibujado[p], 0);   // apaga el LED fisico del boton que estaba prendido antes (si habia uno)
    if (nuevo < 4) Boton_LED(p, nuevo, 1);   // prende el LED fisico del nuevo boton (si corresponde prender alguno)
    Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], nuevo);   // actualiza tambien el badge correspondiente en PANTALLA
    btn_paso_dibujado[p] = nuevo;   // guarda el nuevo estado pintado
    if (nuevo < 4) Buzzer_Beep(90);   // beep corto al prender un boton
}

/* Ciclo real de apagado y encendido (no instantaneo) para que 2 presiones
 * consecutivas del mismo color se perciban como 2 confirmaciones distintas;
 * aplica el mismo criterio que SimonJoy2_ConfirmarInput, extendido aqui al
 * LED fisico ademas de la pantalla. */
static void Botones_ConfirmarInput(uint8_t p, uint8_t color) {   // apaga YA el boton actual (LED + pantalla) y programa el prendido del nuevo color tras el apagon, igual criterio que SimonJoy2_ConfirmarInput
    if (btn_paso_dibujado[p] != 0xFF) {   // si habia algo prendido
        Boton_LED(p, btn_paso_dibujado[p], 0);   // apaga el LED fisico
        Renderer_UpdateModoSimonClasicoPaso(p, btn_paso_dibujado[p], 0xFF);   // apaga el badge en pantalla
        btn_paso_dibujado[p] = 0xFF;
    }
    btn_flash_pendiente[p] = color;   // guarda que color prender despues del apagon
    btn_flash_tick[p]      = HAL_GetTick();   // marca el instante del apagon
    Buzzer_Beep(90);
}

static void Botones_ReiniciarJugador(uint8_t p) {   // reinicia SOLO al jugador p tras perder, sin tocar al otro
    btn_seed[p] ^= (HAL_GetTick() + p * 977u + 5u);   // remezcla la semilla de este jugador (offset 5, distinto al resto de las funciones "reiniciar/iniciar" para variar mas)
    if (btn_seed[p] == 0) btn_seed[p] = 1;

    btn_longitud[p]        = 1;   // secuencia nueva de 1 paso
    btn_secuencia[p][0]    = Botones_Random4(p);   // primer color aleatorio
    btn_paso_mostrar[p]    = 0;
    btn_mostrando_on[p]    = 1;
    btn_fase[p]            = SJ_MOSTRANDO;
    btn_tick_fase[p]       = HAL_GetTick();
    btn_flash_pendiente[p] = 0xFF;
    btn_ultimo_input_tick[p] = HAL_GetTick();   // reinicia el cooldown de entrada para este jugador

    Renderer_DrawModoSimonClasicoJugador(p, btn_secuencia[p][0]);   // redibuja SOLO la mitad de este jugador
    Boton_LED(p, btn_secuencia[p][0], 1);   // prende el LED fisico del primer color de la nueva secuencia
    btn_paso_dibujado[p] = btn_secuencia[p][0];
    Renderer_ActualizarRachaBotones(p, btn_longitud[p]);   // actualiza el numero de ronda de este jugador
}

static void Botones_IniciarAmbos(void) {   // arranca una partida nueva de BOTONES a 2 jugadores, ambos lados desde cero
    Renderer_SetSimonClasicoInvertido(1);   // en 2 jugadores, ROJO<->AMARILLO y VERDE<->AZUL cambian de posicion en pantalla para coincidir con la disposicion fisica real de los botones (pedido explicito del usuario)
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    for (uint8_t p = 0; p < 2; p++) {   // inicializa el estado de ambos jugadores
        btn_seed[p] ^= (HAL_GetTick() + p * 977u + 2u);   // remezcla semilla (offset 2, distinto de ReiniciarJugador)
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
        for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 4 LEDs de este jugador antes de arrancar (por si quedo alguno prendido de una partida anterior)
    }
    Renderer_DrawModoSimonClasico(btn_secuencia[0][0], btn_secuencia[1][0]);   // dibuja las 2 mitades completas de una vez
    Boton_LED(0, btn_secuencia[0][0], 1);   // prende el LED del primer color de jugador 0
    Boton_LED(1, btn_secuencia[1][0], 1);   // prende el LED del primer color de jugador 1
    btn_paso_dibujado[0] = btn_secuencia[0][0];
    btn_paso_dibujado[1] = btn_secuencia[1][0];
    Renderer_ActualizarRachaBotones(0, btn_longitud[0]);
    Renderer_ActualizarRachaBotones(1, btn_longitud[1]);
}

/* Arranca el modo de botones a 1 solo jugador (jugador logico 1) -- la
 * mitad del jugador 2 se deja en negro, apagada, y Botones_Actualizar()
 * nunca invoca Botones_ActualizarJugador(1) mientras btn_modo_1p este
 * activo (ver mas abajo). Reutiliza Botones_ReiniciarJugador(1), que ya
 * implementa exactamente esta inicializacion para el reintento tras un
 * game over. */
static void Botones_IniciarSolo(void) {   // arranca el modo de botones a 1 solo jugador; la mitad del jugador 2 queda apagada y sin logica en ejecucion
    Renderer_SetSimonClasicoInvertido(0);   // el modo de 1 jugador NO debe modificarse: siempre el layout historico, sin importar que haya quedado activo en una partida de 2 jugadores anterior
    Buzzer_Fondo_Iniciar(CANCION_TETRIS_IDX);
    ILI9341_SetPortrait(1);
    /* No se aplica el giro de hardware en este modo: la transformacion de
     * Cockpit_FillRect/DrawString/Punto (que orienta correctamente cada
     * jugador en el modo de 2 jugadores) ya deja al jugador logico 0
     * orientado hacia el lado opuesto de la mesa; sumar ademas el giro de
     * hardware produciria una rotacion adicional de 180 grados no deseada. */
    ILI9341_FillScreen(COLOR_BLACK);   // borra toda la pantalla (incluida la mitad del jugador 2, que se queda vacia)
    for (uint8_t c = 0; c < 4; c++) { Boton_LED(0, c, 0); Boton_LED(1, c, 0); }   // apaga los 8 LEDs (de ambos jugadores) antes de arrancar
    Botones_ReiniciarJugador(1);   // reusa la logica de "reiniciar tras perder" para arrancar tambien la primera partida
}

static void Botones_ActualizarJugador(uint8_t p) {   // tick no bloqueante de UN jugador de BOTONES; misma estructura de maquina de estados que SimonJoy2_ActualizarJugador
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
        uint8_t color = Botones_LeerColor(p);   // intenta leer un flanco de boton de este jugador
        if (color == 0xFF) break;   // nada presionado este tick
        if ((ahora - btn_ultimo_input_tick[p]) < BTN_REARME_MIN_MS) break;  /* cooldown: ver BTN_REARME_MIN_MS */
        btn_ultimo_input_tick[p] = ahora;   // marca el instante de esta entrada aceptada, para el cooldown de la proxima

        Botones_ConfirmarInput(p, color);
        printf("[BOTONES] P%u color=%u esperado=%u %s\r\n", p, color, btn_secuencia[p][btn_paso_esperado[p]],
               (color == btn_secuencia[p][btn_paso_esperado[p]]) ? "OK" : "FALLO");

        if (color != btn_secuencia[p][btn_paso_esperado[p]]) {   // fallo de este jugador
            if ((uint8_t)(btn_longitud[p] - 1) > btn_mejor_racha[p]) btn_mejor_racha[p] = (uint8_t)(btn_longitud[p] - 1);
            printf("[BOTONES] P%u GAME OVER racha=%u mejor=%u\r\n", p, (unsigned)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            btn_fase[p]            = SJ_GAMEOVER;
            btn_tick_fase[p]       = ahora;
            btn_flash_pendiente[p] = 0xFF;
            for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 4 LEDs de este jugador al perder
            Buzzer_Beep(350);
            Renderer_DibujarGameOverBotones(p, (uint16_t)(btn_longitud[p] - 1), btn_mejor_racha[p]);
            break;
        }

        btn_paso_esperado[p]++;
        if (btn_paso_esperado[p] >= btn_longitud[p]) {   // este jugador completo toda su secuencia
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
        /* No existe un boton de confirmacion separado: cualquiera de los
         * botones propios del jugador reinicia la partida, siempre que ya
         * haya transcurrido BTN_GAMEOVER_COOLDOWN_MS. Botones_LeerColor
         * debe invocarse en cada ciclo, independientemente de si el
         * cooldown ya se cumplio, para que su logica interna de antirrebote
         * no pierda sincronizacion. */
        uint8_t color = Botones_LeerColor(p);   // se invoca siempre, haya pasado o no el cooldown (ver el comentario anterior)
        if (color != 0xFF && (ahora - btn_tick_fase[p]) >= BTN_GAMEOVER_COOLDOWN_MS) {   // hubo un boton Y ya paso el cooldown minimo de game over
            printf("[BOTONES] P%u retry\r\n", p);
            Botones_ReiniciarJugador(p);
        }
        break;
    }
    }
}

static void Botones_Actualizar(void) {
    if (btn_modo_1p) {
        // En modo de 1 Jugador, actualiza SOLO al jugador de arriba (lógico 1)
        Botones_ActualizarJugador(1);
    } else {
        // En modo de 2 Jugadores, actualiza a ambos lados de forma independiente
        Botones_ActualizarJugador(0);
        Botones_ActualizarJugador(1);
    }
}


/* ========================================================================== */
/* === GUITAR HERO — CARA A CARA (RETRATO, COCKPIT) ========================== */
/* ========================================================================== */
/* Implementa notas reales generadas mediante un spawn periodico
 * independiente por jugador, a diferencia de la maqueta original del
 * recorrido de diseño (DEMO_JUGANDO / GuitarHero_IntentarGolpe), que
 * emplea 2 notas fijas y B1 como boton unico sin distinguir color ni
 * carril. En este modo, cada jugador dispone de su propia mitad del
 * arreglo gs.notas[MAX_NOTES] (posiciones 0-7 para el jugador 1 y 8-15
 * para el jugador 2; ver el comentario de MAX_NOTES en game_state.h), y su
 * color debe coincidir con el carril de la nota para que el golpe sea
 * valido; utiliza los mismos 4 botones arcade que el modo Simon Clasico
 * (ver Botones_LeerColor). El diseño visual (carril angosto y zona de
 * golpe circular) se implementa en las funciones Renderer_DrawModoGuitar-
 * Hero... y Renderer_GH_... de renderer.c (ver guitar_hero/ANALISIS_REFE-
 * RENCIA.md para el diseño de referencia consultado). Cada jugador
 * finaliza su ronda al alcanzar NOTES_PER_GAME notas; en ese momento se
 * muestra "RONDA COMPLETA" unicamente en su mitad de pantalla, y
 * cualquiera de sus propios 4 botones inicia una ronda nueva sin afectar
 * la partida en curso del otro jugador (mismo patron de reintento aplicado
 * en Simon Clasico y Simon + Joystick). */

#define GH_NOTA_BASE(p)     ((uint8_t)((p) * (MAX_NOTES / 2)))
#define GH_NOTAS_POR_JUG    (MAX_NOTES / 2)
#define GH_SPAWN_MS         SPAWN_INTERVAL_L2

static uint32_t gh_seed[2]      = { 5, 13 };   // semillas del LCG de Guitar Hero, independientes de SimonJoy/Botones
static uint8_t  gh_terminado[2] = { 0, 0 };   // 1 = este jugador ya completo su ronda de NOTES_PER_GAME notas y esta esperando reintentar

/* Notas sostenidas ("quemar"): gh_sosteniendo[p] guarda el INDICE (dentro de
 * la mitad de notas de este jugador, 0..GH_NOTAS_POR_JUG-1) de la nota que
 * se esta sosteniendo ahora mismo, o -1 si ninguna. Solo puede haber una a
 * la vez por jugador (si hay una en curso, no se evaluan golpes nuevos, ver
 * GuitarHero_ActualizarJugador). gh_sostener_desde marca el tick en que
 * arranco ese sostenido, para medir cuanto tiempo lleva. */
static int8_t   gh_sosteniendo[2]     = { -1, -1 };
static uint32_t gh_sostener_desde[2];

static uint8_t GH_Random4(uint8_t p) {   // mismo LCG que los otros modos, aplicado a la semilla de Guitar Hero del jugador p
    gh_seed[p] = gh_seed[p] * 1103515245u + 12345u;
    return (uint8_t)((gh_seed[p] >> 16) & 0x3u);
}

/* Inicia (o reinicia) unicamente al jugador p, sin afectar la mitad del otro. */
static void GuitarHero_ReiniciarJugador(uint8_t p) {   // resetea el puntaje, combo y notas de un jugador, y redibuja unicamente su mitad
    gh_seed[p] ^= (HAL_GetTick() + p * 977u + 9u);   // remezcla la semilla de este jugador
    if (gh_seed[p] == 0) gh_seed[p] = 1;

    gs.j[p].puntaje           = 0;   // puntaje en 0 al arrancar/reiniciar
    gs.j[p].combo             = 0;   // combo en 0
    gs.j[p].notas_spawneadas  = 0;   // contador de notas ya generadas en esta ronda, en 0
    gs.j[p].tick_ultimo_spawn = HAL_GetTick();   // marca el instante base para el proximo spawn de nota
    gh_ultimo_input_tick[p]  = HAL_GetTick();   // reinicia el cooldown de entrada de este jugador (ver GH_REARME_MIN_MS)
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) gs.notas[GH_NOTA_BASE(p) + i].activa = 0;   // desactiva todas las notas de la mitad de este jugador (limpia notas viejas de la partida anterior)
    gh_terminado[p]    = 0;   // ya no esta "terminado", arranca una ronda nueva
    gh_sosteniendo[p]  = -1;   // ninguna nota sostenida en curso (una partida anterior no debe dejar una "colgada")

    Renderer_DrawModoGuitarHeroJugador(p);   // redibuja SOLO la mitad de pantalla de este jugador (carril, zona de golpe, encabezado)
    Renderer_GH_ActualizarPuntaje(p, 0, 0);   // muestra puntaje/combo en 0
}

/* Dibuja ambas mitades de la pantalla desde cero (arranque de una partida nueva a 2 jugadores). */
static void GuitarHero2_IniciarAmbos(void) {   // arranca una partida nueva de Guitar Hero a 2 jugadores, ambos lados desde cero
    Buzzer_Fondo_Iniciar(cancion_cursor);   // cancion elegida por los jugadores en DEMO_MENU_CANCIONES (unico paso previo a Guitar Hero, ver MenuCanciones_Procesar); a partir de que esa cancion complete una vuelta, Buzzer_Fondo_RotarSiTermino sigue variando sola
    ILI9341_SetPortrait(1);
    ILI9341_SetFlip180(1);   // aplica un giro de 180 grados en hardware para la orientacion de la mitad del jugador 2
    gs.nota_speed = NOTE_SPEED_L2;   // velocidad de caida de notas de este modo real (no confundir con el recorrido de diseño DEMO_JUGANDO)
    memset(gs.notas, 0, sizeof(gs.notas));   // limpia TODO el arreglo de notas (ambos jugadores) antes de arrancar
    Renderer_DrawModoGuitarHero2P();   // dibuja el layout completo de las 2 mitades (carriles, zonas de golpe, encabezados)
    GuitarHero_ReiniciarJugador(0);   // inicializa el estado de jugador 0
    GuitarHero_ReiniciarJugador(1);   // inicializa el estado de jugador 1
}

/* Arranca Guitar Hero a 1 solo jugador (jugador logico 1) -- la mitad del
 * jugador 2 se deja en negro, apagada, siguiendo el mismo patron que
 * Botones_IniciarSolo. Tampoco se aplica el giro de hardware en este modo,
 * por el mismo motivo explicado en Botones_IniciarSolo: la transformacion
 * de Cockpit_FillRect/DrawString/Punto ya orienta correctamente al jugador
 * logico 0 hacia el lado opuesto, y sumar el giro de hardware produciria
 * una rotacion adicional no deseada. */
static void GuitarHero_IniciarSolo(void) {   // arranca Guitar Hero a 1 solo jugador; la mitad del jugador 2 queda apagada
    Buzzer_Fondo_Iniciar(cancion_cursor);   // misma cancion elegida en DEMO_MENU_CANCIONES, ver GuitarHero2_IniciarAmbos
    ILI9341_SetPortrait(1);
    ILI9341_FillScreen(COLOR_BLACK);   // borra toda la pantalla, incluida la mitad del jugador 2 (que queda vacia)
    gs.nota_speed = NOTE_SPEED_L2;
    memset(gs.notas, 0, sizeof(gs.notas));   // limpia todas las notas antes de arrancar
    GuitarHero_ReiniciarJugador(1);
}

static void GuitarHero_ActualizarJugador(uint8_t p) {   // tick no bloqueante de UN jugador de Guitar Hero: spawnea notas, lee golpes, mueve/dibuja notas y detecta fin de ronda
    /* revierte el flash blanco de impacto del tick anterior (si hubo uno) --
     * ver Renderer_GH_FlashZona/ActualizarFlashes en renderer.c, esto le da
     * exactamente 1 frame (~33ms) de blanco antes de volver a su estilo
     * normal de "blanco/diana". */
    Renderer_GH_ActualizarFlashes(p);

    if (gh_terminado[p]) {   // este jugador ya termino su ronda de NOTES_PER_GAME notas, esta esperando a que reintente
        if (Botones_LeerColor(p) != 0xFF) {   // cualquiera de sus 4 botones arranca una ronda nueva
            printf("[GUITARHERO] P%u nueva ronda\r\n", p);
            GuitarHero_ReiniciarJugador(p);
        }
        return;   // mientras esta terminado, no corre el resto de la logica (spawns/movimiento) de este jugador
    }

    uint32_t ahora = HAL_GetTick();
    uint8_t  base  = GH_NOTA_BASE(p);   // indice base del arreglo gs.notas donde arranca la mitad de este jugador (0 para p=0, MAX_NOTES/2 para p=1)

    /* Spawn periodico -- una nota nueva cada GH_SPAWN_MS mientras queden
     * cupos en la ronda (NOTES_PER_GAME) y un slot libre en la mitad de p. */
    if (gs.j[p].notas_spawneadas < NOTES_PER_GAME &&
        (ahora - gs.j[p].tick_ultimo_spawn) >= GH_SPAWN_MS) {   // todavia no se generaron todas las notas de la ronda Y ya paso el intervalo de spawn
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // busca el primer slot LIBRE (no activo) en la mitad de este jugador
            Nota_t *n = &gs.notas[base + i];
            if (n->activa) continue;   // este slot ya esta ocupado por otra nota, prueba el siguiente
            n->carril    = GH_Random4(p);   // color/carril aleatorio de la nota nueva
            n->jugador   = p;   // marca a que jugador pertenece
            n->x_rel     = COCKPIT_ZONE_W;   // nace justo en el borde derecho de la zona de juego de este jugador
            n->x_prev    = n->x_rel;   // posicion previa igual a la actual, para que el primer borrado de "estela" no borre nada de mas
            n->sostenida = (GH_Random4(p) == 0);   // ~1 de cada 4 notas es "sostenida" (hay que mantenerla, ver GH_SOSTENIDA_DURACION_MS), el resto son de golpe instantaneo como siempre
            n->activa    = 1;   // la marca como activa/en juego
            gs.j[p].notas_spawneadas++;   // cuenta una nota mas generada en esta ronda
            gs.j[p].tick_ultimo_spawn = ahora;   // reinicia el cronometro del proximo spawn
            break;   // ya encontro un slot libre, no sigue buscando otro
        }
    }

    /* Nota sostenida en curso: se maneja SIEMPRE primero (aunque este tick
     * tambien traiga un flanco de boton nuevo, ver el guard gh_sosteniendo[p]
     * < 0 mas abajo) porque necesita nivel (Botones_ColorSostenido), no
     * flanco -- mientras se mantiene el mismo boton apretado, Botones_LeerColor
     * no vuelve a devolver un flanco para el. El puntaje se acredita
     * PROPORCIONAL al tiempo sostenido (SCORE_SOSTENIDA_POR_SEG por segundo),
     * asi que soltar antes de tiempo igual paga lo alcanzado a sostener, no
     * es todo o nada. */
    if (gh_sosteniendo[p] >= 0) {
        Nota_t  *n            = &gs.notas[base + gh_sosteniendo[p]];
        uint32_t sostenido_ms = ahora - gh_sostener_desde[p];   // cuanto lleva sosteniendose esta nota
        uint8_t  completa     = (sostenido_ms >= GH_SOSTENIDA_DURACION_MS);
        if (!n->activa || (!completa && !Botones_ColorSostenido(p, n->carril))) {   // se solto antes de tiempo (o la nota se desactivo por otro motivo)
            if (sostenido_ms > GH_SOSTENIDA_DURACION_MS) sostenido_ms = GH_SOSTENIDA_DURACION_MS;   // no pagar de mas si este tick llego tarde
            uint16_t bonus = (uint16_t)((sostenido_ms * SCORE_SOSTENIDA_POR_SEG) / 1000U);   // proporcional al tiempo realmente sostenido
            gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + bonus);
            gs.j[p].combo   = 0;   /* se corto antes de completarla -- corta combo, igual que un golpe fallado */
            n->activa       = 0;
            Renderer_GH_TerminarSostenida(p, n->carril);
            printf("[GUITARHERO] P%u sostenida CORTADA a %ums bonus=%u puntaje=%u\r\n",
                   p, (unsigned)sostenido_ms, bonus, gs.j[p].puntaje);
            gh_sosteniendo[p] = -1;
        } else if (completa) {   // se mantuvo presionado todo GH_SOSTENIDA_DURACION_MS
            uint16_t bonus = (uint16_t)((GH_SOSTENIDA_DURACION_MS * SCORE_SOSTENIDA_POR_SEG) / 1000U);
            gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + bonus);
            gs.j[p].combo++;   // sostenida completa cuenta como acierto para el combo
            n->activa = 0;
            Buzzer_Beep(120);   // beep mas largo que el golpe instantaneo, distingue la sostenida completa
            Renderer_GH_TerminarSostenida(p, n->carril);
            printf("[GUITARHERO] P%u sostenida COMPLETA bonus=%u puntaje=%u combo=%u\r\n",
                   p, bonus, gs.j[p].puntaje, gs.j[p].combo);
            gh_sosteniendo[p] = -1;
        } else {   // todavia en curso, sigue presionado y no llego a los 2s
            Renderer_GH_DrawNotaSostenida(p, n, (uint8_t)((sostenido_ms * 100U) / GH_SOSTENIDA_DURACION_MS));
        }
    }

    /* Input: el color propio del jugador caza la nota mas cercana de ESE
     * carril (no la mas cercana de cualquier color, a diferencia de la
     * maqueta original de 1 solo boton). Si ya hay una nota sosteniendose
     * (ver arriba), no se evaluan golpes nuevos hasta que termine -- solo
     * puede sostenerse una a la vez por jugador. */
    uint8_t color = Botones_LeerColor(p);   // intenta leer un flanco de boton de este jugador (SIEMPRE se llama, cooldown o no, para no desincronizar su antirrebote interno -- mismo criterio que el game over de Botones_ActualizarJugador)
    if (gh_sosteniendo[p] < 0 && color != 0xFF && (ahora - gh_ultimo_input_tick[p]) >= GH_REARME_MIN_MS) {   // presiono algun boton este tick Y ya paso el cooldown corto de Guitar Hero (ver GH_REARME_MIN_MS)
        gh_ultimo_input_tick[p] = ahora;   // marca el instante de esta entrada aceptada, para el cooldown de la proxima
        int16_t mejor_dist = 0x7FFF;   // arranca en el maximo posible
        int8_t  mejor_i    = -1;   // indice de la mejor nota encontrada del carril del color presionado
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // busca entre las notas de ESTE jugador
            Nota_t *n = &gs.notas[base + i];
            if (!n->activa || n->carril != color) continue;   // ignora notas apagadas o de OTRO carril (color) distinto al presionado
            int16_t centro_nota = (int16_t)(n->x_rel + NOTE_W / 2);   // centro X de la nota
            int16_t dist = (int16_t)((centro_nota > (int16_t)GH_ZONA_CX) ? (centro_nota - (int16_t)GH_ZONA_CX) : ((int16_t)GH_ZONA_CX - centro_nota));   // distancia absoluta al centro de la zona de golpe
            if (dist < mejor_dist) { mejor_dist = dist; mejor_i = (int8_t)i; }   // se queda con la mas cercana de ese carril
        }
        if (mejor_i >= 0 && mejor_dist <= (int16_t)HIT_OK) {   // encontro una nota de ese color Y esta dentro de la ventana de golpe
            Nota_t *n = &gs.notas[base + mejor_i];
            if (n->sostenida) {   // nota larga: arranca el sostenido en vez de puntuar/apagarla de una
                n->x_rel = (int16_t)(GH_ZONA_CX - NOTE_W / 2);   // la centra exacto en la zona de golpe, ahi se queda fija mientras dure (ver el loop de movimiento mas abajo)
                gh_sosteniendo[p]     = mejor_i;
                gh_sostener_desde[p]  = ahora;
                Buzzer_Beep(60);   // beep corto de "enganchada", distinto del golpe normal (80) y de la sostenida completa (120)
                printf("[GUITARHERO] P%u color=%u sostenida INICIO dist=%d\r\n", p, color, mejor_dist);
            } else {
                const char *calidad;
                if      (mejor_dist <= (int16_t)HIT_PERFECT) { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_PERFECT); calidad = "PERFECT"; }
                else if (mejor_dist <= (int16_t)HIT_GOOD)    { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_GOOD);    calidad = "GOOD"; }
                else                                          { gs.j[p].puntaje = (uint16_t)(gs.j[p].puntaje + SCORE_OK);     calidad = "OK"; }
                gs.j[p].combo++;   // suma combo por el golpe acertado
                n->activa = 0;   // la nota golpeada se desactiva (libera el slot para una nueva)
                Buzzer_Beep(80);   // beep corto de golpe
                Renderer_GH_FlashZona(p, color);   // dispara el flash blanco de impacto en la zona de golpe de este color
                printf("[GUITARHERO] P%u color=%u dist=%d %s puntaje=%u combo=%u\r\n",
                       p, color, mejor_dist, calidad, gs.j[p].puntaje, gs.j[p].combo);
            }
        } else {
            gs.j[p].combo = 0;   /* boton sin nota propia en rango -- corta combo */
            /* Registro de diagnostico del fallo (temporal, para diagnosticar
             * el reporte de "los botones dejan de responder" en Guitar Hero
             * a 2 jugadores): antes este caso no dejaba ningun rastro en la
             * consola, indistinguible de un boton que simplemente no se leyo.
             * Distingue el caso real -- no habia NINGUNA nota activa de ese
             * color todavia (hay que esperar a que spawnee y se acerque) --
             * del caso de haber apretado demasiado pronto/tarde respecto a
             * una nota que si estaba en pantalla. */
            if (mejor_i < 0)
                printf("[GUITARHERO] P%u color=%u MISS: sin nota activa en ese carril (spawneadas=%u/%u)\r\n",
                       p, color, (unsigned)gs.j[p].notas_spawneadas, (unsigned)NOTES_PER_GAME);
            else
                printf("[GUITARHERO] P%u color=%u MISS: nota mas cercana a dist=%d (fuera de HIT_OK=%d)\r\n",
                       p, color, mejor_dist, (int)HIT_OK);
        }
    }

    /* Movimiento + render delta de las notas activas de este jugador --
     * viajan de derecha a izquierda (nacen lejos, se acercan a la zona de
     * golpe), al reves de la maqueta original DEMO_JUGANDO. */
    for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) {   // recorre todas las notas (activas o no) de este jugador
        Nota_t *n = &gs.notas[base + i];
        if (!n->activa) continue;   // ignora las apagadas
        if ((int8_t)i == gh_sosteniendo[p]) continue;   // esta nota se esta sosteniendo (ver arriba): queda FIJA en la zona de golpe, no se mueve ni se dibuja aca (Renderer_GH_DrawNotaSostenida ya se encargo)
        n->x_prev = n->x_rel;   // guarda la posicion de este frame como "anterior", para poder borrar solo la estela recorrida
        n->x_rel  = (int16_t)(n->x_rel - gs.nota_speed);   // avanza la nota hacia la izquierda segun la velocidad configurada
        Renderer_GH_EraseNotaTrail(p, n, gs.nota_speed);   // borra solo el tramo de pantalla que la nota acaba de dejar atras (no toda la pantalla)
        if (n->x_rel < -(int16_t)NOTE_W) {   // la nota ya salio completamente por la izquierda sin ser golpeada
            n->activa     = 0;   // se desactiva (se perdio)
            gs.j[p].combo = 0;   /* nota perdida sin presionar -- corta combo */
        } else {
            Renderer_GH_DrawNota(p, n);   // todavia visible: la dibuja en su nueva posicion
        }
    }

    Renderer_GH_ActualizarPuntaje(p, gs.j[p].puntaje, gs.j[p].combo);   // refresca el puntaje/combo mostrados en pantalla

    /* Fin de ronda: se agotaron los spawns y no queda ninguna nota viva */
    if (gs.j[p].notas_spawneadas >= NOTES_PER_GAME) {   // ya se generaron todas las notas posibles de esta ronda
        uint8_t queda_activa = 0;
        for (uint8_t i = 0; i < GH_NOTAS_POR_JUG; i++) if (gs.notas[base + i].activa) { queda_activa = 1; break; }   // busca si queda alguna nota todavia viva en pantalla
        if (!queda_activa) {   // ninguna nota viva: la ronda termino de verdad
            gh_terminado[p] = 1;
            printf("[GUITARHERO] P%u ronda completa puntaje=%u\r\n", p, gs.j[p].puntaje);
            Renderer_GH_DibujarFin(p, gs.j[p].puntaje);   // dibuja "RONDA COMPLETA" con el puntaje final, solo en la mitad de este jugador
        }
    }
}

static void GuitarHero_Actualizar(void) {   // tick de Guitar Hero: en modo Solo actualiza SOLO al jugador 1 (el que GuitarHero_IniciarSolo dibujo e inicializo); en 2 jugadores actualiza ambos
    if (guitar_modo_1p) {
        GuitarHero_ActualizarJugador(1);
    } else {
        GuitarHero_ActualizarJugador(0);
        GuitarHero_ActualizarJugador(1);
    }
}

/* ========================================================================== */
/* === FUNCION PRINCIPAL Y NAVEGACION DE MENUS =============================== */
/* ========================================================================== */

/* --------------------------------------------------------------------------
 * NAVEGACION CON JOYSTICK SIN EXIGIR CENTRADO EXACTO
 * -------------------------------------------------------------------------- */
#define MENU_JOY_COOLDOWN_MS 300U   // tiempo minimo (en milisegundos) entre 2 movimientos de cursor aceptados en el menu de seleccion de jugadores; disminuir este valor hace el menu mas sensible y rapido
#define MENU_MODO_COOLDOWN_MS 250U   // idem, para el menu de seleccion de modo

/* Mueve el cursor unicamente mediante el joystick (cualquiera de los 2),
 * dejando el boton arcade reservado exclusivamente para la accion de
 * confirmar (ver la variable "confirmar_boton" en main()). Esta separacion
 * es necesaria porque, si el mismo boton se utilizara tanto para mover el
 * cursor como para confirmar la seleccion, el menu nunca completaria la
 * transicion a la siguiente pantalla: el cursor se moveria, pero la accion
 * de avanzar nunca se ejecutaria. */
static void MenuJugadores_Procesar(uint8_t *screen) {   // mueve el cursor del menu 1/2 JUGADORES cuando cualquiera de los 2 joystick sale de su zona muerta
    static uint32_t ultimo_mov_joy = 0;   // tick del ultimo movimiento aceptado, para aplicar el cooldown
    uint32_t ahora = HAL_GetTick();

    uint16_t j1x_b, j1x_a, j1y_b, j1y_a, j2x_b, j2x_a, j2y_b, j2y_a;   // rangos "sin movimiento" de los 4 ejes (X/Y de J1 y J2)
    Joy_Umbrales(centro_j1x, &j1x_b, &j1x_a);
    Joy_Umbrales(centro_j1y, &j1y_b, &j1y_a);
    Joy_Umbrales(centro_j2x, &j2x_b, &j2x_a);
    Joy_Umbrales(centro_j2y, &j2y_b, &j2y_a);

    uint8_t joy_movido = 0;   // 1 si corresponde alternar el cursor este tick
    if ((ahora - ultimo_mov_joy) >= MENU_JOY_COOLDOWN_MS) {   // solo revisa si ya paso el cooldown desde el ultimo movimiento
        if (joystick_x < j1x_b || joystick_x > j1x_a || joystick_y < j1y_b || joystick_y > j1y_a ||
            joystick2_x < j2x_b || joystick2_x > j2x_a || joystick2_y < j2y_b || joystick2_y > j2y_a) {   // CUALQUIERA de los 4 ejes (de cualquiera de los 2 joystick) esta fuera de su banda muerta
            joy_movido = 1;
            ultimo_mov_joy = ahora;   // reinicia el cooldown
        }
    }

    if (joy_movido) {
        uint8_t screen_ant = *screen;   // guarda la pantalla (cursor) anterior, para saber que borrar
        *screen = (*screen == DEMO_JUGADORES_1) ? DEMO_JUGADORES_2 : DEMO_JUGADORES_1;   // alterna entre las 2 unicas opciones (no hay mas de 2, asi que "mover" siempre es alternar)
        Renderer_UpdateSeleccionJugadores((uint8_t)(screen_ant - DEMO_JUGADORES_1),
                                          (uint8_t)(*screen - DEMO_JUGADORES_1));   // redibuja solo el cambio de cursor (de que opcion a que opcion), no todo el menu
        printf("[MENU] jugadores -> %u\r\n", (unsigned)(*screen - DEMO_JUGADORES_1));
    }
}

static void MenuModos_Procesar(uint8_t *screen) {   // avanza el cursor del menu de MODO (Simon/Sim+Joy/Guitar) cuando cualquier joystick se mueve
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
        uint8_t screen_ant = *screen;   // pantalla (opcion) anterior, para saber que borrar
        if (*screen == DEMO_MODO_GUITAR)
            *screen = DEMO_MODO_SIMON;   // desde la ultima opcion, vuelve a la primera (ciclico)
        else
            (*screen)++;   // avanza a la siguiente opcion del enum
        Renderer_UpdateSeleccionModo((uint8_t)(screen_ant - DEMO_MODO_SIMON),
                                     (uint8_t)(*screen - DEMO_MODO_SIMON));   // redibuja solo el cambio de cursor
        printf("[MENU] modo -> %s\r\n", DEMO_NOMBRE[*screen]);
    }
}

/* Pantalla de canciones (DEMO_MENU_CANCIONES) SOLO como paso previo a Guitar
 * Hero (ver el bloque de confirmacion de modo mas abajo, unico lugar que
 * lleva a esta pantalla) -- pedido explicito del usuario para que los
 * jugadores elijan con que cancion arranca la musica de fondo antes de
 * jugar. CUALQUIER joystick (el de cualquiera de los 2 jugadores) sube/baja
 * el cursor por la lista, igual que un reproductor de musica; CUALQUIER
 * boton arcade confirma la cancion resaltada y arranca el conteo 3-2-1-GO
 * (via "confirmar_boton", calculado en main() igual que en el resto de las
 * pantallas DEMO_MODO_x y DEMO_JUGADORES_x). */

/* Se llama SIEMPRE justo antes de mostrar DEMO_MENU_CANCIONES (ver el
 * bloque de confirmacion de modo mas abajo). Fuerza a Joystick2_LeerDireccion
 * a exigir ver el stick centrado antes de aceptar el primer movimiento,
 * para no heredar como "subir/bajar" el mismo arrastre de joystick con el
 * que el jugador recien llego a la pantalla de MODO anterior (mover el
 * stick es como se navega entre DEMO_MODO_SIMON/SIMONJOY/GUITAR, ver
 * MenuModos_Procesar) -- mismo riesgo de "arrastre" que ya se evita en el
 * resto de este recorrido. */
static void MenuCanciones_Armar(void) {   // llamar SIEMPRE justo antes de mostrar DEMO_MENU_CANCIONES
    sj2_joy_listo_dir[0] = 0;
    sj2_joy_listo_dir[1] = 0;
}

static void MenuCanciones_Procesar(uint8_t *screen) {   // sube/baja el cursor de la lista con cualquiera de los 2 joystick; confirmar corre en main() via confirmar_boton
    (void)screen;   // esta pantalla no cambia sola de pantalla: la confirmacion (boton) se maneja en main()
    for (uint8_t p = 0; p < 2; p++) {   // revisa el joystick de cada jugador por separado, cualquiera de los 2 puede navegar
        uint8_t dir = Joystick2_LeerDireccion(p);   // 0=ARRIBA, 1=ABAJO, 2/3=IZQUIERDA/DERECHA (ignoradas aca), 0xFF=sin movimiento nuevo
        if (dir != 0 && dir != 1) continue;   // solo arriba/abajo mueven el cursor de esta lista

        uint8_t ant = cancion_cursor;
        cancion_cursor = (dir == 0)
            ? (uint8_t)((cancion_cursor == 0) ? (CANCIONES_N - 1) : (cancion_cursor - 1))   /* ARRIBA: cancion anterior, con vuelta ciclica */
            : (uint8_t)((cancion_cursor + 1) % CANCIONES_N);                                 /* ABAJO: cancion siguiente, con vuelta ciclica */
        Renderer_UpdateListaCanciones(ant, cancion_cursor);
        Buzzer_Patron(CANCIONES_DATA[cancion_cursor], CANCIONES_LEN[cancion_cursor]);   // previsualiza el sonido de la cancion resaltada
    }
}

/* Recorre ciclicamente el alfabeto A-Z para la letra actual del nombre en
 * edicion (nombre_jugadores[nombre_jugador_actual][nombre_pos_actual]),
 * usando el joystick del jugador que esta escribiendo (arriba = siguiente
 * letra, abajo = letra anterior). La confirmacion, ya sea para avanzar de
 * posicion o de jugador, se gestiona en la logica de confirmacion de menus
 * de main(), no en esta funcion. */
#define INICIALES_JOY_COOLDOWN_MS 180U   // tiempo minimo (en milisegundos) entre 2 cambios de letra aceptados; disminuir este valor permite recorrer las letras mas rapido
static void MenuIniciales_Procesar(void) {   // recorre ciclicamente A-Z la letra actual del nombre en edicion, segun el joystick del jugador correspondiente
    static uint32_t ultimo_mov = 0;
    uint32_t ahora = HAL_GetTick();
    if ((ahora - ultimo_mov) < INICIALES_JOY_COOLDOWN_MS) return;   // todavia no paso el cooldown desde el ultimo cambio de letra

    /* El mismo joystick fisico que un jugador utilizara durante la partida
     * (determinado por JugadorFisico(), ver mas arriba) es el que emplea
     * para escribir su nombre en esta pantalla. */
    uint8_t hw = JugadorFisico(nombre_jugador_actual);
    uint16_t jy = (hw == 0) ? joystick_y : joystick2_y;
    uint16_t bajo_y, alto_y;
    Joy_Umbrales((hw == 0) ? centro_j1y : centro_j2y, &bajo_y, &alto_y);

    char *c = &nombre_jugadores[nombre_jugador_actual][nombre_pos_actual];   // puntero directo a la letra que se esta editando ahora mismo
    if (jy >= alto_y) {   // stick empujado hacia abajo del rango (arriba, segun la convencion ya corregida)
        *c = (char)((*c >= 'Z') ? 'A' : (char)(*c + 1));   // siguiente letra, con vuelta ciclica de Z a A
        ultimo_mov = ahora;
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);   // redibuja SOLO esa letra en pantalla
    } else if (jy <= bajo_y) {   // stick empujado hacia el otro lado (abajo)
        *c = (char)((*c <= 'A') ? 'Z' : (char)(*c - 1));   // letra anterior, con vuelta ciclica de A a Z
        ultimo_mov = ahora;
        Renderer_UpdateNombreLetra(nombre_jugador_actual, nombre_pos_actual, *c);
    }
}

int main(void) {   // punto de entrada del programa: inicializa todo el hardware, calibra los joysticks y entra al bucle principal a aproximadamente 30 cuadros por segundo
    HAL_Init();   // inicializa el HAL de ST (SysTick, flash latency, etc.), requerido antes de cualquier otro periferico
    SystemClock_Config();   // configura el reloj del sistema (generado, ver definicion mas abajo)

    MX_GPIO_Init();   // configura todos los pines GPIO (entradas de botones/joystick, salidas de LED/pantalla/buzzer)
    MX_SPI1_Init();   // configura SPI1 para la pantalla
    TIM3_ADCTrigger_Init();   // configura TIM3 (disparador del ADC cada 20ms), pero todavia sin arrancarlo
    ADC1_Joystick_Init();   // configura el ADC1 en modo escaneo de 4 canales (joystick 1 y 2), pero todavia sin arrancarlo
    MX_TIM4_Buzzer_Init();   // configura TIM4 (interrupcion periodica del buzzer)
    MX_USART2_UART_Init();   // configura USART2 (consola de depuracion)

    BotonesChase_Iniciar();   // arranca la animacion de LEDs tipo "tira" que corre durante el splash
    ILI9341_Init();   // inicializa el controlador de la pantalla (secuencia de comandos de arranque del ILI9341)

    HAL_TIM_Base_Start(&htim3);   // arranca TIM3 -- desde aca en adelante, el ADC se dispara cada 20ms
    HAL_ADC_Start_IT(&hadc1);   // arranca el ADC en modo interrupcion -- desde aca, HAL_ADC_ConvCpltCallback empieza a recibir conversiones

    printf("\r\n=== Beat Clash boot OK (PA2/PA3 @ 115200 8N1) ===\r\n");   // primer mensaje de arranque por consola, confirma que el UART esta vivo

    /* Indicador visual de calibración */
    ILI9341_FillScreen(COLOR_BLACK);   // pantalla negra mientras se calibra (todavia no hay nada mas que mostrar)
    ILI9341_DrawString(20, 100, "Calibrando joysticks...", COLOR_WHITE, COLOR_BLACK, 2);   // aviso principal
    ILI9341_DrawString(20, 130, "No tocar los sticks", COLOR_YELLOW, COLOR_BLACK, 1);   // instruccion critica: si se toca el stick aca, la calibracion sale mal
    HAL_Delay(300);   // espera bloqueante de 300ms para dejar que el filtro EMA se asiente en el valor real de reposo antes de medir el centro; SOLO se usa HAL_Delay largo aca, antes de que exista juego real

    centro_j1x = joystick_x;   // toma la lectura filtrada actual como centro "real" medido del eje X de J1
    centro_j1y = joystick_y;   // idem eje Y de J1
    centro_j2x = joystick2_x;   // idem eje X de J2
    centro_j2y = joystick2_y;   // idem eje Y de J2
    printf("[CALIB] centro j1=(%u,%u) j2=(%u,%u)\r\n", centro_j1x, centro_j1y, centro_j2x, centro_j2y);   // log de los 4 centros medidos, para poder verificarlos por consola

#if BUZZER_DIAGNOSTICO_BARRIDO
    Buzzer_BarridoDiagnostico();   // solo compila/corre si BUZZER_DIAGNOSTICO_BARRIDO esta en 1 (ver su #define mas arriba)
#endif

    uint8_t  screen        = DEMO_SPLASH;   // pantalla inicial: splash de bienvenida
    uint8_t  last_paso_sim = 0xFF;   /* fuerza el primer dibujo de las vistas previas */
    Demo_Enter(screen);   // dibuja el primer cuadro (el splash)

    uint32_t last_frame_tick = 0;   /* para sincronización no bloqueante */

    while (1) {   // loop principal: corre para siempre, ritmo fijo de RENDER_TICK_MS por vuelta
        /* ---- sincronización de frame (no bloqueante) ---- */
        if (HAL_GetTick() - last_frame_tick < RENDER_TICK_MS) continue;   // todavia no paso el tiempo de un frame -- vuelve a evaluar sin hacer nada (no bloqueante: el ADC/UART siguen funcionando por interrupcion mientras tanto)
        last_frame_tick = HAL_GetTick();   // marca el inicio de este nuevo frame

        /* ---- diagnóstico ADC (solo cuando no hay partida) ---- */
        if (!en_juego_real && !en_juego_real_2p && !en_juego_real_botones && !en_juego_real_guitar) {   // ningun modo real esta corriendo ahora mismo
            static uint32_t dbg_tick = 0;
            if (HAL_GetTick() - dbg_tick >= 500) {   // cada 500ms (no cada frame, para no inundar la consola)
                dbg_tick = HAL_GetTick();
                printf("[ADC] j1=(%u,%u) j2=(%u,%u)\r\n", joystick_x, joystick_y, joystick2_x, joystick2_y);   // log periodico de diagnostico con las 4 lecturas crudas filtradas
            }
        }

        /* Recuperacion automatica de la pantalla tras ruido electrico
         * persistente en el bus SPI (ver ILI9341_FalloComunicacionDetectado
         * en ili9341.c, y el comentario de LCD_SPI_Send en ese mismo
         * archivo) -- maxima prioridad, igual que el combo de salida de
         * abajo: el controlador de la pantalla puede haber quedado en
         * blanco o desincronizado (p. ej. un glitch en RST por el mismo
         * ruido de los LEDs de los botones via ULN2003A), y no hay forma de
         * leer su estado de vuelta (SPI1 es solo-escritura) para confirmarlo
         * antes de actuar. Se reinicializa el controlador por completo y se
         * reconstruye desde el splash -- perder la partida en curso es
         * preferible a una pantalla en blanco permanente hasta reiniciar la
         * placa a mano. Esto es un parche sobre el sintoma, NO arregla la
         * causa electrica de fondo. */
        if (ILI9341_FalloComunicacionDetectado()) {
            printf("[LCD] fallas de SPI persistentes -> reinicializando pantalla\r\n");
            en_juego_real = en_juego_real_2p = en_juego_real_botones = en_juego_real_guitar = 0;
            btn_modo_1p    = 0;
            guitar_modo_1p = 0;
            conteo_auto    = 0;
            Buzzer_Fondo_Detener();
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);
            ILI9341_Init();          // reinicializa el controlador de la pantalla por completo (recupera de un posible glitch en RST)
            ILI9341_SetPortrait(1);
            ILI9341_SetFlip180(0);
            screen = DEMO_SPLASH;
            Demo_Enter(screen);
            BotonesChase_Iniciar();
            continue;
        }

        /* Combo de salida ROJO+AMARILLO (máxima prioridad) */
        if (ComboSalir_Detectado()) {   // se revisa ANTES que cualquier otra logica del loop, para poder salir desde cualquier pantalla/modo
            en_juego_real         = 0;   // apaga todos los flags de "partida real" en curso
            en_juego_real_2p      = 0;
            en_juego_real_botones = 0;
            en_juego_real_guitar  = 0;
            btn_modo_1p           = 0;   // limpia el flag de "modo 1 jugador" de BOTONES -- si quedara en 1, la proxima partida de 2 jugadores leeria solo el hardware fisico del jugador 0 para ambos lados (ver Botones_LeerColor/Boton_LED/ComboSalir_Detectado)
            guitar_modo_1p        = 0;   // idem para Guitar Hero -- ambos flags se recalculan igual al confirmar el proximo modo, esto es solo limpieza defensiva al salir
            conteo_auto           = 0;   // cancela cualquier conteo 3-2-1-GO en curso
            Buzzer_Fondo_Detener();   // corta la musica de fondo si estaba sonando
            for (uint8_t p = 0; p < 2; p++) for (uint8_t c = 0; c < 4; c++) Boton_LED(p, c, 0);   // apaga los 8 LEDs de botones arcade
            ILI9341_SetPortrait(1);   // fuerza retrato (algunos modos reales quedan en retrato)
            ILI9341_SetFlip180(0);   // pantalla SIN flip -- el splash/menus deben verse normales, no girados
            screen = DEMO_SPLASH;   // vuelve al splash
            Demo_Enter(screen);   // dibuja el splash
            BotonesChase_Iniciar();   // reactiva la animacion de LEDs del splash
            continue;   // salta el resto del loop este tick (ya se manejo la prioridad maxima)
        }

        Buzzer_Actualizar();   // tick no bloqueante del buzzer (SFX + musica de fondo), siempre corre sin importar la pantalla
        if (screen == DEMO_SPLASH) BotonesChase_Actualizar();   // la animacion de LEDs solo avanza mientras se ve el splash
        uint8_t avanzar = Boton_B1_Flanco();   // flanco de B1 este tick (aunque en la practica el boton fisico esta inalcanzable, la logica sigue viva)

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
                                    screen == DEMO_JUGADORES_2 || screen == DEMO_INICIALES || en_pantalla_modo ||
                                    screen == DEMO_MENU_CANCIONES)
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
        else if (screen == DEMO_MENU_CANCIONES) {
            MenuCanciones_Procesar(&screen);
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
                /* Transicion de la pantalla de bienvenida al menu de
                 * seleccion de jugadores. */
                BotonesChase_Detener();
                screen = DEMO_JUGADORES_1;
                Demo_Enter(screen);
                avanzar = 0;
            }
            else if (screen == DEMO_JUGADORES_1 || screen == DEMO_JUGADORES_2) {
                jugadores_seleccionados = (screen == DEMO_JUGADORES_2) ? 2 : 1;
                printf("[MENU] jugadores = %u\r\n", jugadores_seleccionados);
                /* Reutiliza nombres ingresados en una partida anterior dentro
                 * de la misma sesion de encendido: los arreglos
                 * nombre_confirmado[] y nombre_jugadores[] son variables
                 * estaticas que no se reinician entre partidas, de modo que,
                 * si los jugadores requeridos para este modo ya confirmaron
                 * su nombre previamente, la pantalla DEMO_INICIALES se omite
                 * y se avanza directamente a la seleccion de modo (este
                 * estado se pierde unicamente al apagar la placa, ya que no
                 * se persiste en memoria flash). Se utiliza el indicador
                 * nombre_confirmado[] en lugar de verificar si el buffer esta
                 * vacio, porque el valor por defecto del buffer ("AAA") nunca
                 * lo esta. */
                uint8_t nombres_listos = nombre_confirmado[0] &&
                                          (jugadores_seleccionados < 2 || nombre_confirmado[1]);
                if (nombres_listos) {
                    printf("[INICIALES] reutilizando J1=%s J2=%s\r\n", nombre_jugadores[0], nombre_jugadores[1]);
                    screen = DEMO_MODO_SIMON;
                } else {
                    screen = DEMO_INICIALES;
                }
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
                    nombre_confirmado[0]  = 1;
                    nombre_jugador_actual = 1;
                    nombre_pos_actual     = 0;
                    Renderer_DrawNombre(1, nombre_jugadores[1], 0);
                } else {
                    nombre_confirmado[nombre_jugador_actual] = 1;
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
                if (modo_confirmado == MODO_SEL_GUITAR) {
                    /* Unicamente Guitar Hero pasa primero por la lista de
                     * canciones (pedido explicito del usuario) para que los
                     * 2 jugadores elijan con que cancion arrancar la musica
                     * de fondo -- SimonJoy y Botones siguen yendo derecho al
                     * conteo, sin tocar su flujo. */
                    screen = DEMO_MENU_CANCIONES;
                    Demo_Enter(screen);
                    MenuCanciones_Armar();   // exige ver el stick centrado antes de aceptar el primer arriba/abajo (evita heredar el "arrastre" de haber inclinado el joystick para llegar hasta GT HERO en el menu anterior)
                    Buzzer_Beep(100);
                } else {
                    screen = DEMO_CONTEO_3;
                    Demo_Enter(screen);
                    Buzzer_Beep(100);
                    conteo_auto = 1;
                    conteo_tick = HAL_GetTick();
                }
                avanzar = 0;
            }
            else if (screen == DEMO_MENU_CANCIONES) {
                /* Cualquier boton confirma la cancion resaltada por el
                 * cursor (movido con el joystick, ver MenuCanciones_Procesar)
                 * y arranca el conteo 3-2-1-GO de Guitar Hero. */
                printf("[MENU] cancion inicial de Guitar Hero = idx %u\r\n", cancion_cursor);
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
                btn_modo_1p    = (jugadores_seleccionados == 1);
                guitar_modo_1p = 0;   // limpia el flag de Guitar Hero por si quedo en 1 de una sesion anterior (ver el mismo reset en el conteo automatico, mas abajo)
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
                    /* Limpia SIEMPRE los 2 flags de "modo 1 jugador" antes de
                     * arrancar el modo elegido, sin importar cual haya
                     * quedado activo en una partida anterior de OTRO modo:
                     * btn_modo_1p y guitar_modo_1p comparten la logica de
                     * lectura de hardware de Botones_LeerColor/Boton_LED/
                     * ComboSalir_Detectado ("if (btn_modo_1p ||
                     * guitar_modo_1p) p = 0"), asi que si uno de los 2
                     * quedaba en 1 de una sesion 1-jugador anterior, una
                     * partida de 2 jugadores del OTRO modo forzaba p=0 para
                     * ambos lados -- exactamente el bug reportado de
                     * "arranca en 2 jugadores pero solo responde un lado". */
                    btn_modo_1p    = 0;
                    guitar_modo_1p = 0;
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
    /* Bajado de /2 (8MHz) a /8 (2MHz): hipotesis de diagnostico para la
     * pantalla que ocasionalmente se pone blanca durante partidas largas.
     * No hay evidencia de un bug de software que explique un llenado blanco
     * (no se encontro overflow de buffers ni comandos SPI mal armados); el
     * cableado de protoboard, a 8MHz y cerca de los ULN2003A que conmutan
     * los LEDs de los botones, es candidato tipico a ruido/glitches en la
     * linea SPI que se acumulan con el tiempo. Si el problema persiste con
     * este cambio, la causa esta en otro lado y esto puede revertirse. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  /* 2MHz, antes /2 = 8MHz */
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
/* === CONSOLA DE DEPURACION — USART2 POR EL VCP DEL ST-LINK ================= */
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
