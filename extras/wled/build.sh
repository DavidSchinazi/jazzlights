#!/bin/bash
# Builds WLED with the JazzLights usermod, see extras/docs/WLED.md.
#
# Usage: extras/wled/build.sh [PlatformIO arguments...]
# For example: extras/wled/build.sh -e jazzlights_atom_s3_lite -t upload -t monitor
# Without arguments, this builds every environment in platformio_override.ini.
# Run `-t erase` on its own: WLED's PlatformIO platform turns off library dependency resolution whenever erase is among
# the targets, so building in the same command fails to find WiFi.h.
#
# The first run clones WLED at WLED_COMMIT into extras/.wled, and every run copies platformio_override.ini there.
# WLED's build bundles its web UI with Node.js, which must be on the PATH.

set -euo pipefail

# The WLED commit we build against. Update this deliberately, since WLED's usermod API changes over time, and then
# regenerate test/test_wled/wled_data.h with extras/wled/generate_test_data.py.
WLED_COMMIT=fffa05a765d41d7126b66ab5a3d1ce1f41e7d11e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
WLED_DIR="${REPO_DIR}/extras/.wled"

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

# The usermod is the library declared by library.json at the root of this repository, but the WLED checkout, its build
# folder and its PlatformIO packages are all inside the repository, and PlatformIO would count them as part of a library
# rooted there. So point WLED at a folder of links to the files the library uses, inside the WLED checkout.
USERMOD_DIR="${WLED_DIR}/jazzlights"
mkdir -p "${USERMOD_DIR}/extras"
ln -sfn "${REPO_DIR}/library.json" "${USERMOD_DIR}/library.json"
ln -sfn "${REPO_DIR}/src" "${USERMOD_DIR}/src"
ln -sfn "${REPO_DIR}/extras/wled" "${USERMOD_DIR}/extras/wled"
# Checkouts from before this folder have the usermod linked from extras/wled, which PlatformIO wouldn't update.
for link in "${WLED_DIR}"/.pio/libdeps/*/jazzlights.pio-link; do
  if [ -f "${link}" ] && ! grep -q '"symlink://jazzlights"' "${link}"; then
    rm "${link}"
  fi
done

# WLED's PlatformIO platform installs a different Arduino core under the same package name as the one this repo's
# platformio.ini uses, so sharing ~/.platformio would make each build replace the other's. Keep WLED's packages apart.
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-${WLED_DIR}/.platformio}"

cd "${WLED_DIR}"
if [ $# -eq 0 ]; then
  pio run
else
  pio run "$@"
fi
