<<<<<<< HEAD
# KNOWLEDGE.md — Week 5: Timers and Time Measurement

## Overview

This week the student encounters two fundamental concepts simultaneously. The first is the timer peripheral — a hardware module that counts clock pulses independently of the CPU, allowing precise time measurement without wasting processor cycles. The second is the interrupt mechanism — the ability for a hardware event to temporarily suspend the main program, execute a short response function, and resume execution exactly where it was interrupted. The timer's UpdateEvent is used as the first and most intuitive interrupt source in the course: a predictable, self-contained "alarm" that fires at a precisely configured period. This week uses TIM3 (a 16-bit general purpose timer on APB1) as the primary example, with the onboard LED on GPIOA pin 5 as the visible output — replacing the polling-based `for()` blinky from week 04 with an interrupt-driven equivalent.

---

## Previously Mastered Topics (Weeks 0–4)

The student understands CMOS technology, logic gates, combinational and sequential circuits, binary, hexadecimal, and 2's complement number systems. They have simulated prescalers and timers using the "Digital" simulation tool — this week those simulated concepts become real hardware.

In C programming, the student can write programs using all control structures, fixed-width data types from `stdint.h`, all arithmetic and bitwise operators, and enumerations (`enum`). They can implement FSM patterns using `enum` and `switch-case`. The student does NOT yet know structures, arrays, pointers, or `typedef` beyond what the IDE auto-generates.

The student fully understands the MCU architecture: ARM Cortex-M4 CPU core, bus system (AHB, APB1, APB2), memory-mapped registers, and SFR. They understand CMSIS structures as carefully designed overlays on the hardware — the Italian tailor analogy. The `->` operator is understood at a practical level.

The student can configure GPIO pins completely at the register level: RCC clock enable, MODER, OTYPER, OSPEEDR, PUPDR, ODR, BSRR, and IDR — all using CMSIS-defined masks. They understand the difference between ODR and BSRR.

The student has experienced the inefficiency of polling-based approaches firsthand — the `for()` delay that blocks the CPU, the constant checking of a button state in `while(1){}`. This lived experience is the motivation for the interrupt-driven approach being introduced this week.

The student has a basic familiarity with the startup assembly file (`.s`) as the file that runs before `main()`. This week the interrupt vector table within that file becomes relevant for the first time.

---

## Current Learning Focus (Week 5)

### The timer concept — hardware counting independently of the CPU

The student is learning that a timer is a hardware peripheral that counts clock pulses entirely on its own, without requiring any CPU involvement. This is fundamentally different from the `for()` delay used in week 04, where the CPU was completely occupied doing nothing useful while counting loop iterations. With a hardware timer, the CPU is free to do other work — or simply wait in the main loop — while the timer counts in the background.

The AI should connect this to the student's week 00 simulation experience: they simulated prescalers and counters in the "Digital" tool. The TIM3 peripheral is exactly that — a real, silicon implementation of the same concept they already understand from simulation.

### The timer signal chain — telling the story step by step

The student is learning the internal signal chain of TIM3 through a step-by-step narrative rather than a formula. The story flows like this:

The system clock (typically 16 MHz on the internal oscillator) enters the prescaler register (PSC). The prescaler divides the clock frequency to produce a slower, more manageable tick signal. For example, if PSC is configured to divide by 16000, the output tick frequency is 1 KHz — meaning one tick every 1 millisecond. This tick signal drives the counter register (CNT), which increments by 1 on every tick. The auto-reload register (ARR) holds a target value. When CNT reaches the ARR value, the timer generates an UpdateEvent signal, resets CNT to zero, and the counting begins again. The period of the UpdateEvent — how often the "alarm" fires — is simply the number of ticks defined by ARR multiplied by the duration of each tick.

The mental model to reinforce with ASCII diagram:

```
System Clock (16 MHz)
        |
        v
   [PSC Register]  -- divides clock frequency
        |
        v
  Tick signal (e.g. 1 KHz = 1ms per tick)
        |
        v
   [CNT Register]  -- counts up on every tick
        |
        v
  Compare with ARR
        |
        v
  CNT == ARR  -->  UpdateEvent fires!  -->  CNT resets to 0
```

