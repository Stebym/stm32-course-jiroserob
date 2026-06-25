# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Target hardware

- Board: **NUCLEO-F411RE** (STM32F411RETx — Cortex-M4, 512 KB Flash, 128 KB RAM)
- Toolchain: **GNU Tools for STM32 13.3.rel1** (`arm-none-eabi-gcc`)
- CMSIS/device headers: `/home/jimmy/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0/`

## Build

The build system is a set of generated makefiles living inside `Debug/`. Build from that directory:

```bash
# Build (from repo root)
make -C Debug/

# Clean
make -C Debug/ clean
```

Output artifact: `Debug/semana-01.elf` (plus `.map` and `.list`).

Key compiler flags used (from `Debug/Src/subdir.mk`):
- `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb`
- `-std=gnu11 -O0 -g3`
- Defines: `DEBUG NUCLEO_F411RE STM32 STM32F4 STM32F411RETx STM32F411xE`
- Include: `Inc/` + CMSIS Core + STM32F4xx device headers

Flashing is done through STM32CubeIDE (`.launch` files) or OpenOCD with an ST-Link probe.

## Code architecture

This is a **bare-metal** project — no HAL, no RTOS, no STM32Cube middleware. All peripheral access is direct register manipulation via CMSIS device headers (`stm32f4xx.h`).

### Execution model

`main()` performs hardware init then enters `while(1) { __NOP(); }`. All application logic runs inside ISRs:

| ISR | Source | Purpose |
|-----|--------|---------|
| `EXTI0_IRQHandler` | PA0 rising edge | Sensor 1 → increment counter |
| `EXTI1_IRQHandler` | PA1 falling edge | Sensor 2 → decrement counter |
| `TIM3_IRQHandler` | TIM3 @ 2 ms | Multiplex 4 display digits + decrement debounce timer |

### Display multiplexing

`slot_matrix[4]` holds the four individual digits of `global_counter` (split by `split_matrix_val()`). TIM3 fires every 2 ms and cycles `active_slot` 0–3, enabling one PNP transistor at a time while `apply_segment_mask()` drives the segment lines. The display uses **common-anode** logic (active LOW on segments and digit enables).

### Debounce

`deadtime_buffer` is set to 60 in either EXTI ISR and decremented by 1 each TIM3 tick (every 2 ms), giving a ~120 ms lockout window after any sensor event.

### Peripheral setup functions

All init is split into three functions called at the top of `main()`:
- `sys_setup_io()` — GPIO MODER/PUPDR for segment lines, digit-select transistors, and sensor inputs
- `sys_setup_exti()` — SYSCFG routing, EXTI RTSR/FTSR/IMR, NVIC enable
- `sys_setup_timer3()` — TIM3 PSC/ARR for 2 ms period, UIE enable, NVIC enable

### Segment–GPIO mapping

Segment lines are spread across four ports (PA11/PA12, PB12, PC0/PC2/PC11/PC12, PD2). Digit-select transistors are on PB13, PB14, PC10, PC13. The mapping is encoded directly in `apply_segment_mask()` as a switch-case lookup.

## File layout

```
Src/
  main.c          — application logic (only file to edit for coursework)
  syscalls.c      — newlib syscall stubs (do not modify)
  sysmem.c        — heap sbrk stub (do not modify)
Inc/              — project headers (currently empty)
Startup/
  startup_stm32f411retx.s — vector table + Reset_Handler
STM32F411RETX_FLASH.ld    — linker script for flash execution
STM32F411RETX_RAM.ld      — linker script for RAM execution
Debug/            — generated build output (do not hand-edit makefiles)
semaforo.txt      — earlier exercise: traffic-light FSM on PA5/PA6/PA7
```
