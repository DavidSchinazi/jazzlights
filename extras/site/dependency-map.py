#!/usr/bin/env python3

# Generates a dependency map of src/jazzlights and checks architectural invariants.

import argparse
import collections
import html
import json
import os
import re
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

parser = argparse.ArgumentParser()
parser.add_argument("--src", default=os.path.join(REPO_ROOT, "src"))
parser.add_argument("--baseline", default=os.path.join(SCRIPT_DIR, "dependency-baseline.json"))
parser.add_argument("--output", default="-")
parser.add_argument("--check", action="store_true")
parser.add_argument("--update-baseline", action="store_true")
parser.add_argument("--require-graphviz", action="store_true")
parser.add_argument("--dot-binary", default="dot")
parser.add_argument("--css", default="style.css")
parser.add_argument("--embed-css")
parser.add_argument("--version", default="")
args = parser.parse_args()

if args.check and args.update_baseline:
    print("error: --check and --update-baseline are mutually exclusive")
    sys.exit(2)

SITE_URL = "https://davidschinazi.github.io/jazzlights/"
REPO_URL = "https://github.com/DavidSchinazi/jazzlights"

# Colors borrowed from extras/site/style.css so the page matches the rest of the site.
COLOR_BG = "#3b383f"
COLOR_PANEL = "#5c5663"
COLOR_LINK = "#61c0cb"
COLOR_TEXT = "#ffffff"
COLOR_CYCLE = "#e8845b"
COLOR_LEAF = "#3f5c4e"
COLOR_EDGE = "#9a94a1"
FONT = "Verdana,Helvetica,sans-serif"

# Every first-party include in this tree is an unindented #include "jazzlights/...". There are no
# angle-bracket first-party includes, no commented-out includes, and the only #if 0 block in the
# tree (effect/palette.h) contains no includes -- so this single regex is exact. If that ever stops
# being true, this is the place to teach the scanner about the preprocessor.
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"(jazzlights/[^"]+)"')


def module_of(path):
    # src/main.cpp is the only file outside src/jazzlights/, and the four files directly inside
    # src/jazzlights/ (primary_runloop, websocket_server) form the "jazzlights" module.
    if path == "main.cpp":
        return "main"
    parts = path.split("/")
    return parts[1] if len(parts) == 3 else "jazzlights"


def anchor_id(prefix, name):
    return "%s-%s" % (prefix, re.sub(r"[^a-zA-Z0-9]+", "-", name).strip("-"))


def scan_sources(src_root):
    # Returns (sorted file list, sorted edge list). Each edge is (source, target, line number).
    files = []
    edges = []
    for dir_path, dir_names, file_names in os.walk(src_root):
        dir_names.sort()
        for file_name in sorted(file_names):
            if not file_name.endswith((".h", ".cpp")):
                continue
            full_path = os.path.join(dir_path, file_name)
            rel_path = os.path.relpath(full_path, src_root).replace(os.sep, "/")
            files.append(rel_path)
            with open(full_path, encoding="utf-8") as source_file:
                for line_number, line in enumerate(source_file, start=1):
                    match = INCLUDE_RE.match(line)
                    if match:
                        edges.append((rel_path, match.group(1), line_number))
    return sorted(files), sorted(edges)


def strongly_connected_components(nodes, adjacency):
    # Iterative Tarjan, so a deep graph can never blow the recursion limit.
    index_of = {}
    low_of = {}
    on_stack = set()
    stack = []
    components = []
    counter = 0
    for root in nodes:
        if root in index_of:
            continue
        work = [(root, iter(adjacency.get(root, ())))]
        index_of[root] = low_of[root] = counter
        counter += 1
        stack.append(root)
        on_stack.add(root)
        while work:
            node, children = work[-1]
            advanced = False
            for child in children:
                if child not in index_of:
                    index_of[child] = low_of[child] = counter
                    counter += 1
                    stack.append(child)
                    on_stack.add(child)
                    work.append((child, iter(adjacency.get(child, ()))))
                    advanced = True
                    break
                if child in on_stack:
                    low_of[node] = min(low_of[node], index_of[child])
            if advanced:
                continue
            work.pop()
            if work:
                parent = work[-1][0]
                low_of[parent] = min(low_of[parent], low_of[node])
            if low_of[node] == index_of[node]:
                component = []
                while True:
                    member = stack.pop()
                    on_stack.discard(member)
                    component.append(member)
                    if member == node:
                        break
                components.append(sorted(component))
    return sorted(components)


