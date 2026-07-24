/**
 ******************************************************************************
 * @file    main.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Practica de diseño visual para Beat Clash — ILI9341 320x240.
 *
 * Usa el renderer.c REAL del proyecto principal (copiado sin cambios) con
 * un GameState_t de ejemplo, para poder ajustar colores/tamaños/layout aqui
 * sin necesitar joysticks, botones ni audio conectados. Cuando el diseño
 * quede a gusto, renderer.c se copia tal cual de vuelta a project/Src/.
 *
 * Control manual con B1 (PC13, boton de usuario de la Nucleo): cada
 * pulsacion avanza a la siguiente pantalla del recorrido de diseño.
 * Las animaciones (parpadeo del splash, notas cayendo) siguen vivas
 * mientras se espera la siguiente pulsacion.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "board_pins.h"
#include "game_state.h"
#include "ili9341.h"
#include "renderer.h"
#include <string.h>

SPI_HandleTypeDef hspi1;
static GameState_t gs;

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

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
    DEMO_COUNT
} DemoScreen_t;

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
        Renderer_DrawModoSimonJoystick(0xFF, 0xFF);
        break;

    case DEMO_CONTEO_3:
    case DEMO_CONTEO_2:
    case DEMO_CONTEO_1:
    case DEMO_CONTEO_GO:
        Renderer_DrawConteo((uint8_t)(DEMO_CONTEO_GO - screen));
        break;

    case DEMO_JUGANDO:
        gs.estado       = ESTADO_JUGANDO;
        gs.nota_speed   = NOTE_SPEED_L2;
        gs.j[0].puntaje = 350;
        gs.j[0].combo   = 4;
        gs.j[1].puntaje = 275;
        gs.j[1].combo   = 2;
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
    }
}

/* Detecta el flanco de presion de B1 (activo BAJO, pull-up externo en la Nucleo) */
static uint8_t Boton_B1_Flanco(void) {
    static GPIO_PinState prev = GPIO_PIN_SET;
    GPIO_PinState cur = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);
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
    MX_SPI1_Init();

    ILI9341_Init();

    uint8_t  screen        = DEMO_SPLASH;
    uint8_t  last_paso_sim = 0xFF;   /* fuerza el primer dibujo de las vistas previas */
    Demo_Enter(screen);

    while (1) {
        uint8_t avanzar = Boton_B1_Flanco();

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
                last_paso_sim = paso;
                if (screen == DEMO_PREVIEW_SIMON) Renderer_DrawModoSimonClasico(paso, paso);
                else                              Renderer_DrawModoSimonJoystick(paso, paso);
            }
            break;
        }

        default:
            /* Jugadores / Modo / Conteo: pantallas estaticas, nada que animar */
            break;
        }

        if (avanzar) {
            screen        = (uint8_t)((screen + 1) % DEMO_COUNT);
            last_paso_sim = 0xFF;
            Demo_Enter(screen);
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
/* === GPIO — SOLO PINES DE CONTROL DE LA PANTALLA =========================== */
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
/* === MANEJO DE ERRORES ===================================================== */
/* ========================================================================== */

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
