# Homework Exercises — Week 05
## STM32F4xx Embedded Systems Course
### Timers and Time Measurement

This week's exercises replace the inefficient `for()` delay approach from week 04 with hardware timer-driven timing. Every exercise from this week onward includes a mandatory **LED_OK** — a heartbeat LED controlled by its own dedicated timer that blinks continuously to show the system is alive and running. If LED_OK stops blinking, something is wrong at the system level, regardless of what the rest of the code is doing. This is a standard practice in real embedded systems and you should adopt it as a habit from this point forward.

All exercises use the following timer assignment convention unless otherwise specified:

- **TIM2** — dedicated to LED_OK, always blinking at 1 Hz (500ms ON, 500ms OFF). Never use TIM2 for any other purpose.
- **TIM3** — the main exercise timer, configured for the period required by each exercise.

Both timers must be configured completely at the register level using CMSIS notation and CMSIS-defined mask names. Every ISR must follow the mandatory pattern: check the interrupt flag, clear it immediately, then set a `volatile` flag for `main()` to process. The main loop checks flags and responds — never perform complex logic directly inside an ISR.

Before writing any code for each exercise, always complete these steps in order: enable the RCC clock for the timer, configure PSC and ARR for the desired period, enable the update interrupt in DIER, call `NVIC_EnableIRQ()`, and start the counter by setting CEN in CR1 last.

---

### Exercise 5.1 — Two-timer blinky: main LED and LED_OK

This exercise replicates what was done in the theory class, but now with two timers running simultaneously. Configure TIM2 to blink LED_OK at exactly 1 Hz using GPIOA pin 5 (the onboard LED). Configure TIM3 to blink a second external LED at a different rate of your choice — something clearly distinguishable from 1 Hz, such as 4 Hz or 0.5 Hz.

Each timer needs its own ISR (`TIM2_IRQHandler` and `TIM3_IRQHandler`), its own `volatile` flag variable, and its own handling block in the main loop. The two LEDs must blink completely independently — the rate of one must never affect the rate of the other.

Before writing any code, calculate your PSC and ARR values for both timers on paper, walking through the signal chain: system clock → PSC → tick frequency → ARR → UpdateEvent period. Verify your calculations before loading them into the registers. After the program is running, open the SFR viewer and observe both TIM2->CNT and TIM3->CNT counting simultaneously — this is hardware parallelism in action. The CPU is not involved in the counting at all.

Think about this: in week 04, two LEDs blinking at different rates required careful management of a single blocking delay. How does the timer approach change the structure of your `while(1){}` loop? What is the main loop doing now while the timers are counting?

---

### Exercise 5.2 — Adjustable blink rate with two buttons

Extend exercise 5.1 by making TIM3's blink rate adjustable through two buttons. One button increases the blink frequency (shorter period), the other decreases it (longer period). LED_OK on TIM2 must continue blinking at exactly 1 Hz regardless of what happens to TIM3.

To change TIM3's period while it is running, you must write new values to TIM3->PSC and TIM3->ARR. Think carefully about when and how to do this safely — modifying a running timer's registers can cause unexpected behavior if not done correctly. Define a minimum and maximum period to prevent the blink from becoming too fast to see or too slow to be useful.

Read the button states through polling inside the main loop — do not use interrupts for the buttons yet. Structure your code so that button reading, TIM3 period adjustment, and LED toggling are all handled cleanly and separately in the main loop. Think about what happens if both buttons are pressed simultaneously — define that behavior explicitly.

After the program is running correctly, reflect on a limitation you might notice: what happens to the LED blink if you hold a button down? Is the behavior smooth? What would make it better? Write your observations as a comment in the code.

---

### Exercise 5.3 — Binary counter with 6 LEDs

Connect 6 external LEDs to 6 consecutive GPIO output pins. These LEDs will display a binary number that increments on every TIM3 UpdateEvent. LED_OK continues on TIM2 at 1 Hz.

Configure TIM3 with a period of your choice between 250ms and 500ms — pick one value and stick with it. Every time TIM3 fires, increment a `uint8_t` counter variable in the main loop. Map each bit of the counter to a corresponding LED: bit 0 controls LED0, bit 1 controls LED1, and so on up to bit 5 controlling LED5. When the counter reaches 63 (all 6 bits set), the next increment wraps it back to 0 automatically — this happens naturally with a `uint8_t` if you mask to 6 bits, or you can let it wrap and mask when writing to the GPIO.

The bit extraction is the key challenge of this exercise. To get the state of bit N from the counter and apply it to a specific LED pin, you need to combine the bitwise skills from weeks 2 and 3 with the GPIO output skills from week 4. Think about how to isolate each bit and map it to the correct output pin. Do not use `if/else` chains for this — find a more elegant approach using bitwise operations and shifts.

Before running the code, predict the sequence: what binary pattern should you see on the LEDs? At your chosen period, how long does it take to count from 0 to 63 and wrap back to 0? Verify your prediction by observing the LEDs and checking the counter variable in the debugger. Also observe LED_OK throughout — it must never stop blinking, even as the counter is running.

---

### Exercise 5.4 — Traffic light FSM controlled by a timer

Implement a complete traffic light system using TIM3 as the timing source for all state transitions. LED_OK continues on TIM2 at 1 Hz. The traffic light uses three external LEDs: red, yellow, and green.

Design your FSM before writing any code. Draw the complete state diagram on paper with all states, transitions, and timing. The approximate durations should be: green — 4 seconds, yellow — 1 second, red — 4 seconds. Since TIM3 fires at a fixed period, you will need a counter inside the main loop that tracks how many TIM3 events have occurred in the current state, transitioning to the next state when the count reaches the required number. Think carefully about how to implement this — it is the core challenge of this exercise.

Use `enum` for state definitions and `switch-case` for the FSM implementation, following the pattern established in week 04. The `enum` should have meaningful names that clearly describe each traffic light state.

After the system is running, observe the LED_OK LED carefully. It must blink at exactly 1 Hz throughout the entire traffic light sequence — through green, yellow, and red states. If LED_OK ever pauses or slows down, there is blocking code somewhere in your main loop. Find and fix it.

Think about this: what is the relationship between TIM3's configured period and the accuracy of your traffic light timing? If TIM3 fires every 100ms and you want a 4-second green phase, how many TIM3 events do you need to count? What is the timing error if TIM3's actual period is slightly off from your calculation?

---

### Exercise 5.5 — Traffic light with pedestrian crossing request

Extend the traffic light from exercise 5.4 by adding a pedestrian crossing request button. When the button is pressed, the system should note the request and transition to a pedestrian crossing phase after the current green phase completes — never interrupting a yellow or red phase already in progress. The pedestrian crossing phase should hold red for vehicles for an additional 3 seconds beyond the normal red duration, then resume the normal cycle.

LED_OK continues on TIM2 at 1 Hz, unaffected by anything else in the system.

This exercise requires careful FSM design. The pedestrian request must be stored as a flag that the FSM checks at the appropriate transition point — specifically when transitioning from green to yellow. If the flag is set at that moment, the FSM enters an extended red state; if not, it follows the normal cycle. Read the button state through polling in the main loop — not through interrupts.

Before writing any code, redesign your state diagram from exercise 5.4 to include the new pedestrian states and the conditional transition. Have a classmate review your diagram before you start coding — a flaw in the design is much easier to fix on paper than in code.

After the system is running, test edge cases deliberately: what happens if you press the button multiple times? What happens if you press it during the yellow phase? Does the system behave sensibly in all cases? Write a brief test report as comments in your code describing each case you tested and what behavior you observed. This habit of systematic testing is as important as writing the code itself.
