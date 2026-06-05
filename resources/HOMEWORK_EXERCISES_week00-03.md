# Homework Exercises — Weeks 00 to 03
## STM32F4xx Embedded Systems Course

This document contains the exercises for weeks 00 through 03. You should work through them in order, at your own pace, picking up from wherever the workshop session left off. All exercises run directly on the STM32F4xx microcontroller connected to your laptop through STM32CubeIDE. There is no `printf` or serial output — the debugger is your only window into what the microcontroller is doing, and learning to use it well is itself one of the goals of these exercises.

Before running any piece of code, always predict the result first. Write your prediction on paper or in a comment. Then run the code and compare what you see in the debugger against what you expected. If they match, great — you understood it. If they don't match, even better — you have something real to investigate and learn from. Never skip the prediction step.

Work with your classmates. Discuss your predictions, compare your results, and help each other when you get stuck. If you need guidance, use the AI teaching assistant — but always arrive with your own attempt first.

---

## Week 00 — Number Systems, Variable Sizes, and Representations

In this week's exercises, everything you do follows the same simple pattern: declare a variable, assign a value, set a breakpoint on the next line, and inspect the variable in the debugger. The microcontroller's memory is the laboratory, and the debugger is your instrument. Pay attention to the display format options in the debugger — you can view the same variable in decimal, hexadecimal, and binary, and switching between them will teach you a great deal.

### Exercise 0.1 — First contact with the debugger

Create a new project in STM32CubeIDE without using CubeMX code generation. The IDE will generate a skeleton project with a `main.c` file. The first thing you should always do is replace the auto-generated `for(;;)` loop with `while(1){}` — this is how we write an infinite loop in this course.

Inside `main()`, before the `while(1){}`, declare the following variable and set a breakpoint on the line immediately after it:

```c
uint8_t my_variable = 42;
```

Compile, flash, and run the project in debug mode. When execution stops at your breakpoint, find `my_variable` in the variable inspector and verify that it contains the value 42. This exercise is about one thing only: making sure your complete workflow — write, compile, flash, debug, inspect — is working correctly. If you see 42 in the variable inspector, you are ready to continue.

### Exercise 0.2 — Same number, different representations

Declare the following three variables:

```c
uint8_t dec = 65;
uint8_t hex = 0x41;
uint8_t bin = 0b01000001;
```

Before running the code, predict: do these three variables hold the same value or different values? Run the code, set a breakpoint after the declarations, and inspect all three in the debugger. Then try changing the display format of each variable between decimal, hexadecimal, and binary. Observe how the same underlying value looks different depending on the representation. Think about this: if the microcontroller only works with bits, what does it mean that you can write the same value in three different ways in your source code?

### Exercise 0.3 — How big is a variable?

Declare the following variables and predict what value each one will hold before running the code:

```c
uint8_t  a = 255;
uint16_t b = 255;
uint32_t c = 255;
uint8_t  d = 256;
```

Inspect all four in the debugger. One of them will surprise you. After verifying the result, add one more variable:

```c
uint8_t  e = 257;
```

What value does `e` hold? Why? Think about what it means that the variable type defines the "size of the container" and what happens when the value you want to store is larger than the container can hold.

### Exercise 0.4 — The sign matters

Declare the following variables:

```c
uint8_t a = 200;
int8_t  b = 200;
int8_t  c = -1;
```

Before running, predict the value of each variable in the debugger. Run the code and inspect the results. At least one will be different from what you might expect. After inspecting the values in decimal, switch the display format of variable `c` to hexadecimal. What do you see? Think about why -1 has that particular hexadecimal representation in an 8-bit signed variable. This is 2's complement in action — the same concept you studied in the theory class, now visible inside real hardware.

### Exercise 0.5 — Boundaries and overflow

Type the following code and step through it line by line in the debugger, inspecting the variable after each step:

```c
int8_t  x = 127;
x = x + 1;

uint8_t y = 255;
y = y + 1;
```

What happens to `x` after you add 1 to 127? What happens to `y` after you add 1 to 255? After observing both results, explore the boundary constants that `stdint.h` provides. Declare variables and assign them the values `INT8_MAX`, `INT8_MIN`, `UINT8_MAX`, and `UINT16_MAX`, then inspect each one in the debugger. Build a mental table of the minimum and maximum value for each type. This knowledge is not optional in embedded systems — choosing the wrong type for a variable is a real source of bugs in real products.

### Exercise 0.6 — Hexadecimal mental arithmetic

Declare `uint8_t result;` and write the following assignments one at a time. Before each one, write your prediction on paper. Then run and verify in the debugger:

```c
result = 0x0F + 0x01;
result = 0xFF + 0x01;
result = 0xA0 + 0x5F;
result = 0xA0 + 0x60;
```

