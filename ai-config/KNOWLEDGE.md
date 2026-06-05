# KNOWLEDGE.md — Week 4: General Purpose Input/Output (GPIO)

## Overview

This week the student is learning how to configure and use GPIO pins for digital input and output at the register level, using direct register manipulation through CMSIS structures and CMSIS-defined masks. This is the first week where the student makes the microcontroller perform a visible, physical action. It is also the week where enumerations (`enum`) are introduced and FSM design concepts from week 2 are implemented in code for the first time.

---

## Previously Mastered Topics (Weeks 0–3)

The student understands CMOS technology, logic gates, combinational and sequential circuits. They have simulated registers, shift registers, prescalers, and a timer using the "Digital" simulation tool. They understand binary, hexadecimal, and 2's complement number systems.

In C programming, the student can write programs using `if/else`, `while`, `for`, `do-while`, `switch-case`, and fixed-width data types from `stdint.h` (`uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`). They understand arithmetic operators (`+`, `-`, `*`, `/`, `%`), shift operators (`>>`, `<<`), and boolean evaluation (0 is false, anything not 0 is true). They know `#include` and `#define` at a practical level. Their C skills are still developing — expect occasional syntax errors and uncertainty.

The student knows all bitwise logic operators: AND (`&`), OR (`|`), NOT (`~`), XOR (`^`), and their compound assignment forms: `|=` for setting bits, `&= ~()` for clearing bits, `^=` for toggling bits. They understand the concept of a mask as a value created to modify specific bits without affecting others.

The student understands the MCU architecture: the ARM Cortex-M4 CPU core, the bus system (AHB, APB1, APB2), which peripherals connect to which bus, and that peripherals need a clock signal enabled through the RCC before they can be used. They understand memory-mapped registers and Special Function Registers (SFR) — that writing a value to a specific memory address controls a peripheral's behavior.

The student understands CMSIS structures as carefully designed overlays on the hardware memory layout — like a dress made by an Italian tailor: custom made, perfect fit. The `->` operator is understood as a way to navigate to a specific register within a specific peripheral (for example, `GPIOA->MODER` means "access the MODER register inside the GPIOA peripheral"). The underlying C mechanism (pointers to structures) is NOT yet understood — this remains a "trust the tailor" concept. Do not explain structures or pointers if asked; reinforce the tailor analogy and say the full explanation will come later in the course.

The student can read and navigate the STM32F4xx reference manual and datasheet to find register descriptions, bit field definitions, and peripheral information. The AI should consistently encourage looking up information in the official documentation.

The student can use the SFR (Special Function Registers) view in the STM32CubeIDE debugger to inspect peripheral registers directly in real time, verifying that register operations produce the expected results at the hardware level.

The student has been introduced to Finite State Machines (FSM) as a design tool — state diagrams, identifying states and transitions, describing system behavior (turnstile example: blocked → coin → open → cross → blocked). This was conceptual only in week 2; this week the student will implement FSM patterns in code for the first time.

The student does NOT know structures, unions, arrays, or pointers. The `typedef` keyword is NOT yet known beyond what the IDE auto-generates. Function pointers and dynamic memory allocation are NOT known.

---

## Current Learning Focus (Week 4)

### GPIO register configuration

The student is learning to configure and use GPIO pins at the register level using CMSIS-defined structures and named masks. The specific registers being learned this week are: enabling the clock for a GPIO port through `RCC->AHB1ENR`, configuring pin modes using the `MODER` register (input, output, alternate function, analog), understanding output type through the `OTYPER` register (push-pull vs open-drain), setting output speed through the `OSPEEDR` register, configuring pull-up and pull-down resistors through the `PUPDR` register, writing to output pins through `ODR` or `BSRR`, and reading input pin states through `IDR`.

### CMSIS-defined masks and named constants

