#!/usr/bin/env python
import os
import stat
import struct
import sys
from errno import ENOENT

from fuse import FUSE, FuseOSError, Operations


class WestwoodMixVFS(Operations):
    def __init__(self, mix_path, database_ini=None):
        self.mix_path = mix_path
        self.files = {}  # Format: { "filename": {"offset": x, "size": y} }

        # Load local filename-to-hash mappings if available (Westwood uses 32-bit ID hashes)
        self.hash_lookup = self._load_hash_database(database_ini)
        self._parse_mix_header()

    def _load_hash_database(self, database_ini):
        """Maps filename text to Westwood 32-bit CRC hashes so they display semantically."""
        # You can populate this with known filenames like 'rulesmd.ini', 'artmd.ini'
        # If a hash isn't found, we fall back to displaying the raw hex string.
        return {
            0x32A6F1D4: "rulesmd.ini",  # Example theoretical hash
            0x4B5F12C0: "artmd.ini",
        }

    def _parse_mix_header(self):
        """Reads the binary header of a Red Alert 2 / YR MIX file."""
        with open(self.mix_path, "rb") as f:
            # Check for standard encryption flags or basic body count
            # Westwood structure: [2 bytes num_files] [4 bytes total_body_size]
            header_bytes = f.read(6)
            num_files, total_size = struct.unpack("<HI", header_bytes)

            # Start of index map calculation
            # Each entry is 12 bytes: [4 bytes ID Hash] [4 bytes Offset] [4 bytes Size]
            header_offset = 6

            for i in range(num_files):
                f.seek(header_offset + (i * 12))
                file_id, offset, size = struct.unpack("<III", f.read(12))

                # Resolve hash name or use fallback hex string
                filename = self.hash_lookup.get(file_id, f"file_{hex(file_id)}.bin")

                # Actual file position on disk = header_offset + (num_files * 12) + index offset
                physical_offset = header_offset + (num_files * 12) + offset

                self.files[filename] = {"offset": physical_offset, "size": size}
        print(
            f"📦 Successfully indexed {len(self.files)} files inside {os.path.basename(self.mix_path)}"
        )

    # --- FUSE OPERATIONS CORE INTERFACE ---
    def getattr(self, path, fh=None):
        """Defines structural properties (permissions/sizes) for the virtual directory."""
        clean_path = path.lstrip("/")

        # Root directory properties
        if path == "/":
            return dict(st_mode=(stat.S_IFDIR | 0o555), st_nlink=2)

        # Virtual file properties mapped out of the packed archive
        if clean_path in self.files:
            file_info = self.files[clean_path]
            return dict(
                st_mode=(stat.S_IFREG | 0o444),  # Read-Only regular file
                st_nlink=1,
                st_size=file_info["size"],
            )

        raise FuseOSError(ENOENT)

    def readdir(self, path, fh):
        """Returns the virtual contents when 'ls' is called on the mountpoint."""
        direntries = [".", ".."]
        if path == "/":
            direntries.extend(self.files.keys())
        for entry in direntries:
            yield entry

    def read(self, path, size, offset, fh):
        """Streams raw byte slices on-demand when a file is viewed or grepped."""
        clean_path = path.lstrip("/")
        if clean_path not in self.files:
            raise FuseOSError(ENOENT)

        file_info = self.files[clean_path]

        # Enforce target file bounds bounds
        if offset >= file_info["size"]:
            return b""

        actual_seek_pos = file_info["offset"] + offset
        read_length = min(size, file_info["size"] - offset)

        with open(self.mix_path, "rb") as f:
            f.seek(actual_seek_pos)
            return f.read(read_length)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python mix_vfs.py <path_to_mix> <mount_point>")
        sys.exit(1)

    mix_file = sys.argv[1]
    mount_dir = sys.argv[2]

    # Run the virtual loop
    FUSE(WestwoodMixVFS(mix_file), mount_dir, foreground=True, ro=True)
