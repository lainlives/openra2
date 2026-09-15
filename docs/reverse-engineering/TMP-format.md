TMPs or template files are terrain files used in `{{ts}}`{=mediawiki}
through `{{yr}}`{=mediawiki}. They constitute the solid, mostly
indestructible terrain upon which
[every](TerrainTypes "every"){.wikilink}
[other](OverlayTypes "other"){.wikilink} [game
object](TechnoTypes "game object"){.wikilink} is layered.

TMP files use the same theater-specific file extensions as
[TerrainTypes](TerrainTypes "TerrainTypes"){.wikilink}: .tem, .sno, etc.

## Tiles

Each TMP file holds one terrain piece called **tile**. Tiles come in
various shapes, each comprised of one or more
[cells](cells "cells"){.wikilink}[^1].

Each tile cell has a normal image that covers the cell shape, plus an
optional \"extra\" image perpendicular to the normal image. This extra
image is essential for creating the illusion of a 3D-environment, as it
allows for hiding terrain and objects behind it, thus creating a sense
of depth, or the filling of space between cells at different height
levels.

In addition to graphics, each tile cell has several parameters encoded
that specify characteristics such as
[height](Height#In_Maps "height"){.wikilink}, [land
type](LandTypes "land type"){.wikilink} and slope type.

Furthermore, tiles are divided into [tile
sets](TileSets "tile sets"){.wikilink}, which define certain attributes
that each tile in the set should have.

The maximum tile size is 10x10 cells.

## File Format {#file_format}

A TMP file consists of two header parts, the file header and tile cell
headers.

`struct FileHeader`\
`{`\
`   unsigned int BlockWidth; // size in blocks`\
`   unsigned int BlockHeight;`\
`   unsigned int BlockImageWidth; // size of each block`\
`   unsigned int BlockImageHeight;`\
`};`

Following the file header is an index of pointers to each tile cell
header.

`struct TileCellHeader`\
`{`\
`   signed int X; // tile cell offset`\
`   signed int Y;`\
`   unsigned int ExtraDataOffset;`\
`   unsigned int ZDataoffset;`\
`   unsigned int ExtraZDataoffset;`\
`   signed int ExtraX; // extra image offset`\
`   signed int ExtraY;`\
`   unsigned int ExtraWidth;`\
`   unsigned int ExtraHeight;`\
`   unsigned char Bitfield;`\
`   BYTE padding[3];`\
`   unsigned char Height;`\
`   unsigned char LandType;`\
`   unsigned char SlopeType;`\
`   RGB TopLeftRadarColor;`\
`   RGB BottomRightRadarColor;`\
`   BYTE padding2[3];`\
`};`\
`struct RGB `\
`{ `\
`   unsigned char Red; `\
`   unsigned char Green; `\
`   unsigned char Blue; `\
`};`

### Explanation

- **Block** is the size of a tile cell canvas. Its size is in practice
  fixed in each game: 48x24 pixels for TS and 60x30 pixels for RA2.

<!-- -->

- **Z-Data** contains the information for layering the graphics.

<!-- -->

- **Bitfield** - The 3 least significant bits out of the 8 bits are used
  as boolean (0/1) flags for three tile cell attributes: `HasExtraData`
  (least significant), `HasZData`, and `HasDamagedData` respectively.
  The game checks these bits to process the extra-data or z-data or the
  damaged logic. If `HasExtraData`/`HasZData` bits are set to 0, the
  game won\'t use its respective extra-data or z-data even if present.
  If the `HasDamagedData` bit is set to 0, the game randomly picks the
  tile cell from one of the TMP variant files, if present. These
  variants are identified by a single character suffix (from A to G) at
  the end of the filename, e.g. clear01.tem, clear01**a**.tem,
  clear01**b**.tem etc. `HasDamagedData` bit is set to 1 for bridges, so
  that the bridge tiles are not randomized. The remaining leading 5 bits
  out of the 8 bits are not used irrespective of the values present in
  those. For example, a byte value of 0xCB or 0x03 or 0x83 or 0x4B, all
  have the same result of binary xxxxx011 which means `HasDamagedData`
  bit is not set, `HasZData` bit is set and `HasExtraData` bit is set.

<!-- -->

- **[LandType](LandTypes "LandType"){.wikilink}**[^2] - Numbering used
  for land characteristics. 0, 1 or 13 is used for Clear. Ice uses 0
  to 4. Tunnel is 5. Railroad is 6. Rock uses 7 or 8 (15 is also used as
  rock in cliff tiles). Water is 9. Beach is 10. Road uses 11 or 12.
  Rough is 14. Cliff is 15. Wall, Tiberium and Weeds don\'t have numbers
  but are processed based on their overlay placed on the cells.

<!-- -->

- **Radar colors** - The top-left radar color and bottom-right radar
  color being different, enhances the demarcation in the minimap where
  the adjacent tiles/terrain change in the map. These also give a sense
  of the light source being at top-left in the minimap. Compare to the
  unused [LowRadarColor](LowRadarColor "LowRadarColor"){.wikilink} and
  [HighRadarColor](HighRadarColor "HighRadarColor"){.wikilink}.

<!-- -->

- **Padding** bytes are used as fillers, could have any value, typically
  assigned with 0xCD in original game files.

The actual tile and extra graphics are encoded after the tile cell
header.

For old, potentially outdated documentation reference, see [TMP
documentation](http://xhp.xwis.net/documents/TMP_TS_Format.html).

## Footnotes

<references />

## See Also {#see_also}

- [IsoMapPack5](IsoMapPack5 "IsoMapPack5"){.wikilink} -- how tiles are
  stored in maps.

[Category:General_Editing_Information](Category:General_Editing_Information "Category:General_Editing_Information"){.wikilink}
[Category:File
Formats](Category:File_Formats "Category:File Formats"){.wikilink}

[^1]: Confusingly, these cells sometimes referred to as \"tiles\". This
    is especially true in `{{td}}`{=mediawiki} and `{{ra}}`{=mediawiki},
    where a [tileset](TileSets "tileset"){.wikilink} refers to what
    [tile](#tiles "tile"){.wikilink} does here (a collection of cells).

[^2]: Sometimes referred to as terrain type (not to be confused with
    [TerrainTypes](TerrainTypes "TerrainTypes"){.wikilink})