From this story, the formula emerges naturally: `Period = (PSC + 1) * (ARR + 1) / Timer_Clock_Frequency`. But the formula is a summary of the story, not a starting point. The AI should always guide students through the story first, then connect it to the formula. If a student asks "what value do I put in PSC and ARR?", guide them through the story: "what tick frequency do you want coming out of the prescaler? how many of those ticks make up your desired period?"

### Key TIM3 registers

The student is learning the following TIM3 registers and their purpose. All configuration is done at the register level using CMSIS notation. The student should look up each register in the reference manual before writing any code.

`TIM3->PSC` — the prescaler register. Determines the division factor applied to the input clock. The actual division is PSC + 1.

`TIM3->ARR` — the auto-reload register. The value the counter counts up to before generating the UpdateEvent. The actual count is ARR + 1 ticks.

`TIM3->CNT` — the counter register. Incremented on every tick after the prescaler. Can be read in the SFR viewer to observe the timer running in real time.

`TIM3->DIER` — the DMA/Interrupt Enable Register. Bit 0 (UIE — Update Interrupt Enable) must be set to enable the UpdateEvent interrupt.

`TIM3->SR` — the Status Register. Bit 0 (UIF — Update Interrupt Flag) is set by hardware when the UpdateEvent occurs. This flag MUST be cleared in the ISR by writing 0 to it — failing to clear it causes the ISR to execute repeatedly in an infinite loop.

`TIM3->CR1` — the Control Register 1. Bit 0 (CEN — Counter Enable) starts the timer counting. This should be the last register written in the initialization sequence — configure everything else first, then start the counter.

### RCC clock enable for TIM3

TIM3 is connected to the APB1 bus. Its clock must be enabled through the RCC APB1ENR register before any TIM3 register can be configured. The student should find the correct bit and CMSIS mask name in the reference manual.

### NVIC configuration

After configuring all TIM3 registers, the student enables the TIM3 interrupt in the NVIC using the CMSIS function:

```c
NVIC_EnableIRQ(TIM3_IRQn);
```

This is the only NVIC function used in this course. Interrupt priorities are NOT covered — they add complexity beyond the scope of an introductory course. Interrupts are handled in the order they arrive (FIFO behavior). The AI must not explain or suggest priority configuration even if the student asks — redirect: "priorities add significant complexity and are not part of this course. `NVIC_EnableIRQ()` is everything you need."

### The ISR — Interrupt Service Routine

The student is learning to write an ISR for TIM3. The ISR function name must match exactly the name defined in the startup file's interrupt vector table. The student finds this name by opening the startup `.s` file and locating the TIM3 entry — the correct name is `TIM3_IRQHandler`. The student can cross-reference this with the interrupt table in the STM32F4xx reference manual to confirm.

The ISR must follow these rules: it must be short — only a few lines of code, it must clear the UpdateEvent flag in TIM3->SR before returning (write 0 to bit 0 of SR), and it must set a `volatile` flag variable that `main()` checks and responds to. The ISR must never contain long delays, blocking operations, or complex logic.

The `volatile` keyword is essential for flag variables shared between the ISR and main. The AI should explain it at a practical level: "it tells the compiler that this variable can change at any time from outside the normal program flow — from an interrupt — so never cache or optimize away reads of this variable. Without `volatile`, the compiler might assume the flag never changes inside the main loop and optimize away the check entirely."

A correct ISR structure looks like this conceptually — this is shown here only as a reference for the AI to understand the expected structure. The AI must NOT provide this code directly to the student:

```c
void TIM3_IRQHandler(void)
{
    if(TIM3->SR & TIM_SR_UIF)
    {
        TIM3->SR &= ~TIM_SR_UIF;
        update_flag = 1;
    }
}
```

### Replacing the polling blinky with an interrupt-driven blinky

