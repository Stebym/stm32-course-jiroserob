/**
 ******************************************************************************
 * @file    main.c
 * @author  Jimmy Stebym Rosero Barrera
 * @brief   Beat Clash — consola de juego tipo Guitar Hero para 2 jugadores.
 *          STM32 Nucleo-F411RE, ILI9341 320x240, 4 botones + joystick x2.
 *
 * ARQUITECTURA:
 *   - Scheduler cooperativo sin HAL_Delay() en el loop
 *   - TIM5 ISR a 5ms: muestrea botones → setea gs.input_flag
 *   - ADC1 + DMA2 circular: joysticks actualizados en background
 *   - SPI1 polling: envio de pixels sin DMA para simplicidad de sincronizacion
 *   - TIM2/TIM3 PWM: buzzers J1 y J2 con frecuencia variable
 *
 * RELOJES: HSI 16MHz (sin PLL). PCLK1=PCLK2=16MHz.
 *   - TIM5:  PSC=7999, ARR=9  → 200Hz = 5ms por tick (game input)
 *   - TIM2:  PSC=15, ARR=var  → audio J1 (~1MHz/ARR Hz)
 *   - TIM3:  PSC=15, ARR=var  → audio J2
 *   - SPI1:  PCLK2/2 = 8MHz
 *   - ADC1:  PCLK2/4 = 4MHz, 4 canales escaneo continuo con DMA circular
 ******************************************************************************
 */

/* === INCLUDES ============================================================= */
#include "stm32f4xx_hal.h"
#include "board_pins.h"
#include "game_state.h"
#include "ili9341.h"
#include "input.h"
#include "audio.h"
#include "renderer.h"
#include "game_fsm.h"
#include "imagen_test.h"    /* DEBUG: borrar despues de confirmar display */

/* === HANDLES DE PERIFERICOS (extern en los modulos que los necesitan) ===== */
SPI_HandleTypeDef  hspi1;
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim2;   /* buzzer jugador 1 — PA15 TIM2_CH1             */
TIM_HandleTypeDef  htim3;   /* buzzer jugador 2 — PB4  TIM3_CH1             */
TIM_HandleTypeDef  htim5;   /* game tick 5ms                                */

/* === ESTADO GLOBAL DEL JUEGO ============================================== */
GameState_t gs;

/* === PROTOTIPOS PRIVADOS ================================================== */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM5_Init(void);

/* === LOGICA =============================================================== */

