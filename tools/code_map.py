#!/usr/bin/env python3
"""Live code map: lists source files with a one-line description, computed from the code.

Usage:
    python tools/code_map.py              # whole map (engine/, editor/, tests/)
    python tools/code_map.py slider       # only entries matching the keyword
    python tools/code_map.py editor gui   # all keywords must match (path or description)

The description is the file's first-line "// ..." comment. A .h/.cpp pair is shown as one
row and uses the header's description. Nothing is stored, so the map cannot go stale.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ("engine", "editor", "tests")
EXTENSIONS = (".h", ".cpp")
RULE_LINE = re.compile(r"^//\s*[=\-]+\s*$")
NO_DESCRIPTION = "(no description - add a first-line // comment)"


def read_lines(path):
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def first_line_comment(lines):
    """Returns the text of the first non-blank line if it is a // comment, else None."""
    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("//") and not RULE_LINE.match(stripped):
            return stripped.lstrip("/").strip()
        return None
    return None


def banner_fallback(lines):
    """For headers without a first-line comment: first prose comment in the opening lines."""
    for line in lines[:40]:
        stripped = line.strip()
        if stripped.startswith("//") and not RULE_LINE.match(stripped):
            return stripped.lstrip("/").strip()
    return None


def collect():
    groups = defaultdict(dict)  # (directory, stem) -> {extension: (lines, line_count)}
    for scan_dir in SCAN_DIRS:
        base = ROOT / scan_dir
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix in EXTENSIONS and path.is_file():
                lines = read_lines(path)
                groups[(path.parent.relative_to(ROOT).as_posix(), path.stem)][path.suffix] = lines
    return groups


def describe(files):
    header = files.get(".h")
    source = files.get(".cpp")
    for lines in (header, source):
        if lines is not None:
            text = first_line_comment(lines)
            if text:
                return text
    if header is not None:
        text = banner_fallback(header)
        if text:
            return text
    return NO_DESCRIPTION


def main():
    keywords = [k.lower() for k in sys.argv[1:]]
    groups = collect()
    by_dir = defaultdict(list)

    for (directory, stem), files in groups.items():
        exts = [e for e in EXTENSIONS if e in files]
        label = f"{stem}.{exts[0][1:]}" if len(exts) == 1 else f"{stem}.{{h,cpp}}"
        counts = "+".join(str(len(files[e])) for e in exts)
        desc = describe(files)
        haystack = f"{directory}/{stem} {desc}".lower()
        if all(k in haystack for k in keywords):
            by_dir[directory].append((label, counts, desc))

    if not by_dir:
        print("No matches. Try a shorter keyword, or search the code with grep/glob.")
        return 1

    sys.stdout.reconfigure(encoding="utf-8")
    for directory in sorted(by_dir):
        print(f"{directory}/")
        for label, counts, desc in sorted(by_dir[directory]):
            print(f"  {label:<34} {counts:>9} lines  {desc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
