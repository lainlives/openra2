import binascii
from argparse import ArgumentParser
from pathlib import Path
from struct import pack, unpack
from sys import stderr, stdin
from typing import IO

from blowfish import Cipher

names_db_filename = "local mix database.dat"


SIZE_OF_ENCRYPTED_KEY = 80


def decrypt_blowfish_key_fast(encrypted_blowfish_key):
    BLOCK_SIZE = 40
    PUBLIC_EXPONENT = 65537
    PUBLIC_MODULUS = 681994811107118991598552881669230523074742337494683459234572860554038768387821901289207730765589

    if len(encrypted_blowfish_key) < SIZE_OF_ENCRYPTED_KEY:
        raise ValueError("Buffer is not long enough")

    block1 = encrypted_blowfish_key[:40]
    block2 = encrypted_blowfish_key[40:80]

    result_parts = []

    for encrypted_block in (block1, block2):
        decrypted_int = pow(
            int.from_bytes(encrypted_block, "little"), 65537, PUBLIC_MODULUS
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
    """RA2 CRC"""
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
    from sys import stdout

    return stdout.buffer if "b" in mode else stdout


def as_source(path="-", mode="rb"):
    if path and path != "-":
        return open(path, mode)
    from sys import stdin

    return stdin.buffer if "b" in mode else stdin


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


def create(mix_file="file.mix", files=[""], names_db=None, game="RA2_YR", _what="c"):
    from shutil import copyfileobj

    file_map: dict[str, tuple[str, int]] = {}

    def add_path(f=""):
        p = Path(f)
        if p.is_dir():
            for q in p.rglob("*"):
                add_path(str(q))
        else:
            file_map[p.name] = (str(p), p.stat().st_size)

    for f in files:
        if f == "-":
            for v in stdin:
                if f := v.strip() and not f.startswith("#"):
                    add_path(f)
        else:
            add_path(f)

    if names_db is not False and not file_map.get(names_db_filename):
        file_map[names_db_filename] = (None, None)
        db_data = b"".join(names_db_enum(list(file_map.keys()), get_game(game)))
        file_map[names_db_filename] = (db_data, len(db_data))
    data_map = sorted(
        [(ra2_crc_fast(k), (*v, k)) for k, v in file_map.items()],
        key=(lambda id, *_: id),
    )
    # mixing
    flags = 0
    file_count = len(data_map)
    data_size = sum(size for id, (file, size, name) in data_map)
    assert file_count > 0
    assert data_size > 0
    # return
    print(f"{mix_file}", file=stderr)
    with as_sink(mix_file) as out:
        # header
        out.write(pack("=I H I", flags, file_count, data_size))
        # list
        offset = 0
        for id, (file, size, name) in data_map:
            out.write(pack("=iII", id, offset, size))
            offset += size
        # data
        for id, (file, size, name) in data_map:
            print(f" - {id & 0xFFFFFFFF:08X} {size}b {name}", file=stderr)
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


def walk(
    fp: "IO[bytes]",
    out: "IO",
    id_name_map={},
    sort_offset="",
    file_offset=0,
    extract_dir="",
    mix_db_id=0,
):
    file_count = unpack("<H", fp.read(2))[0]
    flags = 0
    if file_count:  # old
        data_size = unpack("<I", fp.read(4))[0]
        index_data = fp.read(4 * 3 * file_count)
    else:  # new
        flags = unpack("<H", fp.read(2))[0]
        if (flags & 0x2) != 0:
            encrypted_blowfish_key = fp.read(80)
            decrypted_blowfish_key = decrypt_blowfish_key_fast(encrypted_blowfish_key)
            cipher = Cipher(decrypted_blowfish_key)
            decrypted_block = cipher.decrypt_block(fp.read(8))
            file_count, data_size, _ = unpack("<HIH", decrypted_block)
            remaining = (file_count * (4 * 3)) - 2
            padding = 8 - remaining % 8
            data_decrypted = b"".join(cipher.decrypt_ecb(fp.read(remaining + padding)))
            index_data = decrypted_block[-2:] + data_decrypted[:-padding]
        else:
            file_count, data_size = unpack("<HI", fp.read(2 + 4))
            index_data = fp.read(4 * 3 * file_count)
    body_start = fp.tell()

    def sflag(n):
        if n & 1:
            yield "checksumed"
        if n & 2:
            yield "encrypted"
        if n & 0xFFFC:
            yield hex(n)

    max_offset = max_size = 0
    id_filename_map = {}
    entries = []
    for i in range(file_count):
        o = i * 4 * 3
        id, offset, size = unpack("<iII", index_data[o : o + (4 * 3)])
        max_offset = max(max_offset, offset)
        max_size = max(max_size, size)
        entries.append((id & 0xFFFFFFFF, offset, size, i))
        if id == mix_db_id:
            fp.seek(file_offset + body_start + offset)
            local_mix_db_blob = fp.read()
            try:
                filenames = [
                    x.decode("latin1") for x in local_mix_db_blob[52:].split(b"\x00")
                ]
                filenames and id_filename_map.update(
                    {
                        ra2_crc_fast(filename) & 0xFFFFFFFF: filename
                        for filename in filenames
                    }
                )
            except Exception as ex:
                print(
                    f"Failed to get local filenames db : {ex}",
                    file=stderr,
                )
    if extract_dir:
        block_size = 4096
        d = Path(extract_dir).resolve()
        d.mkdir(exist_ok=True, parents=True)
        for id, offset, size, i in entries:
            fname = id_filename_map.get(id) or id_name_map.get(id) or f"_{id:08X}"
            fp.seek(file_offset + body_start + offset)
            n = size
            p = d / fname
            with p.open("bw") as w:
                while n > 0:
                    c = min(block_size, n)
                    w.write(fp.read(c))
                    n -= c
            print(f"{id:08X} {p}", file=stderr)
    else:
        if sort_offset:
            entries.sort(key=lambda v: v[1], reverse=(sort_offset[0] == "d"))
        pad_nth = len(str(file_count))
        pad_offset = len(str(max_offset))
        pad_size = len(str(max_size))
        head = ["nth", "id", "offset", "size", "name"]
        print(
            f"{head[0]} {head[1]:>{pad_nth + 8 - len(head[0])}} {head[2]:>{pad_offset}} {head[3]:>{pad_size}} {head[4]}",
            file=out,
        )
        for id, offset, size, i in entries:
            fname = id_filename_map.get(id) or id_name_map.get(id) or f"_{id:08X}"
            print(
                f"{(i + 1):>{pad_nth}} {id:08X} {offset:>{pad_offset}} {size:>{pad_size}}",
                fname,
                file=out,
            )
        print(
            "   ",
            ", ".join(
                [f"{file_count} files", f"{data_size} data bytes", *sflag(flags)]
            ),
            file=out,
        )


def list_files(mix_files=[], sort_offset="", extract_dir="", _what=""):
    from sys import stdout

    from util.names import names as names_map

    MIX_DB_ID = ra2_crc_fast(names_db_filename)
    # names = set(names_map.keys())
    id_name_map = dict(
        (ra2_crc_fast(filename) & 0xFFFFFFFF, filename) for filename in names_map
    )
    sname = set()
    sext = set()
    for filename in names_map.keys():
        name, dot, ext = filename.rpartition(".")
        sname.add(name)
        sext.add(ext)
    for x in sext:
        for n in sname:
            f = f"{n}.{x}"
            g = ra2_crc_fast(f)
            id_name_map[g & 0xFFFFFFFF] = f
    for i, mix_file in enumerate(mix_files):
        if i > 0:
            print(file=stdout)
        print("File:", mix_file, file=stdout)
        try:
            with as_source(mix_file) as fh:
                walk(
                    fh,
                    stdout,
                    id_name_map,
                    sort_offset=sort_offset,
                    extract_dir=extract_dir,
                    mix_db_id=MIX_DB_ID,
                )
        except BrokenPipeError:
            break


def main():

    cli = ArgumentParser(prog="ra2mix", description="List, extract, create MIX files")
    cli.set_defaults(_what="")

    subparsers = cli.add_subparsers()
    # list
    sub = subparsers.add_parser("list", aliases=("l"), help="list files of MIX file")
    sub.add_argument("mix_files", help="path to the .mix files", nargs="+")
    sub.add_argument(
        "--sort-by-offset",
        help="Sort entries by offset",
        dest="sort_offset",
        choices=["ascending", "descending", "a", "d"],
    )
    sub.set_defaults(_what="l")
    # extract
    sub = subparsers.add_parser(
        "extract", aliases=("x",), help="extract files of MIX file"
    )
    sub.add_argument("mix_files", help="path to the .mix files", nargs="+")
    sub.add_argument(
        "-d",
        help="extract files into DIR",
        dest="extract_dir",
        metavar="DIR",
        default=".",
    )
    sub.set_defaults(_what="x")
    # create
    sub = subparsers.add_parser(
        "create", aliases=("c",), help="create MIX file from files"
    )
    sub.add_argument(
        "--no-names-db",
        help="Dont add 'local mix database.dat'",
        action="store_false",
        dest="names_db",
        default=None,
    )
    sub.add_argument(
        "--game",
        help="which game TD, RA, TS, DUNE2, DUNE2000, RA2, RA2_YR, RG, GR, GR_ZH, EBFD, NOX, BFME, BFME2, TW, TS_FS, UNKNOWN",
        default="RA2_YR",
    )
    sub.add_argument("mix_file", help="the mix file to create")
    sub.add_argument("files", help="files to include", nargs="+")
    sub.set_defaults(_what="c")
    # parse
    ns = cli.parse_args().__dict__
    what = ns.pop("_what") or ""
    if what.startswith("c"):
        create(**ns)
    elif what.startswith("l"):
        list_files(**ns)
    else:
        list_files(**ns)


if __name__ == "__main__":
    main()
#
