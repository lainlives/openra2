#!/usr/bin/python
import struct

from PIL import Image


def generate_ra2_voxel(filename="test_unit.vxl", size_x=20, size_y=20, size_z=10):
    # 1. Red Alert 2 VXL Header Specs
    file_id = b"Voxel Animation"
    num_headers = (
        1  # We are generating a single-section voxel (e.g., chassis or turret)
    )
    num_bodies = 1
    total_body_size = 0  # Will be calculated dynamically
    palette_size = (
        0  # Unused in RA2 header (the game forces its own unit/theater palette)
    )

    # Section-specific Header information
    section_name = b"Chassis".ljust(16, b"\x00")
    section_id = 0

    # 2. Mock a 3D volume grid (X, Y, Z)
    # Index 144 is generally a standard gray/green in the RA2 unit palette.
    # Normal index 0 points dummy up.
    grid = {}
    for x in range(size_x):
        for y in range(size_y):
            for z in range(size_z):
                # Create a simple solid bounding cube for the unit
                if x in (0, size_x - 1) or y in (0, size_y - 1) or z in (0, size_z - 1):
                    color_index = 144
                    normal_index = 0
                    grid[(x, y, z)] = (color_index, normal_index)

    # 3. Build Body Tail (Span Allocations)
    # The format maps columns (X, Y) containing vertical lists of voxel layers (Z)
    span_data = b""
    start_offsets = []

    for x in range(size_x):
        for y in range(size_y):
            # Record where this specific column's raw binary payload begins
            start_offsets.append(len(span_data))

            # Find continuous blocks of active voxels along the Z-axis
            z = 0
            while z < size_z:
                if (x, y, z) in grid:
                    z_start = z
                    # Find out how long this voxel stack stretches
                    while z < size_z and (x, y, z) in grid:
                        z += 1
                    z_end = z - 1

                    # Pack a voxel cluster line segment ("span")
                    # Structure: [Z-Start, Z-End] + [Color, Normal]*Length + [Duplicate Z-Start, Duplicate Z-End]
                    span_head = struct.pack("BB", z_start, z_end)
                    span_body = b""
                    for sz in range(z_start, z_end + 1):
                        col, norm = grid[(x, y, sz)]
                        span_body += struct.pack("BB", col, norm)

                    span_data += span_head + span_body + span_head
                else:
                    z += 1

            # Append 4 bytes of termination flag (\x00\x00\x00\x00) indicating column complete
            span_data += b"\x00\x00\x00\x00"

    total_body_size = len(span_data)

    # 4. Compile the Final VXL Binary File
    with open(filename, "wb") as f:
        # Write Main File Header (Size: 28 bytes)
        f.write(
            struct.pack(
                "<16sIIII",
                file_id,
                num_headers,
                num_bodies,
                total_body_size,
                palette_size,
            )
        )

        # Write Section Header (Size: 28 bytes)
        f.write(struct.pack("<16sIII", section_name, section_id, num_bodies, 0))

        # Write Section Dimensions & Metadata (Size: 76 bytes)
        # Includes voxel boundaries and scaling vectors
        f.write(struct.pack("<III", size_x, size_y, size_z))  # Lat/Max dimensions
        f.write(
            struct.pack("<fff", size_x / 2, size_y / 2, size_z / 2)
        )  # Center bounds
        f.write(struct.pack("<fff", 1.0, 1.0, 1.0))  # Scale multipliers
        f.write(b"\x00" * 28)  # Required blank padding bytes

        # Write Span Lookup Tables (Mappers)
        # Tells the game engine where each column chunk starts in the file
        for offset in start_offsets:
            f.write(struct.pack("<I", offset))

        # Write end offsets lookup array
        for offset in start_offsets:
            f.write(struct.pack("<I", offset))  # Simplified mapper logic

        # Append the raw 3D body payload
        f.write(span_data)

    print(f"Successfully generated Red Alert 2 Voxel file: {filename}")


