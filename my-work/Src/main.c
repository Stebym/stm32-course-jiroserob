/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Jimmy Stebym Rosero Barrera
 * @brief          : Contador bidireccional  de 4 digitos.
 * Control de display 7 segmentos donde me toco el anodo comun y  2 sensores, uno para el flanco postivo y el otro para el flanco negatico.
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


volatile int contador = 0; // variable donde guardamos la cuenta actual
uint8_t digitos[4] = {0, 0, 0, 0}; // arreglo para separar unidades, decenas,es decir para los 4 digitos.

uint8_t s1_bloqueado = 0; // candado para el sensor 1
uint8_t s2_bloqueado = 0; // candado para el sensor 2
uint32_t retardo_debounce = 0; // tiempo de espera para el antirebote


/* ==================================================================== */
/* ======================== PROTOTIPOS ================================ */
/* ==================================================================== */


// declaramos las funciones antes del main para que el compilador las reconozca

void descomponerNumero(int valor);
void encenderLetrasDisplay(uint8_t numero);
void delay_ms(uint32_t ms);




/* ==================================================================== */
/* ============================== LOGICA ============================== */
/* ==================================================================== */


int main(void) {
    // 1. HABILITAMOS LOS RELOJES que son losGPIOA, GPIOB, GPIOC, GPIOD
    RCC->AHB1ENR |= (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);

    // 2. CONFIGURAMOS PINES COMO SALIDA, donde lo ajustamos ajustado a la board

    // Por que el 7 segementos es anodo comun toca usar Transistores PNP, en los lugares de PB13(T1), PB14(T2), PC10(T3), PC13(T4)
    GPIOB->MODER &= ~((3U << 26) | (3U << 28)); // limpiamos los registros
    GPIOB->MODER |=  ((1U << 26) | (1U << 28)); // los configuramos como salida
    GPIOC->MODER &= ~((3U << 20) | (3U << 26));
    GPIOC->MODER |=  ((1U << 20) | (1U << 26));

    // Segmentos para: PA11(F), PA12(B), PB12(A), PC2(G), PC11(C), PC12(E), PD2(D), PC0(DP)
    GPIOA->MODER &= ~((3U << 22) | (3U << 24));
    GPIOA->MODER |=  ((1U << 22) | (1U << 24));
    GPIOB->MODER &= ~(3U << 24);
    GPIOB->MODER |=  (1U << 24);
    GPIOC->MODER &= ~((3U << 0) | (3U << 4) | (3U << 22) | (3U << 24));
    GPIOC->MODER |=  ((1U << 0) | (1U << 4) | (1U << 22) | (1U << 24));
    GPIOD->MODER &= ~(3U << 4);
    GPIOD->MODER |=  (1U << 4);

    // 3. CONFIGURAMOS SENSORES es decir en la board axuiliar son los puertos PA0 y PA1
    GPIOA->MODER &= ~((3U << 0) | (3U << 2)); // los dejamos como entradas puras

    // Se activan las resistencias Pull-Up internas de la STM32
    GPIOA->PUPDR &= ~((3U << 0) | (3U << 2)); // limpiamos configuracion previa
    GPIOA->PUPDR |=  ((1U << 0) | (1U << 2)); // activamos el pull-up

    // sacamos los digitos del 0 inicial para que la pantalla arranque bien desde el principio
    descomponerNumero(contador);

    while(1) {

            // aQUI LA LOGICA DE SENSORES: donde activos inmediatamente al tapar

            // si el retardo esta activo le bajamos el valor y esperamos

            if (retardo_debounce > 0) {
                retardo_debounce--;
            }
            else {
                // SENSOR 1 (PA0) -> SUMA

                // Detecta el 1 cuando tapamos
                if (GPIOA->IDR & (1U << 0)) {
                    if (s1_bloqueado == 0) { // si el candado esta abierto
                        if (contador < 9999) contador++; else contador = 0; // sumamos o damos la vuelta
                        s1_bloqueado = 1; // cerramos el candado
                        retardo_debounce = 15; // aplicamos el anti-rebote compensado
                        descomponerNumero(contador); // separamos los nuevos digitos
                    }
                } else {
                    s1_bloqueado = 0; // abre el candado al retirar el objeto
                }


                // SENSOR 2 (PA1) -> RESTA

                // Detecta el 0 cuando destapamos
                if (GPIOA->IDR & (1U << 1)) {
                    // Solo le avisamos que el sensor lee 1, es decir esta taado
                    s2_bloqueado = 1; // armamos el gatillo
                }
                else {
                    // El sensor lee 0  es decir esta destapado
                    if (s2_bloqueado == 1) { // si el gatillo estaba armado, es decir, estaba tapado antes
                        if (contador > 0) contador--; else contador = 9999; // restamos o damos la vuelta

                        s2_bloqueado = 0; // soltamos el gatillo, entonces abrimos el cadado
                        retardo_debounce = 15; // aplicamos el anti rebote
                        descomponerNumero(contador); // separamos los nuevos digitos
                    }
                }
            }


            // Apagamos los 4 transistores PNP (1 = OFF)
            GPIOB->ODR |= (1U << 13) | (1U << 14);
            GPIOC->ODR |= (1U << 10) | (1U << 13);


        // -juste Anti Parpadeo

        // apagamos los 4 transistores PNP (1 = OFF en logica invertida, por el 7 segementos) para evitar fantasmas visuales
        GPIOB->ODR |= (1U << 13) | (1U << 14);
        GPIOC->ODR |= (1U << 10) | (1U << 13);

        // --- Millares (T1 -> PB13) ---
        encenderLetrasDisplay(digitos[3]); // cargamos el numero a mostrar
        GPIOB->ODR &= ~(1U << 13); // prendemos el transistor (0 = ON)
        delay_ms(1);               // esperamos 1ms para que se vea bien y no parpadee
        GPIOB->ODR |= (1U << 13);  // apagamos el transistor

        // --- Centenas (T2 -> PB14) ---
        encenderLetrasDisplay(digitos[2]);
        GPIOB->ODR &= ~(1U << 14);
        delay_ms(1);               // bajamos a 1ms
        GPIOB->ODR |= (1U << 14);

        // --- Decenas (T3 -> PC10) ---
        encenderLetrasDisplay(digitos[1]);
        GPIOC->ODR &= ~(1U << 10);
        delay_ms(1);               // bajamos a 1ms
        GPIOC->ODR |= (1U << 10);

        // --- Unidades (T4 -> PC13) ---
        encenderLetrasDisplay(digitos[0]);
        GPIOC->ODR &= ~(1U << 13);
        delay_ms(1);               // bajamos a 1ms
        GPIOC->ODR |= (1U << 13);
    }
}


