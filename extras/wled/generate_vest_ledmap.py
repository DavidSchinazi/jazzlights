#!/usr/bin/env python3
"""Generates a WLED ledmap.json for a vest on an ATOM Matrix, see extras/docs/WLED.md.

WLED runs its 2D effects on a rectangular canvas, and a ledmap places each physical LED on a cell of it. This reads
the vest's LED coordinates from src/jazzlights/layout/layout_data_vest.cpp, rounds them to a grid, and places the
ATOM Matrix's 5x5 screen to the right of the vest, in the top rows. The vest's coordinates use halves, but rounding
them to whole cells puts every LED on its own cell.

The physical LED order matches the jazzlights_atom_matrix environment in extras/wled/platformio_override.ini: the
screen's 25 LEDs first, then the vest's 360.

Usage: extras/wled/generate_vest_ledmap.py [--flip-x] [--flip-y] [--no-screen] > ledmap.json
Then upload ledmap.json in WLED's file editor (http://<device>/edit) and reboot.
"""

import argparse
import json
import math
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
VEST_LAYOUT = os.path.join(REPO_DIR, "src", "jazzlights", "layout", "layout_data_vest.cpp")

SCREEN_SIZE = 5


def vest_points():
    with open(VEST_LAYOUT, encoding="utf-8") as layout:
        source = layout.read()
    body = source[source.index("pixelMap[] = {") :]
    body = body[: body.index("};")]
    return [(float(x), float(y)) for x, y in re.findall(r"\{\s*(-?[\d.]+),\s*(-?[\d.]+)\s*\}", body)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--flip-x", action="store_true", help="mirror the vest left to right")
    parser.add_argument("--flip-y", action="store_true", help="mirror the vest top to bottom")
    parser.add_argument("--no-screen", action="store_true", help="leave the ATOM Matrix's 5x5 screen out")
    args = parser.parse_args()

    screen_leds = 0 if args.no_screen else SCREEN_SIZE * SCREEN_SIZE
    points = vest_points()
    # Round halves up, so that the turnaround LEDs at x.5 land on the next column.
    cells = [(math.floor(x + 0.5), math.floor(y + 0.5)) for x, y in points]
    min_x = min(x for x, _ in cells)
    min_y = min(y for _, y in cells)
    cells = [(x - min_x, y - min_y) for x, y in cells]
    vest_width = max(x for x, _ in cells) + 1
    vest_height = max(y for _, y in cells) + 1
    if args.flip_x:
        cells = [(vest_width - 1 - x, y) for x, y in cells]
    if args.flip_y:
        cells = [(x, vest_height - 1 - y) for x, y in cells]

    width = vest_width + (0 if args.no_screen else SCREEN_SIZE)
    height = max(vest_height, 0 if args.no_screen else SCREEN_SIZE)
    grid = [-1] * (width * height)
    for index, (x, y) in enumerate(cells):
        cell = y * width + x
        if grid[cell] != -1:
            sys.exit("Vest LEDs %d and %d land on the same cell (%d, %d)" % (grid[cell] - screen_leds, index, x, y))
        grid[cell] = screen_leds + index
    if not args.no_screen:
        # The screen's LEDs go left to right, then top to bottom.
        for index in range(screen_leds):
            grid[(index // SCREEN_SIZE) * width + vest_width + index % SCREEN_SIZE] = index

    json.dump({"n": "JazzLights vest", "width": width, "height": height, "map": grid}, sys.stdout, separators=(",", ":"))
    sys.stdout.write("\n")
    sys.stderr.write(
        "Canvas %dx%d: vest in columns 0-%d, rows 0-%d, %d LEDs%s\n"
        % (
            width,
            height,
            vest_width - 1,
            vest_height - 1,
            len(points),
            "" if args.no_screen else "; screen in columns %d-%d, rows 0-%d" % (vest_width, width - 1, SCREEN_SIZE - 1),
        )
    )


main()
