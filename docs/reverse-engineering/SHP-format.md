
           Game|Command & Conquer: Tiberian Sun
           Game|Command & Conquer: Red Alert 2

```
The sprite format used in [Command & Conquer: Tiberian
Sun](Command_&_Conquer:_Tiberian_Sun "Command & Conquer: Tiberian Sun"){.wikilink}
and [Red Alert
2](Command_&_Conquer:_Red_Alert_2 "Red Alert 2"){.wikilink} is a
collection of 8-bit frames that all have the same dimensions. The format
uses the [Westwood
RLE-Zero](Westwood_RLE-Zero "Westwood RLE-Zero"){.wikilink} algorithm to
compact transparent areas, and uses X and Y offsets to align smaller
frame data in the full frame size, but, unlike its predecessors, uses no
real data compression like [LCW](Westwood_LCW "LCW"){.wikilink}.

An additional \'compression\', though applied on game design level
rather than on graphics level, is that all separate structure animations
in these games are typically stored in separate files. This has the
advantages that it both saves space by not including the entire
structure graphics in the animation graphics (a problem previously
solved by using [XOR Delta](Westwood_XOR_Delta "XOR Delta"){.wikilink}),
and that it gives more freedom for playing multiple looping animations
with different amounts of frames on the same structure.

Note that Tiberian Sun and Red Alert 2 are not DOS games. This format is
included on this wiki for the sake of completeness in the scope of
documenting all indexed graphics formats in the Westwood Studios games,
and as logical successor to the [Dune II
SHP](Westwood_SHP_Format_(Dune_II) "Dune II SHP"){.wikilink}, [Lands of
Lore
SHP](Westwood_SHP_Format_(Lands_of_Lore) "Lands of Lore SHP"){.wikilink}
and [C&C 1 SHP](Westwood_SHP_Format_(TD) "C&C 1 SHP"){.wikilink}
formats.

## File format {#file_format}

### Header

The file starts with a very simple 8-byte header:

  Offset   Data type                                    Name         Description
  -------- -------------------------------------------- ------------ --------------------------------------------------------------------------------
  0x00     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   Empty        Always zero. This allows a very simple first check to identify this file type.
  0x02     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FullWidth    Width of the frames.
  0x04     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FullHeight   Height of the frames.
  0x06     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   NrOfFrames   Number of frames in the file.

### Frames info table {#frames_info_table}

After the header comes an array with `NrOfFrames` entries, where each
entry has the following structure:

  Offset   Data type                                    Name          Description
  -------- -------------------------------------------- ------------- -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  0x00     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FrameX        X-offset to align the frame in the full `FullWidth*FullHeight` image.
  0x02     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FrameY        Y-offset to align the frame in the full `FullWidth*FullHeight` image.
  0x04     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FrameWidth    Width of the frame image data.
  0x06     [UINT16LE](UINT16LE "UINT16LE"){.wikilink}   FrameHeight   Height of the frame image data.
  0x08     [UINT32LE](UINT32LE "UINT32LE"){.wikilink}   Flags         A series of bit flags that determine how the game interprets the data. The two flags are `HasTransparency` (bit 1) and `UsesRle` (bit 2). Since the RLE compression only collapses transparency, bit 2 should never be enabled if bit 1 isn\'t.
  0x0C     [BYTE](BYTE "BYTE"){.wikilink}\[4\]          FrameColor    The first three bytes of this contain R/G/B colour bytes that can be used on the game\'s minimap. Unlike the game\'s colour palettes, this uses 8 bits per colour component.
  0x10     [UINT32LE](UINT32LE "UINT32LE"){.wikilink}   Reserved      Always 0.
  0x14     [UINT32LE](UINT32LE "UINT32LE"){.wikilink}   DataOffset    Offset of the frame\'s data. If there is no data, this should be 0. In the original files, the data, and thus these offsets, are always aligned to multiples of 8 bytes, though this is technically not necessary. Since the zero-compression algorithm has line lengths in its format, and uncompressed data is exactly `FrameWidth * FrameHeight` bytes, no end offset is needed.

The `flags` aren\'t really decompression options but options to tell the
game\'s blitters how to paint the graphics on the screen. Because of
this, it is important to enable the `HasTransparency` bit if the
graphics data contains zero-value bytes, otherwise they may get painted
as actual zero-value colours, instead of getting handled as
transparency.

#### Image data decompression {#image_data_decompression}

The main way the image data is reduced in size is by saving only a
cropped frame, which needs to be realigned in the full frame size using
the X and Y offsets. To get the cropped frame, though, the data first
needs to be interpreted correctly.

As mentioned, there are two flags that can be set on graphics, and the
second should never be enabled if the first one isn\'t, giving three
different types of stored data:

- On fully opaque data, both the `HasTransparency` and `UsesRle` flags
  are disabled.
- On data containing transparency which is not RLE-compressed, the
  `HasTransparency` flag is enabled, but the `UsesRle` flags isn\'t.
  Typically, this is only used in case the RLE algorithm gives larger
  output than the original data, however, the game\'s mouse cursor
  graphics are saved without RLE, and will not work correctly if RLE is
  enabled on them.
- On data containing transparency which is RLE-compressed, both the
  `HasTransparency` and `UsesRle` flags are enabled.

The used compression is the Tiberian Sun variant of the [Westwood
RLE-Zero](Westwood_RLE-Zero "Westwood RLE-Zero"){.wikilink} algorithm, a
[flag-based RLE](RLE_Compression#Flag "flag-based RLE"){.wikilink}
triggering on value 00, which is treated per line, and which has a
[UINT16LE](UINT16LE "UINT16LE"){.wikilink} at the start of each line
indicating that line\'s input data length.

#### Frame colour {#frame_colour}

The `FrameColor` is the average colour of all non-transparent pixels in
the frame\'s image data, when seen in its intended palette, or rather,
in the graphics fed into the original encoder used by Westwood. Since
reapplying an average-colour algorithm to existing game sprites gives
slightly different values than the original ones, it is likely that the
original images had 8-bit colour components, and that the process of
mapping them to the intended game palette was done by the encoding tool
itself.

The main purpose of this colour is to serve as display colour on the
game\'s \'radar\' minimap. Most units and structures in the game don\'t
need this, since anything owned by either an AI or human player is
displayed on the minimap with the colour of its owner, but it is
important to have this for map resources and SHP-based terrain pieces
such as bridges.

The original encoder used by Westwood seems to have contained some
system to compensate for remappable house colours, however, or had a way
to override the house colours to the intended in-game colours, because
the frames of the tiberium resource in the game, which is remappable,
have green colours set, despite the game using red colours on the
remapping range on its colour palette. Likewise, some of the
tiberium-based mutated wildlife in the game, which is also typically
remapped to green in the game, contains green frame colours that can
only be achieved by using green-remapped graphics as source.

It must be noted that, on the tiberium graphics, the game uses a
somewhat odd system to change these colours when the tiberium in the
game is remapped to a different colour than green. For that reason, it
is advised that when editing the tiberium graphics, the original
graphics\' frame colours are copied exactly, otherwise blue tiberium in
the game has a tendency to look strangely purple on the minimap. The
*TS/RA2 SHP Radar Colour Editor* (see
[Tools](#Tools "Tools"){.wikilink}) can apply this operation.

### Shadow filter area {#shadow_filter_area}

On the preceding games, unit and structure shadows are typically
indicated on the graphics using colour index 4 (though in C&C1 this is
technically defined by the game\'s [fading
table](Westwood_Fading_Table "fading table"){.wikilink} files),
typically coloured bright green
`<span style="background-color: #000000; color: #54FC54">`{=html}#54FC54`</span>`{=html}
on the palettes. Tiberian Sun and Red Alert 2 work differently; the
shadows are split off into additional frames behind the normal graphics,
and are painted in colour index 1.

Such shadow frames are not a specific part of the format; there are
plenty of graphics in the games (like UI graphics) that are not units or
structures in the play field that need to cast a shadow, so these frames
are not always an expected thing, but if they are present they should be
somewhat detectable by the fact there is an even number of frames, and
the pixels in the second half of the frames only use palette indices 0
and 1.

Since shadows should never overlap the main graphics, the correct way to
combine the frames is to draw the shadows *under* the main graphics.
Most conversion tools convert the shadow data to the classic index 4,
for the sake of compatibility, because index 4 is never really used in
graphics, and because this index is still set to the clearly visible
green colour on most default unit and structure palettes in Tiberian Sun
and Red Alert 2.

## Tools

```{=mediawiki}
{{BeginFileFormatTools|Type=image}}
```
```{=mediawiki}
{{FileFormatTool
| Name = [[Engie File Converter]]
| Platform = Windows
| canView = Yes
| canExport = Yes
| canImport = Yes
| editHidden = No
| editMetadata = Has save options to tweak <tt>FrameColor</tt>.
| notes = Generates the <tt>FrameColor</tt> data from the input frames, and has an option to override the red remap colours in that operation with tiberium-green.
}}
```
```{=mediawiki}
{{FileFormatTool
| Name = [http://xhp.xwis.net/ XCC Mixer]
| Platform = Windows
| canView = Yes
| canExport = Yes
| canImport = Yes
| editHidden = No
| editMetadata = No
| notes = The main conversion tool used by the modding community.
}}
```
```{=mediawiki}
{{FileFormatTool
| Name = [https://www.ppmsite.com/?go=shpbuilderinfo OS SHP Builder]
| Platform = Windows
| canView = Yes
| canExport = Yes
| canImport = Yes
| editHidden = No
| editMetadata = No
| notes = An actual graphical sprite editor
}}
```
```{=mediawiki}
{{FileFormatTool
| Name = [https://ppmforums.com/viewtopic.php?t=18679 TS/RA2 SHP Radar Colour Editor]
| Platform = Windows
| canView = No
| canExport = No
| canImport = No
| editHidden = No
| editMetadata = Yes
| notes = Specific tool for editing the <tt>FrameColor</tt> data, since most SHP tools have no support for setting it. Can only view the colours set for the SHP frames, not the graphics themselves.
}}
```
```{=mediawiki}
{{FileFormatTool
| Name = [https://github.com/OpenRA/OpenRA/wiki/Utility OpenRA Utility]
| Platform = Windows, Linux, Mac
| canView = No
| canExport = Yes
| canImport = No
| editHidden = No
| editMetadata = No
| notes = Command line tool that can export to png
}}
```
```{=mediawiki}
{{EndFileFormatTools}}
```
[Category:Westwood Studios File
Formats](Category:Westwood_Studios_File_Formats "Category:Westwood Studios File Formats"){.wikilink}
