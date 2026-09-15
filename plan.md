## I am going for a Red Alert 2 and Red Alert 2: Yuri's revenge engine reimplementation
- tools/ dir is just random tools whos code may be useful and im sure i can make use of in some way outside of that
 - scripts/mixer.py contains the Mix extractor and packer
- scripts/mix_vfs.py is an incomplete (non functional) mix fuse mounter it mostly dont have the file referencing bits implemented maybe roll its functionality into mixer.py?
 - reference/Red Alert 2 Yuri's Revenge    the target game dir, its a bit messy its a copy of my actual over 20 year old install It has game mod loose ends that may prove useful. it has a junk/ subdir, this is very likely just noise you can ignore it, the campaign-saves subfolder contains savegames, and the maps subfolder contains maps, these are normally in the base dir but im reducing ls noise
 - reference/{Phobos,SyringeEX,Phobos/YRpp}   Various RA2/YR code mods that have source availble, by far, the most useeful single directory will be the YRpp submdule in  Phobos, anything different from tiberian sun should be referenced and documented in the YRpp project at least most of it.
 - reference/Tiberian sun reimplementation  - A reimplementation of the previous title, the engines are nearly identical with some small changes and updates this is an absolute goldmine
  - reference/mix contains extracted .MIX files from the yuri's revenge folder
 - reference/ida/ and reference/ghidra/ Contain ida and ghidra decompiled main binary
 - reference/fish_tycoon  - tools me and various llms made for reimplementing a different game, but its also compiled by the same compiler against the same system and compiler libs im pretty sure dumping tables should be similar


normally filename.ext is the base game 
and filenamemd.ext is the mission disk, or expansion pack
## 1. Base Red Alert 2 Files (8 Files)

    ra2.mix — The master archive holding all secondary data (audio, units, animations).
    language.mix — Text strings, localizations, and UI assets.
    maps01.mix — Allied campaign maps.
    maps02.mix — Soviet campaign maps.
    movies01.mix — Allied cinematic cutscenes.
    movies02.mix — Soviet cinematic cutscenes.
    multi.mix — Vanilla multiplayer and skirmish maps.
    theme.mix — Red Alert 2 soundtrack (Frank Klepacki masterpieces). 
    expandxx.mix  -  Official update asset, never used, only ever used by modifications.

## 2. Yuri's Revenge Expansion Files (8 Files)

    ra2md.mix — The master archive for Yuri's Revenge expansion data.
    langmd.mix — Expansion specific UI and localizations.
    maps03md.mix — Yuri's Revenge campaign maps.
    movmd03.mix (or mov03md.mix I tink ive even seen some releases name it as movies03.mix and it work) — Yuri's Revenge cutscenes.
    multimd.mix — Expansion skirmish/multiplayer maps.
    thememd.mix — Yuri's Revenge expansion music tracks.
    ecachemd01.mix — Official patches/bugfixes.
    expandmd01.mix — Official asset, any increments higher are unofficial modifications



## more notes

- the main binaries (game.exe gamemd.exe) are nocd patched to even work at all on modern systems, its the binary from a later release that amusingly hashmatches with an unofficial patched binary, this means they are unpacked and unofficially patched therefore their internal vfs may be odd and decompile in unpredictable ways - its worth noting since im sure this is why .rtext and .rdata and other odd places are where all the executable code is

- games code is case sensitive as lowercase because the operating system back then was NOT i have lowercased all the files by now since wine but its notable

- the main binaries are normally launched by ra2.exe or ra2md.exe

- Ghidra tools should be in path, but the symlinks may be broken and i dont remember which binary was the useful cli one if you need ghidra let me know i can get it back in path



# Any loose files that BELONG in those archives is loaded last, they too are mod files, rulesmd.ini artmd.ini are notable examples i itnend to have most loose mod stuff cleaned up but do note  loose game assets that belong in an archive are a mod override, its a thing we should support but if you are looking for reference reasons the data in the mix files is the hard truth excluding expandmd files over 01