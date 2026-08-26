#!/usr/bin/env python3

# Prints a markdown table of (board, LED type, framework) tuples and the platformio.ini
# environments that use them.
#
# Nothing here is hardcoded: the board comes from -DJL_CONTROLLER and the framework from the
# resolved `framework` key, both read out of platformio.ini, while the LED type is worked out
# by evaluating the preprocessor conditionals in src/jazzlights/util/config.h and
# src/jazzlights/layout/*.cpp for every environment and seeing which AddLeds<CHIPSET, ...>
# calls survive. Adding a board, a config, or a new chipset therefore needs no change here.
#
# The preprocessor implemented below understands only the subset those files use. It does not
# follow #include; instead config.h is evaluated first and the macros it leaves behind are fed
# to the layout files, which is what including it would have achieved.

import argparse
import collections
import configparser
import functools
import os
import re
import shlex

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

DIRECTIVE_RE = re.compile(r"^\s*#\s*(\w+)\s*(.*)$")
DEFINE_RE = re.compile(r"^(\w+)(\([^)]*\))?\s*(.*)$")
ADD_LEDS_RE = re.compile(r"\bAddLeds<\s*(\w+)")
ADD_LEDS_TO_RUNNER_RE = re.compile(r"\bvoid\s+AddLedsToRunner\s*\(")
TOKEN_RE = re.compile(r"\s*(&&|\|\||[<>=!]=|0[xX][0-9a-fA-F]+|\d+|\w+|\S)")

# JL_IS_CONFIG and JL_IS_CONTROLLER paste their argument onto a prefix and compare the result
# against the current setting. Implementing ## in general is not worth it, so these two are
# evaluated directly; the values they compare still come from config.h.
TOKEN_PASTING_MACROS = {
    "JL_IS_CONFIG": "JL_CONFIG_",
    "JL_IS_CONTROLLER": "JL_CONTROLLER_",
}

HEX_LITERAL_RE = re.compile(r"0[xX][0-9a-fA-F]+")
COMPARISON_OPERATORS = {
    "==": lambda a, b: 1 if a == b else 0,
    "!=": lambda a, b: 1 if a != b else 0,
    ">=": lambda a, b: 1 if a >= b else 0,
    "<=": lambda a, b: 1 if a <= b else 0,
    ">": lambda a, b: 1 if a > b else 0,
    "<": lambda a, b: 1 if a < b else 0,
}
AND_OPERATOR = {"&&": lambda a, b: 1 if a and b else 0}
OR_OPERATOR = {"||": lambda a, b: 1 if a or b else 0}


class PreprocessorError(Exception):
    """Raised when a #error directive is reached or a source file cannot be evaluated."""


@functools.cache
def read_source(path):
    with open(path, encoding="utf-8") as source_file:
        # Join backslash continuations so that each directive is on a single line.
        return re.sub(r"\\\n", " ", source_file.read()).splitlines()


# The expression parser below is a recursive descent over the operators that appear in the
# jazzlights sources. ParserState carries the token list, a single-element `cursor` list
# holding the read position, the macro table, and `expanding`: the macros currently being
# expanded, which stops a self-referential one from looping.
ParserState = collections.namedtuple(
    "ParserState", ["tokens", "cursor", "macros", "expanding"]
)


def peek(state):
    return (
        state.tokens[state.cursor[0]] if state.cursor[0] < len(state.tokens) else None
    )


def take(state):
    token = peek(state)
    state.cursor[0] += 1
    return token


def skip_arguments(state):
    # Consumes a balanced parenthesized argument list that we do not understand.
    depth = 0
    while peek(state) is not None:
        token = take(state)
        if token == "(":
            depth += 1
        elif token == ")":
            depth -= 1
            if depth == 0:
                return


def macro_value(state, name):
    # An undefined macro evaluates to 0, as it does in a real #if.
    if name not in state.macros or name in state.expanding:
        return 0
    return evaluate_expression(
        state.macros[name], state.macros, state.expanding | {name}
    )


