/*  GlobalExtract
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
#include <cassert>

// Need some anchor point, since loosefiles is stored all around
std::string_view filters[]{
    "*nimations.pack$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = GlobalExtract_DESC " v" GlobalExtract_VERSION
                                 ", " GlobalExtract_COPYRIGHT "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "names.txt");
  return true;
}

struct DynamicPackDesc {
  uint32 assetIndex;
  std::string name;
  uint8 data[28];
  std::vector<FileId> textures;
  std::vector<FileId> meshes;
  uint32 dataOffset;
  uint32 numMehes;
  uint32 numTextures;
  uint32 numPhys;
  uint32 unk0;
  uint32 unk1;
  uint32 unk2;
  uint32 unk3;
  uint32 numFlashes;
  uint32 unk5;
  uint32 unk6;
  uint32 unk7;
  uint32 unk8;

  void Read(BinReaderRef_e rd) {
    rd.Read(assetIndex);
    rd.ReadContainer<uint16>(name);
    name = name.c_str();
    rd.Read(data);
    rd.ReadContainer(textures);
    rd.ReadContainer(meshes);
    rd.Read(dataOffset);
    rd.Read(numMehes);
    rd.Read(numTextures);
    rd.Read(numPhys);
    rd.Read(unk0);
    rd.Read(unk1);
    rd.Read(unk2);
    rd.Read(unk3);
    rd.Read(numFlashes);
    rd.Read(unk5);
    rd.Read(unk6);
    rd.Read(unk7);
    rd.Read(unk8);

    assert(unk0 == 0);
    assert(unk1 == 0);
    assert(unk2 == 0);
    assert(unk3 == 0);
    assert(unk5 == 0);
    assert(unk6 == 0);
    assert(unk7 == 0);
    assert(unk8 == 0);

    if (numFlashes) {
      assert(numMehes == 0);
      assert(numPhys == 0);
    }
  }
};

std::vector<DynamicPackDesc> LoadGlobalMap(BinReaderRef_e rd) {
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

  uint32 numDynamics;
  rd.Read(numDynamics);
  std::vector<DynamicPackDesc> idkPreloadPatterns;
  rd.ReadContainer(idkPreloadPatterns);
  std::vector<DynamicPackDesc> patterns;
  rd.ReadContainer(patterns);

  std::vector<DynamicPackDesc> dynamics;
  rd.ReadContainer(dynamics, numDynamics);

  dynamics.insert(dynamics.end(), patterns.begin(), patterns.end());

  return dynamics;
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

void AppProcessFile(AppContext *ctx) {
  std::string workFolder(ctx->workingFile.GetFolder());
  std::vector<DynamicPackDesc> dynpacks;
  int verbosity = appInfo.internalSettings->verbosity;

  try {
    auto found = ctx->FindFile(workFolder, "loosefiles_*.pack$");

    if (verbosity) {
      PrintInfo("Found loosefiles package");
    }

    BinReaderRef rd(*found.Get());
    const size_t filesSize = rd.GetSize();

    while (rd.Tell() < filesSize) {
      uint32 hash;
      rd.Read(hash);
      uint32 dataSize;
      rd.Read(dataSize);
      char name[120];
      rd.Read(name);
      if (std::string_view(name).ends_with("lobal.map")) {
        dynpacks = LoadGlobalMap(rd);
        break;
      }
      rd.Skip(dataSize);
      rd.ApplyPadding(16);
    }
  } catch (const es::FileNotFoundError &) {
    if (verbosity) {
      PrintInfo("loosefiles package not found, looking up global.map");
    }
    auto found = ctx->FindFile(workFolder, "lobal.map$");
    dynpacks = LoadGlobalMap(*found.Get());
  }

  if (dynpacks.empty()) {
    throw std::runtime_error("global.map not found");
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
    PrintInfo("Looking up dynamic0.megapack");
  }
  megapacks.emplace_back(ctx->FindFile(workFolder, "ynamic0.megapack$"));

  try {
    megapacks.emplace_back(ctx->FindFile(workFolder, "alettes0.megapack$"));
  } catch (const es::FileNotFoundError &) {
  }

  auto ectx = ctx->ExtractContext("global");
  std::string inBuffer;
  std::string outBuffer;

  for (auto &d : dynpacks) {
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
      rd.ReadContainer(meshes, d.numMehes);

      std::vector<DynFile> phys;
      rd.ReadContainer(phys, d.numPhys);

      // No idea about order
      std::vector<DynFile> flashes;
      rd.ReadContainer(flashes, d.numFlashes);

      std::vector<DynFile> textures;
      rd.ReadContainer(textures, d.numTextures);

      for (auto &df : meshes) {
        auto mName = ExtractMeshPack(rd, curPath, ectx, inBuffer, outBuffer);
        hash::GetStringHash(df.hash0, mName);
      }

      for (auto &df : phys) {
        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".phy");
        Extract(ectx, df.size, df.uncompressedSize, inBuffer, outBuffer, rd);
      }

      for (auto &df : flashes) {
        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(curPath + std::to_string(hash::GetStringHash(df.hash0)) +
                      ".swf");
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
      if (auto found = m.files.find(d.assetIndex); !es::IsEnd(m.files, found)) {
        found->second.used = true;
        m.rd.Seek(found->second.offset);
        ExtractFromPacks(m.rd);
        found_ = true;
        break;
      }
    }

    if (!found_)
      try {
        auto stream = ctx->FindFile(workFolder, d.name + ".pack");
        ExtractFromPacks(*stream.Get());
        found_ = true;
      } catch (const es::FileNotFoundError &) {
      }

    if (!found_) {
      printerror("Couldn't find: ["
                 << std::to_string(hash::GetStringHash(d.assetIndex)) << "] "
                 << d.name);
    }
  }

  for (auto &m : megapacks) {
    for (auto &d : m.files) {
      if (!d.second.used) {
        // This might be loose file, since it's only one and has already used id
        printwarning("Unused resource [" << std::hex << d.first << "]");
      }
    }
  }
}
