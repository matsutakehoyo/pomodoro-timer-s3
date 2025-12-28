# Repository Guidelines

## Project Structure & Module Organization
- `pomodoro-3-v2.ino` is the main Arduino sketch and entry point for the UI and device logic.
- `timer_core.h`/`timer_core.cpp` hold the timer state machine and persistence logic.
- `background.h`, `pomodoro_19.h`, `pomodoro_25.h`, `flower.h`, and `bud.h` are LVGL image assets.
- `pomodoro_symbols.c` defines the custom symbol font used in the UI.

## Build, Test, and Development Commands
- This project does not include build scripts or a CLI wrapper.
- Use the Arduino IDE (or your preferred Arduino build workflow) to compile and upload `pomodoro-3-v2.ino` to a LILYGO T-Display S3 board.
- If you use `arduino-cli`, configure the board/toolchain in your environment and compile the sketch from the project root.

## Coding Style & Naming Conventions
- Language: Arduino/C++ with LVGL and TFT_eSPI.
- Indentation: 2 spaces, no tabs.
- Constants: `UPPER_SNAKE_CASE` (e.g., `PIN_BUTTON_1`).
- Functions and variables: `lowerCamelCase` (e.g., `update_pomodoro_display` is an exception; keep existing names).
- Prefer concise, local comments only where logic is non-obvious.

## Testing Guidelines
- There are no automated tests in this repository.
- Validate changes by flashing to hardware and checking:
  - encoder behavior,
  - button handling,
  - display layout for all timer states.

## Commit & Pull Request Guidelines
- No Git history is available in this repository, so no commit convention is defined.
- Use short, imperative commit messages (e.g., `Add flower/bud cluster visuals`).
- If you open a PR, include:
  - a short description of the change,
  - hardware verification notes,
  - screenshots or photos of the display where relevant.

## Configuration Notes
- Pins and UI layout constants are defined near the top of `pomodoro-3-v2.ino`.
- Update image assets by replacing the corresponding `*.h` files with new LVGL image data.