def find_cycle(nodes, adjacency):
    # Returns one concrete cycle as a list of nodes, or None. Used to make failures actionable.
    color = {}
    for root in nodes:
        if color.get(root):
            continue
        path = []
        work = [(root, iter(adjacency.get(root, ())))]
        color[root] = 1
        path.append(root)
        while work:
            node, children = work[-1]
            advanced = False
            for child in children:
                if color.get(child) == 1:
                    return path[path.index(child):] + [child]
                if not color.get(child):
                    color[child] = 1
                    path.append(child)
                    work.append((child, iter(adjacency.get(child, ()))))
                    advanced = True
                    break
            if not advanced:
                color[node] = 2
                work.pop()
                path.pop()
    return None


source_files, include_edges = scan_sources(args.src)
file_set = set(source_files)

unresolved = [(src, dst, line) for src, dst, line in include_edges if dst not in file_set]

modules = sorted({module_of(path) for path in source_files})
files_by_module = collections.defaultdict(list)
for path in source_files:
    files_by_module[module_of(path)].append(path)

file_adjacency = collections.defaultdict(set)
for src, dst, _ in include_edges:
    if dst in file_set:
        file_adjacency[src].add(dst)

module_adjacency = collections.defaultdict(set)
module_pair_edges = collections.defaultdict(list)
module_pair_count = collections.Counter()
for src, dst, line in include_edges:
    if dst not in file_set:
        continue
    src_module = module_of(src)
    dst_module = module_of(dst)
    module_pair_count[(src_module, dst_module)] += 1
    module_pair_edges[(src_module, dst_module)].append((src, dst, line))
    if src_module != dst_module:
        module_adjacency[src_module].add(dst_module)

module_dependencies = {module: sorted(module_adjacency.get(module, set())) for module in modules}
module_cycles = [comp for comp in strongly_connected_components(modules, module_adjacency) if len(comp) > 1]
file_cycle = find_cycle(source_files, {k: sorted(v) for k, v in file_adjacency.items()})
file_graph_is_acyclic = file_cycle is None

fan_in = collections.Counter()
fan_out = collections.Counter()
for src, dst, _ in include_edges:
    if dst in file_set:
        fan_out[src] += 1
        fan_in[dst] += 1

cycle_modules = {module for component in module_cycles for module in component}

current_baseline = {
    "module_dependencies": module_dependencies,
    "module_cycles": module_cycles,
    "file_graph_is_acyclic": file_graph_is_acyclic,
}

if args.update_baseline:
    preserved = {}
    if os.path.exists(args.baseline):
        with open(args.baseline, encoding="utf-8") as baseline_file:
            for key, value in json.load(baseline_file).items():
                if key.startswith("_"):
                    preserved[key] = value
    preserved.update(current_baseline)
    with open(args.baseline, "w", encoding="utf-8") as baseline_file:
        baseline_file.write(json.dumps(preserved, sort_keys=True, indent=2) + "\n")
    print("wrote %s" % args.baseline)
    sys.exit(0)

with open(args.baseline, encoding="utf-8") as baseline_file:
    baseline = json.load(baseline_file)

baseline_dependencies = baseline.get("module_dependencies", {})
baseline_cycles = [sorted(component) for component in baseline.get("module_cycles", [])]
baseline_acyclic = baseline.get("file_graph_is_acyclic", True)

failures = []

for src, dst, line in unresolved:
    failures.append(
        'unresolvable include: %s:%d includes "%s" which does not exist' % (src, line, dst)
    )

