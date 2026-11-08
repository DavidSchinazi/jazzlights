# PlatformIO script for the JazzLights WLED usermod, run by WLED's build through extras/wled/library.json.
# See extras/docs/WLED.md.

Import("projenv")

# Appends _JazzLights to WLED's release name. WLED refuses updates whose release name doesn't match the running one, and
# its one-click update downloads the official image matching the release name. Without this, either would replace a
# build with the usermod with stock WLED. PlatformIO creates the project's build environment, which compiles WLED's own
# sources, before running library scripts, but only compiles those sources afterwards, so changing it here works.
RELEASE_NAME_SUFFIX = "_JazzLights"


def _with_suffix(release_name):
    # The value is a string literal with escaped quotes, such as \"ESP32\".
    if release_name.endswith(RELEASE_NAME_SUFFIX + '\\"'):
        return release_name
    if release_name.endswith('\\"'):
        return release_name[: -len('\\"')] + RELEASE_NAME_SUFFIX + '\\"'
    return release_name + RELEASE_NAME_SUFFIX


def _add_suffix_to_release_name(build_env):
    cppdefines = []
    found_release_name = False
    for define in build_env.get("CPPDEFINES", []):
        if isinstance(define, (tuple, list)) and define[0] == "WLED_RELEASE_NAME":
            define = ("WLED_RELEASE_NAME", _with_suffix(str(define[1])))
            found_release_name = True
        cppdefines.append(define)
    if not found_release_name:
        # WLED uses "Custom" when the environment doesn't set a release name.
        cppdefines.append(("WLED_RELEASE_NAME", '\\"Custom' + RELEASE_NAME_SUFFIX + '\\"'))
    build_env.Replace(CPPDEFINES=cppdefines)


# The project's environment compiles wled_metadata.cpp, which embeds the release name in the image. The global one is
# where WLED's output_bins.py reads the release name to name the image file.
_add_suffix_to_release_name(projenv)
_add_suffix_to_release_name(DefaultEnvironment())
