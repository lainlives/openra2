# Test assets wanted

CI must stay asset-free (see `AGENTS.md`), so format tests need small fixtures
that we are allowed to redistribute. The goal is one tiny, clearly-licensed
sample per format and per interesting case, used by unit tests and the tools.

## Rules for accepting an asset

- The author must grant redistribution, in writing in the repository, under a
  license compatible with GPL-3.0-or-later: CC0, CC-BY, MIT, BSD, GPL. Record
  the source and license next to the file (for example `tests/data/LICENSE`).
- Keep samples small (ideally a few KB) and strip anything unrelated (no audio
  in a graphics sample, one frame unless frames are the point).
- Never include retail Westwood/EA art, extracted archives, original binaries,
  or anything of unclear provenance.

Some formats we can generate ourselves with our own tooling (`scripts/mixer.py`
writes MIX; the engine writes PPM). Those need no external source and are the
preferred test vectors where possible.

## Current fixtures

`tests/data/` already holds synthesized, CC0 fixtures and is exercised by
`tests/test_data.cpp`:

- SHP: raw (type 1) and RLE-Zero (type 3), 1/4/8 frames. Type 2 is still
  wanted.
- Maps: an 80x80 map with triggers and `[Lighting]`; a dense 100x100 map in
  plain and `.yro` form (the test asserts they expand identically); a large
  near-limit `.mpr`.
- Strings: `test.csf` (version 3, two entries) with its expected contents.

`tests/data/generate/generate-testing-data.py` generates VXL/HVA/PCX/CPS
fixtures; those stay uncommitted until the matching decoders exist. The goal is
to generate fixtures from scripts and keep the directory generated rather than
checked in.

## Formats and cases

| Format | Extension | Cases wanted | Can we author it? |
|---|---|---|---|
| MIX | `.mix` | old format; new format; one nested MIX; a `local mix database.dat` | yes (mixer.py) |
| SHP | `.shp` | raw (type 1); counted rows (type 2); RLE-Zero (type 3); cropped frame; multi-frame; transparency | partially (we write SHP?) |
| TMP terrain | `.tmp`, `.tem`, `.sno`, `.urb`, `.ubn`, `.des`, `.lun` | 1x1; multi-cell (2x2, 3x3); `HasExtraData` extra graphics; height map | partially |
| Palette | `.pal` | plain 768-byte VGA palette | yes (hand-written bytes) |
| Voxel | `.vxl` + `.hva` | one small voxel model with its transform | no, external |
| Image | `.pcx`, `.cps` | one each, 8-bit palette | yes (author with a tool) |
| Font | `.fnt` | one bitmap font | no, external |
| Strings | `.csf` | small string table with values | no, external |
| INI | `.ini` | rules/art/theater fragments | yes (hand-written) |
| Maps | `.map`, `.mpr`, `.yrm` | one tiny map each version; one `.mmx`/`.yro` packaged map | partially (author tiny maps) |
| Audio | `.wav`, `.aud`, `.voc` | short clip each of WAV, Westwood AUD, VOC | WAV yes; AUD/VOC external |
| Video | `.vqa`, `.bik` | one tiny clip each | no, external; low priority |
| Cursor | `.cur`, `.ani` | one static, one animated | yes (author) |

Formats are decoded in this order of priority: MIX, SHP, TMP, PAL, map, INI,
VXL/HVA, PCX/CPS, CSF, AUD/VOC/WAV. VQA/BINK are last because a correct decoder
is a large amount of work for little test value early on.

## External sources to ask

- ModEnc / PPM / XCC community members who have published format tools, if they
  will license a small sample.
- Open-source C&C-adjacent projects that already ship permissively licensed
  placeholder art compatible with these formats.
- Artists who can produce simple originals in these formats (a 32x32 SHP, a
  1x1 terrain tile, a small voxel).
