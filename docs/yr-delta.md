# YR delta: how Yuri's Revenge differs from Tiberian Sun

This is the living record of every engine difference between the Tiberian Sun
reconstruction (`reference/Tiberian sun reimplementation/`, "OpenTS") and the
Yuri's Revenge engine we are rebuilding. It drives milestone M1 in
[`plan.md`](../plan.md) and is updated as each difference is confirmed.


 - Some notes on bugs:

I have noticed that many reference documentations and code for RA2/YR formats
is notably incorrect or bugged.  We should document anything odd we find in
the file type documentation we have on hand since they are considered _THE_
reference. I would like to be able to finally provide the authorative reference
that even the original developers lost.

RA2 was forked from tiberian sun in early development, so theres some weirdness
due to that too.  And at some point between Red alert 2 and yuris revenge when
Westwood was moving to EA studios, they actually *lost* a lot of dev tools and
a lot of YR stuff uses unofficial implementation references which makes up most
of that specific game's engine oddities.

## Method

A delta entry is only accepted with evidence. In rough order of strength:

1. **YRpp** header in `reference/Phobos/YRpp/` - gives class layouts, member
   offsets, vtables, and static addresses.
2. **Phobos docs** (`reference/Phobos/docs/`) - behavior of a specific logic.
3. **Decompiler** - `reference/ghidra/gamemd.exe.c` (YR) vs `game.exe.c` (RA2),
   cross-checked against `reference/ida/`.
4. **OpenTS** source - the TS baseline to compare against.
5. **Golden capture / replay** - runtime proof, recorded later.

Unknowns stay listed as open questions rather than guesses.

## Class tree

Class-by-class differences confirmed so far. "YRpp" means the class exists in
the YR engine per `reference/Phobos/YRpp/`; the TS side must still be confirmed
against OpenTS by file and behavior.

| Area | YR class / feature | YRpp evidence | TS (OpenTS) status | Notes |
|---|---|---|---|---|
| Capture / mind control | `CaptureManagerClass` | `CaptureManagerClass.h` | to confirm | YR mind-control, `MindControl`/`PermaMindControl` |
| Slaves / spawning | `SlaveManagerClass`, `SpawnManagerClass` | both headers | to confirm | carried by `TechnoClass` in YR |
| Superweapon effects | `AirstrikeClass`, `EMPulseClass`, `RadSiteClass` | headers | to confirm | American air strike, EMP, radiation |
| Special objects | `BombClass`, `BombListClass`, `DiskLaserClass`, `EBolt`, `RadBeam`, `AlphaShapeClass` | headers | to confirm | Ivan bombs, disc laser, electric bolts |
| Locomotion | `JumpjetLocomotionClass`, `TeleportLocomotionClass`, `TunnelLocomotionClass`, `DropPodLocomotionClass`, `RocketLocomotionClass` | headers | to confirm | YR adds jumpjet/teleport/tunnel/droppod behaviors |
| Special units | `ParasiteClass`, `TemporalClass`, `VeinholeMonsterClass`, `BeaconClass` | headers | to confirm | terror drone, chrono, veins, beacons |

## Rules and art schema

- `rulesmd.ini` (743 KB) and `artmd.ini` (337 KB) are the authoritative YR data,
  extracted at `reference/mix/expandmd01/rulesmd.ini` and
  `reference/mix/expand97/artmd.ini`.
- A key-by-key diff against the TS `rules.ini` is required before M4. The
  modular INI system (`[#include]`, `$` overrides) must be modelled first.

## File formats

| Format | Status | Notes |
|---|---|---|
| MIX | done | recursive reader/writer in `scripts/mixer.py`; native reader is M2 |
| SHP / RLE-Zero | documented | `docs/reverse-engineering/SHP-format.md` |
| TMP | documented | `docs/reverse-engineering/TMP-format.md` |
| VXL / HVA | reference code | `reference/tools/Red_Alert_2_Formats/` |
| MAP / PKT | reference code | `MapReader.cpp`; maps also embedded in maps0*.mix |
| CSF | reference | `reference/tools/CSF-Studio/` |
| VQA / BINK video | open | decode path TBD |

## Renderer, UI, audio

- YR UI adds sidebar tabs, more cameos, and superweapon buttons; see Phobos
  `docs/User-Interface.md`.
- YR cutscenes use BINK (`*.bik`), matching the movies0*.mix contents.
- Theater set is temperat/snow/urban plus YR desert/lunar/new urban.

## Networking and multiplayer

- YR multiplayer modes and CnCNet client integration; protocol behaviour is
  documented only by packet captures so far.
       - If we manage to find the existing protocol documentation to be incorrect, that has to be documented.

## Open questions

- Exact save-game compatibility target.
      - Current idea is, 'would be nice, not at all required' I mostly included savegame
        files in the chance they are helpful.

- Which YR logic bugs to reproduce versus fix behind a flag.
      - This is the question of hte century, even if we plan on 'original' and 'enhanced' 
        modes, some of the original bugs probably should get fixed I should think.

- Full enumeration of rules keys new in YR versus TS.
- Native reader for the Blowfish-encrypted MIX index in the C++ VFS (M2).
