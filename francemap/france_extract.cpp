/*  FranceExtract
    Copyright(C) 2023 Lukas Cone

    This program is free software : you can redistribute it and / or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.If not, see <https://www.gnu.org/licenses/>.
*/

#include "datas/app_context.hpp"
#include "datas/binreader_stream.hpp"
#include "datas/except.hpp"
#include "datas/master_printer.hpp"
#include "datas/reflector.hpp"
#include "hashstorage.hpp"
#include "megapack.hpp"
#include "meshpack.hpp"
#include "project.h"
#include <algorithm>
#include <cassert>
#include <set>

// Need some anchor point, since loosefiles is stored all around
std::string_view filters[]{
    "*nimations.pack$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = FranceExtract_DESC " v" FranceExtract_VERSION
                                 ", " FranceExtract_COPYRIGHT "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "saboteur_strings.txt");
  return true;
}

struct FrancePath {
  std::string longPath;
  std::string name0;
  float unk[12];
  std::string name1;

  void Read(BinReaderRef_e rd) {
    rd.ReadContainer(longPath);
    rd.ReadContainer(name0);
    rd.Read(unk);
    rd.ReadContainer(name1);

    longPath = longPath.c_str();
    name0 = name0.c_str();
    name1 = name1.c_str();

    hash::GetStringHash(hash::GetHash(longPath), longPath);
    hash::GetStringHash(hash::GetHash(name0), name0);
    hash::GetStringHash(hash::GetHash(name1), name1);
  }
};

struct Tile {
  uint32 hash;
  uint16 nameLen;
  float unk[6];
  uint16 null;
  uint16 lod; //??

  uint32 null0[13];
  std::vector<uint32> hashes;
  uint32 null1;

  void Read(BinReaderRef_e rd) {
    rd.Read(hash);
    rd.Read(nameLen);
    rd.Read(unk);
    rd.Read(null);
    rd.Read(lod);

    assert(nameLen == 0);
    assert(null == 0);
    assert(lod < 3);

    if (lod == 2) {
      rd.Read(null0);
      rd.ReadContainer(hashes);
      rd.Read(null1);
      assert(null1 == 0);
    }
  }
};

struct DynamicPackDesc {
  uint32 hash;
  std::string name;
  float unk0[6];
  uint16 unk1[2];
  uint32 null0[2];
  uint32 dataStart;
  uint32 numMeshes;
  uint32 numTextures;
  uint32 numPhys;
  uint32 numLayouts;
  uint32 numFB;
  uint32 null1[3];
  uint32 numPV;
  uint32 null2;
  std::vector<uint32> files;
  uint32 null;

  void Read(BinReaderRef_e rd) {
    rd.Read(hash);
    rd.ReadContainer<uint16>(name);
    name = name.c_str();
    rd.Read(unk0);
    rd.Read(unk1);
    rd.Read(null0);
    rd.Read(dataStart);
    rd.Read(numMeshes);
    rd.Read(numTextures);
    rd.Read(numPhys);
    rd.Read(numLayouts);
    rd.Read(numFB);
    rd.Read(null1);
    rd.Read(numPV);
    rd.Read(null2);
    rd.ReadContainer(files);
    rd.Read(null);

    assert(null == 0);
    assert(!name.empty());
    assert(null0[0] == 0);
    assert(null0[1] == 0);
    assert(null1[0] == 0);
    assert(null1[1] == 0);
    assert(null1[2] == 0);
    assert(null2 == 0);
  }
};

struct FranceMapItems {
  std::set<uint32> tiles;
  std::vector<DynamicPackDesc> packs;
};

