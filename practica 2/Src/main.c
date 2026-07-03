/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Jimmy Stebym Rosero Barrera
 * @brief          : Contador bidireccional  de 4 digitos.
 * Control de display 7 segmentos donde me toco el anodo comun y  2 sensores, uno para el flanco postivo y el otro para el flanco negatico.
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32f4xx.h"


/* ==================================================================== */
/* ======================== VARIABLES GLOBALES ======================== */
/* ==================================================================== */

// esta variable es la que guarda el conteo del senspr
volatile int global_counter = 0;

// arreglo donde guardamos los digitos ya separados para la pantalla
uint8_t slot_matrix[4] = {0, 0, 0, 0};

// variable que controla cual digito estamos dibujando en el momento
volatile uint8_t active_slot = 0;

// un buffer para el control del rebote para que el senspr no sume doble
volatile uint32_t deadtime_buffer = 0;

/* ==================================================================== */
/* ======================== PROTOTIPOS ================================ */
/* ==================================================================== */

void sys_setup_io(void);
void sys_setup_exti(void);
void sys_setup_timer3(void);
void split_matrix_val(int value);
void apply_segment_mask(uint8_t lookup_num);

/* ==================================================================== */
/* ============================== LOGICA ============================== */
/* ==================================================================== */

int main(void) {

    // 1. Configuracion inicial de todo el hardware
    sys_setup_io();
    sys_setup_exti();
    sys_setup_timer3();

    // dejamos la matriz lista en 0000 para que prenda bien el displey
    split_matrix_val(global_counter);

    // bucle principal, no hace nada porque todo lo hacen las interrupciones
    while(1) {
        // el micro se queda aca esperando a que ocurra un evento
        __NOP();
    }
}


/* ==================================================================== */
/* ================== CONFIGURACION DE PERIFERICOS ==================== */
/* ==================================================================== */

void sys_setup_io(void) {

    // activamos los relojes de todos los puertos que usamos
    RCC->AHB1ENR |= (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);

    // configuramos los transistores PNP: PB13, PB14, PC10, PC13
    // esto es para habilitar los digitos del displey
    GPIOB->MODER &= ~((3U << 26) | (3U << 28));
    GPIOB->MODER |=  ((1U << 26) | (1U << 28));
    GPIOC->MODER &= ~((3U << 20) | (3U << 26));
    GPIOC->MODER |=  ((1U << 20) | (1U << 26));

    // configuramos las pistas de los segmentos segun el ruteo de la placa
    // esto es lo que va a cada letra del display
    GPIOA->MODER &= ~((3U << 22) | (3U << 24));
    GPIOA->MODER |=  ((1U << 22) | (1U << 24));
    GPIOB->MODER &= ~(3U << 24);
    GPIOB->MODER |=  (1U << 24);
    GPIOC->MODER &= ~((3U << 0) | (3U << 4) | (3U << 22) | (3U << 24));
    GPIOC->MODER |=  ((1U << 0) | (1U << 4) | (1U << 22) | (1U << 24));
    GPIOD->MODER &= ~(3U << 4);
    GPIOD->MODER |=  (1U << 4);

    // entradas para los senspres PA0 y PA1
    // los configuramos como entradas puras con su resistencia pullup
    GPIOA->MODER &= ~((3U << 0) | (3U << 2));
    GPIOA->PUPDR &= ~((3U << 0) | (3U << 2));
    GPIOA->PUPDR |=  ((1U << 0) | (1U << 2));
}

void sys_setup_exti(void) {

    // habilitamos la configuracion del sistema para las interrupciones
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // enrutamiento de los puertos PA0 y PA1 a las lineas exti
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI1);

    // senspr 1 configurado para que dispare cuando se tapa (flanco de subida)
    EXTI->RTSR |= (1U << 0);
    // senspr 2 configurado para que dispare cuando se destapa (flanco de bajada)
    EXTI->FTSR |= (1U << 1);

    // habilitamos las lineas de interrupcion
    EXTI->IMR |= (1U << 0) | (1U << 1);

    // activamos la prioridad en el nvic
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

void sys_setup_timer3(void) {

    // iniciamos el temporizador 3 para el refresco del displey
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // calculamos los registros para que la interrupcion sea cada 2ms
    TIM3->PSC = 16000 - 1;
    TIM3->ARR = 2 - 1;

    TIM3->DIER |= TIM_DIER_UIE;  // habilitamos la interrupcion del timer
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1 |= TIM_CR1_CEN;    // arrancamos el contador
}

/* ==================================================================== */
/* ======================== INTERRUPCIONES (ISR) ====================== */
/* ==================================================================== */

// esta es la interrupcion que dispara el senspr 1 para sumar

