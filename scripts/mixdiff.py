#!/usr/bin/env python3
"""mixdiff - compare two extracted MIX trees (or plain file lists).

The retail game's data lives in nested MIX archives. Tooling that reads only
the top level (or fails to rebuild the base archives) silently reports a
different file set than the game can actually resolve. This tool diffs the
sets of member names from two trees so those gaps are visible.

Usage:
    mixdiff.py OLD_DIR NEW_DIR [-o REPORT] [--case-sensitive]

Each tree may also be a newline-delimited file list. Names are lower-cased for
comparison unless --case-sensitive is given.
"""

from __future__ import annotations

import sys
from argparse import ArgumentParser
from pathlib import Path


def collect(root: str, case_sensitive: bool) -> set[str]:
    p = Path(root)
    if p.is_dir():
        names = (f.name for f in p.rglob("*") if f.is_file())
    else:
        names = (
            line.strip()
            for line in p.read_text(errors="replace").splitlines()
            if line.strip() and not line.startswith("#")
        )
    out = set()
    for n in names:
        n = n.rsplit("/", 1)[-1].rsplit("\\", 1)[-1]
        out.add(n if case_sensitive else n.lower())
    return out


def main(argv: list[str] | None = None) -> int:
    cli = ArgumentParser(prog="mixdiff", description=__doc__)
    cli.add_argument("old", help="old tree/listing")
    cli.add_argument("new", help="new tree/listing")
    cli.add_argument("-o", "--output", help="write the report to a file")
    cli.add_argument("--case-sensitive", action="store_true")
    cli.add_argument("--only", choices=["added", "removed", "common"])
    args = cli.parse_args(argv)

    old = collect(args.old, args.case_sensitive)
    new = collect(args.new, args.case_sensitive)
    added = new - old
    removed = old - new
    common = old & new

    lines: list[str] = []
    lines.append(f"old: {args.old}  ({len(old)} names)")
    lines.append(f"new: {args.new}  ({len(new)} names)")
    lines.append(f"added: {len(added)}   removed: {len(removed)}   common: {len(common)}")
    lines.append("")

    def dump(title: str, items: set[str]) -> None:
        lines.append(f"## {title} ({len(items)})")
        for name in sorted(items):
            lines.append(name)
        lines.append("")

    if args.only in (None, "added"):
        dump("added (in new, missing from old)", added)
    if args.only in (None, "removed"):
        dump("removed (in old, missing from new)", removed)
    if args.only == "common":
        dump("common", common)

    if args.output:
        Path(args.output).write_text("\n".join(lines))
        print(f"wrote {args.output}", file=sys.stderr)
    else:
        print("\n".join(lines))

    print(
        f"{len(added)} added, {len(removed)} removed, {len(common)} common",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())