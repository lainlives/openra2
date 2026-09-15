# ra2yr repository instructions

## What this is

A standalone, modern reimplementation of the Red Alert 2 / Yuri's Revenge
engine. It loads retail game data (which it never distributes) and reproduces
the original behavior while running cleanly on modern Windows, Wine, and
(eventually) native Linux. Read `plan.md` for scope, strategy, and milestones.

## Required context

- `plan.md` - the master plan, milestones, and decisions.
- `docs/` - reverse-engineering notes and the living `docs/yr-delta.md`.
- `reference/` - **read-only evidence**. Never modify, and never commit it
  (it is git-ignored). It holds the retail install, OpenTS, YRpp/Phobos,
  decompiler output, and extracted data.

## Build and test

Initialize the vendored dependencies once after cloning:

```
git submodule update --init --recursive
```

```
cmake --preset default
cmake --build --preset default
ctest --preset default
```

On Windows use the same commands; presets use Ninja, or configure manually
with `cmake -S . -B build -DRA2YR_BUILD_TESTS=ON`.

Third-party libraries live in `third-party/` as submodules and are built only
when their `RA2YR_ENABLE_*` option is on, so the default build and CI need no
external code.

The MIX tooling:

```
python3 scripts/mixer.py tree   <archive.mix>
python3 scripts/mixer.py list   -r <archive.mix>
python3 scripts/mixer.py extract -r -d out <archive.mix>
python3 scripts/mixer.py manifest -o out.csv <archive.mix> ...
python3 scripts/mixer.py mount  <archive.mix> <mountpoint>
python3 scripts/mixdiff.py old-tree new-tree -o report.txt
```

The native reader needs no key file; retail archives carry no names, so pass a
name database (one filename per line) to resolve them:

```
./build/default/src/ra2yr --names reference/ra2-names.txt --mix-list <archive.mix>
```

## Layout

- `src/core/` - platform-free kernel: logging, version, utilities.
- `src/platform/` - platform abstraction and backends. Nothing below this
  directory may include it.
- `src/vfs/` - archives, loose-file override, INI, CSF.
- `src/` game simulation, rendering, audio, UI - to be added per `plan.md`.
- `scripts/` - asset and reverse-engineering tooling.
- `tests/` - asset-free unit and integration tests.

## Rules

- Keep the simulation deterministic and free of OS/rendering calls.
- Match existing naming, layout, and style. Follow `.clang-format`.
- Handle errors explicitly; no silent failures.
- Never commit game assets, original binaries, proprietary SDKs, extracted
  archives, credentials, or build output.
- Add no third-party dependency without explicit approval. Vendor approved
  dependencies via git submodules under `third-party/`.
- Preserve GPL/EA provenance notices. New files are GPL-3.0-or-later.
- Every behavior change states its evidence (YRpp, decompiler address,
  golden capture, replay hash) and updates the owning docs.

## Verification

- Run the narrowest relevant tests first; report exact commands and results.
- Asset-free tests must remain runnable in CI.
- Do not turn a build result into a runtime claim.
