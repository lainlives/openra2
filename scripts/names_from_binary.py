#!/usr/bin/env python3
"""names_from_binary - harvest filename strings from game binaries.

The retail engine carries the names of many files it loads in its string data.
This tool scans one or more binaries (or any text/decompiler output) and emits
a newline-delimited list of filename candidates, suitable for use as a MIX name
database.

It is deliberately conservative: a token is kept only if it looks like a plain
filename with a known data extension, so version strings, register names, and
format templates are rejected.

Usage:
    names_from_binary.py BINARY [BINARY ...] [-o OUT] [--ext a,b,c]
                               [--min-length N]

Examples:
    names_from_binary.py "<game>/gamemd.exe" "<game>/game.exe" -o names.txt
    names_from_binary.py reference/ghidra/gamemd.exe.c -o names.txt
"""

from __future__ import annotations

import subprocess
import sys
from argparse import ArgumentParser
from pathlib import Path

DEFAULT_EXTENSIONS = {
    "shp", "vxl", "hva", "tmp", "tem", "sno", "urb", "ubn", "des", "lun",
    "map", "pcx", "pal", "ini", "mix", "wav", "aud", "vox", "vqa", "bik",
    "csf", "fnt", "cps", "mrf", "bag", "idx", "txt", "pkt", "sha", "vpl",
    "dat", "wal", "cel", "wsa", "ico", "cur", "bmp", "rgb",
}

ALLOWED = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-$")


def scan_file(path: str, min_length: int) -> list[str]:
    try:
        data = subprocess.run(
            ["strings", "-n", str(min_length), path],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"warning: cannot scan {path}: {exc}", file=sys.stderr)
        return []
    return data.splitlines()


def candidate(token: str, exts: set[str], max_length: int) -> str | None:
    token = token.strip().strip("\"'`,;:()[]{}")
    if not token or len(token) > max_length:
        return None
    if any(ch not in ALLOWED for ch in token):
        return None
    token = token.replace("\\", "/").rsplit("/", 1)[-1]
    if "." not in token:
        return None
    name, _, ext = token.rpartition(".")
    if not name or ext.lower() not in exts:
        return None
    return token


def main(argv: list[str] | None = None) -> int:
    cli = ArgumentParser(prog="names_from_binary", description=__doc__)
    cli.add_argument("binaries", nargs="+")
    cli.add_argument("-o", "--output", help="output file (default stdout)")
    cli.add_argument("--ext", help="comma-separated extension allowlist")
    cli.add_argument("--min-length", type=int, default=4)
    cli.add_argument("--max-length", type=int, default=64)
    args = cli.parse_args(argv)

    exts = (
        {e.strip().lower() for e in args.ext.split(",") if e.strip()}
        if args.ext
        else set(DEFAULT_EXTENSIONS)
    )

    seen: dict[str, str] = {}
    for binary in args.binaries:
        for token in scan_file(binary, args.min_length):
            name = candidate(token, exts, args.max_length)
            if name is not None:
                seen.setdefault(name.lower(), name)

    names = sorted(seen.values(), key=str.lower)
    text = "\n".join(names) + ("\n" if names else "")

    if args.output:
        Path(args.output).write_text(text)
        print(f"wrote {len(names)} names to {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(text)
    print(f"{len(names)} unique filename candidates", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())