def parse_defined(state):
    parenthesized = peek(state) == "("
    if parenthesized:
        take(state)
    name = take(state)
    if parenthesized and peek(state) == ")":
        take(state)
    return 1 if name in state.macros else 0


def parse_token_paste(state, token):
    take(state)
    argument = take(state)
    if peek(state) == ")":
        take(state)
    prefix = TOKEN_PASTING_MACROS[token]
    # JL_IS_CONFIG inspects JL_CONFIG, JL_IS_CONTROLLER inspects JL_CONTROLLER.
    setting = state.macros.get(token.replace("JL_IS_", "JL_", 1), "")
    return (
        1
        if macro_value(state, prefix + setting) == macro_value(state, prefix + argument)
        else 0
    )


def parse_primary(state):
    token = take(state)
    if token is None:
        return 0
    if token == "(":
        value = parse_or(state)
        if peek(state) == ")":
            take(state)
        return value
    if token == "!":
        return 0 if parse_primary(state) else 1
    if HEX_LITERAL_RE.fullmatch(token):
        return int(token, 16)
    if token.isdigit():
        return int(token)
    if token == "defined":
        return parse_defined(state)
    if peek(state) != "(":
        return macro_value(state, token)
    if token in TOKEN_PASTING_MACROS:
        return parse_token_paste(state, token)
    # Some other function-like macro, such as __has_include. Treat it as 0.
    skip_arguments(state)
    return 0


def parse_binary(state, operators, sub_parser):
    value = sub_parser(state)
    while peek(state) in operators:
        operator = take(state)
        value = operators[operator](value, sub_parser(state))
    return value


def parse_comparison(state):
    return parse_binary(state, COMPARISON_OPERATORS, parse_primary)


def parse_and(state):
    return parse_binary(state, AND_OPERATOR, parse_comparison)


def parse_or(state):
    return parse_binary(state, OR_OPERATOR, parse_and)


def evaluate_expression(expression, macros, expanding=frozenset()):
    return parse_or(ParserState(TOKEN_RE.findall(expression), [0], macros, expanding))


def scan_source(path, macros, raise_on_error=True):
    # Yields the lines of `path` that survive its preprocessor conditionals, applying any
    # #define and #undef it encounters to `macros` along the way.
    stack = []  # (currently_active, some_branch_taken, enclosing_active)
    for line in read_source(path):
        directive_match = DIRECTIVE_RE.match(line)
        if not directive_match:
            if all(active for active, _, _ in stack):
                yield line
            continue
        directive, rest = directive_match.group(1), directive_match.group(2)
        rest = rest.split("//")[0].strip()
        enclosing_active = all(active for active, _, _ in stack)
        if directive in ("if", "ifdef", "ifndef"):
            if directive == "ifdef":
                taken = rest.split()[0] in macros
            elif directive == "ifndef":
                taken = rest.split()[0] not in macros
            else:
                taken = bool(evaluate_expression(rest, macros))
            stack.append((enclosing_active and taken, taken, enclosing_active))
        elif directive in ("elif", "else"):
            if not stack:
                raise PreprocessorError("Unbalanced #{} in {}".format(directive, path))
            _, branch_taken, outer_active = stack[-1]
            taken = (not branch_taken) and (
                directive == "else" or bool(evaluate_expression(rest, macros))
            )
            stack[-1] = (outer_active and taken, branch_taken or taken, outer_active)
        elif directive == "endif":
            if not stack:
                raise PreprocessorError("Unbalanced #endif in {}".format(path))
            stack.pop()
        elif enclosing_active and directive == "define":
            define_match = DEFINE_RE.match(rest)
            if define_match:
                macros[define_match.group(1)] = define_match.group(3).strip()
        elif enclosing_active and directive == "undef":
            macros.pop(rest.split()[0], None)
        elif enclosing_active and directive == "error" and raise_on_error:
            raise PreprocessorError(
                "{} reports: {}".format(os.path.basename(path), rest)
            )


