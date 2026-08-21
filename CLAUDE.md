# Build and formatting

- This is a PlatformIO project (`platformio.ini` defines ~90 environments).
- After making code changes, always run, in order:
  1. `./check_format.sh --fix` — auto-fixes formatting with clang-format.
  2. `pio run -e vest` — compiles the `vest` environment as the standard build check.
