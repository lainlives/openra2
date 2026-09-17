# Test fixtures

Small, self-made files used by the asset-free tests. They contain **no retail
game data** and nothing derived from it; every file here was synthesized for
this project.

## Layout

- `shp/` - SHP sprites. `1frame-uncompressed` and `uncompressed-4frame` use
  compression type 1 (raw); `1frame-compressed` and `compressed-8frame-simple`
  use type 3 (RLE-Zero). Type 2 (counted rows, no zero runs) is not covered yet.
- `maps/` - authored maps. `test_80x80_scripting+lighting.yrm` carries triggers
  and a `[Lighting]` block; `test_newurban.yrm`/`.yro` are the same dense map in
  plain and MIX-packaged form; `test_vanilla_engine_limit_256x256.mpr` is a
  large, empty map near the original engine's limit.
- `strings/test.csf` - a string table with a long/awkward string and a
  `TXT_BAD_MAP` entry; `csf.test.md` records its expected contents.
- `generate/generate-testing-data.py` - generates VXL/HVA/PCX/CPS fixtures.
  The generated files are not committed while the corresponding decoders are
  still missing; once they exist the plan is to generate fixtures from scripts
  and keep this directory generated rather than checked in.

## Provenance and license

Synthesized by the project author and contributors for this repository. See
`LICENSE` in this directory. If a file here is ever found to be incorrect or of
unclear origin, delete it rather than keep it.