for module in sorted(set(baseline_dependencies) | set(module_dependencies)):
    allowed = set(baseline_dependencies.get(module, []))
    actual = set(module_dependencies.get(module, []))
    for added in sorted(actual - allowed):
        blame = sorted(module_pair_edges[(module, added)])[0]
        failures.append(
            "new module dependency: %s -> %s\n"
            "      introduced by: %s:%d -> %s\n"
            '      If this is intentional, add "%s" to module_dependencies["%s"] in %s.'
            % (module, added, blame[0], blame[2], blame[1], added, module, os.path.basename(args.baseline))
        )
    for removed in sorted(allowed - actual):
        failures.append(
            "module dependency in baseline no longer exists: %s -> %s\n"
            '      The baseline is stale. Remove "%s" from module_dependencies["%s"] in %s\n'
            "      to lock in the improvement."
            % (module, removed, removed, module, os.path.basename(args.baseline))
        )

if module_cycles != baseline_cycles:
    failures.append(
        "module cycles changed\n      baseline: %s\n      current:  %s"
        % (json.dumps(baseline_cycles), json.dumps(module_cycles))
    )

if baseline_acyclic and not file_graph_is_acyclic:
    failures.append(
        "file-level include cycle detected (baseline requires an acyclic graph):\n        "
        + "\n     -> ".join(file_cycle)
    )

if args.check:
    print(
        "%d files, %d includes, %d modules, file graph %s"
        % (len(source_files), len(include_edges), len(modules), "acyclic" if file_graph_is_acyclic else "CYCLIC")
    )
    for failure in failures:
        print("FAIL: %s" % failure)
    if failures:
        print(
            "\n%d architectural invariant violation(s). See %s."
            % (len(failures), os.path.relpath(args.baseline, REPO_ROOT))
        )
        sys.exit(1)
    print("All architectural invariants hold.")
    sys.exit(0)

# ---------------------------------------------------------------------------
# Graph rendering.
# ---------------------------------------------------------------------------

dot_path = shutil.which(args.dot_binary)
if dot_path is None:
    if args.require_graphviz:
        print("error: graphviz '%s' not found on PATH (--require-graphviz)" % args.dot_binary)
        sys.exit(1)
    print("warning: graphviz '%s' not found on PATH; rendering graphs as DOT source only" % args.dot_binary,
          file=sys.stderr)


def render_dot(dot_text):
    # Returns inline-ready SVG, or None if graphviz is unavailable or failed.
    if dot_path is None:
        return None
    try:
        result = subprocess.run([dot_path, "-Tsvg"], input=dot_text, capture_output=True,
                                encoding="utf-8", check=False)
    except OSError as error:
        print("warning: failed to run %s: %s" % (dot_path, error), file=sys.stderr)
        return None
    if result.returncode != 0:
        print("warning: %s exited %d: %s" % (dot_path, result.returncode, result.stderr.strip()), file=sys.stderr)
        return None
    svg = result.stdout
    if "<svg" not in svg:
        return None
    # Slicing to <svg drops the XML prolog, the DOCTYPE, and the "Generated by graphviz version"
    # comment in one move. That last one keeps the published bytes stable across runner upgrades.
    return svg[svg.index("<svg"):]


def dot_preamble(graph_id):
    return (
        "digraph deps {\n"
        '  graph [id="%s", bgcolor="transparent", rankdir="TB", nodesep="0.35", ranksep="0.6",\n'
        '         fontname="%s"];\n'
        '  node [shape="box", style="filled,rounded", fillcolor="%s", color="%s", fontcolor="%s",\n'
        '        fontname="%s", fontsize="12", margin="0.15,0.08"];\n'
        '  edge [color="%s", fontcolor="#cfcbd4", fontname="%s", fontsize="9", arrowsize="0.7"];\n'
        % (graph_id, FONT, COLOR_PANEL, COLOR_LINK, COLOR_TEXT, FONT, COLOR_EDGE, FONT)
    )


