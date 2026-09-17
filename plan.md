# RA2 / YR Engine Reimplementation — Master Plan

Goal: a **standalone, modern, open-source reimplementation of the Red Alert 2 / Yuri's Revenge
engine** that loads the retail game data and plays the real game — campaigns, skirmish, maps,
mods — without the DXVK/ddraw/DirectPlay shim stack the original needs to run at all on modern
systems. Behavioral fidelity first; a cleaner, smoother, faster engine second. Not a cleaned
decompile.

---

## 1. Scope and target

| Thing | Decision |
|---|---|
| Reference executable | `gamemd.exe` (YR 1.001, 2001-03-11) is the **superset** engine; RA2 `game.exe` is a mode/config of it. Target YR. |
| Data | Retail RA2 + YR install. Engine ships **no** game assets. |
| Fidelity | Deterministic simulation matching YR behavior; UI/feel close but allowed to be modern. |
| Platforms | Windows first (native, no wrappers). Wine/Proton must run it cleanly. Native Linux later. |
| Architecture | Portable simulation core + swappable platform/render/audio layer. No Win32/GDI/DirectDraw in the core. |
| License | GPLv3-compatible (EA GPL source + OpenTS + YRpp/Phobos all GPLv3). Never commit assets or original binaries. |

**Non-goals (for now):** console/portable ports, remaster-quality art, a new campaign, being a
generic RTS engine. (OpenRA's RA2 mod is the reference alternate-timeline project; we are explicitly
building the *actual* YR engine behavior, not re-authoring it in another engine's data language.)

---

## 2. What already exists (reference map)

Everything under `reference/` is git-ignored; treat it as read-only evidence.

| Path | What it is | How we use it |
|---|---|---|
| `reference/Tiberian sun reimplementation/` | **OpenTS** — a *playable, GPLv3 reconstruction of the Tiberian Sun engine*, ~11,150 functions, ~98% instruction match, bgfx renderer, modern resolutions. TS is the direct ancestor of the YR engine. | **Primary code base.** Fork/derive; the shared Westwood engine (object model, INI, MIX, pathfinding, UI, audio) is largely already reconstructed here. |
| `reference/Phobos/YRpp/` | **336 C++ headers** re-declaring YR engine classes, layouts, vtables, and static addresses (e.g. `RulesClass::Instance = 0x8871E0`). GPLv3. | **Primary structural spec for the YR delta.** Class layouts, method signatures, member offsets, addresses. |
| `reference/Phobos/` | Phobos — a DLL that hooks living YR. `docs/` documents hundreds of logics and fixes; `gamemd.edb` maps crash IPs. | Behavior documentation, edge cases, engine quirks. PDB is `Phobos.dll`'s, not the game's. |
| `reference/ghidra/` | Decompiled `gamemd.exe.c`/`.h` (727k lines) and `game.exe.c`/`.h`, plus `ra2.rep` project. | Pseudocode for functions absent from OpenTS/YRpp. `FUN_xxxx` names only. |
| `reference/ida/` | `gamemd.exe.i64`, `Phobos.dll.i64`, `gamemd.exe.h` (types). | Cross-check; IDB may carry community names. |
| `reference/Phobos.pdb.bndb`, `reference/RA2MD.exe.bndb` | Binary Ninja databases. | Analysis cross-check. |
| `reference/Red Alert 2 Yuri's Revenge/` | The actual retail install (RA2 + YR), incl. `gamemd.exe`, `game.exe`, video, saves, maps, mods. | Runtime test target; source of codec golden data. Keep unmodified. |
| `reference/mix/` | **Partially** extracted MIX contents (1.8 GB, 5381 files, one level deep). 48 nested `.mix` still packed; 7 entries unnamed. | Needs a recursive extractor + name DB before it is useful as an asset oracle. |
| `docs/reverse-engineering/` | Own RE docs: `Red_Alert_2_File_Formats` (SHP/TMP/VXL/HVA/pal/map), `SHP-format.md`, `TMP-format.md`. | Codec specs. |
| `reference/tools/Red_Alert_2_Formats/` | C++ reference readers for SHP/TMP/VXL/HVA/map + LZO/LCW. | Port/reference for our codecs; golden decoder. |
| `reference/tools/Advanced-TMP-Editor`, `CSF-Studio` | TMP editor, CSF (string table) tool. | Format reference. |
| `reference/fish_tycoon/` | Prior project: **`ftspy`** — a Wine DLL proxy that logs retail engine draws by hooking the renderer. | **Template for our golden-capture harness** (see §8). |
| `scripts/mixer.py` | Working MIX extractor/packer with RSA/Blowfish key support and name DB. | Extend into the canonical asset tool. |
| `scripts/mix_vfs.py` | Incomplete FUSE MIX mounter. | Fold into `mixer.py` or drop. |
| `scripts/util/names.py` | Filename hash → description DB (TD/TS-era). | Seed; needs a YR name DB. |
| `reference/SyringeEx/` | Hook injector. | Understanding how Phobos/Ares hook YR (for mod-compat later). |

**Already-reusable conclusions**
- OpenTS is the single largest derisker: the YR engine is a later revision of the same Westwood
  engine, so most subsystems exist there.
- YRpp already recovers the hardest YR-specific info (layouts + addresses).
- Retail assets, save games, maps, and mod `.mix`/loose files are all present for testing.
- The `gamemd.pdb` in the install is a copy of `Phobos.pdb` (identical MD5) — **not** game symbols.

---

## 3. Recommended strategy

**Derive the engine from OpenTS, rebased on the EA GPL sources, and drive the YR delta from YRpp +
Ghidra.** Concretely:

1. **Fork OpenTS** (or vendor it as `third-party/opents/`) as the simulation + game-logic base. Keep
   its behavior; lift its license headers.
2. **Inventory the YR delta** against TS — new classes, members, rules keys, formats, renderer
   features, AI actions, superweapons, units — into a living `docs/yr-delta.md`, sourced from YRpp +
   Phobos docs + Ghidra.
3. **Replace the platform/presentation layer** with a portable one (window, input, audio, renderer,
   timing, filesystem) so the result is smooth on modern Windows and Wine without wrappers.
4. **Add the YR delta** subsystem by subsystem, verifying behavior against the retail binary.
5. Only then **modernize internally** (the entity-component direction OpenTS already commits to).

Why not clean-slate: a 4.8 MB monolithic binary with ~11k functions is a multi-year effort from
zero; OpenTS already did that work for the shared engine. Why not OpenRA-style re-authoring: it
diverges from real YR behavior and won't run real mods/maps/saves.

### The engine/simulation portability caveat (important)

OpenTS is MSVC Win32 **x86**, with layout-sensitive structs and hand-written assembly, and its
verification method (objdiff) requires the original compiler. We will treat it as two lanes:

- **Shipping lane:** portable C++20, cross-platform, modern platform layer.
- **Matching lane:** optional MSVC/Win32 build used to diff reconstructed functions against the
  retail executable. Kept as a verification tool, not the product.

Short term this means the shipping engine initially builds on Windows (MSVC) and runs under Wine;
native Linux is a later milestone once Win32 dependencies are removed from the core. This matches
the stated goal (smooth on modern Windows **and** Wine) while leaving a path to native Linux.

---

## 4. Decisions to lock before large spend

These change the plan materially; answer these first (recommended default in **bold**).

1. **Code base:** fork OpenTS vs. clean-slate using OpenTS as spec. → **Fork OpenTS.**
2. **Platform:** Windows-only / Wine, vs. cross-platform core. → **Portable core, Windows first.**
3. **Renderer:** reuse bgfx vs. adopt SDL3/D3D12/Vulkan explicitly. → **bgfx initially** (already
   integrated in OpenTS), revisit for HDR/scaling.
4. **Audio:** miniaudio vs. OpenAL vs. XAudio2. → **miniaudio** (single-file, cross-platform).
5. **Fidelity bar:** binary-exact vs. behavioral. → **Behavioral + replay/state-hash proof.**
6. **First playable target:** a skirmish match, or a specific campaign mission. → **Skirmish match,
   then "Allied Mission 1".**
7. **Repo layout / license header policy** for EA-derived files.

---

## 5. Prerequisites

### 5.1 Toolchain and build
- Windows: VS 2022 (MSVC) x86/x64, CMake ≥ 3.23, Ninja.
- Linux dev: clang/gcc 16, CMake 4.x present; a 32-bit multilib target is only needed for the
  matching lane.
- Dependency policy: vendor via git submodules (`third-party/`), not a package manager, to keep
  reproducible builds. Candidate deps: `bgfx`, `SDL3`, `miniaudio`, `stb`, `zlib`, `luajit` (if a
  scripting layer is ever wanted), `Catch2`/`doctest`.
- CI (GitHub Actions): build both lanes; run asset-free tests; publish nightly.

### 5.2 Reverse-engineering tooling
- Ghidra (headless) — **the symlinks are currently broken; restore the CLI to PATH.** Needed to
  re-decompile individual functions and to bulk-export.
- IDA + the provided `.i64` databases.
- Binary Ninja + the provided `.bndb` databases.
- `objdiff` (https://github.com/encounter/objdiff) for the matching lane.
- A symbol/type pipeline: YRpp addresses + Ghidra output → normalized symbol map the engine can use
  as a cross-reference. Automate: function address → YRpp name → source file.
- (Optional) `decompyle`/`decomp2dbg`-style tooling for mapping addresses into a debugger.

### 5.3 Assets and asset tooling
- **Complete the MIX extraction recursively** (nested `.mix` remain) and **name every entry** via a
  `local mix database.dat` / XCC name DB. Without names, VFS lookups can't be validated.
- Consolidate `scripts/mixer.py` into a proper CLI: `mixer list|extract|pack|mount`, recursive,
  name-resolving, with a manifest/hash report. Retire or absorb `mix_vfs.py`.
- Build a **file inventory + hash manifest** of a clean install. This is the regression oracle for
  "does our VFS see exactly the files the game sees, in the right precedence".
- Confirm precedence rules: base `.mix` → `expand*.mix` → mission disk `*md.mix` → loose files
  (loose wins), per the original notes in Appendix A.
- Acquire/derive a **YR-specific name database** (the current `names.py` is TD/TS).

### 5.4 Golden-capture harness (verification)
- Port the `ftspy` idea to `gamemd.exe`: a Wine proxy DLL that logs the engine's *logical* calls
  (asset loads, sprite draws with rects, audio triggers, frame boundaries) with caller addresses.
- Capture golden frame hashes, asset-resolution traces, and input→state traces from retail, to test
  our engine without needing the proprietary binary in CI.

### 5.5 Legal and process
- LICENSE/`NOTICE`: GPLv3 + EA Section 7 terms; document provenance per file.
- `AGENTS.md` for the repo (build commands, no-assets rule, verification expectations).
- Contribution rules: keep mechanical changes separate; every behavior change cites evidence
  (YRpp, decompile, replay, capture).

---

## 6. Architecture (target)

```
app/            entry point, game bootstrap, config
platform/       window, input, timing, files, threads, logging, crash handler
render/         isometric 2D, SHP, voxel, palette, shroud/fog, UI compositing
audio/          mixer, EVA/voice/SFX, themes, positional
video/          VQA (decode) + BINK (decode or bundled decoder)
vfs/            MIX (recursive), loose override, CSF strings, INI, cache
formats/        SHP, TMP, VXL/HVA, PAL, PCX, CPS, FNT, AUD/VOC/WAV, MAP, PKT
game/           object model, rules, map/theater, simulation, combat, economy, AI, triggers, UI
ext/            mod/plugin API (Phobos-compatible surface)
tools/          mixer CLI, format dumpers, golden capture, RE symbol pipeline
tests/          unit + asset-free integration + replay/state-hash
```

Rules:
- **Simulation is platform-free and deterministic.** No rendering/OS calls from `game/`.
- Fixed-point where the original used it; preserve RNG and tick order.
- Every data format has a codec + round-trip test that needs no proprietary asset in CI (use small
  synthetic fixtures; full retail verification is a manual/nightly job).

---

## 7. Milestones

Estimates are rough engineer-weeks with AI assistance and OpenTS reuse; they are for sequencing, not
commitments. Each milestone must be *demoable* and have a hard definition of done (DoD).

### M0 — Foundations and build (2–4 wk)
**Do:** repo scaffold, license/notice, `AGENTS.md`, CI, vendored deps, OpenTS vendored as a
subproject, stub app that opens a window and logs.
**DoD:** clean build on Windows + Linux CI; app starts and exits cleanly; no game assets in repo;
`mixer` CLI runs from CI.

### M1 — Delta inventory (2–3 wk) — *parallel with M0*
**Do:** produce `docs/yr-delta.md`: every class/member/rules-key/format/AI-action/feature that
differs from TS, with source citations (YRpp, Phobos docs, Ghidra address).
**DoD:** reviewed, covers the class tree, rules schema, file formats, renderer/UI, networking; a
grep-able table; open questions listed.

### M2 — Asset pipeline and VFS (4–6 wk)
**Do:** recursive MIX reader/writer, local mix DB, loose-file precedence, CSF, INI, on-disk cache;
port the needed codecs (SHP/RLE-Zero, TMP, PAL, PCX, CPS, FNT, VXL+HVA, AUD/VOC/WAV, MAP/PKT).
**DoD:** `mixer` extracts a retail install to a byte-complete, fully-named tree; repacking is
byte-identical; every codec round-trips; a decoded SHP/TMP/VXL matches `Red_Alert_2_Formats`
output pixel-for-pixel; VFS file list + precedence matches the golden capture.

### M3 — Renderer bring-up (6–10 wk)
**Do:** bgfx backend; palette and SHP blitting; isometric tile map with TMP height/slope/ramps;
overlays, smudges, terrain objects; object draw order; basic shroud/fog; arbitrary resolutions.
**DoD:** an official map renders correctly at 1080p/4K and ultrawide; a screenshotted frame matches
the retail golden frame within a defined tolerance; 60+ fps with thousands of tiles.

### M4 — Rules, types, object model (6–10 wk)
**Do:** parse `rulesmd.ini`/`artmd.ini` (+ modular INIs), CSF strings, type registry and the
Object→Techno→Foot→Infantry/Unit/Aircraft/Building hierarchy with correct layouts.
**DoD:** YR rules load with zero unknown-key diagnostics (or an explicit known-unknown list);
type counts match the rules file; objects instantiate, persist, and are inspectable; art lookup
resolves for every type.

### M5 — Simulation core: movement and combat (8–14 wk)
**Do:** locomotion (drive/walk/fly/hover/jumpjet/teleport/tunnel/ship/rocket), A* pathfinding, cell
occupancy, weapons/warheads/damage/armor, ROF, projectiles, targeting, veterancy, crates.
**DoD:** scripted sandbox where two forces path, engage, and destroy each other; deterministic
replay reproduces an identical state hash; damage tables match documented YR values.

### M6 — Economy, production, base building (6–10 wk)
**Do:** ore/gems + harvesters + refineries, power grid, build queues and prerequisites, sidebar
production, placement, repair/sell, upgrades, superweapon charging.
**Do:** start from a Work Shop-less MCV, deploy, build power→refinery→harvester→war factory→tank;
income accumulates; low power slows production; placement rules enforced.

### M7 — UI, HUD, audio (8–12 wk)
**Do:** tactical view, sidebar tabs, cameos, radar/minimap, shroud reveal, tooltips, in-game
dialogs/menus, EVA/voices, positional SFX, music themes. (Audio can start earlier behind M3.)
**DoD:** a full skirmish is playable mouse+keyboard-only using real YR assets; audio triggers match
the golden capture; no wrapper DLLs required.

### M8 — Triggers, AI, campaign (10–16 wk)
**Do:** trigger/tag/event/action system, team types, AI trigger + mission scripting, map mission
loading from `maps0x*.mix`, briefings, objectives, win/lose, difficulty.
**DoD:** one full campaign mission per side completes with correct objectives and outcomes; a
skirmish AI builds and attacks credibly; mission scripts run in the original order.

### M9 — Persistence (4–6 wk)
**Do:** read/write the YR save format and map format; deterministic state serialization.
**DoD:** save mid-mission, reload, continue identically; maps round-trip through our editor IO;
(state-hash equality after save/load).

### M10 — Video and campaign shell (3–5 wk)
**Do:** VQA + BINK playback, main menu/shell flow, credits, mission select.
**DoD:** campaign intro + mission cutscenes play; menu navigation reaches a mission without
retail dependencies beyond data.

### M11 — Multiplayer (8–16 wk)
**Do:** deterministic lockstep, LAN, CnCNet tunneling + client, lobby, game modes, anti-desync.
**DoD:** two clients complete a skirmish with no desync over 20+ minutes; CnCNet tunnel connects;
desync diagnostics on mismatch.

### M12 — Modernization and mod platform (ongoing)
**Do:** incremental ECS-style migration, de-hardcoding, extension/plugin API, Phobos feature
parity, QoL (alt-tab, high refresh, scaling, controller, accessibility), native Linux.
**DoD:** a Phobos-style mod compiles against the new API and runs; a published feature-parity
matrix is met; the core has no Win32 dependency.

### Cross-cutting (continuous)
- **Determinism/replay suite** grows from M5 onward; every sim PR runs it.
- **Golden-capture differential tests** against retail under Wine.
- **Performance budget:** 60 fps at 4K with typical late-game unit counts.
- **Asset-free CI** at all times.

Dependency sketch: M0→M2→M3→M7; M1→M4→M5→M6→M8; M5→M9/M11; M3→M10.

---

## 8. Verification strategy

Multiple independent layers, strongest first:

1. **Instruction match (matching lane):** objdiff against `gamemd.exe` for reconstructed functions.
   Strongest evidence, but requires the historical compiler and is not the shipping build.
2. **Golden capture (differential):** the Wine proxy logs the retail engine's asset loads, draw
   rects, and frame hashes; our engine must reproduce them for the same scenario. This is the
   `ftspy` pattern and is the backbone of renderer/UI/audio correctness.
3. **Replay/state-hash:** record inputs from retail, replay in ours, compare a canonical state hash
   every tick. Backbone of simulation correctness and multiplayer determinism.
4. **Format round-trip + oracle:** our codecs vs. `Red_Alert_2_Formats`/XCC on the same file.
5. **Rule/schema diff:** our parsed rules/art structures vs. a reference extraction.
6. **Playthrough:** scripted and manual campaign/skirmish runs.

Every claim in a PR must name which layer proves it. No proprietary asset or original binary in CI.

---

## Open rendering questions

- **Map brightness (resolved).** Maps looked about 4x dark because Westwood VGA
  `.pal` files store **6-bit** DAC values (0-63) and `Palette::from_bytes` used
  them as 8-bit. All 128 retail palettes max out at 63. The loader now expands
  6-bit palettes to 8-bit by bit replication and passes through palettes that
  already contain values above 63. Measured before/after average luminance on
  `all01t.map`: 27.9 -> 112.9 (4.05x). Unrelated to `[Lighting]` (map ambient
  is 1.0) and to `[MultiPlayer] Color`/`ColorEx`, which are the player's
  side/colour selection, not gamma.
- Foundation-aware sprite anchoring and depth-interleaving of objects with
  terrain.
- Whether NewTheater buildings should switch on the theater letter (the YR
  engine's `Theater` table has a `Letter` field) rather than using the base
  image name.

## 9. Major risks

| Risk | Mitigation |
|---|---|
| Scope: ~11k functions + YR delta is huge | Derive from OpenTS; milestone gating; community contribution path. |
| OpenTS is MSVC/Win32/x86/matching-oriented | Two-lane strategy; portable core rewrite bounded to platform layer. |
| YR delta larger/uglier than assumed | M1 inventory *before* committing to M4+; re-plan then. |
| Missing/incomplete asset naming breaks validation | Prioritize recursive extraction + name DB in M2. |
| No game symbols (PDB is Phobos's) | YRpp + Ghidra + IDA + BN cross-check; symbol pipeline in §5.2. |
| Determinism drift (floating point, RNG, ordering) | Fixed-point/fp flags, RNG preservation, tick-order tests, replay hashes. |
| Multiplayer/protocol undocumented | Start from CnCNet docs + packet captures; defer until sim is stable. |
| Legal | GPLv3 provenance, no assets/binaries in repo, AGENTS.md guardrails. |

---

## 10. Immediate next actions (first two weeks)

- [ ] Lock the §4 decisions.
- [ ] Restore Ghidra CLI to PATH; verify `objdiff` and a round-trip decompile of one known function.
- [ ] Scaffold the repo (M0): CMake, CI, `AGENTS.md`, vendored deps, submodule for OpenTS.
- [ ] Extend `scripts/mixer.py`: recursive extraction + name resolution; dump a full manifest.
- [ ] Start `docs/yr-delta.md` (M1) with the class tree and rules schema diffs from YRpp vs. OpenTS.
- [ ] Prototype the `gamemd.exe` Wine capture proxy from `ftspy`; capture one golden frame.
- [ ] Stand up the first asset-free codec test (SHP round-trip) in CI.

---

## Status log

Reverse-chronological; evidence belongs in the owning docs.

**2026-09-15**
- M0 done: CMake project, presets, CI (Linux + Windows + MIX smoke test),
  `AGENTS.md`, `README.md`, test suite, GPL/EA `LICENSE.md`.
- Third-party vendored as submodules (`third-party/bgfx.cmake` incl.
  `bgfx`/`bimg`/`bx`, `third-party/SDL`, `third-party/miniaudio`), built only
  behind `RA2YR_ENABLE_*` so CI stays dependency-free.
- M2 seed done: `scripts/mixer.py` now recursive (`tree`, `list -r`,
  `extract -r`, `manifest`, `mount`); `scripts/mixdiff.py` compares trees.
- Full retail extraction to `reference/mix-full`: 13,665 files, 1.7 GB,
  7 unnamed. Filelist diff vs the old one-level tree: one-level tooling
  resolved 646 unique names, recursion resolves 13,015 (12,417 added; the 48
  "removed" are the nested `.mix` files, now expanded).
- Native C++ `MixArchive` (`src/vfs/`): old/new headers and Blowfish-encrypted
  indexes. The Blowfish key is recovered from the 80-byte keysource using only
  the Westwood public modulus and e=65537 - no private key or key file.
  Verified by parity with the Python tool on `ra2.mix` (21 entries, exact body
  size) and on nested archives; Blowfish standard vectors and file-hash
  vectors are unit-tested.
- Known gap: retail archives do not embed a name database, so the reader takes
  an optional external name list; an embedded/derived YR name database is
  still to be built (M2).
- Name DB: `scripts/names_from_binary.py` harvests filename strings from the
  game binaries (540 candidates). The community list already covers 99.93% of
  entries; the binary adds the two patch files it missed (`WAITCYLO.SHP`,
  `WAITCYLO.PAL`). Combined dev database covers 13,010/13,015 entries; the 5
  remaining are unnamed mod/patch files.
- Native reader is recursive: `--mix-tree` and `--mix-extract` descend nested
  archives; verified by full extraction of `ra2.mix` (5,683 files) matching the
  Python tool, and by a nested-archive unit test.
- Platform bring-up: SDL3 window backend and bgfx renderer are wired behind
  `RA2YR_ENABLE_SDL3`/`RA2YR_ENABLE_BGFX`. Verified on Linux by opening an SDL3
  window and initializing bgfx (Vulkan/AMD radv), rendering to timeout and
  shutting down cleanly. Default dependency-free build and tests unchanged.
- bgfx diagnostics route through the engine logger via a `bgfx::CallbackI`;
  debug builds default to `Debug` level (verbose, including bgfx), release
  builds to `Info`, with `--verbose` / `--log-level` overrides. CI gained a
  `gfx-build` job that configures and builds the SDL3/bgfx configuration.
- Isometric asset layer (M3 start): palette and TMP decoders
  (`src/formats/`) plus isometric projection and draw ordering
  (`src/render/iso`). TMP reads the raw "isometric to square" cell image and
  height map at `cell + z_offset`, plus optional extra graphics. Verified
  against retail `clat01.tem` + `temperat.pal`: C++ output matches an
  independent Python decode byte-for-byte (hash and pixels). Tests added.
- Shader pipeline and terrain draw: bgfx's `shaderc` is built and the
  `vs_tile`/`fs_tile` shaders are embedded as generated headers by the
  `bgfx_compile_shaders` helper. `BgfxRenderer` uploads the decoded tile as an
  RGBA8 texture and draws the whole isometric grid in one indexed draw call
  with an orthographic projection and alpha blending; a screenshot callback
  writes PPM. Verified on Linux/Vulkan: a 16x16 grid paints exactly 230,400
  pixels, equal to 256 tiles x 900 pixels and to the full 960x480 diamond area,
  with the clear colour in the corners.
- Camera and Z: left-drag panning and wheel zoom drive a camera uniform;
  tiles carry a painter-order key so height (Z) and diagonal depth sort
  correctly, and higher cells are offset upward by half a tile per level.
- Map rendering (M3/M4 start): `src/formats/` gained a minimal INI reader,
  base64, an LZO1X decompressor ported from OpenTS, the map reader
  (`.map`/`.mpr`/`.yrm`: IsoMapPack5 -> base64 -> LZO1X blocks -> 11-byte
  cells), and the theater tile-set reader. `src/render/scene` packs the
  map's distinct tiles into one texture atlas and builds the instance list.
  Verified on Linux/Vulkan against `maps01/all01t.map` (URBAN): 6,336 cells,
  188 distinct tiles resolved 187/187 present on disk, 5,831 drawn, z up to 6,
  screenshot captured. `.mmx`/`.yro` are MIX archives containing a `.map`;
  their `.pkt` companion is redundant and not parsed.
- MIX-packaged maps: `vfs::extract_mix_member` unwraps a MIX holding a map by
  content, since retail members are anonymous. Verified against retail
  `maps/GoldSt.mmx` (URBAN, 103x82, 16,810 cells, 14,696 drawn) and
  `maps/Transylv.yro` (NEWURBAN, 70x70, 9,730 cells, 8,714 drawn). The archive
  reader now also rejects entries that lie outside the file, so non-archive
  input can never be misread as a huge MIX.
- Install VFS: `vfs::Vfs` resolves assets by Westwood hash the way the game
  does - loose files first, then nested MIXes opened on demand by name
  (`isourb.mix`, `localmd.mix`, ...), then the top-level archives in priority
  order (expansion/mod archives before ra2md before ra2). No filename database
  is needed at runtime. The theater table (control INI, tile MIX, palette,
  extension) matches the YR engine's `Theater` array. `--map` now needs only
  `--install <retail dir>`; verified on the same three maps with identical
  tile counts, so the manual theater/tiles/palette paths are gone.
- Prototype map find: `maps02.mix` entry `_439FEA47` (the unnamed entry noted
  earlier) is a Tiberian Sun-era GDI mission (`NextScenario=GDI2A.map`) with TS
  `[Lighting]` and TS tile names (`mclif*`, `mdrod*`). Those graphics survive
  in Red Alert 2 only as the desert theater, so it renders with
  `--theater DESERT` (all 7,070 non-empty cells resolve). Preview at
  `reference/proto-map-preview.png`.
- Renderer fixes found while rendering it: the camera refactor left a stale
  `+1` in the Y projection that shifted every view up by half a screen, and a
  TMP cell with no image data produced an empty atlas slot that crashed the
  builder. Both fixed; `--fit` and `--theater` added.
- Map scene (`src/render/scene`): terrain tiles and `[Structures]` sprites are
  packed into one atlas and one instance list, drawn in a single depth-sorted
  pass, so objects and terrain interleave correctly instead of objects always
  painting over terrain. Sprite anchoring is foundation-aware: `Foundation=NxM`
  from art(md).ini (or rules(md).ini) places the sprite centred horizontally on
  the footprint and stood on its bottom edge, with (x,y) the footprint's
  top-left. Depth is `diagonal * 64 + layer`, terrain layer = z and object
  layer = 32. Verified on `all01t.map`: 5,831 tiles + 189 sprites = 6,020
  instances, one 4,080x1,078 atlas, 1 missing sprite (an absent file).
- Map object sprites, first pass (now in `src/render/scene`): `[Structures]` entries
  resolve through `art(md).ini` (Image, else the type name) to an SHP, frame 0
  is packed into an object atlas and drawn as a second pass over the terrain.
  Verified on `all01t.map`: 189 sprites from 50 images, 1 missing (a file not
  present in retail). `Image=` falls back from art(md).ini to rules(md).ini
  (street lamps set it there) and the `*md` theater mixes are opened
  (`snowmd.mix` holds some civilian buildings). Superseded by the unified map
  scene above; remaining limits are no NewTheater per-theater letter
  substitution, no animation, and no house remap.
- Test fixtures adopted: synthesized CC0 SHPs (raw + RLE-Zero), maps
  (`[Lighting]`/trigger map, dense map in plain and `.yro` form, near-limit
  `.mpr`), and a CSF, under `tests/data/` with `tests/test_data.cpp`. The new
  test asserts each fixture parses and that `.yro` expands to exactly the plain
  map. A generator for VXL/HVA/PCX/CPS is included for when those decoders land.
- Palette fix: Westwood `.pal` files are 6-bit (all retail palettes max at 63);
  the loader now expands to 8-bit. Rendered maps brightened 4.05x (luminance
  27.9 -> 112.9 on `all01t`). Unit test covers both 6-bit and 8-bit palettes.
- SHP decoder (`src/formats/shp`): TS/RA2 SHP with compression types 1 (raw),
  2 (counted rows) and 3 (RLE-Zero), cropped frames placed into the full frame,
  palette index 0 transparent, plus `--shp-dump` to write a frame sheet without
  a renderer. Verified against the recovered `WAITCYLO.SHP`/`WAITCYLO.PAL`
  (247x32, 22-frame pulse). Unnamed-entry catalogue started in
  `docs/found-files.md`: the maps02 prototype map, WAITCYLO, two unnamed
  `multimd` maps, a `multimd` light-definition INI fragment, and a `wdt`
  palette.

---

## Appendix A — Original project notes (preserved)

- This is a RA2 + YR engine reimplementation. Functionally close but smoother and modern is
  preferred over something indistinguishable from a cleaned decompile.
- `tools/` is a grab-bag of possibly-useful code.
  - `scripts/mixer.py` contains the MIX extractor/packer.
  - `scripts/mix_vfs.py` is an incomplete (non-functional) FUSE MIX mounter; the file-referencing
    bits are unimplemented. Fold its functionality into `mixer.py`.
  - `reference/Red Alert 2 Yuri's Revenge/` is the target game dir: a copy of a 20-year-old install
    with mod loose ends. `junk/` is mostly noise. `campaign-save-files/` and `maps/` were moved out
    of the base dir to reduce `ls` noise.
  - `reference/{Phobos, SyringeEx, Phobos/YRpp}`: RA2/YR code mods with source. `Phobos/YRpp` is the
    most useful; anything different from Tiberian Sun should be referenced/documented there.
  - `reference/Tiberian sun reimplementation/`: a reimplementation of the previous title; the
    engines are nearly identical with small changes/updates. Absolute goldmine.
  - `reference/mix/`: extracted `.MIX` files from the YR folder (the assets).
  - `reference/ida/` and `reference/ghidra/`: decompiled main binaries.
  - `reference/fish_tycoon/`: tools made for reimplementing another game, but compiled by the same
    compiler against the same system/compiler libs; dumping tables should be similar.

### Archive layout

`filename.ext` is the base game; `filenamemd.ext` is the mission disk / expansion.

**Base RA2 (8):** `ra2.mix` (master data: audio, units, animations), `language.mix`
(strings/localization/UI), `maps01.mix` (Allied campaign), `maps02.mix` (Soviet campaign),
`movies01.mix` (Allied cutscenes), `movies02.mix` (Soviet cutscenes), `multi.mix` (skirmish/MP),
`theme.mix` (soundtrack), plus `expandxx.mix` (official update slot, mods only).

**Yuri's Revenge (8):** `ra2md.mix` (master expansion data), `langmd.mix` (expansion
UI/localization), `maps03md.mix` (YR campaign), `movmd03.mix` (YR cutscenes; some releases name it
`movies03.mix`), `multimd.mix` (expansion skirmish/MP), `thememd.mix` (expansion music),
`ecachemd01.mix` (official patches), `expandmd01.mix` (official assets; higher increments are
unofficial mods).

### Notes that must shape the engine

- The main binaries (`game.exe`, `gamemd.exe`) are nocd-patched to run at all on modern systems;
  they hash-match an unofficial patched binary, so they are unpacked and unofficially patched. Their
  internal VFS may be odd and they decompile unpredictably — likely why code lives in `.rtext`/
  `.rdata` and odd places.
- Game code paths are case-sensitive lowercase; the original OS was not. All files were lowercased
  already (for Wine).
- The binaries are normally launched by `ra2.exe`/`ra2md.exe`.
- Ghidra tools should be in PATH but the symlinks may be broken; ask and they can be restored.
- **Precedence:** loose files that belong in the archives load last and override them
  (`rulesmd.ini`, `artmd.ini` are the notable examples) — mod support must honor this. For
  reference truth, the data inside the MIX files is authoritative, excluding `expandmd` files above
  `01`.

---

## Appendix B — Open questions to expand later

- Exact save-game compatibility target (read retail saves? write compatible saves?).
- CnCNet client integration vs. a native lobby.
- Whether to keep pursuing instruction-level matching long-term or transition fully to behavioral
  tests once playable.
- Mod API shape: source-level hooks (Phobos-style) vs. a stable plugin ABI vs. both.
- Native Linux/Wayland target date and whether to keep the matching lane alive on Windows only.
- Scope of YR-specific "logic" bugs/quirks: reproduce faithfully or fix behind a flag?
- Asset cache format and whether to ship shaders/palettes pre-baked.
