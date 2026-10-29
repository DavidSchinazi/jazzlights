# Enables link-time optimization for our sources and libraries. build_src_flags covers our sources, and this script
# adds -flto to each library's environment. ESP-IDF components and the bootloader keep their regular objects, because
# ESP-IDF's linker fragments place code in IRAM by archive name, which LTO objects don't keep.
Import("env")

for lib_builder in env.GetLibBuilders():
    lib_builder.env.Append(CCFLAGS=["-flto=auto"])

# The prebuilt Arduino ld_flags contain -fno-lto, which makes the GCC driver drop the linker plugin even when -flto
# comes later on the command line.
env["LINKFLAGS"] = [flag for flag in env["LINKFLAGS"] if flag != "-fno-lto"]
env.Append(LINKFLAGS=["-flto=auto"])

# Archives of LTO objects need gcc-ar so that their symbol index includes LTO symbols. ESP-IDF builds already use it.
ar = env.subst("$AR")
if ar.endswith("-ar") and not ar.endswith("-gcc-ar"):
    env.Replace(AR=ar[: -len("-ar")] + "-gcc-ar", RANLIB=ar[: -len("-ar")] + "-gcc-ranlib")
