#!/usr/bin/env python3
"""ra2mix - Westwood MIX archive tool for the C&C engines (TD/RA/TS/RA2/YR).

The retail game hides its data inside nested MIX archives: the top-level
archives (ra2.mix, ra2md.mix, ...) mostly contain *other* .mix files, and the
actual assets live one or more levels down.  This tool reads the archive
headers directly (old and new formats, including Blowfish-encrypted indexes),
resolves file names from each archive's embedded "local mix database.dat", and
can list, extract, manifest, or FUSE-mount the fully expanded tree.

Subcommands
    list       list entries, optionally descending into nested .mix archives
    tree       show the archive hierarchy
    extract    extract entries, optionally recursively
    manifest   emit a machine-readable inventory of every leaf file
    create     build a MIX archive from files/directories
    mount      FUSE-mount archives as one merged read-only filesystem

Run ``ra2mix.py <subcommand> --help`` for details.
"""

from __future__ import annotations

import binascii
import json
import sys
from argparse import ArgumentParser
from io import BytesIO
from pathlib import Path
from struct import pack, unpack
from sys import stderr, stdout
from typing import IO, cast

from blowfish import Cipher

names_db_filename = "local mix database.dat"
SIZE_OF_ENCRYPTED_KEY = 80
MAX_DEPTH = 8


# ---------------------------------------------------------------------------
# Crypto / hash helpers
# ---------------------------------------------------------------------------

def decrypt_blowfish_key_fast(encrypted_blowfish_key):
    PUBLIC_EXPONENT = 65537
    PUBLIC_MODULUS = 681994811107118991598552881669230523074742337494683459234572860554038768387821901289207730765589

    if len(encrypted_blowfish_key) < SIZE_OF_ENCRYPTED_KEY:
        raise ValueError("Buffer is not long enough")

    block1 = encrypted_blowfish_key[:40]
    block2 = encrypted_blowfish_key[40:80]

    result_parts = []

    for encrypted_block in (block1, block2):
        decrypted_int = pow(
            int.from_bytes(encrypted_block, "little"), PUBLIC_EXPONENT, PUBLIC_MODULUS
        )

        decrypted = decrypted_int.to_bytes(
            (decrypted_int.bit_length() + 7) >> 3, "little"
        )

        if decrypted[-1] == 0:
            for i in range(len(decrypted) - 1, -1, -1):
                if decrypted[i] != 0:
                    decrypted = decrypted[: i + 1]
                    break
            else:
                decrypted = b""  # zeros

        result_parts.append(decrypted)

    return b"".join(result_parts)


def ra2_crc_fast(filename: str) -> int:
    """Westwood filename hash used as the MIX entry ID (RA2/YR variant)."""
    length = len(filename)
    salt = length & 0xFFFFFFFC

    obfuscated = filename.upper()

    remainder = length & 3
    if remainder:
        obfuscated += chr(length - salt) + obfuscated[salt] * (3 - remainder)

    crc = binascii.crc32(obfuscated.encode())
    return crc - 0x100000000 if crc >= 0x80000000 else crc


def names_db_enum(filenames: list[str], game=0):
    n = len(filenames)
    yield b"XCC by Olaf van der Spek\x1a\x04\x17\x27\x10\x19\x80\x00"
    yield pack("=5I", 52 + sum([len(v) + 1 for v in filenames]), 0, 0, game, n)
    for v in filenames:
        yield v.encode() + b"\x00"


def as_sink(path="-", mode="wb"):
    if path and path != "-":
        return open(path, mode)
    return stdout.buffer if "b" in mode else stdout


def as_source(path="-", mode="rb"):
    if path and path != "-":
        return open(path, mode)
    return sys.stdin.buffer if "b" in mode else sys.stdin