For each result, ask yourself: did the addition overflow? How can you tell from the hexadecimal value alone? If your predictions were wrong, go back and work through the addition by hand in binary to understand where your reasoning failed. The debugger gives honest, immediate feedback — use it.

### Exercise 0.7 — Building a number bit by bit

Start with `uint8_t x = 0;` and write the following assignments one at a time, setting a breakpoint after each one and inspecting `x` in binary display format before moving to the next:

```c
x = 0x01;
x = 0x02;
x = 0x04;
x = 0x08;
x = 0x10;
x = 0x20;
x = 0x40;
x = 0x80;
```

Look at the pattern that forms in the binary view. Each assignment activates exactly one bit, and each step doubles the previous value. Think about the connection between this pattern and the shift registers you simulated in week 00's theory — and think about what operation in C might produce this pattern automatically, without having to write eight separate assignments.

### Exercise 0.8 — The bang operator and boolean logic

Declare the following variables and write your prediction for each result before running the code:

```c
uint8_t a = 5;
uint8_t b = 0;
uint8_t c = 255;

uint8_t r1 = !a;
uint8_t r2 = !b;
uint8_t r3 = !c;
uint8_t r4 = ~c;
```

Inspect all four results in the debugger. Pay careful attention to `r3` and `r4` — they are computed from the same variable `c`, but using different operators. Are the results the same? Now add one more experiment:

```c
uint8_t r5 = ~a;
```

Compare `r1` (which is `!a`) with `r5` (which is `~a`). Are they the same? The difference between what you observe here is one of the most important distinctions in C programming for embedded systems. Make sure you can explain in your own words what `!` does versus what `~` does, and why they produce different results when applied to a value like 5.

---

## Week 01 — C Language Fundamentals

All exercises this week continue to use the debugger as the only verification tool. The pattern is the same as week 00: predict, run, verify, understand. The focus shifts from number representations to arithmetic operations, shift operators, and control structures. Some exercises include code to type directly. Others describe what the code should do — you must write it yourself.

### Exercise 1.1 — Arithmetic operators and integer division

Declare the following variables and inspect each result in the debugger:

```c
uint8_t a          = 17;
uint8_t b          = 5;
uint8_t div_result = a / b;
uint8_t mod_result = a % b;
uint8_t mul_result = a * b;
```

Before running, predict each result. Pay particular attention to `div_result` — C performs integer division, which means the decimal part is simply discarded. After verifying, think about the relationship between `div_result` and `mod_result`: if you multiply `div_result` by `b` and add `mod_result`, what do you get? Verify your answer in the debugger.

### Exercise 1.2 — Overflow in arithmetic

Declare the following variables and predict each result before running:

```c
uint8_t x   = 200;
uint8_t y   = 100;
uint8_t sum = x + y;
```

The result will not be 300. Why not? After verifying in the debugger, think about this: if `sum` is wrong and your program uses it to make a decision — for example, to check if a sensor value exceeded a safety threshold — what could happen in a real system? This is not a hypothetical problem. Overflow-related bugs have caused real failures in real embedded systems.

### Exercise 1.3 — Shift operators as multiplication and division

Declare `uint8_t val = 3;` and then declare the following variables, predicting each result before running:

```c
uint8_t left1  = val << 1;
uint8_t left2  = val << 2;
uint8_t left3  = val << 3;
uint8_t right1 = val >> 1;
```

Inspect each result in binary display format to watch the bits move. Then test the edge case: what happens when you shift a bit off the end of the variable? Write a variable initialized to `0b10000000` and left-shift it by 1. What do you observe? Think about the connection between this behavior and the bit-by-bit exercise from week 00.

### Exercise 1.4 — Boolean evaluation and if/else

Write a program that declares three variables with different values, then uses `if` and `if-else` statements to set a `result` variable based on conditions. The logic should test at least one non-zero value as a condition, one zero value as a condition, and one equality comparison. Set a breakpoint after each `if` block and watch `result` change in the debugger as you step through.

Think carefully about how C decides whether a condition is true or false. What is the only value that C considers false? What does that tell you about how the language treats any non-zero number?

### Exercise 1.5 — The for loop as a counter

Write a `for` loop that counts from 0 to 9 and increments a variable called `counter` on each iteration. Set a breakpoint after the loop completes and verify that `counter` holds the value you expect. Then modify the loop in three different ways, one at a time: make it count in steps of 2, make it count backwards from 10 to 1, and make it stop at a different value. For each modification, predict the final value of `counter` before running.

### Exercise 1.6 — The while loop and a simple accumulator

Write a `while` loop that adds all integers from 1 to 10 and stores the running total in a variable called `sum`. Before running, calculate the expected result on paper. After verifying in the debugger, modify the exercise: change the data type of `sum` to `uint16_t` and sum all integers from 1 to 100. What is the expected result? Would it fit in a `uint8_t`? Verify your answer.

