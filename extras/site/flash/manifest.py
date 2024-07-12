#!/usr/bin/python3

# Creates a manifest.json suitable for ESP Web Tools.

import argparse
import json

parser = argparse.ArgumentParser()
parser.add_argument("--output", default="-")
parser.add_argument("--name", default="JazzLights")
parser.add_argument("--version", default="0")
parser.add_argument("--firmware", default="firmware-merged.bin")
args = parser.parse_args()

manifest = {}
manifest["name"] = args.name
manifest["version"] = args.version
manifest["new_install_prompt_erase"] = False
manifest["new_install_improv_wait_time"] = 0
manifest["builds"] = [
    {"chipFamily": "ESP32", "parts": [{"path": args.firmware, "offset": 0}]}
]

output_str = json.dumps(manifest, sort_keys=True, indent=4)

if args.output != "-":
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(output_str)
else:
    print(output_str)