/* ==================================================================== */
/* ================== IMPLEMENTACION DE FUNCIONES ===================== */
/* ==================================================================== */



// --- AYUDA PARA LOS NUMEROS EN EL / SEGMENTOS ---

// funcion para sacar cada numero por separado usando divisiones y modulo, ya que es mas facil.
void descomponerNumero(int valor) {
    digitos[0] = valor % 10;
    digitos[1] = (valor / 10) % 10;
    digitos[2] = (valor / 100) % 10;
    digitos[3] = (valor / 1000) % 10;
}

// --- DICCIONARIO DE LETRAS (0 = Encendido, 1 = Apagado) ---

// Adaptado a las pistas de la board auxiliar: A=PB12, B=PA12, C=PC11, D=PD2, E=PC12, F=PA11, G=PC2, DP=PC0

void encenderLetrasDisplay(uint8_t numero) {

    // 1. apagar todos los segmentos (como es anodo comun, un 1 logico los apaga)
    GPIOA->ODR |= (1U << 11) | (1U << 12);
    GPIOB->ODR |= (1U << 12);
    GPIOC->ODR |= (1U << 0) | (1U << 2) | (1U << 11) | (1U << 12);
    GPIOD->ODR |= (1U << 2);

    // 2. encender solo los leds necesarios (un 0 logico cierra el circuito a tierra)

    switch(numero) {
        case 0: // Encienden: A, B, C, D, E, F
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~((1U << 12) | (1U << 11));
            GPIOC->ODR &= ~((1U << 11) | (1U << 12));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 1: // Encienden: B, C
            GPIOA->ODR &= ~(1U << 12);
            GPIOC->ODR &= ~(1U << 11);
            break;
        case 2: // Encienden: A, B, D, E, G
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~(1U << 12);
            GPIOC->ODR &= ~((1U << 12) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 3: // Encienden: A, B, C, D, G
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~(1U << 12);
            GPIOC->ODR &= ~((1U << 11) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 4: // Encienden: B, C, F, G
            GPIOA->ODR &= ~((1U << 12) | (1U << 11));
            GPIOC->ODR &= ~((1U << 11) | (1U << 2));
            break;
        case 5: // Encienden: A, C, D, F, G
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~(1U << 11);
            GPIOC->ODR &= ~((1U << 11) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 6: // Encienden: A, C, D, E, F, G
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~(1U << 11);
            GPIOC->ODR &= ~((1U << 11) | (1U << 12) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 7: // Encienden: A, B, C
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~(1U << 12);
            GPIOC->ODR &= ~(1U << 11);
            break;
        case 8: // Encienden: Todos menos DP
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~((1U << 12) | (1U << 11));
            GPIOC->ODR &= ~((1U << 11) | (1U << 12) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
        case 9: // Encienden: A, B, C, D, F, G
            GPIOB->ODR &= ~(1U << 12);
            GPIOA->ODR &= ~((1U << 12) | (1U << 11));
            GPIOC->ODR &= ~((1U << 11) | (1U << 2));
            GPIOD->ODR &= ~(1U << 2);
            break;
    }
}

// --- RETARDO, este ayuda para que no se vea tan pixeleado y se va ajustando para que no parpadee tanto el 7 segmentos ---

// funcion simple para perder tiempo de cpu y hacer el retardo visual

void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < (ms * 3000); i++) {
        __NOP(); // no hace nada
    }
}
