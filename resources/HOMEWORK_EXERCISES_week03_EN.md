# Homework Exercises — Week 03
## STM32F4xx Embedded Systems Course
### MCU Architecture and Bare-Metal Programming

In week 03, the exercises shift away from code snippets entirely. You are now expected to read the reference manual, identify the relevant registers and bit fields, and write the correct operations yourself. The SFR viewer in the debugger is your primary verification tool — always check the register state after each operation, not just the final result.

Keep the STM32F4xx reference manual open alongside STM32CubeIDE at all times during these exercises. If you are not sure which register to use, the answer is in the documentation — not in the AI assistant, not in a classmate's code. Look it up yourself first.

The exercises in this week follow an incremental path. The first exercises focus on individual registers and isolated operations. The later exercises combine multiple peripherals and registers into complete, working systems. By exercise 10, you will have a real input-output interaction running on the microcontroller, verified both through the SFR viewer and through physical observation of the hardware.

---

### Exercise 3.1 — Identifying peripherals on their buses

Open the STM32F4xx reference manual and find the section that describes the system architecture and bus connections. For each of the following peripherals, identify which bus it is connected to (AHB1, AHB2, APB1, or APB2) and which bit in the corresponding RCC enable register controls its clock: GPIOA, GPIOB, GPIOC, USART2, TIM2, SPI1.

Do not guess — find each one in the documentation. Write your answers as a small table. This knowledge is the foundation of everything that follows: before you can use any peripheral, you must know where its clock enable bit lives.

---

### Exercise 3.2 — Enabling and disabling peripheral clocks

Write code that enables the clocks for GPIOA and GPIOB, then verify in the SFR viewer that both bits are set. Then write a line that disables the GPIOB clock using a `&= ~()` operation. Verify in the SFR viewer that only the GPIOB bit changed and the GPIOA bit was not affected.

This exercise tests whether your masking operations are precise — a poorly written mask can accidentally disable a peripheral you need. Always verify both what changed and what did not change.

---

### Exercise 3.3 — Reading the MODER register

Enable the GPIOA clock. Then read the current value of `GPIOA->MODER` in the SFR viewer before writing anything to it. Write down the value you see. Now look up the MODER register in the reference manual and decode the reset value bit by bit — what mode is each pin in at reset?

After decoding, configure pin 5 as a digital output using the correct bit operations. Verify the change in the SFR viewer and confirm that no other pin's mode was affected by your operation. Remember: each pin occupies two bits in MODER, so be precise about which bit positions you are modifying.

---

### Exercise 3.4 — Exploring the full GPIO register set

With GPIOA clock enabled and pin 5 configured as output, explore each of the following registers in the SFR viewer — reading their reset values before and after you write to them: OTYPER (output type), OSPEEDR (output speed), PUPDR (pull-up/pull-down), ODR (output data). For each register, look up its description in the reference manual before touching it.

Write a value that turns the LED on, then write a value that turns it off. Verify each change in the SFR viewer. The goal is not to memorize the registers — it is to become comfortable navigating between the documentation, the code, and the SFR viewer as a natural debugging workflow.

---

### Exercise 3.5 — Configuring an input pin

Enable the clock for GPIOC. Configure pin 13 (the user button on the Nucleo board) as a digital input with the internal pull-up resistor enabled. Read the reference manual to find the correct MODER and PUPDR values for this configuration.

After configuring, read the IDR register in the SFR viewer while pressing and releasing the button. What changes? Which bit corresponds to pin 13? Think about the relationship between the physical button state and the bit value you observe — and notice that the relationship might be inverted from what you expect, depending on how the button is wired on the board.

---

### Exercise 3.6 — Reading the full peripheral state in the SFR viewer

With both GPIOA and GPIOC configured from the previous exercises, open the SFR viewer and expand both peripherals simultaneously so you can see all their registers at once — MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR — for both ports at the same time.

Without running any new code, read the current state of every register for both peripherals and answer the following questions in writing: which registers still have their reset values? Which ones have been modified by your previous exercises? For each modified register, can you explain why each bit is set to its current value? This exercise is about developing the habit of reading and interpreting the full peripheral state, not just the single register you last touched. A good embedded developer always knows the complete state of the hardware they are working with.

---

### Exercise 3.7 — Reading before writing

This exercise introduces one of the most important disciplines in embedded programming: always read and understand the current state of a register before modifying it.

Enable the clock for GPIOB. Before writing a single line that modifies `GPIOB->MODER`, open the SFR viewer and record the reset value of that register. Now configure pin 7 of GPIOB as a digital output. After writing your operation, return to the SFR viewer and verify two things: first, that bits 15 and 14 (which control pin 7) now show the output mode value. Second, that every other bit field in the register is identical to the reset value you recorded before. If any other bit changed, your masking operation is incorrect — go back and fix it before continuing.

---

### Exercise 3.8 — Combining registers in the correct sequence

Configure both GPIOA pin 5 (output — LED) and GPIOC pin 13 (input — button) from scratch in a single, clean sequence. Write the operations in the correct order: clock enables first, then mode configurations, then pull-up/pull-down settings. Do not copy code from previous exercises — write everything fresh from memory and from the reference manual.

After writing the complete configuration sequence, step through each line one at a time in the debugger and verify in the SFR viewer that each register changes correctly after each line. The goal is to practice writing a complete peripheral initialization sequence and to build the habit of verifying each step individually rather than running everything at once and hoping it works.

---

### Exercise 3.9 — Connecting input to output: button controls LED using BSRR

With GPIOA pin 5 configured as output and GPIOC pin 13 configured as input with pull-up, write a program that reads the button state continuously inside the `while(1){}` loop and controls the LED accordingly. Use the BSRR register to turn the LED on and off — not the ODR register.

Look up the BSRR register in the reference manual carefully before writing any code. Understand how its bit fields are organized and how it differs from ODR in the way you write to it. After your program is running and the LED correctly follows the button state, implement the same behavior a second time using ODR instead of BSRR — you can comment out the BSRR version and write the ODR version below it.

Once both versions work correctly, write a short description — in comments inside your code — answering the following question: what is the practical difference between controlling the LED through ODR versus through BSRR? Think about what happens at the register level when you write to each one, and what could go wrong with ODR in a more complex system where multiple parts of the code try to control different pins of the same port simultaneously.

---

### Exercise 3.10 — Observing the live system in the SFR viewer

With your button-LED program from exercise 3.9 running in debug mode, configure the SFR viewer to show both GPIOA and GPIOC registers simultaneously. Now, instead of stepping through the code line by line, let the program run freely and observe the SFR viewer while physically pressing and releasing the button on the Nucleo board.

Watch the IDR register of GPIOC — specifically bit 13 — as you press and release the button. Then watch the ODR register of GPIOA — specifically bit 5 — and observe how it responds to the button state. Notice the timing: does the LED respond immediately? Is there any behavior that seems inconsistent or unexpected, like the LED flickering slightly? If you observe unexpected behavior, think about what might cause it — the answer connects directly to a concept you will study in week 05.

This exercise is about learning to read a live running system through the SFR viewer, which is one of the most powerful debugging skills in embedded development. The hardware is talking to you through those register values — your job is to learn how to listen.
