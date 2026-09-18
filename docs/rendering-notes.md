# Rendering and performance notes

Distilled from advice given by RTS engine developers (see
`reference/advice-i-was-given.md` for the raw text) plus our own reverse
engineering. Each item is marked with where we stand.

## Coordinates and anchors

- **Integer sub-tile coordinates.** The engine tracks positions in sub-tile
  units and converts to screen with integer shifts, not floats, so rounding
  cannot accumulate. *Status:* our cell to screen projection is integer
  (`render/iso.h`); scene placement is integer arithmetic cast to float only at
  the end. Keep it that way, and use fixed-point if sub-cell placement is ever
  needed.
- **Building anchor (verified against CNCMaps, which renders retail-identical
  images).** The map cell in `[Structures]` is the **top-left** of the
  foundation. Placement ignores the foundation:
  ```
  screen.x = tile_iso_pixel.x - full_frame_width/2  + frame.X + XDrawOffset
  screen.y = tile_iso_pixel.y - full_frame_height/2 + frame.Y + YDrawOffset
  ```
  `frame.X`/`frame.Y` are the SHP frame's offsets within the full frame, which
  our full-frame decode already bakes in. `XDrawOffset`/`YDrawOffset` come from
  art(md).ini. This also matches the engine decompile: `Draw_Shape(SHAPE_CENTER)`
  centres the full frame on the draw point, and `BuildingClass::Render_Coord`
  shifts by a constant half-cell that cancels into the map's global origin.
  Foundation is used **only** for the depth/sort footprint (the front corner is
  the object's `BottomTile`). An earlier implementation wrongly subtracted half
  a tile and applied foundation centring; both are fixed.

## Palettes

Westwood palettes are 6-bit (0-63) and expand to 8-bit (already fixed; see the
status log). RA2 selects a palette per art type, not one global palette:

- Terrain tiles use the **terrain palette**: `temperat.pal`, `snow.pal`,
  `urban.pal`, `urbann.pal`, `desert.pal`, `lunar.pal`.
- Structures default to the **building/iso palette**: `isotem.pal`,
  `isosno.pal`, `isourb.pal`, `isoubn.pal`, `isodes.pal`, `isolun.pal`.
- `art(md).ini`/`rules(md).ini` keys override it: `TerrainPalette=yes` uses the
  terrain palette; `AltPalette=yes` uses the unit palette
  (`unit<theater>.pal`); `AnimPalette=yes` uses `anim.pal`; `Palette=<base>`
  uses `<base><theater>.pal` (for example `Palette=lib` -> `liburb.pal`,
  `Palette=City` -> `cityurb.pal`).
- Units use `unit<theater>.pal`.

*Status:* implemented in `render/scene` (terrain palette for tiles, per-type
palette for structures). House colour remapping (below) is not.

## GPU techniques (future)

- **Instanced rendering / one atlas.** Thousands of sprites should be batched
  into few draw calls with per-instance data. *Status:* we already pack terrain
  and objects into one atlas and one indexed draw call; per-instance data is a
  vertex buffer today. Moving to real GPU instancing is a later optimization.
- **GPU palette lookup.** Upload SHP frames as 8-bit `R8_UINT` textures plus a
  1D palette texture, and resolve indices in the fragment shader, applying
  house remap (Westwood reserves index ranges, e.g. 16-31) per instance.
  *Status:* not started; we expand to RGBA on the CPU. This is the clean way to
  do remapping and multiple palettes without duplicating atlas pixels.
- **Voxels.** Options range from greedy-meshed faces to ray marching a 3D
  texture of color/normal indices with HVA transforms in the vertex shader.
  *Status:* VXL/HVA not decoded yet; the ray-march approach is attractive once
  we get there.
- **Data-oriented simulation.** Tight arrays and systems over the object
  hierarchy, parallelised. *Status:* long-term direction (see the plan's
  entity-component note); the current engine derives from OpenTS's hierarchy.
