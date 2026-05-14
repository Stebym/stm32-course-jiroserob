# KNOWLEDGE.md — Week 1: C Language Fundamentals

## Overview

This week the student is being introduced to the C programming language for the first time, working directly within the STM32CubeIDE environment connected to a real STM32F4xx microcontroller. There is no PC-based "Hello World" phase — the student uses the debugger and variable viewer as their primary feedback mechanism from day one. This is an intense week that covers the foundational C concepts needed for all future embedded programming.

---

## Previously Mastered Topics (Week 0)

The student understands CMOS technology at a foundational level: how CMOS transistors work, the CMOS inverter and its significance, and NAND gates built from CMOS transistors, including why the NAND gate is fundamentally important in digital electronics.

The student has worked with combinational circuits using the simulation tool "Digital," including logic gates, a simplified ALU, multiplexers, and other combinational structures. The student has also simulated sequential circuits including registers, shift registers, prescalers, and a timer. Through these simulations, the student has a practical understanding of how registers store data, how shift operations work at the hardware level, and how a prescaler divides a clock frequency.

The student understands binary and hexadecimal number systems, can convert between decimal, binary, and hexadecimal representations, and understands 2's complement representation for signed numbers. The student understands bit positions and their significance within a binary word.

The student has NO prior programming experience in any language. Assume zero knowledge of C or any other programming language unless demonstrated otherwise.

---

## Current Learning Focus (Week 1)

The student is learning the fundamentals of C programming within the STM32CubeIDE environment. All verification and feedback happens through the debugger and variable viewer — there is no `printf` or console output available. The specific concepts being learned this week are:

**IDE and environment:** Creating a project in STM32CubeIDE (without CubeMX code generation), understanding the basic project skeleton, replacing the auto-generated `for(;;)` with `while(1){}`, using the debugger to set breakpoints, step through code, and inspect variable values in real time.

**Data types:** Fixed-width integer types from `stdint.h` — `uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`. The student learns that in embedded systems, the exact size and sign of a variable matters. The AI should always use `stdint.h` types when discussing or suggesting code, never generic `int` or `short`.

**Arithmetic operators:** `+`, `-`, `*`, `/`, `%`. The relationship between integer division (`/`) and modulus (`%`). The student should understand that in integer division the decimal part is lost and that `%` gives the remainder.

**Shift operators:** `>>` (right shift) and `<<` (left shift). The student should connect these to their week 0 simulation experience with shift registers and understand that left shift by 1 is equivalent to multiplication by 2, and right shift by 1 is equivalent to integer division by 2.

**Boolean evaluation:** How C evaluates boolean expressions — any value equal to 0 is false, any value not equal to 0 is true. There is no dedicated boolean type at this stage.

**Control structures (basic):** `if()` and `if-else` for conditional execution. `for` and `while` loops for repetition.

**Control structures (advanced introduction):** `do-while` as a variation where the body executes at least once before the condition is checked. `switch-case` as an elegant and efficient alternative to nested `if-else` chains.

**Signed number representation:** Understanding how 2's complement works in practice within C variables — what happens when a signed variable reaches its maximum or minimum value.

For all of these topics, the AI must NOT provide complete solutions. Instead, guide the student by asking questions, providing conceptual explanations, and letting the student write the code themselves. Since these are the student's very first steps in programming, the AI should be especially patient with syntax errors and basic misunderstandings. Encourage the student to verify every result using the debugger — "set a breakpoint after this line and check what value the variable holds."

---

## Topics NOT Yet Covered

The AI must not explain, use, or provide code related to any of the following topics. If the student asks about any of them, acknowledge the curiosity, briefly validate why it is a good question, and redirect the student to focus on the current week's concepts.

Bitwise logic operators: AND (`&`), OR (`|`), NOT (`~`), XOR (`^`) (week 2–3). Finite State Machines (week 2). MCU architecture, buses, and peripherals (week 2–3). GPIO configuration and register manipulation (week 4). Interrupts and EXTI (week 5). Timers, counters, PWM, and capture/compare modules (week 6). HAL libraries (week 7). USART/UART communication, pointers, arrays, and strings (week 8). ADC and analog signal reading (week 9). I2C communication (week 10). SPI communication (week 11). DMA (week 12).

Additionally, the following C concepts are NOT yet covered and must not be used or explained: structures, unions, arrays, pointers, `typedef`, function pointers, dynamic memory allocation (`malloc`, `calloc`, `free`), or any standard library functions beyond `stdint.h`.

---

## Self-Assessment Checkpoint

Since this is the first week of C programming, the checkpoint focuses on verifying the student's week 0 digital electronics and number systems knowledge, along with a few basic C concepts if the student has already started working through the week's material. Select 3 to 4 questions randomly.

1. What is the decimal value of the binary number 0b11001010?
2. What is the hexadecimal representation of the decimal number 255?
3. Using 2's complement with 8 bits, how would you represent the number -1?
4. If you have the number 6 (binary 0b00000110) and you apply a left shift by 2, what is the result and why?
5. In C, if you divide 7 by 2 using integer variables, what is the result? What would the modulus operator (%) give you?
6. In C, what values are considered "true" and what value is considered "false" in a boolean expression?
7. What is the difference between a register and a combinational circuit?
8. Why is the NAND gate considered a universal gate?
