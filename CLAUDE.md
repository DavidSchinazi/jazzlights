# Build and formatting

- This is a PlatformIO project (`platformio.ini` defines ~90 environments).
- After making code changes, always run, in order:
  1. `./check_format.sh --fix` — auto-fixes formatting with clang-format.
  2. `pio run -e vest` — compiles the `vest` environment as the standard build check.
  3. Also build `extras/` (see `.github/workflows/jazzlights.yml`'s `extras` job):
     ```
     cd extras
     cmake -S . -B build
     cmake --build build
     ```
     This builds with `-Wall -Wextra -Werror`, catching things `pio run -e vest` won't (e.g. unused
     parameters), and its `demo`/`bench` binaries call into `src/jazzlights` directly with their own
     call sites — signature changes there need updating too.