### Exercise 1.7 — The do-while loop

Write two separate pieces of code side by side. The first uses a `while` loop with a condition that is false from the start. The second uses a `do-while` loop with the same false condition. Both loops, if they execute, should set a result variable to 42. Predict which result variable will be 42 and which will remain at 0. Verify in the debugger. In your own words, describe the fundamental difference between `while` and `do-while`.

### Exercise 1.8 — The switch-case as a decision tree

Write a `switch-case` structure that maps an `input` variable to an `output` variable according to at least four different cases, with a `default` case for any value that does not match. Set `input` to different values, recompile, and verify in the debugger that `output` changes correctly each time. Then deliberately remove one of the `break` statements and observe what happens — this behavior is called "fall-through" and it is important to understand, even if you almost never want it to happen accidentally.

---

## Week 02 — Bitwise Logic Operators and FSM Introduction

This week's exercises have two tracks. The first track is hands-on in the IDE — applying bitwise logic operators to variables and then to real hardware registers. The second track is design work on paper — building FSM state diagrams for real-world systems. Both tracks are equally important. The code exercises build the tools; the FSM exercises build the discipline for using them well.

In this week, code snippets are provided for the foundational exercises, but the later exercises become more descriptive. Read the description carefully, understand what is being asked, and write the code yourself.

### Exercise 2.1 — AND operator: the mask that reveals

Type and run the following code, predicting the result before running:

```c
uint8_t value  = 0b10110101;
uint8_t mask   = 0b00001111;
uint8_t result = value & mask;
```

Inspect `result` in binary display format. The AND operator keeps only the bits where the mask has a 1 and forces all other bits to 0 — it acts like a window that reveals specific bits while hiding others. After verifying, answer this question on paper before continuing: what mask would you use to extract only the upper 4 bits (bits 7 through 4)? Write the code and verify your answer.

### Exercise 2.2 — OR operator: the mask that sets

Type and run the following code, predicting the result:

```c
uint8_t value  = 0b10100000;
uint8_t mask   = 0b00000101;
uint8_t result = value | mask;
```

Inspect `result` in binary view. Answer these two questions before continuing: if a bit is already 1 and you OR it with 1, what happens to that bit? If a bit is 0 and you OR it with 0, what happens? Your answers explain why `|=` is the safe way to set bits in a hardware register without disturbing the bits you did not intend to change.

### Exercise 2.3 — NOT operator: the complement

Type and run the following code, predicting each result:

```c
uint8_t a       = 0b00001111;
uint8_t result1 = ~a;

uint8_t b       = 0b10100101;
uint8_t result2 = ~b;
```

After verifying, build the bit-clearing pattern step by step. Declare `uint8_t value = 0b11111111;` and then clear bits 0 and 1 without changing any other bits. Do this using the `&= ~()` pattern. Before writing the code, work out the mask and its complement on paper first, then verify your result in the debugger.

### Exercise 2.4 — XOR operator: the toggle

Type and run the following code, predicting each result:

```c
uint8_t value   = 0b10110011;
uint8_t mask    = 0b00001111;
uint8_t result1 = value ^ mask;
uint8_t result2 = result1 ^ mask;
```

What is special about `result2`? XOR applied twice with the same mask always returns to the original value — XOR is its own inverse. Think about what this property means for a practical problem: if you wanted to change a bit back and forth between 0 and 1 repeatedly, how would you use XOR to do that with a single operation?

### Exercise 2.5 — Combining operators: the full pattern

Declare `uint8_t simulated_register = 0x00;` and then perform the following sequence of operations, setting a breakpoint after each one and verifying the result in binary view before moving to the next:

- Set bits 3 and 4 simultaneously using a single `|=` operation
- Clear bit 3 without changing bit 4 using `&= ~()`
- Toggle bit 4 using `^=`
- Toggle bit 4 again using `^=`

Write each operation yourself — do not look up the syntax. If you are unsure, reason it out from what you know about each operator. After completing the sequence, read the comment in your code: this variable behaves exactly like a hardware register. The only difference in the next exercise is that the register is connected to real silicon.

### Exercise 2.6 — First real register: enabling the clock

Open the STM32F4xx reference manual and navigate to the RCC (Reset and Clock Control) section. Find the AHB1ENR register and identify which bit enables the clock for GPIOA. Then write a single line of code that enables that clock using the CMSIS-defined constant name — do not use a raw number. After executing the line, open the SFR viewer in the debugger, navigate to RCC → AHB1ENR, and confirm that the correct bit changed state. This is the same `|=` operation from exercise 2.5 — the only difference is that this register controls real hardware.

### Exercise 2.7 — First LED on GPIOA Pin 5