def layout_sources(layout_dir):
    return sorted(
        os.path.join(layout_dir, name)
        for name in os.listdir(layout_dir)
        if name.endswith(".cpp")
    )


def chipsets_for(defines, config_header, layout_dir):
    # Evaluates config.h with this environment's -D flags, then walks the layout sources to see
    # which AddLeds<CHIPSET, ...> calls are compiled in.
    macros = dict(defines)
    for _ in scan_source(config_header, macros):
        pass
    chipsets = set()
    definitions = 0
    for source in layout_sources(layout_dir):
        for line in scan_source(source, dict(macros)):
            chipsets.update(ADD_LEDS_RE.findall(line))
            definitions += len(ADD_LEDS_TO_RUNNER_RE.findall(line))
    if definitions != 1:
        raise PreprocessorError(
            "Expected exactly one AddLedsToRunner() definition for JL_CONFIG={}"
            " and JL_CONTROLLER={}, found {}".format(
                macros.get("JL_CONFIG"), macros.get("JL_CONTROLLER"), definitions
            )
        )
    return sorted(chipsets)


def read_ini(path):
    # platformio.ini has duplicate keys in a few sections, so disable strict mode. Disable
    # interpolation too since PlatformIO's ${section.key} syntax is not configparser's.
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    if not parser.read(path):
        raise SystemExit("Failed to read {}".format(path))
    return {section: dict(parser.items(section)) for section in parser.sections()}


def find_key(sections, section, key):
    # Walks the `extends` chain to find which section actually defines `key`.
    seen = set()
    while section in sections and section not in seen:
        seen.add(section)
        if key in sections[section]:
            return sections[section][key], section
        section = sections[section].get("extends")
    return None, None


def resolve(sections, section, key, depth=0):
    # Returns the value of `key` for `section` with all ${...} references expanded.
    if depth > 32:
        raise SystemExit("Cycle while resolving {} in {}".format(key, section))
    value, owner = find_key(sections, section, key)
    if value is None:
        return ""

    def expand(match):
        reference = match.group(1).strip()
        if reference.startswith("this."):
            return resolve(sections, owner, reference[len("this.") :], depth + 1)
        if "." in reference:
            referenced_section, referenced_key = reference.rsplit(".", 1)
            return resolve(sections, referenced_section, referenced_key, depth + 1)
        return ""

    return re.sub(r"\$\{([^}]+)\}", expand, value)


def preprocessor_defines(sections, section):
    # Returns the -D macros that survive after applying every -D and -U in order.
    tokens = []
    for line in resolve(sections, section, "build_flags").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            parts = shlex.split(line, comments=True)
        except ValueError:
            parts = line.split()
        # A single quoted flag may hold several macros, e.g. '-UBOOT_NAME -DBOOT_NAME=DEV'.
        for part in parts:
            tokens.extend(part.split())
    defines = {}
    for token in tokens:
        if token.startswith("-D"):
            name, equals, value = token[len("-D") :].partition("=")
            defines[name] = value if equals else "1"
        elif token.startswith("-U"):
            defines.pop(token[len("-U") :], None)
    return defines


def framework_of(sections, section):
    frameworks = [f.strip() for f in resolve(sections, section, "framework").split(",")]
    return "ESP-IDF" if "espidf" in frameworks else "Arduino"


def controller_order(config_header):
    # Orders boards by their JL_CONTROLLER_* value so the table follows config.h's declaration
    # order. Unknown boards sort last, alphabetically. The JL_CONTROLLER_* defines are
    # unconditional, so config.h is read without an environment here; that trips the #error
    # guarding an unset JL_CONFIG, which is not interesting for the numbering.
    macros = {}
    for _ in scan_source(config_header, macros, raise_on_error=False):
        pass
    order = {}
    for name, value in macros.items():
        if name.startswith("JL_CONTROLLER_") and value.isdigit():
            order[name[len("JL_CONTROLLER_") :]] = int(value)
    return lambda controller: (order.get(controller, len(order)), controller)