FranceMapItems LoadFranceMap(BinReaderRef_e rd) {
  static constexpr uint32 MAP6_ID = CompileFourCC("6PAM");
  static constexpr uint32 MAP6_ID_BE = CompileFourCC("MAP6");

  uint32 id;
  rd.Read(id);
  if (id != MAP6_ID) {
    if (id == MAP6_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  std::string mapName;
  rd.ReadString(mapName);

  uint32 numTiles;
  rd.Read(numTiles);
  const bool isDLC = numTiles == 0;

  if (isDLC) {
    rd.Read(numTiles);
  } else {
    uint32 unk0;
    uint32 numPaths;
    uint32 unk1;

    rd.Read(unk0);
    rd.Read(numPaths);
    rd.Read(unk1);

    FrancePath dummy;

    for (size_t i = 0; i < numPaths; ++i) {
      rd.Read(dummy);
    }

    float unk2[18];
    rd.Read(unk2);
    uint16 unk3[6];
    rd.Read(unk3);
  }

  std::set<uint32> tileHashes;
  Tile dummyTile;

  for (size_t i = 0; i < numTiles; i++) {
    rd.Read(dummyTile);

    if (dummyTile.lod < 2) {
      tileHashes.emplace(dummyTile.hash);
    }
  }

  std::vector<DynamicPackDesc> dyn0;
  rd.ReadContainer(dyn0);
  std::vector<DynamicPackDesc> dyn1;
  rd.ReadContainer(dyn1);

  dyn0.insert(dyn0.end(), dyn1.begin(), dyn1.end());

  return {tileHashes, dyn0};
}

struct DynFile {
  uint32 hash0;
  uint32 offset;
  uint32 size;
  uint32 uncompressedSize;
  uint32 null;
  uint32 hash1;
};

template <> void FByteswapper(DynFile &id, bool) {
  FByteswapper(id.hash0);
  FByteswapper(id.offset);
  FByteswapper(id.size);
  FByteswapper(id.uncompressedSize);
  FByteswapper(id.hash1);
}

struct Cinematic {
  uint32 offset;
  uint32 size = 0;
  mutable bool used = false;
};

std::map<uint32, Cinematic> LoadCinpack(BinReaderRef_e rd, size_t endOffset) {
  uint32 id;
  rd.Read(id);
  if (id != 0xC) {
    if (id == 0xC000000) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  uint32 numItems;
  rd.Read(numItems);

  std::map<uint32, Cinematic> items;
  std::vector<uint32> offsets;

  for (uint32 i = 0; i < numItems; i++) {
    uint32 id;
    uint32 offset;
    bool unk;

    rd.Read(id);
    rd.Read(offset);
    rd.Read(unk);

    items.emplace(id, Cinematic{offset});
    offsets.emplace_back(offset);
  }

  std::sort(offsets.begin(), offsets.end());

  for (auto &[_, item] : items) {
    auto nextOffset =
        std::upper_bound(offsets.begin(), offsets.end(), item.offset);

    if (es::IsEnd(offsets, nextOffset)) {
      item.size = endOffset - item.offset;
    } else {
      item.size = *nextOffset - item.offset;
    }
  }

  return items;
}

static constexpr uint32 SBLA_ID = CompileFourCC("ALBS");

void AppProcessFile(AppContext *ctx) {
  std::string workFolder(ctx->workingFile.GetFolder());
  FranceMapItems dynpacks;
  int verbosity = appInfo.internalSettings->verbosity;
  std::map<uint32, Cinematic> cinematics;
  AppContextFoundStream looseFiles;
  BinReaderRef_e cinpacks;

  try {
    looseFiles = ctx->FindFile(workFolder, "loosefiles_*.pack$");

    if (verbosity) {
      PrintInfo("Found loosefiles package");
    }

    BinReaderRef rd(*looseFiles.Get());
    const size_t filesSize = rd.GetSize();

    while (rd.Tell() < filesSize) {
      uint32 hash;
      rd.Read(hash);
      uint32 dataSize;
      rd.Read(dataSize);
      char name[120];
      rd.Read(name);
      std::string_view strName(name);
      if (strName.ends_with("rance.map")) {
        rd.Push();
        dynpacks = LoadFranceMap(rd);
        rd.Pop();
      } else if (strName.ends_with("inematics.cinpack")) {
        rd.Push();
        cinpacks = rd;
        cinpacks.SetRelativeOrigin(rd.Tell());
        cinematics = LoadCinpack(rd, dataSize);
        rd.Pop();
      }
      rd.Skip(dataSize);
      rd.ApplyPadding(16);
    }
  } catch (const es::FileNotFoundError &) {
    if (verbosity) {
      PrintInfo("loosefiles package not found, looking up france.map");
    }

    try {
      auto found = ctx->FindFile(workFolder, "rance.map$");
      dynpacks = LoadFranceMap(*found.Get());
    } catch (const es::FileNotFoundError &) {
      auto found = ctx->FindFile(workFolder, "FRANCE.map$");
      dynpacks = LoadFranceMap(*found.Get());
    }

    if (verbosity) {
      PrintInfo("loosefiles package not found, looking up cinematics.cinpack");
    }

    looseFiles = ctx->FindFile(workFolder, "inematics.cinpack$");
    BinReaderRef rd_(*looseFiles.Get());
    cinematics = LoadCinpack(rd_, rd_.GetSize());
    cinpacks = rd_;
  }

  if (dynpacks.packs.empty() && dynpacks.tiles.empty()) {
    throw std::runtime_error("france.map not found");
  }

  struct Megapacks {
    AppContextFoundStream stream;
    BinReaderRef_e rd;
    std::map<uint32, FileRange> files;

    Megapacks(AppContextFoundStream &&stream_)
        : stream(std::move(stream_)), rd(*stream.Get()),
          files(LoadMegaPack(rd)) {}
  };

  std::vector<Megapacks> megapacks;

  if (verbosity) {
    PrintInfo("Looking up mega0.megapack");
  }
  megapacks.emplace_back(ctx->FindFile(workFolder, "ega0.megapack$"));

  auto ectx = ctx->ExtractContext("france");
  std::string inBuffer;
  std::string outBuffer;

  for (auto &d : dynpacks.packs) {
    std::string curPath = d.name;
    curPath.push_back('/');

    auto ExtractFromPacks = [&](BinReaderRef_e rd) {
      static constexpr uint32 SBLA_ID = CompileFourCC("ALBS");
      static constexpr uint32 SBLA_ID_BE = CompileFourCC("SBLA");
      uint32 id;
      rd.Read(id);

      if (id != SBLA_ID) {
        if (id == SBLA_ID_BE) {
          rd.SwapEndian(true);
        } else {
          throw es::InvalidHeaderError(id);
        }
      }

      const char *dtex = rd.SwappedEndian() ? "XETD" : "DTEX";

      rd.Read(id); // dummy
      assert(id == 0);
      std::vector<DynFile> meshes;
      rd.ReadContainer(meshes, d.numMeshes);

      std::vector<DynFile> phys;
      rd.ReadContainer(phys, d.numPhys);

      // No idea about order
      std::vector<DynFile> layouts;
      rd.ReadContainer(layouts, d.numLayouts);

      std::vector<DynFile> fbData;
      rd.ReadContainer(fbData, d.numFB);

      std::vector<DynFile> pvData;
      rd.ReadContainer(pvData, d.numPV);

      std::vector<DynFile> textures;
      rd.ReadContainer(textures, d.numTextures);

      std::map<uint32, std::string> meshNames;

      for (auto &df : meshes) {
        auto mName = ExtractMeshPack(rd, curPath, ectx, inBuffer, outBuffer);
        hash::GetStringHash(df.hash0, mName);
      }

      for (auto &df : phys) {
        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".phy");
        Extract(ectx, df.size, df.uncompressedSize, inBuffer, outBuffer, rd);
      }

      for (auto &df : layouts) {
        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".lay");
        ectx->SendData(inBuffer);
      }

      for (auto &df : fbData) {
        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".fb");
        ectx->SendData(inBuffer);
      }

      for (auto &df : pvData) {
        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".pv");
        ectx->SendData(inBuffer);
      }

      for (auto &df : textures) {
        if (!df.size) {
          continue;
        }

        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".dtex");
        ectx->SendData(dtex);
        ectx->SendData(inBuffer);
      }
    };

    bool found_ = false;

    for (auto &m : megapacks) {
      if (auto found = m.files.find(d.hash); !es::IsEnd(m.files, found)) {
        found->second.used = true;
        m.rd.Seek(found->second.offset);
        ExtractFromPacks(m.rd);
        found_ = true;
        break;
      }
    }

    if (!found_)
      try {
        auto stream =
            ctx->FindFile(workFolder, std::to_string(d.hash) + ".pack");
        ExtractFromPacks(*stream.Get());
        found_ = true;
      } catch (const es::FileNotFoundError &) {
      }

    if (!found_) {
      if (auto found = cinematics.find(d.hash); !es::IsEnd(cinematics, found)) {
        cinpacks.Seek(found->second.offset);
        cinpacks.ReadContainer(inBuffer, found->second.size);
        found->second.used = true;
        found_ = true;
        auto fileName = curPath;
        fileName.pop_back();
        ectx->NewFile(fileName + ".cin");
        ectx->SendData(inBuffer);
      }
    }

    if (!found_) {
      printerror(
          "Couldn't find: " << std::to_string(hash::GetStringHash(d.hash)));
    }
  }

  for (auto &m : megapacks) {
    for (auto &d : m.files) {
      if (!d.second.used) {
        m.rd.Seek(d.second.offset);
        uint32 id0;
        uint32 id1;
        m.rd.Read(id0);
        m.rd.Read(id1);

        if (id0 == SBLA_ID && id1 == 0) {
          printwarning("Unused resource "
                       << std::to_string(hash::GetStringHash(d.first)));
        }
      }
    }
  }
}