def get_game(
    s="RA2",
    _game=[
        "TD",
        "RA",
        "TS",
        "DUNE2",
        "DUNE2000",
        "RA2",
        "RA2_YR",
        "RG",
        "GR",
        "GR_ZH",
        "EBFD",
        "NOX",
        "BFME",
        "BFME2",
        "TW",
        "TS_FS",
        "UNKNOWN",
    ],
):
    return _game.index(s.upper())


# ---------------------------------------------------------------------------
# Global name database (fallback when an archive has no embedded db)
# ---------------------------------------------------------------------------

_GLOBAL_NAMES: dict[int, str] | None = None


def global_names() -> dict[int, str]:
    global _GLOBAL_NAMES
    if _GLOBAL_NAMES is not None:
        return _GLOBAL_NAMES
    try:
        from util.names import names as names_map
    except Exception:  # noqa: BLE001 - optional fallback
        _GLOBAL_NAMES = {}
        return _GLOBAL_NAMES

    id_name_map = {ra2_crc_fast(k) & 0xFFFFFFFF: k for k in names_map}
    sname, sext = set(), set()
    for filename in names_map:
        name, _dot, ext = filename.rpartition(".")
        sname.add(name)
        sext.add(ext)
    for x in sext:
        for n in sname:
            f = f"{n}.{x}"
            id_name_map.setdefault(ra2_crc_fast(f) & 0xFFFFFFFF, f)
    _GLOBAL_NAMES = id_name_map
    return id_name_map


# ---------------------------------------------------------------------------
# MIX archive reader
# ---------------------------------------------------------------------------

class MixEntry:
    __slots__ = ("id", "offset", "size", "name", "index")

    def __init__(self, id, offset, size, index, name=""):
        self.id = id
        self.offset = offset
        self.size = size
        self.index = index
        self.name = name or ""

    def __repr__(self):
        return f"MixEntry({self.id:08X}, off={self.offset}, size={self.size}, {self.name!r})"


class MixArchive:
    """A single MIX archive, read from a path or an in-memory buffer."""

    def __init__(self, source, name=None, fallback_names=None):
        self.source = source
        self.name = name or (Path(source).name if isinstance(source, (str, Path)) else "<mem>")
        self.flags = 0
        self.data_size = 0
        self.header_format = None
        self.entries: list[MixEntry] = []
        self._own_fh = False
        self._names: dict[int, str] = {}
        self.path: Path | None = None
        self._fh: IO[bytes]
        if isinstance(source, (bytes, bytearray)):
            self._fh = BytesIO(bytes(source))
        elif hasattr(source, "read"):
            self._fh = cast(IO[bytes], source)
        else:
            self.path = Path(source)
            self._fh = open(self.path, "rb")
            self._own_fh = True
        self._fallback = fallback_names if fallback_names is not None else global_names()
        self._parse()

    def close(self):
        if self._own_fh:
            try:
                self._fh.close()
            except Exception:
                pass

    def _parse(self):
        fh = self._fh
        fh.seek(0)
        first = fh.read(2)
        if len(first) < 2:
            raise ValueError(f"{self.name}: too short to be a MIX archive")
        file_count = unpack("<H", first)[0]
        if file_count:  # old format: count then body size
            self.header_format = "old"
            self.data_size = unpack("<I", fh.read(4))[0]
            index_data = fh.read(4 * 3 * file_count)
        else:  # new format: flags, optional encrypted index
            self.header_format = "new"
            self.flags = unpack("<H", fh.read(2))[0]
            if (self.flags & 0x2) != 0:
                encrypted_blowfish_key = fh.read(80)
                decrypted_blowfish_key = decrypt_blowfish_key_fast(encrypted_blowfish_key)
                cipher = Cipher(decrypted_blowfish_key)
                decrypted_block = cipher.decrypt_block(fh.read(8))
                file_count, self.data_size, _ = unpack("<HIH", decrypted_block)
                remaining = (file_count * (4 * 3)) - 2
                padding = 8 - remaining % 8
                data_decrypted = b"".join(
                    cipher.decrypt_ecb(fh.read(remaining + padding))
                )
                index_data = decrypted_block[-2:] + data_decrypted[:-padding]
            else:
                file_count, self.data_size = unpack("<HI", fh.read(2 + 4))
                index_data = fh.read(4 * 3 * file_count)
        self.body_start = fh.tell()

        db_id = ra2_crc_fast(names_db_filename) & 0xFFFFFFFF
        for i in range(file_count):
            o = i * 4 * 3
            id_, offset, size = unpack("<iII", index_data[o : o + (4 * 3)])
            id_ &= 0xFFFFFFFF
            self.entries.append(MixEntry(id_, offset, size, i))

        # Resolve names from this archive's embedded local mix database first.
        for e in self.entries:
            if e.id == db_id:
                self._read_local_db(e)

        for e in self.entries:
            e.name = (
                self._names.get(e.id)
                or self._fallback.get(e.id)
                or f"_{e.id:08X}"
            )

    def _read_local_db(self, entry):
        try:
            blob = self.read_entry(entry)
        except Exception as ex:  # noqa: BLE001
            print(f"{self.name}: failed to read local filename db: {ex}", file=stderr)
            return
        try:
            # Header is 52 bytes, then NUL-separated ASCII filenames.
            names = [x.decode("latin1") for x in blob[52:].split(b"\x00") if x]
            for filename in names:
                self._names[ra2_crc_fast(filename) & 0xFFFFFFFF] = filename
        except Exception as ex:  # noqa: BLE001
            print(f"{self.name}: failed to parse local filename db: {ex}", file=stderr)

    def read_entry(self, entry) -> bytes:
        self._fh.seek(self.body_start + entry.offset)
        return self._fh.read(entry.size)

    def sflags(self):
        if self.flags & 1:
            yield "checksumed"
        if self.flags & 2:
            yield "encrypted"
        if self.flags & 0xFFFC:
            yield hex(self.flags)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