def build_module_dot():
    max_weight = max([count for pair, count in module_pair_count.items() if pair[0] != pair[1]] or [1])
    lines = [dot_preamble("g_modules")]
    if cycle_modules:
        lines.append(
            # The cluster needs its own id: graphviz otherwise reuses the root graph's id for it,
            # which produces a duplicate id in the inlined SVG.
            '  subgraph cluster_scc {\n'
            '    graph [id="g_modules_scc", label="dependency cycle", labelloc="t", fontcolor="%s",\n'
            '           color="%s", style="dashed", penwidth="1.5"];\n' % (COLOR_CYCLE, COLOR_CYCLE)
        )
        for module in sorted(cycle_modules):
            lines.append('    "%s" [URL="#%s", tooltip="%s: %d files"];\n'
                         % (module, anchor_id("module", module), module, len(files_by_module[module])))
        lines.append("  }\n")
    for module in modules:
        if module in cycle_modules:
            continue
        extra = ', fillcolor="%s"' % COLOR_LEAF if not module_dependencies[module] else ""
        lines.append('  "%s" [URL="#%s", tooltip="%s: %d files"%s];\n'
                     % (module, anchor_id("module", module), module, len(files_by_module[module]), extra))
    for (src_module, dst_module), count in sorted(module_pair_count.items()):
        if src_module == dst_module:
            continue
        in_cycle = src_module in cycle_modules and dst_module in cycle_modules
        penwidth = 1.0 + 1.6 * (float(count) / max_weight) ** 0.5
        attrs = 'label=" %d", penwidth="%.2f"' % (count, penwidth)
        if in_cycle:
            attrs += ', color="%s"' % COLOR_CYCLE
        lines.append('  "%s" -> "%s" [%s];\n' % (src_module, dst_module, attrs))
    lines.append("}\n")
    return "".join(lines)


def build_module_file_dot(module):
    lines = [dot_preamble("g_module_%s" % re.sub(r"[^a-zA-Z0-9]+", "_", module))]
    member_files = files_by_module[module]
    for path in member_files:
        lines.append('  "%s" [label="%s", URL="#%s", tooltip="%s"];\n'
                     % (path, path.split("/")[-1], anchor_id("file", path), path))
    for src in member_files:
        for dst in sorted(file_adjacency.get(src, ())):
            if module_of(dst) == module:
                lines.append('  "%s" -> "%s";\n' % (src, dst))
    lines.append("}\n")
    return "".join(lines)


def build_full_dot():
    lines = [dot_preamble("g_full")]
    for module in modules:
        lines.append('  subgraph "cluster_%s" {\n' % module)
        lines.append('    graph [label="%s", labelloc="t", fontcolor="%s", color="%s"];\n'
                     % (module, COLOR_LINK, COLOR_PANEL))
        for path in files_by_module[module]:
            lines.append('    "%s" [label="%s"];\n' % (path, path.split("/")[-1]))
        lines.append("  }\n")
    for src in source_files:
        for dst in sorted(file_adjacency.get(src, ())):
            lines.append('  "%s" -> "%s";\n' % (src, dst))
    lines.append("}\n")
    return "".join(lines)


# ---------------------------------------------------------------------------
# HTML generation.
# ---------------------------------------------------------------------------

def esc(value):
    return html.escape(str(value))


def graph_block(dot_text, caption):
    # Renders the SVG when graphviz is available, and always ships the DOT source alongside it.
    svg = render_dot(dot_text)
    parts = []
    if svg is None:
        parts.append('<p class="note">Graph not rendered: graphviz was not available when this page '
                     "was generated. The DOT source below can be rendered with "
                     "<code>dot -Tsvg</code>.</p>\n")
    else:
        parts.append('<div class="scroll">\n%s</div>\n' % svg)
    parts.append("<details><summary>DOT source for %s</summary>\n<pre>%s</pre>\n</details>\n"
                 % (esc(caption), esc(dot_text)))
    return "".join(parts)


def table(headers, rows, classes=None):
    parts = ['<div class="scroll">\n<table>\n<tr>']
    for header in headers:
        parts.append("<th>%s</th>" % esc(header))
    parts.append("</tr>\n")
    for row in rows:
        parts.append("<tr>")
        for index, cell in enumerate(row):
            cell_class = classes[index] if classes and index < len(classes) else ""
            parts.append('<td class="%s">%s</td>' % (cell_class, cell))
        parts.append("</tr>\n")
    parts.append("</table>\n</div>\n")
    return "".join(parts)