void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1U << 0)) {
        EXTI->PR = (1U << 0); // limpiamos el bit del exti como dijo el profe

        // revisamos el buffer para evitar el rebote que molesta tanto
        if (deadtime_buffer == 0) {
            global_counter = (global_counter < 9999) ? global_counter + 1 : 0;
            split_matrix_val(global_counter); // mandamos a separar
            deadtime_buffer = 60; // seteamos el tiempo de espera
        }
    }
}

// esta es la interrupcion del senspr 2 para restar

void EXTI1_IRQHandler(void) {
    if (EXTI->PR & (1U << 1)) {
        EXTI->PR = (1U << 1);

        // aca tambien filtramos el rebote del sensr
        if (deadtime_buffer == 0) {
            global_counter = (global_counter > 0) ? global_counter - 1 : 9999;
            split_matrix_val(global_counter);
            deadtime_buffer = 60;
        }
    }
}

// interrupcion del timer para multiplexar los 4 digitos
void TIM3_IRQHandler(void) {

    if (TIM3->SR & TIM_SR_UIF) {
        // asignacion directa para no perder bits segun la explicacion del profesor
        TIM3->SR = ~TIM_SR_UIF;

        // apagamos todos los transistores primero (logica inversa PNP)
        GPIOB->ODR |= (1U << 13) | (1U << 14);
        GPIOC->ODR |= (1U << 10) | (1U << 13);

        // encendemos el digito que nos toca en este turno
        switch (active_slot) {
            case 3:
                apply_segment_mask(slot_matrix[3]);
                GPIOB->ODR &= ~(1U << 13); // prendemos el transistor de millares
                break;
            case 2:
                apply_segment_mask(slot_matrix[2]);
                GPIOB->ODR &= ~(1U << 14); // prendemos el de centenas
                break;
            case 1:
                apply_segment_mask(slot_matrix[1]);
                GPIOC->ODR &= ~(1U << 10); // prendemos el de decenas
                break;
            case 0:
                apply_segment_mask(slot_matrix[0]);
                GPIOC->ODR &= ~(1U << 13); // prendemos el de unidades
                break;
        }

        // pasamos al siguiente digito
        active_slot = (active_slot + 1) % 4;

        // aca descontamos el tiempo muerto del rebote de los senspres
        if (deadtime_buffer > 0) {
            deadtime_buffer--;
        }
    }
}


/* ==================================================================== */
/* ================== IMPLEMENTACION DE FUNCIONES ===================== */
/* ==================================================================== */


// funcion para convertir el numero global a cada uno de los 4 espacios de la matriz
void split_matrix_val(int value) {
    slot_matrix[0] = value % 10;
    slot_matrix[1] = (value / 10) % 10;
    slot_matrix[2] = (value / 100) % 10;
    slot_matrix[3] = (value / 1000) % 10;
}

// diccionario de los leds del display para anodo comun
void apply_segment_mask(uint8_t lookup_num) {

    // 1. apagamos todo primero para evitar sombras
    GPIOA->ODR |= (1U << 11) | (1U << 12);
    GPIOB->ODR |= (1U << 12);
    GPIOC->ODR |= (1U << 0) | (1U << 2) | (1U << 11) | (1U << 12);
    GPIOD->ODR |= (1U << 2);

    // 2. segun el numero prendemos las letras que van
    switch(lookup_num) {
        case 0: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~((1U<<12)|(1U<<11)); GPIOC->ODR &= ~((1U<<11)|(1U<<12)); GPIOD->ODR &= ~(1U<<2); break;
        case 1: GPIOA->ODR &= ~(1U<<12); GPIOC->ODR &= ~(1U<<11); break;
        case 2: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~(1U<<12); GPIOC->ODR &= ~((1U<<12)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
        case 3: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~(1U<<12); GPIOC->ODR &= ~((1U<<11)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
        case 4: GPIOA->ODR &= ~((1U<<12)|(1U<<11)); GPIOC->ODR &= ~((1U<<11)|(1U<<2)); break;
        case 5: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~(1U<<11); GPIOC->ODR &= ~((1U<<11)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
        case 6: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~(1U<<11); GPIOC->ODR &= ~((1U<<11)|(1U<<12)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
        case 7: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~(1U<<12); GPIOC->ODR &= ~(1U<<11); break;
        case 8: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~((1U<<12)|(1U<<11)); GPIOC->ODR &= ~((1U<<11)|(1U<<12)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
        case 9: GPIOB->ODR &= ~(1U<<12); GPIOA->ODR &= ~((1U<<12)|(1U<<11)); GPIOC->ODR &= ~((1U<<11)|(1U<<2)); GPIOD->ODR &= ~(1U<<2); break;
    }
}
