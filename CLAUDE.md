# Build and formatting

- This is a PlatformIO project (`platformio.ini` defines ~90 environments).
- After making code changes, always run, in order:
  1. `./check_format.sh --fix` — auto-fixes formatting with clang-format.
  2. `pio run -e vest` — compiles the `vest` environment as the standard build check. For routine
     changes, only build `vest` — do not also compile other pio environments (e.g. `vest_s3`,
     `gauntlet`, `creature`, `orrery_leader`, ...) just to double-check; building multiple
     environments takes too long. It's fine (and worth doing) to additionally build a specific other
     environment when your change is actually specific to it — e.g. code gated behind
     `JL_IS_CONFIG(ORRERY_LEADER)` or `JL_IS_CONTROLLER(CORE2AWS)` — since `vest` won't compile that
     code path at all.
  3. Also build `extras/` (see `.github/workflows/jazzlights.yml`'s `extras` job):
     ```
     cd extras
     cmake -S . -B build
     cmake --build build
     ```
     This builds with `-Wall -Wextra -Werror`, catching things `pio run -e vest` won't (e.g. unused
     parameters), and its `demo`/`bench` binaries call into `src/jazzlights` directly with their own
     call sites — signature changes there need updating too.