# ---------------------------------------------------------------------------
# Recursive expansion
# ---------------------------------------------------------------------------

class Leaf:
    """A non-.mix file, reachable through a chain of nested archives."""

    __slots__ = ("name", "id", "size", "chain", "archive", "entry")

    def __init__(self, name, id, size, chain, archive, entry):
        self.name = name
        self.id = id
        self.size = size
        self.chain = chain
        self.archive = archive
        self.entry = entry

    def read(self) -> bytes:
        return self.archive.read_entry(self.entry)


def iter_leaves(archive: MixArchive, chain=(), depth=0, max_depth=MAX_DEPTH,
                recurse=True, on_archive=None):
    """Yield every non-archive file, descending into nested .mix entries."""
    if on_archive is not None:
        on_archive(archive, chain, depth)
    my_chain = chain + (archive.name,)
    for entry in archive.entries:
        is_mix = entry.name.lower().endswith(".mix")
        if recurse and is_mix and depth < max_depth:
            try:
                data = archive.read_entry(entry)
                sub = MixArchive(
                    data,
                    name=entry.name,
                    fallback_names=archive._fallback,
                )
            except Exception as ex:  # noqa: BLE001
                print(f"{archive.name}: cannot descend into {entry.name}: {ex}", file=stderr)
                yield Leaf(entry.name, entry.id, entry.size, my_chain, archive, entry)
                continue
            yield from iter_leaves(
                sub, my_chain, depth + 1, max_depth, recurse, on_archive
            )
        else:
            yield Leaf(entry.name, entry.id, entry.size, my_chain, archive, entry)


