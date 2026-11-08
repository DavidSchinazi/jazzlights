#!/usr/bin/env python3
"""Generates test/test_wled/wled_data.h from a WLED checkout, see extras/docs/WLED.md.

The native tests of the WLED mapping use WLED's effect names and built-in palettes, to check the effect IDs and to
print the mapping of every effect and palette for review. WLED's sources aren't available to native tests, so this
extracts that data from the WLED checkout made by extras/wled/build.sh.

Usage: extras/wled/generate_test_data.py [WLED checkout, default extras/.wled]
"""

import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
WLED_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO_DIR, "extras", ".wled")
OUTPUT = os.path.join(REPO_DIR, "test", "test_wled", "wled_data.h")


def read(relative_path):
    with open(os.path.join(WLED_DIR, "wled00", relative_path), encoding="utf-8") as source:
        return source.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def effect_names():
    fx_h = read("FX.h")
    raw_fx_cpp = read("FX.cpp")
    # Only strip comments line by line, since block comment markers can appear in string literals.
    fx_cpp = "\n".join(line for line in raw_fx_cpp.splitlines() if not line.strip().startswith("//"))
    ids = {name: int(value) for name, value in re.findall(r"#define\s+FX_MODE_(\w+)\s+(\d+)", fx_h)}
    names = dict(re.findall(r'_data_FX_MODE_(\w+)\[\]\s*PROGMEM\s*=\s*"([^"@;]*)', raw_fx_cpp))
    # Solid is pushed directly into the list of modes, rather than through addEffect().
    effects = {ids["STATIC"]: names["STATIC"]}
    for mode, data in re.findall(r"addEffect\(\s*FX_MODE_(\w+)\s*,[^,]*,\s*_data_FX_MODE_(\w+)\s*\)", fx_cpp):
        effects[ids[mode]] = names[data]
    return sorted(effects.items())


def named_colors():
    fastled_slim = read("src/dependencies/fastled_slim/fastled_slim.h")
    return {name: int(value, 16) for name, value in re.findall(r"\b(\w+)\s*=\s*0x([0-9A-Fa-f]{6})\b", fastled_slim)}


def color_value(token, colors):
    token = token.strip()
    if token.startswith("CRGB::"):
        return colors[token[len("CRGB::"):]]
    return int(token, 0)


def expand_gradient(data):
    # Mirrors CRGBPalette16::loadDynamicGradientPalette() and fill_gradient_RGB() in WLED's fastled_slim.
    entries = [data[i : i + 4] for i in range(0, len(data), 4)]
    count = 0
    while True:
        count += 1
        if entries[count - 1][0] == 255:
            break
    colors = [(0, 0, 0)] * 16
    last_slot_used = -1
    position = 0
    rgb_start = tuple(entries[0][1:4])
    index_start = 0
    while index_start < 255:
        position += 1
        index_end = entries[position][0]
        rgb_end = tuple(entries[position][1:4])
        start = index_start // 16
        end = index_end // 16
        if count < 16:
            if start <= last_slot_used < 15:
                start = last_slot_used + 1
                end = max(end, start)
            last_slot_used = end
        divisor = max(end - start, 1)
        deltas = [((rgb_end[c] - rgb_start[c]) << 16) // divisor if rgb_end[c] >= rgb_start[c]
                  else -((-(rgb_end[c] - rgb_start[c]) << 16) // divisor) for c in range(3)]
        shifted = [rgb_start[c] << 16 for c in range(3)]
        for slot in range(start, end + 1):
            colors[slot] = tuple(value >> 16 for value in shifted)
            shifted = [shifted[c] + deltas[c] for c in range(3)]
        index_start = index_end
        rgb_start = rgb_end
    return [(r << 16) | (g << 8) | b for r, g, b in colors]


def palettes():
    palettes_cpp = strip_comments(read("palettes.cpp"))
    raw_palettes_cpp = read("palettes.cpp")
    colors = named_colors()
    fastled = {}
    for name, body in re.findall(r"TProgmemRGBPalette16\s+(\w+)\s+PROGMEM\s*=\s*\{(.*?)\};", palettes_cpp, re.DOTALL):
        fastled[name] = [color_value(token, colors) for token in body.split(",") if token.strip()]
    gradients = {}
    for name, body in re.findall(r"const (?:uint8_t|byte)\s+(\w+)\[\]\s+PROGMEM\s*=\s*\{(.*?)\};", palettes_cpp, re.DOTALL):
        gradients[name] = [int(token, 0) for token in body.split(",") if token.strip()]
    fastled_order = re.findall(r"&(\w+)", re.search(r"fastledPalettes\[\]\s*PROGMEM\s*=\s*\{(.*?)\};", palettes_cpp,
                                                     re.DOTALL).group(1))
    gradient_order = re.findall(r"(\w+_gp)", re.search(r"gGradientPalettes\[\]\s*PROGMEM\s*=\s*\{(.*?)\};",
                                                        raw_palettes_cpp, re.DOTALL).group(1))
    names = re.findall(r'"([^"]*)"', re.search(r"JSON_palette_names\[\]\s*PROGMEM\s*=\s*R\"=====\((.*?)\)=====\"",
                                               read("FX_fcn.cpp"), re.DOTALL).group(1))
    result = []
    palette_id = 6
    for name in fastled_order:
        result.append((palette_id, names[palette_id], fastled[name]))
        palette_id += 1
    for name in gradient_order:
        result.append((palette_id, names[palette_id], expand_gradient(gradients[name])))
        palette_id += 1
    return result


def main():
    commit = subprocess.run(["git", "-C", WLED_DIR, "rev-parse", "HEAD"], capture_output=True, text=True,
                            check=True).stdout.strip()
    lines = [
        "// Generated by extras/wled/generate_test_data.py from WLED commit %s." % commit,
        "// Do not edit.",
        "",
        "#ifndef JL_TEST_TEST_WLED_WLED_DATA_H",
        "#define JL_TEST_TEST_WLED_WLED_DATA_H",
        "",
        "#include <cstdint>",
        "",
        "struct WLEDEffectName {",
        "  uint8_t id;",
        "  const char* name;",
        "};",
        "",
        "// clang-format off",
        "inline constexpr WLEDEffectName kWLEDEffectNames[] = {",
    ]
    for effect_id, name in effect_names():
        lines.append('    {%d, "%s"},' % (effect_id, name))
    lines += [
        "};",
        "// clang-format on",
        "",
        "struct WLEDBuiltInPalette {",
        "  uint8_t id;",
        "  const char* name;",
        "  uint32_t colors[16];",
        "};",
        "",
        "// WLED's built-in palettes, other than the dynamic ones (IDs 0 to 5), expanded to 16 colors like WLED does.",
        "// clang-format off",
        "inline constexpr WLEDBuiltInPalette kWLEDBuiltInPalettes[] = {",
    ]
    for palette_id, name, colors in palettes():
        lines.append('    {%d, "%s", {%s}},' % (palette_id, name, ", ".join("0x%06X" % c for c in colors)))
    lines += ["};", "// clang-format on", "", "#endif  // JL_TEST_TEST_WLED_WLED_DATA_H", ""]
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as output:
        output.write("\n".join(lines))
    print("wrote %s" % OUTPUT)


main()
