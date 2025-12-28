# Pomodoro Timer (LILYGO T-Display S3)

A custom Pomodoro timer built for the LILYGO T-Display S3 with an EC11 rotary encoder, encoder button, and the two on-board buttons. The UI uses LVGL with a tomato-tree visual progress display.

## Features
- Pomodoro work/break cycles with on-screen progress
- Task-based tomato clusters and visual indicators
- Rotary encoder + button input
- On-board button support

## Hardware
- Board: LILYGO T-Display S3
- Encoder: EC11 quadrature encoder

### Wiring
- EC11 A (CLK)  -> GPIO 21  
- EC11 B (DT)   -> GPIO 18  
- EC11 Common   -> GND  
- EC11 Switch   -> GPIO 16 / GND  
- Button 1      -> GPIO 0  
- Button 2      -> GPIO 14  

## Build & Upload
Use the Arduino IDE (or your preferred workflow) to open and upload:
- `pomodoro-timer-s3.ino`

Libraries used (install via Library Manager or your toolchain):
- `TFT_eSPI`
- `Button2`
- `RotaryEncoder`
- `lvgl`

## Project Layout
- `pomodoro-timer-s3.ino` main sketch
- `timer_core.h` / `timer_core.cpp` timer state machine and persistence
- `background.h`, `pomodoro_19.h`, `pomodoro_25.h`, `flower.h`, `bud.h` LVGL image assets
- `pomodoro_symbols.c` custom symbol font

## Notes
- Pin assignments and UI layout constants are near the top of the main sketch.
- Replace `*.h` image assets with new LVGL exports to update visuals.