def open_archives(paths, fallback_names=None):
    return [
        MixArchive(p, name=Path(p).name, fallback_names=fallback_names)
        for p in paths
    ]


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_list(args):
    archives = open_archives(args.mix_files)
    try:
        for ai, archive in enumerate(archives):
            if ai:
                print(file=stdout)
            print(f"File: {archive.name} ({archive.header_format} format)", file=stdout)
            if args.recursive:
                for leaf in iter_leaves(archive, recurse=True, max_depth=args.max_depth):
                    chain = " > ".join(leaf.chain) if args.chain else ""
                    extra = f"  [{chain}]" if chain else ""
                    print(
                        f"  {leaf.id:08X} {leaf.size:>12} {leaf.name}{extra}",
                        file=stdout,
                    )
            else:
                for entry in archive.entries:
                    print(
                        f"  {entry.index + 1:>4} {entry.id:08X} "
                        f"{entry.offset:>10} {entry.size:>10}  {entry.name}",
                        file=stdout,
                    )
                print(
                    f"   {len(archive.entries)} files, {archive.data_size} data bytes, "
                    + ", ".join(archive.sflags()),
                    file=stdout,
                )
    finally:
        for a in archives:
            a.close()


def cmd_tree(args):
    def on_archive(archive, chain, depth):
        indent = "  " * depth
        flags = ", ".join(archive.sflags())
        suffix = f" ({flags})" if flags else ""
        label = archive.name if depth == 0 else f"{archive.name}"
        print(f"{indent}{label}: {len(archive.entries)} entries{suffix}")
    archives = open_archives(args.mix_files)
    try:
        for ai, archive in enumerate(archives):
            if ai:
                print()
            # Recursion is what builds the tree; iter_leaves reports archives.
            for _leaf in iter_leaves(
                archive, recurse=True, max_depth=args.max_depth, on_archive=on_archive
            ):
                pass
    finally:
        for a in archives:
            a.close()


def cmd_extract(args):
    out_root = Path(args.extract_dir).resolve()
    out_root.mkdir(parents=True, exist_ok=True)
    archives = open_archives(args.mix_files)
    total = 0
    unnamed = 0
    try:
        for archive in archives:
            base = out_root / (Path(archive.name).stem if args.keep_nested else "")
            for leaf in iter_leaves(archive, recurse=args.recursive, max_depth=args.max_depth):
                if args.keep_nested:
                    dest_dir = base
                    for part in leaf.chain[1:]:
                        dest_dir = dest_dir / Path(part).stem
                else:
                    dest_dir = out_root
                dest_dir.mkdir(parents=True, exist_ok=True)
                data = leaf.read()
                if len(data) != leaf.size:
                    print(
                        f"short read: {leaf.name} expected {leaf.size} got {len(data)}",
                        file=stderr,
                    )
                dest = dest_dir / leaf.name
                if dest.exists() and not args.overwrite:
                    stem, dot, ext = leaf.name.rpartition(".")
                    dest = dest_dir / f"{stem}_{leaf.id:08X}{dot}{ext}"
                dest.write_bytes(data)
                total += 1
                if leaf.name.startswith("_"):
                    unnamed += 1
                if not args.quiet:
                    print(f"{leaf.id:08X} {dest}", file=stderr)
    finally:
        for a in archives:
            a.close()
    print(f"extracted {total} files to {out_root} ({unnamed} unnamed)", file=stderr)


def cmd_manifest(args):
    out = cast(IO[str], as_sink(args.output, "w"))
    archives = open_archives(args.mix_files)
    records: list[dict[str, object]] = []
    try:
        for archive in archives:
            for leaf in iter_leaves(archive, recurse=args.recursive, max_depth=args.max_depth):
                rec = {
                    "name": leaf.name,
                    "id": f"{leaf.id:08X}",
                    "size": leaf.size,
                    "chain": list(leaf.chain),
                    "named": not leaf.name.startswith("_"),
                }
                if args.hash:
                    import hashlib
                    rec["sha256"] = hashlib.sha256(leaf.read()).hexdigest()
                records.append(rec)
    finally:
        for a in archives:
            a.close()

    if args.format == "json":
        json.dump(records, out, indent=2)
        out.write("\n")
    elif args.format == "jsonl":
        for rec in records:
            out.write(json.dumps(rec) + "\n")
    else:  # csv
        cols = ["name", "id", "size", "chain", "named"]
        if args.hash:
            cols.append("sha256")
        out.write(",".join(cols) + "\n")
        for rec in records:
            row = [str(rec["name"]), str(rec["id"]), str(rec["size"]),
                   "|".join(cast(list[str], rec["chain"])), str(rec["named"])]
            if args.hash:
                row.append(str(rec["sha256"]))
            out.write(",".join(row) + "\n")
    named = sum(1 for r in records if r["named"])
    print(
        f"{len(records)} leaf files ({named} named, {len(records) - named} unnamed)",
        file=stderr,
    )


