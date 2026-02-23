#!/usr/bin/env python3

# This was meant to help create sdkconfig.defaults but it didn't end up very useful.
# Hitting 'D' in `menuconfig` saves a minimal option, that's much better than this script.

import sys


def load_clean_config(filepath):
    """Reads a file, strips whitespace, and ignores comments/empty lines."""
    cleaned = []
    seen = set()
    try:
        with open(filepath, "r") as f:
            for raw_line in f:
                content = raw_line.strip()
                if content and not content.startswith("#"):
                    # We use a set 'seen' to ensure we don't add
                    # duplicate lines from within the same file
                    if content not in seen:
                        cleaned.append(content)
                        seen.add(content)
        return cleaned, seen
    except FileNotFoundError:
        print(f"Error: Could not find file {filepath}")
        sys.exit(1)


def compare_configs(f1_path, f2_path, out_shared, out_only1, out_only2):
    # list1 maintains order; set1 and set2 allow for fast comparison
    list1, set1 = load_clean_config(f1_path)
    list2, set2 = load_clean_config(f2_path)

    for line in list1:
        if line in set2:
            print("SHARED: " + line)
        else:
            print("ONLY 1: " + line)

    # 1. Shared: Items in list1 that also exist in set2 (Order of File 1)
    shared = [line for line in list1 if line in set2]

    # 2. Only File 1: Items in list1 that are NOT in set2 (Order of File 1)
    only1 = [line for line in list1 if line not in set2]

    # 3. Only File 2: Items in list2 that are NOT in set1 (Order of File 2)
    only2 = [line for line in list2 if line not in set1]

    # Writing outputs
    files_to_write = [(out_shared, shared), (out_only1, only1), (out_only2, only2)]

    for filename, data in files_to_write:
        with open(filename, "w") as f:
            for line in data:
                f.write(f"{line}\n")
        print(f"Created: {filename} ({len(data)} lines)")


if __name__ == "__main__":
    if len(sys.argv) != 6:
        print(
            "Usage: python script.py <in1> <in2> <out_shared> <out_only1> <out_only2>"
        )
    else:
        compare_configs(*sys.argv[1:])
