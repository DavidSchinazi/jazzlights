#!/usr/bin/env python3

# Prints a markdown table of the binary sizes of PlatformIO environments.
#
# The sizes come from the output of `pio run -e <env> -t checkprogsize`, saved as <env>.txt in
# --sizes-dir, which CI produces for every environment it builds. Merged binaries, produced by
# `pio run -t mergebin`, are optionally read from --merged-dir as <env>.bin.

import argparse
import os
import re
import sys

SIZE_LINE_RE = re.compile(
    r"^(RAM|Flash):.*?([\d.]+)% \(used (\d+) bytes from (\d+) bytes\)", re.MULTILINE
)


def parse_sizes(path):
    with open(path, encoding="utf-8") as size_file:
        contents = size_file.read()
    sizes = {}
    for match in SIZE_LINE_RE.finditer(contents):
        sizes[match.group(1)] = (float(match.group(2)), int(match.group(3)))
    for kind in ("RAM", "Flash"):
        if kind not in sizes:
            sys.exit("Failed to find {} size in {}".format(kind, path))
    return sizes


def format_bytes(num_bytes):
    return "{:,} bytes".format(num_bytes)


def format_table(table):
    widths = [max(len(row[i]) for row in table) for i in range(len(table[0]))]

    def format_row(row):
        return "| {} |".format(
            " | ".join(cell.ljust(width) for cell, width in zip(row, widths))
        )

    lines = [
        format_row(table[0]),
        "|{}|".format("|".join("-" * (width + 2) for width in widths)),
    ]
    lines.extend(format_row(row) for row in table[1:])
    return "\n".join(lines)


def main():
    argument_parser = argparse.ArgumentParser(description=__doc__)
    argument_parser.add_argument(
        "--sizes-dir",
        required=True,
        help="directory holding the <env>.txt outputs of `pio run -e <env> -t checkprogsize`",
    )
    argument_parser.add_argument(
        "--merged-dir",
        help="if set, directory holding the <env>.bin merged binaries",
    )
    argument_parser.add_argument(
        "--version",
        help="if set, the revision (such as a commit hash) these sizes were built from",
    )
    argument_parser.add_argument(
        "--title",
        help="if set, emit this as a level-one Markdown heading above the table",
    )
    argument_parser.add_argument(
        "--output",
        default="-",
        help="file to write to (default: - for stdout)",
    )
    arguments = argument_parser.parse_args()

    envs = sorted(
        file_name[: -len(".txt")]
        for file_name in os.listdir(arguments.sizes_dir)
        if file_name.endswith(".txt")
    )
    if not envs:
        sys.exit("Found no sizes in {}".format(arguments.sizes_dir))

    table = [["Environment", "Flash", "RAM", "Merged binary"]]
    for env in envs:
        sizes = parse_sizes(os.path.join(arguments.sizes_dir, env + ".txt"))
        row = ["`{}`".format(env)]
        for kind in ("Flash", "RAM"):
            percent, used = sizes[kind]
            row.append("{} ({:.1f}%)".format(format_bytes(used), percent))
        merged_path = (
            os.path.join(arguments.merged_dir, env + ".bin")
            if arguments.merged_dir
            else None
        )
        if merged_path and os.path.exists(merged_path):
            row.append(format_bytes(os.path.getsize(merged_path)))
        else:
            row.append("—")
        table.append(row)

    paragraphs = []
    if arguments.title:
        paragraphs.append("# {}".format(arguments.title))
    if arguments.version:
        paragraphs.append("Built from revision `{}`.".format(arguments.version))
    paragraphs.append(format_table(table))
    paragraphs.append(
        "Flash is the size of the application image, as a percentage of its partition. RAM is "
        "statically allocated RAM only, and does not include the heap. The merged binary also "
        "includes the bootloader, the partition table and padding, and is only built for "
        "environments that can be flashed from the website."
    )
    output_str = "\n\n".join(paragraphs)
    if arguments.output != "-":
        with open(arguments.output, "w", encoding="utf-8") as output_file:
            output_file.write(output_str + "\n")
    else:
        print(output_str)


if __name__ == "__main__":
    main()