INLINE_CSS = """
body { color: %(text)s; font-family: %(font)s; }
main { max-width: 1100px; margin: 0 auto; padding: 0 16px 64px; }
h1, h2, h3 { color: %(text)s; }
.scroll { overflow-x: auto; max-width: 100%%; border: 1px solid %(panel)s; border-radius: 6px;
          padding: 8px; background-color: #35323a; margin: 12px 0; }
table { border-collapse: collapse; font-size: 13px; }
th, td { color: %(text)s; font-family: %(font)s; border: 1px solid %(panel)s; padding: 4px 8px;
         text-align: left; white-space: nowrap; }
th { background-color: %(panel)s; position: sticky; top: 0; z-index: 1; }
td.num { text-align: right; font-variant-numeric: tabular-nums; }
td.zero { color: #6f6a76; text-align: right; }
td.cycle { background-color: #6b4436; text-align: right; }
td.self { background-color: #4a4750; text-align: right; }
td.pass { color: #7ec89a; }
td.fail { color: %(cycle)s; }
details { margin: 12px 0; }
summary { color: %(link)s; cursor: pointer; font-family: %(font)s; }
summary:hover { text-decoration: underline; }
pre, code { color: %(text)s; background-color: %(panel)s; }
pre { overflow-x: auto; padding: 8px; border-radius: 6px; font-size: 12px; line-height: 1.4; }
.note { border-left: 3px solid %(link)s; padding: 4px 12px; background-color: #45414a; }
.legend { font-size: 12px; }
.chip { display: inline-block; background-color: %(panel)s; border-radius: 10px; padding: 1px 8px;
        margin: 2px 2px 2px 0; font-size: 12px; }
.swatch { display: inline-block; width: 12px; height: 12px; vertical-align: middle;
          border-radius: 2px; margin-right: 4px; }
""" % {"text": COLOR_TEXT, "font": FONT, "panel": COLOR_PANEL, "link": COLOR_LINK, "cycle": COLOR_CYCLE}

out = []
out.append("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n")
out.append('<meta name="viewport" content="width=device-width, initial-scale=1">\n')
out.append("<title>JazzLights Dependency Map</title>\n")
out.append('<meta property="og:title" content="JazzLights Dependency Map" />\n')
out.append('<meta property="og:site_name" content="JazzLights" />\n')
out.append('<meta property="og:type" content="website" />\n')
if args.embed_css:
    with open(args.embed_css, encoding="utf-8") as css_file:
        out.append("<style>\n%s</style>\n" % css_file.read())
else:
    out.append('<link rel="stylesheet" href="%s">\n' % esc(args.css))
out.append("<style>%s</style>\n</head>\n<body>\n<main>\n" % INLINE_CSS)

out.append("<h1>JazzLights Dependency Map</h1>\n")
out.append(
    "<p>Every <code>#include \"jazzlights/&hellip;\"</code> in <code>src/jazzlights/**</code> and "
    "<code>src/main.cpp</code>, as a dependency graph. Generated from the source on every commit by "
    "<code>extras/site/dependency-map.py</code>.</p>\n"
)
out.append(
    '<p class="note">This graph is the <b>union of every build configuration</b>. Includes guarded by '
    "<code>#if JL_IS_CONFIG(&hellip;)</code> are all counted, because the goal is to describe the "
    "structure of the source tree rather than any one firmware image &mdash; which is why, for example, "
    "<code>ui</code> reaches so many modules: it is every board's UI at once. "
    "<code>test/</code> and <code>extras/</code> are consumers of this code and are out of scope.</p>\n"
)

header_count = len([f for f in source_files if f.endswith(".h")])
source_count = len(source_files) - header_count
summary_rows = [
    ("Files", "%d (%d headers, %d sources)" % (len(source_files), header_count, source_count)),
    ("Include edges", str(len(include_edges))),
    ("Modules", str(len(modules))),
    ("File-level graph", "acyclic" if file_graph_is_acyclic else "CONTAINS A CYCLE"),
    ("Module cycles", ", ".join(" &harr; ".join(c) for c in module_cycles) or "none"),
]
if args.version:
    summary_rows.append(("Revision", esc(args.version)))
