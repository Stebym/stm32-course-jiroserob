# Homework Exercises — Week 04
## STM32F4xx Embedded Systems Course
### General Purpose Input/Output (GPIO)

This week's exercises bring together everything you have learned so far — register-level GPIO configuration, CMSIS-defined masks, bitwise operations, control structures, and FSM design — and connect them to real, visible hardware behavior. You will configure output pins to control LEDs, configure input pins to read buttons, and progressively build more sophisticated programs culminating in a complete FSM implementation.

The exercises follow a deliberate three-phase progression. The first phase focuses on outputs and the software delay. The second phase introduces inputs and simple decision making. The third phase develops the same FSM behavior through three increasingly structured implementations, so you can see — and feel — why the final approach is better than the first.

All code in this week must follow the course coding standards: C only, `Src/Inc` folder separation, paired `.c` and `.h` files for each module, CMSIS-defined mask names, proper bitwise operations, and the module-based naming convention for functions. Before writing any code, always check the reference manual for the register and bit field you need.

---

### Exercise 4.1 — Blinky LED: the embedded "Hello World"

Configure GPIOA pin 5 as a push-pull digital output and write a program that blinks the onboard LED continuously. The LED should turn on, wait, turn off, wait, and repeat. Use the following software delay pattern to create the waiting periods — choose a value that produces a visible blink at roughly 1 second on and 1 second off:

```c
for(uint32_t i = 0; i < YOUR_VALUE_HERE; i++){}
```

For the GPIO configuration, use CMSIS-defined mask names — do not use raw numbers for bit positions. Verify the complete register configuration in the SFR viewer before running the program. After the LED is blinking correctly, experiment with different delay values and observe how the blink rate changes. Think about this: if you needed the LED to blink at exactly 1 Hz, how confident are you that your `for()` delay actually achieves that? What factors might affect the accuracy of this approach?

---

### Exercise 4.2 — Two LEDs with independent timing

Connect an external LED to a pin of your choice on GPIOB. Configure both GPIOA pin 5 and your chosen GPIOB pin as push-pull digital outputs. Write a program where the two LEDs blink at different rates — one approximately twice as fast as the other.

Before writing any code, think about how to achieve two different rates using a single `for()` delay value as your base unit. After the program is running, observe it for a few minutes. Does the timing relationship between the two LEDs remain consistent, or does it drift? This observation connects to why the software delay approach has real limitations in embedded systems.

---

### Exercise 4.3 — Three LEDs in sequence

Connect three external LEDs (representing red, yellow, and green) to three different GPIO pins. Configure all three as push-pull digital outputs. Write a program that turns them on and off in a fixed sequence with approximately 3 to 5 seconds between each transition — long enough to clearly see each state before the next one begins.

The sequence should be: red on → red off, green on → green off, yellow on → yellow off, then repeat. Before writing the code, draw the sequence on paper as a timing diagram — which LED is on at each moment in time. This diagram is the first step toward a state machine, even if the code does not yet use one.

---

### Exercise 4.4 — Reading a button and controlling an LED

Configure GPIOC pin 13 (onboard button) as a digital input with the internal pull-up resistor enabled. Configure GPIOA pin 5 as a push-pull output. Write a program inside `while(1){}` that reads the button state through the IDR register and controls the LED accordingly — the LED should be on while the button is pressed and off while it is released.

Read the IDR register using CMSIS notation and use a bitwise AND operation with the appropriate mask to isolate bit 13. Remember the pull-up configuration means the logic may be inverted from what you expect. Verify the button reading in the SFR viewer before relying on it to control the LED. After the basic behavior works, observe carefully: does the LED ever flicker or behave unexpectedly when you press or release the button? Note your observation — it will become relevant in week 05.

---

### Exercise 4.5 — Two buttons controlling LED timing

Connect an external button to a GPIO pin of your choice. Configure both buttons as digital inputs with pull-up resistors enabled, and configure the onboard LED as output. Write a program where one button increases the LED blink speed and the other decreases it.

