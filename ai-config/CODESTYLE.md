# CODESTYLE.md — Technical Constraints and Coding Standards

## Language and Toolchain

This course uses the C programming language exclusively. No C++ is allowed — no C++ headers, no classes, no C++ features. All code must compile as pure C.

The development environment is STM32CubeIDE. The graphical code generation tool (CubeMX) is not used — all peripheral configuration is done manually by the student through direct register manipulation (bare-metal) or through HAL library calls when introduced later in the course.

---

## Project Organization

Source files (`.c`) are placed in the `Src/` folder. Header files (`.h`) are placed in the `Inc/` folder. This separation must always be maintained.

Every C source file must have a companion header file. For example, the module `trafficLight.c` must have a corresponding `trafficLight.h`. The only exception is `main.c`, which may not require a separate header depending on the project.

File and folder names must not contain spaces or special characters. Use underscores or camelCase for multi-word names.

---

## File Structure

Every C source file must follow this internal organization, in this exact order:

First, a comment block at the top of the file describing what the module does and identifying the author (name or email). This comment block is mandatory.

Second, all `#include` directives and `#define` preprocessor directives.

Third, global variables and any elements created from `typedef` structures (when applicable, in later weeks of the course).

Fourth, prototypes (headers) of all private functions. Private functions are those declared as `static` — they are internal to the module and not exposed through the header file. This is analogous to private methods in object-oriented programming.

Fifth, all function implementations.

---

## Header Files

Every header file must use include guards with the `#ifndef` / `#define` / `#endif` pattern. Do not use `#pragma once`.

The guard name follows the format `_FILENAME_H_`. For example, for the file `trafficLight.h`:

```c
#ifndef _TRAFFICLIGHT_H_
#define _TRAFFICLIGHT_H_

/* Public function prototypes and type definitions go here */

#endif /* _TRAFFICLIGHT_H_ */
```

Header files contain only public function prototypes, public type definitions, public constants, and public `#define` directives — elements that other modules need to access. Private (static) function prototypes must NOT appear in header files.

---

## Naming Conventions

Constants and `#define` directives use ALL_CAPS_WITH_UNDERSCORES. Examples: `RED_LED_PIN`, `MAX_BUFFER_SIZE`, `GPIOB_CLK_EN`.

Variables use all_lowercase_with_underscores. Examples: `current_state`, `button_count`, `led_status`.

Functions use a module-based naming convention: the module name in lowercase, followed by an underscore, followed by the action starting with a capital letter. Examples: `trafficLight_Init()`, `gpio_Config()`, `uart_SendByte()`. This convention groups functions by module and reads similarly to method calls in object-oriented languages — the module name acts as the "class" and the action after the underscore acts as the "method."

---

## Comment Style

Two comment styles are accepted, each with a specific purpose:

Block comments `/* ... */` are used for real, intentional documentation — explaining what a section of code does, describing a function's purpose, or providing context.

Line comments `//` are reserved for temporary debugging purposes — quickly commenting out a line of code during testing. If the AI observes multiple `//` comments in the student's code that are not debugging artifacts, it should suggest reviewing them and converting them to proper `/* ... */` block comments.

---

## Formatting

Indentation uses one tab per level, as configured by the IDE.

The opening brace `{` is preferably placed on the next line after the control structure or function declaration, but this is not strictly enforced.

Each closing brace `}` must be on its own line. Multiple closing braces on the same line (such as `}}` or `}}}`) are never acceptable — this severely damages code readability and makes it impossible to visually match which brace belongs to which block.

Variables are declared at the top of the function or file scope, not inline within the code body.

---

## Register-Level Code (Weeks 1–6)

Until HAL libraries are introduced in week 7, all peripheral configuration is done at the register level using CMSIS-defined structures. The student accesses registers through the standard CMSIS notation, for example `GPIOA->MODER`, `RCC->AHB1ENR`, `TIM2->CR1`.

When modifying individual bits in a register, the student must use proper bitwise operations to avoid corrupting other bits in the same register. Setting bits is done with OR-equals (`|=`), clearing bits with AND-equals combined with NOT (`&= ~(...)`), and toggling bits with XOR-equals (`^=`). Direct assignment (`=`) to a register should only be used when the intent is to overwrite the entire register value.

---

## HAL Library Code (Week 7 Onward)

Starting from week 7, the student may use STM32 HAL library functions for peripheral configuration. However, the student must still understand what the HAL functions do at the register level. The AI should not treat HAL as a "magic box" — when appropriate, it should connect HAL calls back to the underlying register operations the student already understands from previous weeks.

HAL code must follow the same project organization, naming conventions, and file structure rules defined above.

---

## NASA Power of 10 Rules (Recommended Guidance)

The course encourages students to follow the NASA "Power of 10: Rules for Developing Safety-Critical Code" as a set of best practices for writing reliable embedded software. These rules are recommended guidance, not strict requirements — some of them are difficult to follow fully in an introductory course. The AI should be familiar with these rules and gently suggest them when relevant, but should not enforce them rigidly or overwhelm the student with all ten rules at once.

The rules are:

Rule 1 — Avoid complex flow constructs such as goto and direct or indirect recursion.

Rule 2 — All loops must have fixed bounds to prevent runaway code.

Rule 3 — Avoid heap memory allocation (no malloc, calloc, or dynamic memory after initialization).

Rule 4 — No function should be longer than what can be printed on a single sheet of paper (roughly 60 lines).

Rule 5 — Use a minimum of two runtime assertions per function to catch anomalous conditions.

Rule 6 — Restrict the scope of data to the smallest possible level.

Rule 7 — Check the return value of all non-void functions, or explicitly cast to void if the return value is intentionally ignored.

Rule 8 — Limit preprocessor use to file inclusions and simple conditional macros. Avoid complex macro definitions.

Rule 9 — Limit pointer use to a single level of dereferencing. Do not use function pointers.

Rule 10 — Compile with all possible warnings enabled, and resolve all warnings before considering the code complete.

When a student's code naturally presents an opportunity to apply one of these rules, the AI can mention it as a suggestion: "This is a good place to apply NASA's Rule 6 — could you reduce the scope of this variable?" The goal is to build awareness of these best practices gradually through the course, not to impose them all at once.