out.append("<h2>Summary</h2>\n")
out.append(table(["", ""], summary_rows))

out.append('<h2 id="module-graph">Module graph</h2>\n')
out.append("<p>Each node is a directory under <code>src/jazzlights/</code>; edges point from dependent "
           "to dependency, so the leaf layer sits at the bottom. Edge labels count includes. "
           "Click a module to jump to its section.</p>\n")
out.append(graph_block(build_module_dot(), "the module graph"))
out.append(
    '<p class="legend">'
    '<span class="swatch" style="background-color:%s"></span>ordinary module &nbsp; '
    '<span class="swatch" style="background-color:%s"></span>leaf layer (depends on nothing internal) &nbsp; '
    '<span class="swatch" style="background-color:%s"></span>participates in a dependency cycle'
    "</p>\n" % (COLOR_PANEL, COLOR_LEAF, COLOR_CYCLE)
)

out.append("<h2>Module dependency matrix</h2>\n")
out.append("<p>Rows depend on columns. Each cell counts includes from a file in the row's module to a "
           "file in the column's module; the diagonal is intra-module.</p>\n")
out.append('<div class="scroll">\n<table>\n<tr><th>depends on &rarr;</th>')
for module in modules:
    out.append("<th>%s</th>" % esc(module))
out.append("</tr>\n")
for src_module in modules:
    out.append("<tr><td><b>%s</b></td>" % esc(src_module))
    for dst_module in modules:
        count = module_pair_count.get((src_module, dst_module), 0)
        if src_module == dst_module:
            cell_class = "self"
        elif count == 0:
            cell_class = "zero"
        elif src_module in cycle_modules and dst_module in cycle_modules:
            cell_class = "cycle"
        else:
            cell_class = "num"
        out.append('<td class="%s">%s</td>' % (cell_class, count if count else "&middot;"))
    out.append("</tr>\n")
out.append("</table>\n</div>\n")

out.append("<h2>Architectural invariants</h2>\n")
out.append(
    "<p>These are enforced on every push by <code>extras/site/dependency-map.py --check</code> in the "
    "<code>check-format</code> CI job. The baseline lives at "
    "<code>extras/site/dependency-baseline.json</code>; changing it is a deliberate, reviewable "
    "architecture decision.</p>\n"
)
invariant_rows = []
util_deps = module_dependencies.get("util", [])
invariant_rows.append(("<code>util</code> depends on nothing internal",
                       "none" if not util_deps else esc(", ".join(util_deps)),
                       '<span class="pass">PASS</span>' if not util_deps else '<span class="fail">FAIL</span>'))
protocol_deps = module_dependencies.get("protocol", [])
protocol_ok = protocol_deps == ["util"]
invariant_rows.append(("<code>protocol</code> depends only on <code>util</code>",
                       esc(", ".join(protocol_deps)) or "none",
                       '<span class="pass">PASS</span>' if protocol_ok else '<span class="fail">FAIL</span>'))
invariant_rows.append(("File-level include graph is acyclic",
                       "acyclic" if file_graph_is_acyclic else "cyclic",
                       '<span class="pass">PASS</span>' if file_graph_is_acyclic
                       else '<span class="fail">FAIL</span>'))
cycles_ok = module_cycles == baseline_cycles
invariant_rows.append(("Module cycles match the baseline",
                       ", ".join(" &harr; ".join(c) for c in module_cycles) or "none",
                       '<span class="pass">PASS</span>' if cycles_ok else '<span class="fail">FAIL</span>'))
invariant_rows.append(("Every include resolves to a real file",
                       "%d unresolved" % len(unresolved),
                       '<span class="pass">PASS</span>' if not unresolved else '<span class="fail">FAIL</span>'))
out.append(table(["Invariant", "Current value", "Status"], invariant_rows))

out.append("<h2>Cycles</h2>\n")
if not module_cycles:
    out.append("<p>The module graph is acyclic.</p>\n")