def create(mix_file="file.mix", files=None, names_db=None, game="RA2_YR"):
    from shutil import copyfileobj

    if files is None:
        files = []
    file_map: dict[str, tuple[str | bytes, int]] = {}

    def add_path(f=""):
        p = Path(f)
        if p.is_dir():
            for q in sorted(p.rglob("*")):
                if q.is_file():
                    add_path(str(q))
        elif p.is_file():
            file_map[p.name] = (str(p), p.stat().st_size)

    for f in files:
        if f == "-":
            for v in sys.stdin:
                v = v.strip()
                if v and not v.startswith("#"):
                    add_path(v)
        else:
            add_path(f)

    if names_db is not False and names_db_filename not in file_map:
        db_names = list(file_map.keys()) + [names_db_filename]
        db_data = b"".join(names_db_enum(db_names, get_game(game)))
        file_map[names_db_filename] = (db_data, len(db_data))

    data_map = sorted(
        [(ra2_crc_fast(k), (*v, k)) for k, v in file_map.items()],
        key=(lambda id, *_: id),
    )
    flags = 0
    file_count = len(data_map)
    data_size = sum(size for id, (file, size, name) in data_map)
    assert file_count > 0
    assert data_size > 0
    print(f"{mix_file}", file=stderr)
    with cast(IO[bytes], as_sink(mix_file)) as out:
        out.write(pack("=I H I", flags, file_count, data_size))
        offset = 0
        for id_, (file, size, name) in data_map:
            out.write(pack("=iII", id_, offset, size))
            offset += size
        for id_, (file, size, name) in data_map:
            print(f" - {id_ & 0xFFFFFFFF:08X} {size}b {name}", file=stderr)
            if isinstance(file, str):
                n = out.tell()
                with open(file, "rb") as inp:
                    copyfileobj(inp, out)
                if n >= 0:
                    assert (out.tell() - n) == size
            else:
                assert isinstance(file, bytes)
                assert len(file) == size
                out.write(file)


def cmd_create(args):
    create(
        mix_file=args.mix_file,
        files=args.files,
        names_db=args.names_db,
        game=args.game,
    )