You will need a variable that stores the current delay value and is modified by the button presses. Think carefully about how to structure the reading of two buttons and the modification of the delay variable inside the `while(1){}` loop. What should happen if both buttons are pressed at the same time? Define that behavior explicitly in your code, even if the choice is arbitrary. An undefined behavior in embedded systems is always a bug waiting to happen.

---

### Exercise 4.6 — Traffic light: first approach with if/else

Using the three LEDs from exercise 4.3 (red, yellow, green), implement a traffic light sequence using only `if/else` statements and a single variable that tracks the current state as a plain number (0, 1, 2). The timing for each state should be approximately 3 to 5 seconds.

The sequence should be: green (state 0) → yellow (state 1) → red (state 2) → back to green. At the end of each state's delay, increment the state variable and use `if/else` logic to decide which LEDs to turn on and off. After the program is running correctly, read your own code critically. How easy is it to understand what state 0, state 1, and state 2 mean just by reading the numbers? How would you add a fourth state — for example, a flashing yellow for pedestrian crossing? Write your honest assessment as a comment at the top of the file.

---

### Exercise 4.7 — Traffic light: refactored with switch-case

Take the traffic light program from exercise 4.6 and rewrite it using a `switch-case` structure instead of `if/else`. Keep using plain numbers (0, 1, 2) for the states for now — do not introduce `enum` yet. The visible behavior of the program should be identical to exercise 4.6.

After the refactored version works correctly, compare the two versions side by side. How does the `switch-case` version read compared to the `if/else` version? Is it easier or harder to see the structure of the traffic light logic? Now think about adding that fourth state again — is it easier to add a new `case` than a new `else if`? Write your comparison as a comment at the top of the file. You are building the intuition for why `switch-case` is the natural backbone of an FSM implementation.

---

### Exercise 4.8 — Traffic light: final version with enum

Take the traffic light program from exercise 4.7 and refactor it one more time — this time introducing an `enum` to give meaningful names to the states. Define the enumeration before `main()` following the course naming convention. Replace every raw number in the `switch-case` with the corresponding enum value.

After the refactored version works correctly, read your code one more time from top to bottom. Can you understand what the program does without running it, just by reading the state names and the `case` labels? Now think about adding that fourth state one more time — is the process clear and clean? Compare all three versions (4.6, 4.7, 4.8) and write a final comment that answers this question: if you had to hand this code to another engineer who has never seen it before, which version would you give them and why? This is the engineering argument for using `enum` and `switch-case` in FSM implementations — not just because it is good style, but because it makes the code self-documenting and maintainable.

---

### Exercise 4.9 — Button-controlled FSM with two states

Design and implement an FSM with two states that is controlled by a button. State A has the LED off. State B has the LED on. Each time the button is pressed and released, the system transitions from one state to the other. Use `enum` for state definitions and `switch-case` for the state machine implementation from the beginning — do not start with raw numbers this time.

Before writing any code, draw the complete state diagram on paper: two states, the transitions between them, and what triggers each transition. Then translate the diagram into code. Think carefully about how to detect a button press-and-release cycle rather than just a button being held down — the difference between detecting a transition and detecting a level is an important concept in embedded input handling.

---

### Exercise 4.10 — FSM with three states and multiple outputs

Design and implement an FSM with at least three states that uses both buttons and multiple LEDs. The specific behavior is yours to define — but it must meet the following requirements: each state must produce a distinct output pattern on the LEDs, at least one transition must be triggered by one button, at least one transition must be triggered by the other button, and there must be at least one state that has different transitions depending on which button is pressed.

Before writing any code, complete the full FSM design on paper: state diagram with all states, all transitions, all triggers, and all outputs for each state. Have your diagram reviewed by a classmate before you start coding. After the implementation is complete and working, verify that your code matches your diagram exactly — if they differ, update whichever one is wrong so that the diagram and the code are always in agreement. In a real embedded project, the design document and the code must always tell the same story.