int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* --- init perifericos de hardware --- */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM5_Init();

    /* --- init modulos del juego --- */
    Audio_Init(&htim2, &htim3);
    ILI9341_Init();
    Input_Init(&gs);

    /* --- arrancar ADC continuo con DMA circular --- */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)gs.adc_raw, 4);
    HAL_Delay(2);  /* dar tiempo al ADC para primer muestreo               */

    /* --- sembrar RNG con tick + ruido ADC --- */
    Game_SeedRandom(HAL_GetTick() ^ ((uint32_t)gs.adc_raw[0] << 16) ^ gs.adc_raw[1]);

    /* --- arrancar game tick ISR --- */
    HAL_TIM_Base_Start_IT(&htim5);

    /* --- FSM en splash --- */
    Game_Init(&gs);

    /* DEBUG: 3 colores solidos (FillScreen) — ROJO 2s, VERDE 2s, AZUL 2s.
     * Confirma que FillScreen funciona y que los colores se ven bien.
     * Si rojo=azul y azul=rojo: cambiar MADCTL de 0x48 a 0x40 (quitar BGR). */
    ILI9341_FillScreen(COLOR_RED);   HAL_Delay(2000);
    ILI9341_FillScreen(COLOR_GREEN); HAL_Delay(2000);
    ILI9341_FillScreen(COLOR_BLUE);  HAL_Delay(2000);

    /* DEBUG: barras de calibracion (FillRect) — prueba de posicion y colores.
     * Deberias ver 7 barras verticales: blanco, amarillo, cyan, verde,
     * magenta, rojo, azul — de izquierda a derecha. Borrar despues.        */
    ILI9341_FillRect(  0, 0,  46, 240, COLOR_WHITE);
    ILI9341_FillRect( 46, 0,  46, 240, COLOR_YELLOW);
    ILI9341_FillRect( 92, 0,  46, 240, COLOR_CYAN);
    ILI9341_FillRect(138, 0,  46, 240, COLOR_GREEN);
    ILI9341_FillRect(184, 0,  46, 240, COLOR_MAGENTA);
    ILI9341_FillRect(230, 0,  45, 240, COLOR_RED);
    ILI9341_FillRect(275, 0,  45, 240, COLOR_BLUE);
    HAL_Delay(5000);

    /* DEBUG: imagen xx.jpeg redimensionada a 320x240 — confirma que los
     * pixeles se ven reconocibles (aunque quizas con colores invertidos).
     * Si la imagen aparece correcta: display 100% OK. Borrar despues.      */
    ILI9341_SetWindow(0, 0, 319, 239);
    ILI9341_WritePixels(imagen_test, sizeof(imagen_test));
    ILI9341_EndWrite();
    HAL_Delay(5000);

    /* === LOOP PRINCIPAL — scheduler cooperativo === */
    while (1) {
        uint32_t now = HAL_GetTick();

        /* Procesar entrada cuando TIM5 ISR seteó el flag */
        if (gs.input_flag) {
            gs.input_flag = 0;
            Input_Update(&gs);
        }

        /* Actualizar logica de juego a ~30fps */
        if (now - gs.tick_logica >= GAME_TICK_MS) {
            gs.tick_logica = now;
            Game_Update(&gs, now);
        }

        /* Render a ~30fps */
        if (now - gs.tick_render >= RENDER_TICK_MS) {
            gs.tick_render = now;
            Renderer_Update(&gs);
        }

        /* Audio: verificar si algun buzzer debe apagarse */
        Audio_Update();
    }
}

/* ========================================================================== */
/* === CONFIGURACION DE RELOJES ============================================= */
/* ========================================================================== */

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSI 16MHz sin PLL — suficiente para SPI@8MHz, TIM y ADC@4MHz          */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;       /* HCLK  = 16MHz            */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;          /* APB1  = 16MHz (TIM2,3,5) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;          /* APB2  = 16MHz (SPI1,ADC) */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);  /* 16MHz → 0 wait states   */
}

/* ========================================================================== */
/* === INIT GPIO ============================================================ */
/* ========================================================================== */

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Pines de control de la pantalla */
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

    /* LEDs J1 via ULN2003A — activo ALTO */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = J1_LED_ALL_PINS;
    HAL_GPIO_Init(J1_LED_PORT, &g);
    HAL_GPIO_WritePin(J1_LED_PORT, J1_LED_ALL_PINS, GPIO_PIN_RESET);

    /* LEDs J2 via ULN2003A */
    g.Pin = J2_LED_ALL_PINS;
    HAL_GPIO_Init(J2_LED_PORT, &g);
    HAL_GPIO_WritePin(J2_LED_PORT, J2_LED_ALL_PINS, GPIO_PIN_RESET);

    /* Botones J1 (PC0-PC3) — entrada con pull-up, activo BAJO */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin  = J1_BTN_MASK;
    HAL_GPIO_Init(J1_BTN_R_PORT, &g);

    /* Botones J2 R/G (PC4-PC5) */
    g.Pin = J2_BTN_R_PIN | J2_BTN_G_PIN;
    HAL_GPIO_Init(J2_BTN_R_PORT, &g);

    /* Botones J2 B/Y (PB1, PB3) */
    g.Pin = J2_BTN_B_PIN | J2_BTN_Y_PIN;
    HAL_GPIO_Init(J2_BTN_B_PORT, &g);

    /* B1 (PC13) — pull-up externo R30=4k7 en Nucleo, no usar PULLUP sw */
    g.Pin  = BTN_START_PIN;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_START_PORT, &g);

    /* Pines ADC — modo analogico */
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = J1_JOY_X_PIN | J1_JOY_Y_PIN | J2_JOY_X_PIN;  /* PA0,PA1,PA4  */
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin  = J2_JOY_Y_PIN;   /* PB0                                         */
    HAL_GPIO_Init(J2_JOY_Y_PORT, &g);
}

