# Allows building FastLED and our sources with `framework = espidf` without the Arduino core. See
# extras/fastled_idf_shim/fastled_idf_shim.h for details. This must be a pre: script so that it runs before the ESP-IDF
# builder compiles our sources. We only apply this to FastLED and our sources, because build_flags would also apply to
# ESP-IDF components where the shim breaks the build.
Import("env")
import os

shim_dir = os.path.join(env.subst("$PROJECT_DIR"), "extras", "fastled_idf_shim")
# The Arduino core defines ESP32, which both FastLED and our sources use to detect the platform. FastLED also uses
# F_CPU, which the Arduino core gets from the board definition.
cppdefines = ["ESP32", ("F_CPU", env.BoardConfig().get("build.f_cpu"))]

env.Append(SRC_BUILD_FLAGS=["-D%s" % d if isinstance(d, str) else "-D%s=%s" % d for d in cppdefines] + ["-I" + shim_dir])


def add_shim_to_fastled(env, node):
    # Return a single node rather than a NodeList, since the ESP-IDF builder's own middleware runs after this one.
    return env.Object(
        node,
        CPPDEFINES=list(env.get("CPPDEFINES", [])) + cppdefines,
        CPPPATH=list(env.get("CPPPATH", [])) + [shim_dir],
        CCFLAGS=list(env.get("CCFLAGS", [])) + ["-include", os.path.join(shim_dir, "fastled_idf_shim.h")],
    )[0]


env.AddBuildMiddleware(add_shim_to_fastled, "*/FastLED/*")