def collect_rows(ini_path, config_header, layout_dir):
    sections = read_ini(ini_path)
    rows = {}
    for section in sections:
        if not section.startswith("env:"):
            continue
        env = section[len("env:") :]
        # Sections starting with an underscore are shared bases that are never built.
        if env.startswith("_"):
            continue
        defines = preprocessor_defines(sections, section)
        controller = defines.get("JL_CONTROLLER")
        # The layout sources are all inside #ifdef ESP32, so the host build has no LEDs.
        if controller is None or controller == "NATIVE":
            continue
        defines.setdefault("ESP32", "1")
        try:
            chipsets = chipsets_for(defines, config_header, layout_dir)
        except PreprocessorError as error:
            raise SystemExit("Environment {}: {}".format(env, error)) from error
        framework = framework_of(sections, section)
        for chipset in chipsets:
            rows.setdefault((controller, chipset, framework), []).append(env)
    return rows


def format_table(table):
    # Pads every cell so that the column separators line up in a monospaced font. The last
    # column is sized from its header alone so that long cells are not followed by hundreds
    # of trailing spaces; it has no separator after it to line up with anyway.
    widths = [max(len(row[column]) for row in table) for column in range(len(table[0]))]
    widths[-1] = len(table[0][-1])

    def format_row(row):
        cells = (cell.ljust(width) for cell, width in zip(row, widths, strict=True))
        return "| {} |".format(" | ".join(cells))

    lines = [
        format_row(table[0]),
        "|{}|".format("|".join("-" * (width + 2) for width in widths)),
    ]
    lines.extend(format_row(row) for row in table[1:])
    return "\n".join(lines)


def code(text, backticks):
    # Identifiers read better as Markdown code spans once the table is rendered as HTML.
    return "`{}`".format(text) if backticks else text


def main():
    argument_parser = argparse.ArgumentParser(description=__doc__)
    argument_parser.add_argument(
        "--platformio-ini",
        default=os.path.join(REPO_ROOT, "platformio.ini"),
        help="path to platformio.ini (default: the one in this repository)",
    )
    argument_parser.add_argument(
        "--config-header",
        default=os.path.join(REPO_ROOT, "src", "jazzlights", "util", "config.h"),
        help="path to config.h (default: the one in this repository)",
    )
    argument_parser.add_argument(
        "--layout-dir",
        default=os.path.join(REPO_ROOT, "src", "jazzlights", "layout"),
        help="directory holding the layout sources (default: the one in this repository)",
    )
    argument_parser.add_argument(
        "--backticks",
        action="store_true",
        help="wrap board, LED type and environment names in Markdown backticks",
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

    rows = collect_rows(
        arguments.platformio_ini, arguments.config_header, arguments.layout_dir
    )
    board_key = controller_order(arguments.config_header)
    table = [["Board", "LED type", "Framework", "Environments"]]
    for controller, chipset, framework in sorted(
        rows, key=lambda row: (board_key(row[0]), row[1], row[2])
    ):
        environments = ", ".join(
            code(env, arguments.backticks)
            for env in sorted(rows[(controller, chipset, framework)])
        )
        # Framework is prose rather than an identifier, so it never gets backticks.
        table.append(
            [
                code(controller, arguments.backticks),
                code(chipset, arguments.backticks),
                framework,
                environments,
            ]
        )

    output_str = format_table(table)
    if arguments.title:
        output_str = "# {}\n\n{}".format(arguments.title, output_str)
    if arguments.output != "-":
        with open(arguments.output, "w", encoding="utf-8") as output_file:
            output_file.write(output_str + "\n")
    else:
        print(output_str)


if __name__ == "__main__":
    main()