else:
    out.append(
        "<p>The <b>file-level</b> include graph is acyclic &mdash; there are no circular includes. "
        "The cycles below exist only at the <b>module</b> level: they are an artifact of how files are "
        "grouped into directories, not of any file including itself transitively.</p>\n"
    )
    for component in module_cycles:
        out.append("<h3>%s</h3>\n" % " &harr; ".join(esc(module) for module in component))
        for src_module in component:
            for dst_module in component:
                if src_module == dst_module:
                    continue
                pair = module_pair_edges.get((src_module, dst_module), [])
                if not pair:
                    continue
                out.append("<p><b>%s &rarr; %s</b> (%d include%s):</p>\n"
                           % (esc(src_module), esc(dst_module), len(pair), "" if len(pair) == 1 else "s"))
                rows = [(esc(src), str(line), esc(dst)) for src, dst, line in sorted(pair)]
                out.append(table(["From", "Line", "To"], rows, ["", "num", ""]))

out.append("<h2>Hotspots</h2>\n")
out.append("<p>High fan-in is expected for foundational headers such as <code>util/config.h</code>. "
           "High fan-out is a complexity signal: changing that file means understanding many "
           "neighbors. <code>main.cpp</code> has zero fan-in by design &mdash; it is the entry "
           "point.</p>\n")
out.append("<h3>Most depended upon</h3>\n")
out.append(table(["File", "Included by"],
                 [(esc(path), str(count)) for path, count in fan_in.most_common(20)],
                 ["", "num"]))
out.append("<h3>Depends on the most</h3>\n")
out.append(table(["File", "Includes"],
                 [(esc(path), str(count)) for path, count in fan_out.most_common(20)],
                 ["", "num"]))

out.append("<h2>Modules</h2>\n")
for module in modules:
    out.append('<h3 id="%s">%s</h3>\n' % (anchor_id("module", module), esc(module)))
    depends_on = module_dependencies[module]
    depended_by = sorted(m for m in modules if module in module_dependencies.get(m, []))
    out.append("<p>%d file%s. Depends on: %s<br>Depended on by: %s</p>\n" % (
        len(files_by_module[module]),
        "" if len(files_by_module[module]) == 1 else "s",
        "".join('<span class="chip">%s</span>' % esc(m) for m in depends_on) or "nothing internal",
        "".join('<span class="chip">%s</span>' % esc(m) for m in depended_by) or "nothing",
    ))
    intra_edges = sum(1 for src in files_by_module[module]
                      for dst in file_adjacency.get(src, ()) if module_of(dst) == module)
    if intra_edges:
        out.append("<details><summary>Internal structure of <code>%s</code></summary>\n" % esc(module))
        out.append(graph_block(build_module_file_dot(module), "%s internals" % module))
        out.append("</details>\n")
    rows = [('<span id="%s">%s</span>' % (anchor_id("file", path), esc(path)),
             str(fan_in.get(path, 0)), str(fan_out.get(path, 0)))
            for path in files_by_module[module]]
    out.append(table(["File", "Included by", "Includes"], rows, ["", "num", "num"]))

out.append("<h2>Appendix</h2>\n")
out.append("<p>The full file-level graph is not rendered here &mdash; at %d nodes it is too wide to "
           "read. Its DOT source is below; render it locally with "
           "<code>dot -Tsvg -o deps.svg</code> (or try <code>-Kfdp</code> for a force-directed "
           "layout).</p>\n" % len(source_files))
out.append("<details><summary>DOT source for the full file-level graph</summary>\n<pre>%s</pre>\n</details>\n"
           % esc(build_full_dot()))

out.append('<p><br><br>JazzLights is hosted on <a href="%s">GitHub</a>. '
           'Back to the <a href="%s">JazzLights site</a>.</p>\n' % (REPO_URL, SITE_URL))
out.append("</main>\n</body>\n</html>\n")

output_str = "".join(out)

if args.output != "-":
    with open(args.output, "w", encoding="utf-8") as output_file:
        output_file.write(output_str)
else:
    print(output_str)
