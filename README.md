# SaboteurToolset

Saboteur Toolset is a collection of modding tools and research for Saboteur (2009).

The game itself runs on custom engine (probably called WildStar internally), which uses heavily compiled assets.
There are multiple steps for successful extraction (or possible modification):

- `global_extract` will extract global assets
- `france_extract` will extract assets related to main map, although it doesn't extract map itsef
- `megapack_extract` will extract pack files from any megapack or kilopack files, this tool should be only used on mega0, mega1 and mega2 megapacks, these contain map tiles
  - `tilepack_extract` will extract map tile packages extracted by `megapack_extract` tool

After all of this, following tools can be used:

- `mesh_to_gltf` converts extracted meshes into gltf format
- `dtex_to_dds` converts extracted dtex textures into dds format

All other tools are independent on each other.

This toolset runs on Spike foundation.

Head to this **[Wiki](https://github.com/PredatorCZ/PreCore/wiki/Spike)** for more information on how to effectively use it.

## AnimationsExtract

### Module command: anim_extract

Extracts `animations.pack` package. Extracts hkx files and converts internal FSM/metadata into json file.
There is no way as in current version to convert extracted hkx files because of separated metadata.

## DTEX to DDS

### Module command: dtex_to_dds

Converts extracted dtex textures into DDS format.
This tool can process only files extracted by `megapack_extract` + `tilepack_extract` or `global_extract` or `france_extract` tools.

## FranceExtract

### Module command: france_extract

Extracts assets that relates to france map iself. Mostly cinematic assets.
This tool is used in a same way as `global_extract` tool.
Just for a detail, these files will be required: loosefiles pack anywhere in subfolders or `france.map`, `cinematics.cinpack`, `animations.pack`, `france/mega0.megapack`.

## GlobalExtract

### Module command: global_extract

Extracts global assets.
Input path is a folder, where Saboteur.exe or EBOOT.BIN resides or `DLC/01` folder (tool will extract DLC content as well, so there is no need to provide DLC path).
Tool will look up all necessary files itself and extracts them accordingly.
Just for a detail, these files will be required: loosefiles pack anywhere in subfolders or `global.map`, `animations.pack`, `global/dynamic.megapack` and `global/palettes.megapack`
Similar rules apply to DLC, although most of the stuff is hard-coded due to nature of the engine.

## LoosefilesExtract

### Module command: loosefiles_extract

Extracts contents of 'loosefiles' archive.

## LUAPExtract

### Module command: luap_extract

Extracts binary Lua files from `luascripts.luap` archive. Binary lua files can be disassembled by `ChunkSpy.lua`.

## MaterialsExtract

### Module command: materials_extract

Extracts and converts materials into JSON.

## MegapackExtract

### Module command: megapack_extract

Extracts and megapack or kilopack archives.
This tool should be used on `mega0`, `mega1` and `mega2` megapacks. Other megapacks rely on tools like `global_extract` or `france_extract` because of the way files are indexed.
Kilopacks are in a weird spot, since they have duplicated files across the entire game, so there is no need to extract them at all.

## Model to GLTF

### Module command: mesh_to_gltf

Converts extracted models into GLTF format.
This tool can process only files extracted by `megapack_extract` + `tilepack_extract` or `global_extract` or `france_extract` tools.

## Extract map tiles

### Module command: tilepack_extract

Extracts map tiles from packs extracted by `megapack_extract` tool.

## [Latest Release](https://github.com/PredatorCZ/SaboteurToolset/releases)

## License

This toolset is available under GPL v3 license. (See LICENSE.md)\
This toolset uses following libraries:

- PreCore, Copyright (c) 2016-2023 Lukas Cone
- zlib, Copyright (C) 1995-2022 Jean-loup Gailly and Mark Adler