/* ========================================================================== */
/* === INIT DMA ============================================================= */
/* ========================================================================== */

static void MX_DMA_Init(void) {
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA2 Stream0 Ch0 → ADC1 (circular, half-word, sin interrupcion manual) */
    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;
    hdma_adc1.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_adc1);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* ========================================================================== */
/* === INIT SPI1 ============================================================ */
/* ========================================================================== */

static void MX_SPI1_Init(void) {
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL=0            */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA=0 → SPI Mode 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  /* 8MHz         */
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
        g.Pin       = GPIO_PIN_5 | GPIO_PIN_7;  /* PA5=SCK, PA7=MOSI       */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* ========================================================================== */
/* === INIT ADC1 ============================================================ */
/* ========================================================================== */

static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sc = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4; /* 4MHz   */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 4;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    sc.SamplingTime = ADC_SAMPLETIME_15CYCLES;

    sc.Channel = ADC_CHANNEL_0; sc.Rank = 1;  /* PA0 — J1 JoyX (3.3V!)   */
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    sc.Channel = ADC_CHANNEL_1; sc.Rank = 2;  /* PA1 — J1 JoyY            */
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    sc.Channel = ADC_CHANNEL_4; sc.Rank = 3;  /* PA4 — J2 JoyX            */
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    sc.Channel = ADC_CHANNEL_8; sc.Rank = 4;  /* PB0 — J2 JoyY            */
    HAL_ADC_ConfigChannel(&hadc1, &sc);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        /* Pines ADC configurados en MX_GPIO_Init */
        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
    }
}

/* ========================================================================== */
/* === INIT TIM2 — BUZZER J1 PA15 ========================================== */
/* ========================================================================== */

static void MX_TIM2_Init(void) {
    TIM_OC_InitTypeDef oc = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 15;      /* 16MHz/16 = 1MHz timer clock   */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 999;     /* 1kHz por defecto              */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim2);

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;  /* silencio — CCR=0 produce ciclo de trabajo 0%     */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
    GPIO_InitTypeDef g = {0};
    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        g.Pin       = J1_BUZ_PIN;          /* PA15 — libre en modo SWD      */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_LOW;
        g.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(J1_BUZ_PORT, &g);
    } else if (htim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
        g.Pin       = J2_BUZ_PIN;          /* PB4 — libre en modo SWD       */
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_LOW;
        g.Alternate = GPIO_AF2_TIM3;
        HAL_GPIO_Init(J2_BUZ_PORT, &g);
    }
}

/* ========================================================================== */
/* === INIT TIM3 — BUZZER J2 PB4 =========================================== */
/* ========================================================================== */

static void MX_TIM3_Init(void) {
    TIM_OC_InitTypeDef oc = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 15;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 999;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/* ========================================================================== */
/* === INIT TIM5 — GAME TICK 5ms =========================================== */
/* ========================================================================== */

static void MX_TIM5_Init(void) {
    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 7999;    /* 16MHz/8000 = 2kHz             */
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 9;       /* 2kHz/10 = 200Hz → 5ms         */
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim5);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM5_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM5_IRQn);
    }
}

/* ========================================================================== */
/* === CALLBACKS DEL HAL ==================================================== */
/* ========================================================================== */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        Input_TIM_Callback(&gs);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    (void)hadc;  /* DMA circular — nada que hacer, adc_raw se actualiza solo */
}
