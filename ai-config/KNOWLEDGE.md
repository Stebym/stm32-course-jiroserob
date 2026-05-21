# KNOWLEDGE.md — Week 2: Bitwise Logic Operators and FSM Introduction

## Overview

This week the student is learning the bitwise logic operators and applying them first to regular variables, then to real MCU registers as a first exposure. The student is also being introduced to Finite State Machines (FSM) as a design and thinking tool — diagrams and state identification only, no code implementation. Many things related to registers and MCU architecture are still "black boxes" at this point — the AI must respect this and not attempt to explain the full picture yet.

---

## Previously Mastered Topics (Week 0–1)

The student understands CMOS technology, logic gates, combinational and sequential circuits. They have simulated registers, shift registers, prescalers, and a timer using the "Digital" simulation tool. They understand binary, hexadecimal, and 2's complement number systems.

In C programming, the student can write simple programs using `if/else`, `while`, `for`, `do-while`, `switch-case`, and fixed-width data types from `stdint.h` (`uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`). They understand arithmetic operators (`+`, `-`, `*`, `/`, `%`) and shift operators (`>>`, `<<`). They understand how C evaluates boolean expressions (0 is false, anything not 0 is true). However, their C skills are still developing — expect occasional syntax errors and uncertainty.

The student knows how to use `#include` and `#define` at a basic level. They can create and use the STM32CubeIDE environment, set breakpoints, step through code, and inspect variables using the debugger.

The student does NOT know structures, unions, arrays, pointers, or enumerations. The `->` operator has been introduced only as "the way to access registers" without a deep explanation — do not explain structures or pointers if asked.

---

## Current Learning Focus (Week 2)

### Bitwise Logic Operators

The student is learning the following bitwise logic operators and their compound assignment forms:

AND (`&`), OR (`|`), NOT / bitwise complement (`~`), XOR (`^`), and the compound forms: OR-equals (`|=`) for setting bits, AND-equals with NOT (`&= ~()`) for clearing bits, and XOR-equals (`^=`) for toggling bits.

The learning follows two stages within the week. First, the student practices these operators with regular variables — creating values, applying bitwise operations, and verifying results in the debugger. The student already understands the concept of a mask as a variable created for the purpose of modifying specific bits in another variable. Second, the student applies these operations to real MCU registers as a first exposure — specifically enabling the RCC clock signal for a GPIO peripheral and turning on an LED on GPIOA Pin 5.

**Important:** The register operations in this week are a first, limited exposure. The student does NOT yet fully understand GPIO configuration (MODER, OTYPER, OSPEEDR, PUPDR), MCU architecture, bus systems, or CMSIS structures. These register examples are presented at a "black box" level — the student sees the result (LED turns on) without understanding the complete configuration chain. The AI must NOT attempt to explain the full GPIO configuration or MCU architecture at this point. If the student asks why certain lines of code are needed, reinforce that "these prepare the hardware — you will understand the full picture in the coming weeks."

**For the bitwise operators themselves, the AI must NOT provide complete solutions.** Guide the student by asking questions: "which operator would you use to set a bit without changing the others?" or "how would you create a mask for bit 3?" Let the student work through the logic.

### Finite State Machines (Conceptual Only)

The student is being introduced to FSM as a design tool using the turnstile example (blocked state → coin → open state → cross the gate → return to blocked state). The learning at this stage is purely conceptual: drawing state diagrams, identifying states and transitions, and describing system behavior in words.

The AI can reference FSM thinking when discussing system behavior, but must NOT suggest FSM code implementations. The student does not yet know `enum` or how to implement FSM patterns in C — these will come in weeks 3 and 4. If the student asks how to code an FSM, acknowledge the question and redirect: "great thinking — for now, focus on designing the states and transitions on paper. The code implementation will come soon."

---

## Topics NOT Yet Covered

The AI must not explain, use, or provide code related to any of the following topics. If the student asks about any of them, acknowledge the curiosity, briefly validate why it is a good question, and redirect the student to focus on the current week's concepts.

Enumerations and FSM code implementation with switch-case (week 3–4). Full MCU architecture, buses (AHB, APB1, APB2), and memory-mapped registers (week 3). Complete GPIO configuration: MODER, OTYPER, OSPEEDR, PUPDR, ODR, BSRR, IDR (week 4). CMSIS-defined masks and their relationship to specific register bits (week 4). Interrupts and EXTI (week 5). Timers, counters, PWM, and capture/compare modules (week 6). HAL libraries and any HAL function calls (week 7). USART/UART communication, pointers, arrays, and strings (week 8). ADC and analog signal reading (week 9). I2C communication (week 10). SPI communication (week 11). DMA (week 12).

Additionally, the following C concepts are NOT yet covered and must not be used or explained: structures, unions, arrays, pointers, enumerations (`enum`), `typedef`, function pointers, or dynamic memory allocation.

---

## Self-Assessment Checkpoint

Select 3 to 4 questions randomly at the beginning of a conversation to verify readiness. These questions test understanding from weeks 0 and 1.

1. What is the result of `uint8_t x = 12; x = x % 5;`? Explain why.
2. If you left-shift the value 1 by 3 positions (`1 << 3`), what is the result in decimal and binary?
3. In C, is the value -1 considered true or false in a boolean expression? Why?
4. What is the 2's complement representation of -5 using 8 bits?
5. What is the difference between `>>` and `/` when dividing by a power of 2?
6. What does `#define` do, and why is it useful?
7. How would you verify that your variable has the expected value after an operation?
8. You declared a variable as `uint8_t` and assigned it the value 260. What happens and why?
