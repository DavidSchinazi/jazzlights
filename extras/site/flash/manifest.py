#!/usr/bin/env python3

# Creates a manifest.json suitable for ESP Web Tools.

import argparse
import json

parser = argparse.ArgumentParser()
parser.add_argument("--output", default="-")
parser.add_argument("--name", default="JazzLights")
parser.add_argument("--version", default="0")
parser.add_argument("--firmware")
parser.add_argument("--firmware-s3")
parser.add_argument("--firmware-c3")
parser.add_argument("--firmware-c6")
# Offers to erase the device before installing, for firmware whose flash layout differs from JazzLights'.
parser.add_argument("--prompt-erase", action="store_true")
# How long to wait for the firmware to answer Improv after installing, so ESP Web Tools can set up Wi-Fi. 0 skips it,
# since JazzLights doesn't support Improv.
parser.add_argument("--improv-wait-time", type=int, default=0)
args = parser.parse_args()

manifest = {}
manifest["name"] = args.name
manifest["version"] = args.version
manifest["new_install_prompt_erase"] = args.prompt_erase
manifest["new_install_improv_wait_time"] = args.improv_wait_time
manifest["builds"] = []
if args.firmware:
    manifest["builds"].append(
        {"chipFamily": "ESP32", "parts": [{"path": args.firmware, "offset": 0}]}
    )
if args.firmware_s3:
    manifest["builds"].append(
        {"chipFamily": "ESP32-S3", "parts": [{"path": args.firmware_s3, "offset": 0}]}
    )
if args.firmware_c3:
    manifest["builds"].append(
        {"chipFamily": "ESP32-C3", "parts": [{"path": args.firmware_c3, "offset": 0}]}
    )
if args.firmware_c6:
    manifest["builds"].append(
        {"chipFamily": "ESP32-C6", "parts": [{"path": args.firmware_c6, "offset": 0}]}
    )
if not args.firmware and not args.firmware_s3 and not args.firmware_c3 and not args.firmware_c6:
    manifest["builds"].append(
        {"chipFamily": "ESP32", "parts": [{"path": "firmware-merged.bin", "offset": 0}]}
    )


output_str = json.dumps(manifest, sort_keys=True, indent=4)

if args.output != "-":
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(output_str)
else:
    print(output_str)