def cmd_mount(args):
    try:
        from fuse import FUSE, FuseOSError, Operations
    except Exception:  # noqa: BLE001
        print("mount requires the 'fusepy' package", file=stderr)
        return 2
    import stat as statmod

    archives = open_archives(args.mix_files)
    # Merged view: later archives override earlier ones, loose files override all.
    files: dict[str, Leaf | Path] = {}
    for archive in archives:
        for leaf in iter_leaves(archive, recurse=True, max_depth=args.max_depth):
            files[leaf.name] = leaf
    if args.loose_dir:
        root = Path(args.loose_dir)
        for p in root.rglob("*"):
            if p.is_file():
                files[p.relative_to(root).as_posix()] = p

    class MergedVFS(Operations):
        def getattr(self, path, fh=None):
            clean = path.lstrip("/")
            if path == "/":
                return dict(st_mode=(statmod.S_IFDIR | 0o555), st_nlink=2)
            if clean in files:
                obj = files[clean]
                size = obj.stat().st_size if isinstance(obj, Path) else obj.size
                return dict(st_mode=(statmod.S_IFREG | 0o444), st_nlink=1, st_size=size)
            raise FuseOSError(2)

        def readdir(self, path, fh):  # type: ignore[override]
            yield "."
            yield ".."
            if path == "/":
                yield from sorted(files)

        def read(self, path, size, offset, fh):  # type: ignore[override]
            clean = path.lstrip("/")
            if clean not in files:
                raise FuseOSError(2)
            obj = files[clean]
            data = obj.read_bytes() if isinstance(obj, Path) else obj.read()
            return data[offset : offset + size]

    print(
        f"mounting {len(archives)} archive(s), {len(files)} merged files at {args.mount_point}",
        file=stderr,
    )
    FUSE(MergedVFS(), args.mount_point, foreground=True, ro=True, allow_other=args.allow_other)
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser():
    cli = ArgumentParser(prog="ra2mix", description="List, extract, create, mount MIX files")
    cli.set_defaults(_what="")
    subparsers = cli.add_subparsers()

    def add_common(p):
        p.add_argument("mix_files", help="path to the .mix files", nargs="+")

    sub = subparsers.add_parser("list", aliases=("l",), help="list entries")
    add_common(sub)
    sub.add_argument("-r", "--recursive", action="store_true",
                     help="descend into nested .mix archives")
    sub.add_argument("--chain", action="store_true",
                     help="with -r, show the archive chain for each file")
    sub.add_argument("--max-depth", type=int, default=MAX_DEPTH)
    sub.set_defaults(_what="l", func=cmd_list)

    sub = subparsers.add_parser("tree", help="show the archive hierarchy")
    add_common(sub)
    sub.add_argument("--max-depth", type=int, default=MAX_DEPTH)
    sub.set_defaults(_what="t", func=cmd_tree)

    sub = subparsers.add_parser("extract", aliases=("x",), help="extract entries")
    add_common(sub)
    sub.add_argument("-d", dest="extract_dir", metavar="DIR", default=".")
    sub.add_argument("-r", "--recursive", action="store_true",
                     help="descend into nested .mix archives")
    sub.add_argument("--keep-nested", action="store_true",
                     help="preserve the nested archive structure as directories")
    sub.add_argument("--overwrite", action="store_true",
                     help="overwrite colliding names instead of de-duplicating")
    sub.add_argument("--max-depth", type=int, default=MAX_DEPTH)
    sub.add_argument("-q", "--quiet", action="store_true")
    sub.set_defaults(_what="x", func=cmd_extract)

    sub = subparsers.add_parser("manifest", help="emit a machine-readable inventory")
    add_common(sub)
    sub.add_argument("-o", "--output", default="-", help="output file (default stdout)")
    sub.add_argument("-r", "--recursive", action="store_true", default=True)
    sub.add_argument("--no-recursive", dest="recursive", action="store_false")
    sub.add_argument("--format", choices=["csv", "json", "jsonl"], default="csv")
    sub.add_argument("--hash", action="store_true", help="include sha256 (slow)")
    sub.add_argument("--max-depth", type=int, default=MAX_DEPTH)
    sub.set_defaults(_what="m", func=cmd_manifest)

    sub = subparsers.add_parser("create", aliases=("c",), help="create a MIX archive")
    sub.add_argument("--no-names-db", action="store_false", dest="names_db", default=None)
    sub.add_argument("--game", default="RA2_YR")
    sub.add_argument("mix_file")
    sub.add_argument("files", nargs="+")
    sub.set_defaults(_what="c", func=cmd_create)

    sub = subparsers.add_parser("mount", help="FUSE-mount archives (recursive, merged)")
    add_common(sub)
    sub.add_argument("mount_point")
    sub.add_argument("--loose-dir", help="directory of loose files that override archives")
    sub.add_argument("--max-depth", type=int, default=MAX_DEPTH)
    sub.add_argument("--allow-other", action="store_true")
    sub.set_defaults(_what="M", func=cmd_mount)

    return cli


def main(argv=None):
    args = build_parser().parse_args(argv)
    func = getattr(args, "func", None)
    if func is None:
        build_parser().print_help()
        return 1
    return func(args)


if __name__ == "__main__":
    sys.exit(main())