def generate_ra2_hva(filename="test_unit.hva", num_sections=1, num_frames=1):
    # 1. HVA Header Configuration (16 bytes)
    # File ID must be 16 bytes long, padded with null characters.
    file_id = b"Voxel Animation".ljust(16, b"\x00")

    # 2. Main Transformation Matrix data
    # An identity matrix structure ensures no scaling, no rotation, and zero offset.
    # Layout: R_00, R_01, R_02, R_10, R_11, R_12, R_20, R_21, R_22
    identity_rotation = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    # Translation vector: X-offset, Y-offset, Z-offset
    zero_translation = [0.0, 0.0, 0.0]

    # Flatten the rotation matrix and translation vector together into 12 floats (48 bytes)
    matrix_payload = identity_rotation + zero_translation

    with open(filename, "wb") as f:
        # Write the File ID Header
        f.write(file_id)

        # Write structural integers (Number of Frames, Number of Sections)
        f.write(struct.pack("<II", num_frames, num_sections))

        # Write Section Names (16 bytes per section, padded with nulls)
        # MUST exactly match the section name in your .vxl file (e.g., "Chassis")
        section_name = b"Chassis".ljust(16, b"\x00")
        f.write(section_name)

        # Write Matrix blocks
        # Loops for every frame, for every section inside that frame.
        for frame in range(num_frames):
            for section in range(num_sections):
                # Pack 12 float fields ('<12f')
                f.write(struct.pack("<12f", *matrix_payload))

    print(f"Successfully generated Red Alert 2 Animation file: {filename}")


def generate_ra2_pcx(filename="test_cameo.pcx", width=60, height=48):
    # 1. Create a blank 8-bit palette-indexed image
    img = Image.new("P", (width, height), color=0)

    # 2. Inject a dummy 256-color palette (RGB triplets)
    # Red Alert 2 requires a specific unit/cameo palette, but for CI testing,
    # we just need a structurally valid 256-color array.
    dummy_palette = []
    for i in range(256):
        dummy_palette.extend([i, i, i])  # Greyscale ramp
    img.putpalette(dummy_palette)

    # 3. Draw a placeholder graphic (e.g., a simple bounding frame)
    pixels = img.load()
    for x in range(width):
        for y in range(height):
            if x == 0 or x == width - 1 or y == 0 or y == height - 1:
                pixels[x, y] = 255  # White border
            elif (x + y) % 8 == 0:
                pixels[x, y] = 144  # Pattern fill

    # 4. Save directly as PCX
    img.save(filename, format="PCX")
    print(f"Generated RA2-compatible PCX: {filename}")


def generate_legacy_cps(filename="test_screen.cps", width=320, height=200):
    # .cps images are fixed 320x200x256 color grids (64,000 bytes uncompressed)
    uncompressed_size = width * height

    # Generate dummy image bytes (stripes of pattern indexes)
    raw_pixels = bytearray()
    for y in range(height):
        for x in range(width):
            raw_pixels.append((x + y) % 256)

    # Pack the Westwood CPS Header
    # Word (FileSize), Word (CompressionType), DWord (UncompressedSize), DWord (PaletteFlag)
    # CompressionType: 0x0000 = Uncompressed / Raw
    compression_type = 0x0000
    palette_flag = 0x00000000  # 0 means use external/mix palette data

    header_size = 12
    total_file_size = header_size + len(raw_pixels)

    header = struct.pack(
        "<HHHHI",
        total_file_size - 2,  # Westwood header sizing offset
        compression_type,
        uncompressed_size & 0xFFFF,  # Low word of uncompressed size
        (uncompressed_size >> 16) & 0xFFFF,  # High word of uncompressed size
        palette_flag,
    )

    with open(filename, "wb") as f:
        f.write(header)
        f.write(raw_pixels)

    print(f"Generated Westwood-structured CPS: {filename}")


if __name__ == "__main__":
    generate_ra2_voxel()
    generate_ra2_hva()
    generate_ra2_pcx()
    generate_legacy_cps()
