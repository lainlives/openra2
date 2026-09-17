# Found files: unidentified and anomalous retail entries

This is the place to record archive entries that have no name in any known
filename database, or whose contents do not match what their archive should
hold. Some are clearly leftovers from Westwood's own tooling. The goal is to
eventually identify every one and, where a file is unique, preserve a decoded
copy for the record.

## Why these exist

Red Alert 2 and Yuri's Revenge were built after the engine was forked from
Tiberian Sun, and development tooling changed hands mid-project. The community
understanding is that Westwood lost parts of their RA2 tooling and even more of
the YR tooling, and fell back on unofficial tools (XCC was, for a long time,
the only third-party MIX editor and the first to support Tiberian Sun). Files
that only those tools could address ended up embedded in retail archives
without a name that the retail name databases (or the binaries' own string
data) know. That makes them invisible to any tool that resolves by name.

## How to identify an entry

1. List the archive and note `_<HASH>` entries (`scripts/mixer.py list` or
   `ra2yr --mix-list`).
2. Search known filename lists against the hash
   (`scripts/names_from_binary.py "<game>/gamemd.exe"`, plus the community name
   database). Our tooling resolves 13,010 of 13,015 leaf entries this way.
3. For the rest, identify by content: first bytes, section names, SHP/TMP/MIX
   magic, and cross-reference with other entries in the same archive.

## The seven unnamed entries

All were found by recursively extracting the retail archives. Sizes are the
archive entry sizes.

| Entry | Archive | Size | Identification | Status |
|---|---|---|---|---|
| `_439FEA47` | `maps02.mix` | 110,638 | **Tiberian Sun-era GDI mission map**; `NextScenario=GDI2A.map`, TS `[Lighting]`/`[SpecialFlags]`, TS tile ids | identified, see below |
| `_A75B3F2B` | `expandmd01.mix` | 175,832 | **`WAITCYLO.SHP`** (name recovered from `gamemd.exe` strings); 22-frame 247x32 animation | identified, see below |
| `_60DE6674` | `expandmd01.mix` | 768 | **`WAITCYLO.PAL`** (same source); a 768-byte 256-colour palette | identified |
| `_9498E004` | `multimd.mix` | 137,823 | A map: `[Map]` + `[IsoMapPack5]`, `Theater=URBAN`, `Size=0,0,85,95`, `NewINIFormat=4` | type known, name unknown |
| `_94C4BDFA` | `multimd.mix` | 113,837 | A map: `[Map]` + `[IsoMapPack5]`, `Theater=TEMPERATE`, `Size=0,0,90,60`, `LocalSize=2,4,86,50` | type known, name unknown |
| `_61B60AB4` | `multimd.mix` | 264,686 | An INI with 40 sections of light definitions (`[NEGLAMP] Name=Negative Light Post Image=GALITE LightVisibility=5000 LightIntensity=-0.15`), plus `[SpecialFlags]`, `[Houses]`, country sections | anomalous; looks like a merged scenario/art fragment |
| `_96400CD4` | `wdt.mix` | 768 | 768-byte palette (RGB triplets) | type known, name unknown |

## The prototype map (`maps02.mix` / `_439FEA47`)

A Tiberian Sun-era GDI mission embedded in a Red Alert 2 archive. Notable data:

- `[Basic] NextScenario=GDI2A.map`, `AltNextScenario=GDI9C.MAP`, `Official=yes`,
  `Player=SovStar House`.
- `[Countries]`: `President`, `SecretService`, `USA1`, `USA2`, `SovStar`,
  `SovReg`; 50 structures, 14 infantry, 6 units, 34 waypoints.
- `[Lighting]`: TS ambient/colour values plus `IonAmbient`, `IonRed`,
  `IonGreen`, `IonBlue`; `[SpecialFlags]` includes `IonStorms=no`.
- 90x65 with 11,635 cells, 7,070 non-empty. Its tile ids resolve to TS-era
  graphics (`mclif*`, `mdrod*`, `hyte*`) that Red Alert 2 ships **only as the
  desert theater** (`isodes.mix`). Rendering therefore needs
  `--theater DESERT`; all non-empty cells then resolve.
- Preview: `reference/proto-map-preview.png`.

This map cannot be loaded by the retail RA2/YR engine as shipped, which is why
it went unnoticed for 25 years.

## The wait animation (`expandmd01.mix`)

- `_A75B3F2B` = `WAITCYLO.SHP`: full-frame 247x32, 22 frames, all stored with
  SHP compression type 2 (counted rows, no zero-RLE). The averaged radar colour
  pulses (R: 95, 83, 80, 84, 93, 103, 109, 113, 115, 114, 112, 94, ...) - a
  pulsing wait/loading animation.
- `_60DE6674` = `WAITCYLO.PAL`: the matching 768-byte palette; entries 0 and 1
  are magenta, which suggests index 0/1 is a mask colour for this asset.
- Preview of all frames: `reference/waitcylo-preview.png`.

## Two `multimd.mix` maps: name search result

The names could not be recovered. We hashed roughly 103,000 candidate names
(every file name in the retail install, the extracted archives, and the
community name database, each tried with `.map`/`.mpr`/`.yrm`/`.mmx`/`.yro`
and no extension) against both entry IDs with no match, and neither entry's
size matches any named entry in the archive. They are either named something
outside every list we have, or were embedded without a name by the tools that
rebuilt the archive.

## Open questions

- What are the two `multimd.mix` maps named, and do they differ from the
  shipped YR multiplayer maps?
- Is the `multimd.mix` light-definition INI a leftover of a merged art/rules
  file, or a scenario fragment?
- What uses the `wdt.mix` palette?
- Were these embedded by XCC or by an internal tool? The absence of names for
  entries that other tools could address suggests the archives were rebuilt by
  tooling that did not carry the original name database.