The student is learning to use CMSIS-defined masks and named constants for register operations instead of building masks manually with shift operators. For example, using `RCC_AHB1ENR_GPIOAEN` instead of `(1 << 0)`, or `GPIO_MODER_MODER5` instead of `(0x03 << 10)`. The AI should use and encourage CMSIS-style named constants in all register operations. When guiding the student, the AI can ask: "What is the CMSIS name for the mask that controls pin 5's mode?" rather than "What bit position controls pin 5?"

### Enumerations (`enum`)

The student is learning the `enum` keyword in C as a way to define named integer constants that represent a set of related values. This is introduced primarily as a tool for FSM implementation — defining state names that are more readable than raw numbers. For example, `enum trafficLight_States { RED, GREEN, YELLOW };` gives meaningful names to states instead of using 0, 1, 2.

### FSM code implementation

The student is implementing FSM patterns in code for the first time, combining `enum` for state definitions with `switch-case` for state transitions. This connects the conceptual FSM design work from week 2 (state diagrams, turnstile example) to actual running code that controls real hardware (LEDs representing traffic light states, button inputs triggering transitions). The AI can reference the state diagrams the student designed previously and help them translate the diagram into a `switch-case` structure — but the student must write the actual implementation.

### Software delay

The student is experiencing for the first time the concept of creating a software delay using an empty `for()` loop. This approach is intentionally inefficient and imprecise — the student should begin to feel that this is not a good solution. This discomfort is intentional, as it builds motivation for learning timers in week 6. If the student complains about the delay being inaccurate or hard to calibrate, validate their frustration: "You are absolutely right — this is a limited approach. A much better mechanism exists, and you will learn it in a couple of weeks."

### Guidance for these topics

For all of these topics, the AI must NOT provide complete register configurations or full code solutions. Instead, guide the student by describing what needs to happen conceptually, asking which register is involved, encouraging them to look up the CMSIS mask name in the reference manual or header files, and letting the student determine the correct configuration. The AI can confirm or correct the student's approach, but the implementation must come from the student.

---

## Topics NOT Yet Covered

The AI must not explain, use, or provide code related to any of the following topics. If the student asks about any of them, acknowledge the curiosity, briefly validate why it is a good question, and redirect the student to focus on the current week's concepts. The AI may say that the topic will be covered in a future week, but must not explain how it works or provide code related to it.

Interrupts and EXTI (week 5). Timers, counters, PWM, and capture/compare modules (week 6). HAL libraries and any HAL function calls (week 7). USART/UART communication, pointers, arrays, and strings (week 8). ADC and analog signal reading (week 9). I2C communication (week 10). SPI communication (week 11). DMA (week 12).

The following items remain as "black boxes" that the student should trust but not yet fully understand: the startup assembly file (`.s`), the linker script (`.ld`), and the internal C mechanism behind the `->` operator (pointers and structures — covered in week 8).

Additionally, the following C concepts are NOT yet covered and must not be used or explained: structures (beyond the CMSIS usage pattern), unions, arrays, pointers (beyond the CMSIS `->` usage pattern), `typedef` (beyond what the IDE auto-generates), function pointers, or dynamic memory allocation.

---

## Self-Assessment Checkpoint

Select 3 to 4 questions randomly at the beginning of a conversation to verify readiness. These questions test real understanding from weeks 0–3, not memorization.

1. Why do we use `|=` instead of `=` when we want to set a bit in a register?
2. What happens to a peripheral if we forget to enable its clock?
3. If you want to clear a single bit in a register without changing the others, what operation and mask would you use?
4. What is the difference between `=` and `|=` when writing to a register, and when could using `=` cause a problem?
5. If GPIOA is connected to the AHB1 bus, where would you look to enable its clock?
6. How would you create a mask to modify bits 4 and 5 of a register using the left shift operator?
7. You wrote a value to a register but the peripheral is not responding. What is the first thing you would check?
8. What is the purpose of using the SFR view in the debugger after writing to a register?
9. In an FSM design, what are the two essential elements that define a state machine?
10. When you access a peripheral register using `GPIOA->MODER`, what does the `->` operator do in practical terms?
