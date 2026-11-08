# Computes the software version of the firmware being built, see "Server implementation" under "Part 2: OTA" in
# extras/docs/OTA.md. It saves it as software_version.bin next to firmware.bin, and generates a header that
# only one source file includes, so that a new version doesn't recompile everything. The header also holds the build
# time, which is when this version was first built in this build directory, so that a build with no changes doesn't
# recompile anything. This is a pre: script so that the
# header exists before the sources get compiled, and it applies to all environments through the [env] section.
import hashlib
import os
import re
import subprocess
import time

Import("env")


def git(project_dir, *args):
    return subprocess.run(["git", *args], cwd=project_dir, check=True, capture_output=True).stdout


def compute_software_version(project_dir):
    """Returns the commit's 20-byte SHA-1, followed by a 32-byte SHA-256 of the code changes if there are any."""
    try:
        commit = bytes.fromhex(git(project_dir, "rev-parse", "HEAD").decode().strip())
        diff = git(project_dir, "diff", "HEAD", "--binary", "--no-ext-diff")
        untracked = git(project_dir, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0")
    except (OSError, subprocess.CalledProcessError) as err:
        print("Warning: not using a software version because git failed: %s" % err)
        return bytes(20)
    untracked = sorted(path for path in untracked if path)
    if not diff and not untracked:
        return commit
    changes = hashlib.sha256(diff)
    for path in untracked:
        changes.update(b"\0" + path + b"\0")
        try:
            with open(os.path.join(os.fsencode(project_dir), path), "rb") as f:
                changes.update(f.read())
        except OSError:
            pass  # This could be a broken symbolic link or a file that was just deleted.
    return commit + changes.digest()


def write_if_changed(path, contents):
    try:
        with open(path, "rb") as f:
            if f.read() == contents:
                return
    except OSError:
        pass
    with open(path, "wb") as f:
        f.write(contents)


project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
generated_dir = os.path.join(build_dir, "generated")
os.makedirs(generated_dir, exist_ok=True)
software_version = compute_software_version(project_dir)
write_if_changed(os.path.join(build_dir, "software_version.bin"), software_version)
header_path = os.path.join(generated_dir, "generated_software_version.h")
header = '#define JL_SOFTWARE_VERSION_BYTES %s\n#define JL_BUILD_ENVIRONMENT "%s"\n' % (
    ",".join("0x%02x" % b for b in software_version),
    env.subst("$PIOENV"),
)
build_time = None
try:
    with open(header_path, encoding="utf-8") as f:
        previous = f.read()
    if previous.startswith(header):
        match = re.search(r"^#define JL_BUILD_TIME ([0-9]+)$", previous, re.MULTILINE)
        build_time = int(match.group(1)) if match else None
except OSError:
    pass
if build_time is None:
    build_time = int(time.time())
write_if_changed(header_path, (header + "#define JL_BUILD_TIME %d\n" % build_time).encode())
env.Append(SRC_BUILD_FLAGS=["-I" + generated_dir])
