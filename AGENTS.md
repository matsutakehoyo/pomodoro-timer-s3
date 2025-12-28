# Repository Guidelines

## Project Structure & Module Organization
- `pomodoro-timer-s3.ino` is the main Arduino sketch and UI entry point.
- `timer_core.h` and `timer_core.cpp` implement timer state, settings, and persistence.
- `background.h`, `pomodoro_19.h`, `pomodoro_25.h`, `flower.h`, and `bud.h` store LVGL image assets.
- `pomodoro_symbols.c` defines the custom symbol font used in the UI.
- `README.md` covers hardware, wiring, and build notes.

## Build, Test, and Development Commands
- Arduino IDE: open `pomodoro-timer-s3.ino`, select the LILYGO T-Display S3 board, then upload.
- `arduino-cli compile --fqbn <board-fqbn> .` builds the sketch from the repo root.
- `arduino-cli upload --fqbn <board-fqbn> -p <serial-port> .` uploads to the device.

## Coding Style & Naming Conventions
- Language: Arduino/C++ with LVGL and TFT_eSPI.
- Indentation: 2 spaces; avoid tabs.
- Constants: `UPPER_SNAKE_CASE` (e.g., `PIN_ENC_A`).
- Functions/variables: `lowerCamelCase` (e.g., `updatePomodoroDisplay`), but keep existing naming as-is to avoid churn.
- Comments should be short and only for non-obvious logic.

## Testing Guidelines
- No automated tests are currently configured.
- Validate changes by flashing hardware and checking:
  - encoder direction and button presses,
  - work/break state transitions,
  - tomato/flower/bud rendering across tasks.

## Commit & Pull Request Guidelines
- Current commit history uses short, imperative messages (e.g., `Initial commit`, `Add README`).
- Keep commits focused on a single change when possible.
- PRs should include a brief summary, hardware verification notes, and photos of display changes when UI is affected.

## Configuration Notes
- Pin mappings and UI layout constants live near the top of `pomodoro-timer-s3.ino`.
- Replace image assets by exporting new LVGL `*.h` files with matching dimensions and `LV_IMG_CF_RGB565A8`.
