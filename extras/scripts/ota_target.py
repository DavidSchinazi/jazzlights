# Adds an "OTA Upload" task to every ESP32 environment, which the pioarduino extension shows under Project Tasks, then
# the environment, then Custom. It builds that environment, then updates every device on the network that runs it,
# using ota_upload.py, see "Client implementation" under "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md. The private key
# comes from the JL_CLIENT_KEY environment variable.
#
# From the command line: `pio run -e vest -t ota`. To update only one device, give its IP address with
# `--upload-port 10.41.41.21` or JL_OTA_HOST.
#
# PlatformIO's own Python doesn't have the cryptography package, so this runs ota_upload.py with uv instead.
import ipaddress
import os
import shutil
import subprocess

Import("env")


def find_uv():
    found = shutil.which("uv")
    if found:
        return found
    # VS Code might not have the shell's PATH, so also look where uv usually gets installed.
    for candidate in ("~/.local/bin/uv", "~/.cargo/bin/uv", "/opt/homebrew/bin/uv", "/usr/local/bin/uv"):
        path = os.path.expanduser(candidate)
        if os.access(path, os.X_OK):
            return path
    return None


def ota_host(env):
    """Returns the IP address from --upload-port or JL_OTA_HOST, if there's one. Most environments set upload_port to
    a pattern for serial ports, which this ignores."""
    for value in (env.get("UPLOAD_PORT", ""), os.environ.get("JL_OTA_HOST", "")):
        try:
            return str(ipaddress.ip_address(value))
        except ValueError:
            pass
    return None


def ota_upload(target, source, env):  # pylint: disable=unused-argument
    if not os.environ.get("JL_CLIENT_KEY"):
        print("Set JL_CLIENT_KEY to the path of the private key, such as ~/.config/jazzlights/client_private_1.pem")
        return 1
    uv = find_uv()
    if uv is None:
        print("OTA uploads need uv, see https://docs.astral.sh/uv/")
        return 1
    project_dir = env.subst("$PROJECT_DIR")
    command = [
        uv,
        "run",
        "--project",
        project_dir,
        "python",
        os.path.join(project_dir, "extras", "scripts", "ota_upload.py"),
        "--environment",
        env.subst("$PIOENV"),
        "--no-build",
    ]
    host = ota_host(env)
    command += ["--host", host] if host else ["--once"]
    return subprocess.call(command, cwd=project_dir)


if env.get("PIOPLATFORM") == "espressif32":
    env.AddCustomTarget(
        name="ota",
        dependencies="$BUILD_DIR/${PROGNAME}.bin",
        actions=ota_upload,
        title="OTA Upload",
        description="Build, then update every device on the network running this environment",
    )
