/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Jimmy Stebym Rosero Barrera
 * @brief          : Contador bidireccional multiplexado de 4 dígitos.
 * Control de display 7 segmentos (Ánodo Común) y sensores.
 ******************************************************************************
 */

/* ==================================================================== */
/* ================= DIRECTIVAS DE PROCESAMIENTO ====================== */
/* ==================================================================== */
#include <stdint.h>
#include "stm32f4xx.h"


/* ==================================================================== */
/* ======================== VARIABLES GLOBALES ======================== */
/* ==================================================================== */
volatile int contador = 0;
uint8_t digitos[4] = {0, 0, 0, 0};

uint8_t s1_bloqueado = 0;
uint8_t s2_bloqueado = 0;
uint32_t retardo_debounce = 0;


/* ==================================================================== */
/* ======================== HEADERS PRIVADOS ========================== */
/* ==================================================================== */
void descomponerNumero(int valor);
void encenderLetrasDisplay(uint8_t numero);
void delay_ms(uint32_t ms);


/* ==================================================================== */
/* ============================== LÓGICA ============================== */
/* ==================================================================== */
int main(void) {
    // 1. HABILITAR RELOJES (GPIOA, GPIOB, GPIOC, GPIOD)
    RCC->AHB1ENR |= (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);

    // 2. CONFIGURAR PINES COMO SALIDA
    // Transistores PNP: PB13(T1), PB14(T2), PC10(T3), PC13(T4)
    GPIOB->MODER &= ~((3U << 26) | (3U << 28));
    GPIOB->MODER |=  ((1U << 26) | (1U << 28));

    GPIOC->MODER &= ~((3U << 20) | (3U << 26));
    GPIOC->MODER |=  ((1U << 20) | (1U << 26));

    // Segmentos: PA11(F), PA12(B), PB12(A), PC2(G), PC11(C), PC12(E), PD2(D), PC0(DP)
    GPIOA->MODER &= ~((3U << 22) | (3U << 24));
    GPIOA->MODER |=  ((1U << 22) | (1U << 24));

    GPIOB->MODER &= ~(3U << 24);
    GPIOB->MODER |=  (1U << 24);

    GPIOC->MODER &= ~((3U << 0) | (3U << 4) | (3U << 22) | (3U << 24));
    GPIOC->MODER |=  ((1U << 0) | (1U << 4) | (1U << 22) | (1U << 24));

    GPIOD->MODER &= ~(3U << 4);
    GPIOD->MODER |=  (1U << 4);

    // 3. CONFIGURAR SENSORES (PA0 y PA1) COMO ENTRADAS PULL-UP
    GPIOA->MODER &= ~((3U << 0) | (3U << 2));
    GPIOA->PUPDR &= ~((3U << 0) | (3U << 2));
    GPIOA->PUPDR |=  ((1U << 0) | (1U << 2));

    descomponerNumero(contador);

    while(1) {
        // --- A. LÓGICA DE SENSORES ---
        if (retardo_debounce > 0) {
            retardo_debounce--;
        }
        else {
            // SENSOR 1 (PA0) -> SUMA
            if (GPIOA->IDR & (1U << 0)) {
                if (s1_bloqueado == 0) {
                    contador = (contador < 9999) ? contador + 1 : 0;
                    s1_bloqueado = 1;
                    retardo_debounce = 15;
                    descomponerNumero(contador);
                }
            } else {
                s1_bloqueado = 0;
            }

            // SENSOR 2 (PA1) -> RESTA
            if (GPIOA->IDR & (1U << 1)) {
                if (s2_bloqueado == 0) {
                    contador = (contador > 0) ? contador - 1 : 9999;
                    s2_bloqueado = 1;
                    retardo_debounce = 15;
                    descomponerNumero(contador);
                }
            } else {
                s2_bloqueado = 0;
            }
        }

        // --- B. MULTIPLEXACIÓN ---
        // Apagamos los 4 transistores PNP (1 = OFF)
        GPIOB->ODR |= (1U << 13) | (1U << 14);
        GPIOC->ODR |= (1U << 10) | (1U << 13);

        // Visualización secuencial
        encenderLetrasDisplay(digitos[3]);
        GPIOB->ODR &= ~(1U << 13); delay_ms(1); GPIOB->ODR |= (1U << 13); // Millares

        encenderLetrasDisplay(digitos[2]);
        GPIOB->ODR &= ~(1U << 14); delay_ms(1); GPIOB->ODR |= (1U << 14); // Centenas

        encenderLetrasDisplay(digitos[1]);
        GPIOC->ODR &= ~(1U << 10); delay_ms(1); GPIOC->ODR |= (1U << 10); // Decenas

        encenderLetrasDisplay(digitos[0]);
        GPIOC->ODR &= ~(1U << 13); delay_ms(1); GPIOC->ODR |= (1U << 13); // Unidades
    }
}

// --------------------------------------------------------------------
// IMPLEMENTACIÓN DE FUNCIONES PRIVADAS
// --------------------------------------------------------------------

void descomponerNumero(int valor) {
    digitos[0] = valor % 10;
    digitos[1] = (valor / 10) % 10;
    digitos[2] = (valor / 100) % 10;
    digitos[3] = (valor / 1000) % 10;
}

void encenderLetrasDisplay(uint8_t numero) {
    // 1. Apagar todos los segmentos (Ánodo Común: 1 = OFF)
    GPIOA->ODR |= (1U << 11) | (1U << 12);
    GPIOB->ODR |= (1U << 12);
    GPIOC->ODR |= (1U << 0) | (1U << 2) | (1U << 11) | (1U << 12);
    GPIOD->ODR |= (1U << 2);

    // 2. Encender necesarios (Ánodo Común: 0 = ON)
    switch(numero) {
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

void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < (ms * 3000); i++) {
        __NOP();
    }
}