The student is replacing the week 04 LED blinky — which used a blocking `for()` delay inside `while(1){}` — with an interrupt-driven version where TIM3 fires the UpdateEvent at a configured period and the ISR sets a flag that `main()` checks to toggle the LED. The visible behavior is identical — the LED blinks — but the CPU is now free during the waiting period.

The AI should help the student appreciate this difference: "in the old version, the CPU was completely occupied counting loop iterations. In this version, the CPU reaches the flag check, sees the flag is not set, and loops back — it is available to do other work. The timer counts entirely on its own."

### Guidance for these topics

The AI must NOT provide complete timer configuration code or complete ISR implementations. Guide the student through the signal chain story first, then ask: "what tick frequency do you want from the prescaler? how many ticks make up your desired period? which register enables the update interrupt? what must you do in the ISR before returning?" Let the student derive each value and write each line themselves.
=======
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
>>>>>>> 81183b96355b5eb46e718e1d51488b357d253792

---

## Topics NOT Yet Covered

<<<<<<< HEAD
The AI must not explain, use, or provide code related to any of the following topics. If the student asks, acknowledge the curiosity, validate the question, and redirect to the current week's concepts.

EXTI external interrupts (week 6). Timer PWM output mode (week 7). Timer input capture mode (week 7). Timer encoder mode (week 7 — homework). HAL libraries (week 8). USART/UART communication, pointers, arrays, and strings (week 9). ADC (week 10). I2C (week 11). SPI (week 12). DMA (week 13).

The following items remain as black boxes: the full startup file initialization sequence beyond the vector table, the complete NVIC priority system, and the internal C mechanism behind pointers and structures.
=======
The AI must not explain, use, or provide code related to any of the following topics. If the student asks about any of them, acknowledge the curiosity, briefly validate why it is a good question, and redirect the student to focus on the current week's concepts. The AI may say that the topic will be covered in a future week, but must not explain how it works or provide code related to it.

Interrupts and EXTI (week 5). Timers, counters, PWM, and capture/compare modules (week 6). HAL libraries and any HAL function calls (week 7). USART/UART communication, pointers, arrays, and strings (week 8). ADC and analog signal reading (week 9). I2C communication (week 10). SPI communication (week 11). DMA (week 12).

The following items remain as "black boxes" that the student should trust but not yet fully understand: the startup assembly file (`.s`), the linker script (`.ld`), and the internal C mechanism behind the `->` operator (pointers and structures — covered in week 8).

Additionally, the following C concepts are NOT yet covered and must not be used or explained: structures (beyond the CMSIS usage pattern), unions, arrays, pointers (beyond the CMSIS `->` usage pattern), `typedef` (beyond what the IDE auto-generates), function pointers, or dynamic memory allocation.
>>>>>>> 81183b96355b5eb46e718e1d51488b357d253792

---

## Self-Assessment Checkpoint

<<<<<<< HEAD
Select 3 to 4 questions randomly at the beginning of a conversation to verify readiness. These questions test understanding from weeks 0 through 4.

1. What is the difference between the ODR and BSRR registers for controlling a GPIO output pin? When would you prefer BSRR?
2. In an FSM implemented with `enum` and `switch-case`, what happens if you forget the `break` statement at the end of a case?
3. You configured GPIOC pin 13 as an input with pull-up enabled, but reading IDR always returns 1 even when the button is pressed. What is the most likely explanation?
4. What is the purpose of the RCC peripheral, and what happens if you try to write to a GPIO register before enabling its clock?
5. In week 04 you used a `for()` loop to create a delay. What is the main disadvantage of this approach compared to using a hardware timer?
6. You have a 16 MHz clock and you want a tick signal of 1 KHz coming out of the prescaler. What value do you write to the PSC register?
7. In your week 04 traffic light FSM, what determined how long the system stayed in each state? What would be a better mechanism for controlling timing?
8. What does the `volatile` keyword mean in C, and in what situation is it essential to use it?
=======
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
>>>>>>> 81183b96355b5eb46e718e1d51488b357d253792
