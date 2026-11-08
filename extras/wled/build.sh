#!/bin/bash
# Builds WLED with the JazzLights usermod, see extras/docs/WLED.md.
#
# Usage: extras/wled/build.sh [PlatformIO arguments...]
# For example: extras/wled/build.sh -e jazzlights_atom_s3_lite -t upload -t monitor
# Without arguments, this builds every environment in platformio_override.ini.
# Run `-t erase` on its own: WLED's PlatformIO platform turns off library dependency resolution whenever erase is among
# the targets, so building in the same command fails to find WiFi.h.
#
# The first run clones WLED at WLED_COMMIT into extras/.wled, and every run copies platformio_override.ini there. The
# checkout must not be inside extras/wled, or PlatformIO would see WLED's own libraries as part of the usermod.
# WLED's build bundles its web UI with Node.js, which must be on the PATH.

set -euo pipefail

# The WLED commit we build against. Update this deliberately, since WLED's usermod API changes over time, and then
# regenerate test/test_wled/wled_data.h with extras/wled/generate_test_data.py.
WLED_COMMIT=fffa05a765d41d7126b66ab5a3d1ce1f41e7d11e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WLED_DIR="${SCRIPT_DIR}/../.wled"

if ! command -v node >/dev/null; then
  echo "Node.js is required to build WLED's web UI, please add it to your PATH." >&2
  exit 1
fi

if [ ! -d "${WLED_DIR}/.git" ]; then
  git init --quiet "${WLED_DIR}"
  git -C "${WLED_DIR}" remote add origin https://github.com/wled/WLED.git
fi
if [ "$(git -C "${WLED_DIR}" rev-parse --verify --quiet HEAD || true)" != "${WLED_COMMIT}" ]; then
  git -C "${WLED_DIR}" fetch --quiet --depth 1 origin "${WLED_COMMIT}"
  git -C "${WLED_DIR}" checkout --quiet --detach "${WLED_COMMIT}"
fi

cp "${SCRIPT_DIR}/platformio_override.ini" "${WLED_DIR}/platformio_override.ini"

# WLED's PlatformIO platform installs a different Arduino core under the same package name as the one this repo's
# platformio.ini uses, so sharing ~/.platformio would make each build replace the other's. Keep WLED's packages apart.
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-${WLED_DIR}/.platformio}"

cd "${WLED_DIR}"
if [ $# -eq 0 ]; then
  pio run
else
  pio run "$@"
fi