With the GPIOA clock already enabled from exercise 2.6, configure pin 5 as a digital output and turn the onboard LED on. You will need to write to two registers: the MODER register to set the pin mode, and the ODR register to set the output value. The exact bit positions are described in the reference manual — look them up. After each register write, verify in the SFR viewer before checking the physical LED on your Nucleo board.

This is the moment where everything you have learned connects: the bitwise operations from this week, the number representations from week 00, and the idea that hardware is controlled by bits in registers. If the LED turns on, you have understood all of it correctly.

### Exercise 2.8 — FSM design: the pedestrian traffic light

This exercise requires no code — only thinking and paper. Design a Finite State Machine for a pedestrian traffic light system. The system controls two sets of lights simultaneously: one for vehicles (red, yellow, green) and one for pedestrians (walk, don't walk). For your design, identify every possible state the system can be in, draw a complete state diagram with labeled transitions between states, and describe in one sentence what triggers each transition and what the outputs (which lights are on) are in each state.

A state is defined by the outputs it produces. If two moments in the system's life produce different outputs, they are different states. Before you finalize your diagram, check it by reading each transition out loud: "the system is in state X, and when Y happens, it moves to state Z." If any transition leads nowhere, or if any state has no way to exit, your design has a gap.

### Exercise 2.9 — FSM design: the vending machine

Design a Finite State Machine for a simple vending machine. The machine accepts coins one at a time and dispenses a product when enough coins have been inserted. Your design must handle not only the expected scenario — user inserts the right number of coins and receives the product — but also the unexpected ones: what happens if the user inserts too many coins? What if they press cancel before inserting enough? What if the product is out of stock?

A state machine that only handles the happy path is an incomplete design. In embedded systems, hardware can receive unexpected inputs at any moment, and the software must respond sensibly in every state to every possible input. Every state in your diagram must have a defined response to every possible input — even if that response is "do nothing and stay in the current state."

---

## Week 03 — MCU Architecture and Bare-Metal Programming

In week 03, the exercises shift away from code snippets entirely. You are now expected to read the reference manual, identify the relevant registers and bit fields, and write the correct operations yourself. The SFR viewer in the debugger is your primary verification tool — always check the register state after each operation, not just the final result.

Keep the STM32F4xx reference manual open alongside STM32CubeIDE at all times during these exercises. If you are not sure which register to use, the answer is in the documentation — not in the AI assistant, not in a classmate's code. Look it up yourself first.

### Exercise 3.1 — Identifying peripherals on their buses

Open the STM32F4xx reference manual and find the section that describes the system architecture and bus connections. For each of the following peripherals, identify which bus it is connected to (AHB1, AHB2, APB1, or APB2) and which bit in the corresponding RCC enable register controls its clock: GPIOA, GPIOB, GPIOC, USART2, TIM2, SPI1.

Do not guess — find each one in the documentation. Write your answers as a small table. This knowledge is the foundation of everything that follows: before you can use any peripheral, you must know where its clock enable bit lives.

### Exercise 3.2 — Enabling and disabling peripheral clocks

Write code that enables the clocks for GPIOA and GPIOB, then verifies in the SFR viewer that both bits are set. Then write a line that disables the GPIOB clock using a `&= ~()` operation. Verify in the SFR viewer that only the GPIOB bit changed and the GPIOA bit was not affected. This exercise tests whether your masking operations are precise — a poorly written mask can accidentally disable a peripheral you need.

### Exercise 3.3 — Reading the MODER register

Enable the GPIOA clock. Then read the current value of `GPIOA->MODER` in the SFR viewer before writing anything to it. Write down the value you see. Now look up the MODER register in the reference manual and decode the reset value bit by bit — what mode is each pin in at reset? After decoding, configure pin 5 as a digital output using the correct bit operations. Verify the change in the SFR viewer and confirm that no other pin's mode was affected by your operation.

### Exercise 3.4 — Exploring the full GPIO register set

With GPIOA clock enabled and pin 5 configured as output, explore each of the following registers in the SFR viewer — reading their reset values before and after you write to them: OTYPER (output type), OSPEEDR (output speed), PUPDR (pull-up/pull-down), ODR (output data). For each register, look up its description in the reference manual before touching it. Write a value that turns the LED on, then write a value that turns it off. Verify each change in the SFR viewer. The goal is not to memorize the registers — it is to become comfortable navigating between the documentation, the code, and the SFR viewer as a natural debugging workflow.

### Exercise 3.5 — Configuring an input pin

Enable the clock for GPIOC. Configure pin 13 (the user button on the Nucleo board) as a digital input with the internal pull-up resistor enabled. Read the reference manual to find the correct MODER and PUPDR values for this configuration. After configuring, read the IDR register in the SFR viewer while pressing and releasing the button. What changes? Which bit corresponds to pin 13? Think about the relationship between the physical button state and the bit value you observe — and notice that the relationship might be inverted from what you expect, depending on how the button is wired on the board.
