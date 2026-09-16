# ra2yr

A standalone, modern reimplementation of the **Command & Conquer: Red Alert 2**
and **Yuri's Revenge** engine.

The retail games (2000/2001) need a stack of wrapper DLLs to run on modern
Windows and Wine at all. This project rebuilds the engine so the real game -
campaigns, skirmish, maps, mods - runs cleanly and smoothly natively, loading
the player's own retail data. The engine ships **no** game assets and no
original binaries.

## Status

Early. The project is scaffolded and the asset tooling is functional; the
engine itself is being built per [`plan.md`](plan.md). See
[`docs/yr-delta.md`](docs/yr-delta.md) for the living engine-difference notes.

## Building

Requires CMake 3.24+ and a C++20 compiler. Vendored dependencies are git
submodules; initialize them once after cloning:

```
git submodule update --init --recursive
```

```
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Third-party code is built only when its `RA2YR_ENABLE_*` option is on, so the
default build and CI need no external code.

### Window and renderer

SDL3 and bgfx are vendored and off by default. To build the windowed engine:

```
cmake -S . -B build/gfx -G Ninja \
    -DRA2YR_ENABLE_SDL3=ON -DRA2YR_ENABLE_BGFX=ON
cmake --build build/gfx
./build/gfx/src/ra2yr
```

This opens an SDL3 window and initializes bgfx (renderer auto-selected).
Debug builds log verbosely, including bgfx's own diagnostics; pass
`--log-level info` to quiet them.

To render an isometric terrain grid from retail assets:

```
./build/gfx/src/ra2yr --terrain <tile.tem> <theater.pal> \
    --grid 16x16 --screenshot terrain.ppm
```

On Windows, configure manually if you prefer:

```
cmake -S . -B build -DRA2YR_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Asset tooling

`scripts/mixer.py` reads and writes Westwood MIX archives, including the
nested archives the retail games use and the embedded name databases:

```
python3 scripts/mixer.py tree   "<game>/ra2md.mix"
python3 scripts/mixer.py list   -r "<game>/ra2.mix"
python3 scripts/mixer.py extract -r -d extracted "<game>/ra2.mix"
python3 scripts/mixer.py manifest -o inventory.csv "<game>"/*.mix
python3 scripts/mixer.py mount  "<game>/ra2md.mix" /mnt/ra2md
```

## Layout

| Path | Contents |
|---|---|
| `src/core/` | platform-free kernel: logging, version |
| `src/platform/` | platform abstraction and backends |
| `src/vfs/` | archives, loose-file override, INI, CSF |
| `scripts/` | asset and reverse-engineering tooling |
| `tests/` | asset-free tests, runnable in CI |
| `docs/` | reverse-engineering notes and delta tracking |
| `reference/` | read-only evidence, never committed |

## License

GPL-3.0-or-later. Derived in part from Electronic Arts' GPL-released
Command & Conquer source (with the additional GPL Section 7 terms), from
OpenTS, and from YRpp/Phobos. See [`LICENSE.md`](LICENSE.md).
