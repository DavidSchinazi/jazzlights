# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "esptool",
#     "pyserial",
#     "rich",
#     "typer",
# ]
# ///
#
# You can run this script with `python multiflash.py .pio/build/creature_c6` (or whatever your build dir is).
# To setup env, run uv init
# uv sync --dev
# source .venv/bin/activate
#
# or run ephemeral with:
# uv run --dev multiflash.py .pio/build/creature_c6
#
# To make `.pio/build/creature_c6`,
# run `pio run -e creature_c6` from the root
from pathlib import Path
import subprocess
import threading
import time

from rich.console import Console
from rich.table import Table
import serial.tools.list_ports
import typer

# --- CONFIGURATION ---
BAUD_RATE = "1500000"
POLL_INTERVAL = 1.0
TARGET_VID = 0x303A  # Espressif's registered Vendor ID

processed_ports = set()
console = Console()


def print_port_info_table(new_port_objects):
    """Print a rich table of newly connected USB/Serial devices."""
    table = Table(
        title="New Espressif Devices Detected", header_style="bold cyan", expand=True
    )

    table.add_column("Device", style="green", no_wrap=True)
    table.add_column("Name")
    table.add_column("Description")
    table.add_column("HWID", overflow="ellipsis")
    table.add_column("VID", justify="right")
    table.add_column("PID", justify="right")
    table.add_column("Location")
    table.add_column("Manufacturer")
    table.add_column("Product")

    for p in new_port_objects:
        vid = f"0x{p.vid:04X}" if p.vid else "N/A"
        pid = f"0x{p.pid:04X}" if p.pid else "N/A"

        table.add_row(
            str(p.device),
            str(p.name) if p.name else "N/A",
            str(p.description) if p.description else "N/A",
            str(p.hwid) if p.hwid else "N/A",
            vid,
            pid,
            str(p.location) if p.location else "N/A",
            str(p.manufacturer) if p.manufacturer else "N/A",
            str(p.product) if p.product else "N/A",
        )

    console.print(table)
    console.print()


def flash_device(port: str, build_dir: Path):
    # Escaped brackets with \\
    console.print(f"[bold yellow]\\[{port}][/bold yellow] Starting flash...")

    command = [
        "esptool",
        "--chip",
        "auto",
        "--port",
        port,
        "--baud",
        BAUD_RATE,
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "write-flash",
        "-z",
        "--flash-mode",
        "dio",
        "--flash-freq",
        "80m",
        "--flash-size",
        "detect",
        "0x0",
        f"{build_dir}/bootloader.bin",
        "0x8000",
        f"{build_dir}/partitions.bin",
        "0x10000",
        f"{build_dir}/firmware.bin",
    ]

    try:  # noqa: PLR1702
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )

        for line in process.stdout:
            console.print(
                f"[{port}] {line.strip()}", markup=False, highlight=False
            )  # Standard print doesn't need escaping

        process.wait()

        if process.returncode == 0:
            console.print(
                f"[bold green]\\[{port}] ✅ Successfully flashed![/bold green]"
            )

            # --- Native PySerial Monitor (No Subprocess) ---
            console.print(
                f"[dim]\\[{port}] Starting serial monitor for 5 seconds...[/dim]"
            )

            try:
                # Open the port natively
                with serial.Serial(port, BAUD_RATE, timeout=0.1) as ser:
                    end_time = time.time() + 5.0

                    # Read lines until 5 seconds have passed
                    while time.time() < end_time:
                        line = ser.readline()
                        if line:
                            # Use replace to handle any weird bootloader garbage characters gracefully
                            decoded = line.decode("utf-8", errors="replace").strip()
                            if decoded:
                                console.print(
                                    f"[{port}] {decoded}", markup=False, highlight=False
                                )

            except Exception as e:  # noqa: BLE001
                console.print(f"[bold red]\\[{port}] Monitor error: {e}[/bold red]")

            console.print(
                f"[bold green]\\[{port}] Monitor closed. You can unplug this device.[/bold green]"
            )

        else:
            console.print(
                f"[bold red]\\[{port}] ❌ Flashing failed with return code {process.returncode}.[/bold red]"
            )

    except Exception as e:  # noqa: BLE001
        console.print(f"[bold red]\\[{port}\\] Error running esptool: {e}[/bold red]")


def main(
    build_dir: Path = typer.Argument(
        ...,
        help="Path to the PlatformIO build directory (e.g., .pio/build/creature_c6)",
    ),
):
    """Auto-flasher for ESP32 devices. Continuously monitors for new devices and flashes them."""
    console.print("[bold blue]Starting ESP32 Auto-Flasher...[/bold blue]")
    console.print(f"Target Build Directory: [bold white]{build_dir}[/bold white]")

    required_files = ["bootloader.bin", "partitions.bin", "firmware.bin"]
    missing_files = [f for f in required_files if not (build_dir / f).exists()]
    if missing_files:
        console.print(
            f"[bold red]Error: Missing required files in {build_dir}: {', '.join(missing_files)}. Please build the project first.[/bold red]"
        )
        return

    console.print(
        f"Waiting for devices (Target VID: 0x{TARGET_VID:04X}). Press [bold white]Ctrl+C[/bold white] to exit.\n"
    )

    while True:
        try:
            current_port_objects = serial.tools.list_ports.comports()
            current_port_names = {p.device for p in current_port_objects}

            # Handle unplugged devices
            unplugged = processed_ports - current_port_names
            if unplugged:
                for port in unplugged:
                    console.print(
                        f"[dim]\\[{port}] Device unplugged. Ready for next device on this port.[/dim]"
                    )
                    processed_ports.remove(port)

            # Handle newly plugged-in devices
            new_port_names = current_port_names - processed_ports
            if new_port_names:
                new_objects = [
                    p for p in current_port_objects if p.device in new_port_names
                ]
                objects_to_flash = []

                for p in new_objects:
                    # Immediately mark as processed to prevent re-evaluation
                    processed_ports.add(p.device)

                    # Filter by Target VID
                    if p.vid == TARGET_VID:
                        objects_to_flash.append(p)
                    else:
                        vid_str = f"0x{p.vid:04X}" if p.vid else "N/A"
                        console.print(
                            f"[dim]\\[{p.device}] Ignored device. VID is {vid_str}, expected 0x{TARGET_VID:04X}.[/dim]"
                        )

                if objects_to_flash:
                    print_port_info_table(objects_to_flash)

                    for port_obj in objects_to_flash:
                        t = threading.Thread(
                            target=flash_device, args=(port_obj.device, build_dir)
                        )
                        t.daemon = True
                        t.start()

            time.sleep(POLL_INTERVAL)

        except KeyboardInterrupt:  # noqa: PERF203
            console.print("\n[bold blue]Exiting Auto-Flasher. Goodbye![/bold blue]")
            break
        except Exception as e:  # noqa: BLE001
            console.print(f"[bold red]Loop error: {e}[/bold red]")
            time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    typer.run(main)
