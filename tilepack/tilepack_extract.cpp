/*  TilePack
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
#include "datas/reflector.hpp"
#include "hashstorage.hpp"
#include "meshpack.hpp"
#include "project.h"
#include <cassert>

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header =
        TilePack_DESC " v" TilePack_VERSION ", " TilePack_COPYRIGHT "Lukas Cone",
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "saboteur_strings.txt");
  return true;
}

static constexpr uint32 SBLA_ID = CompileFourCC("ALBS");
static constexpr uint32 SBLA_ID_BE = CompileFourCC("SBLA");
static constexpr uint32 HEI1_ID = CompileFourCC("1IEH");

struct HeiFile {
  uint32 hash0;
  uint32 offset;
  uint32 size;
  uint32 uncompressedSize;
  uint32 null;
  uint32 hash1;
};

template <> void FByteswapper(HeiFile &id, bool) {
  FByteswapper(id.hash0);
  FByteswapper(id.offset);
  FByteswapper(id.size);
  FByteswapper(id.uncompressedSize);
  FByteswapper(id.hash1);
}

struct HeightHeader {
  uint32 id;
  uint32 numWBlocks;
  uint32 numHBlocks;
  float width;
  float height;
};

template <> void FByteswapper(HeightHeader &id, bool) {
  FByteswapper(id.id);
  FByteswapper(id.numWBlocks);
  FByteswapper(id.numHBlocks);
  FByteswapper(id.width);
  FByteswapper(id.height);
}

struct HeightMeta {
  uint32 null0[3];
  uint32 numMeshes;
  uint32 numMasks;
  uint32 numPhys;
  uint32 numFB;
  uint32 numPV;
  uint32 unk2[3];
  uint32 numLayouts;
  uint32 null1;
};

template <> void FByteswapper(HeightMeta &id, bool) {
  FByteswapper(id.null0[3]);
  FByteswapper(id.numMeshes);
  FByteswapper(id.numMasks);
  FByteswapper(id.numPhys);
  FByteswapper(id.numFB);
  FByteswapper(id.numPV);
  FByteswapper(id.unk2[3]);
  FByteswapper(id.numLayouts);
  FByteswapper(id.null1);
}

struct HeightBlock {
  uint32 hash;
  std::vector<uint32> hashes;

  void Read(BinReaderRef_e rd) {
    rd.Read(hash);
    rd.ReadContainer(hashes);
  }
};

struct Height {
  HeightHeader hdr;
  std::vector<uint8> data;
  HeightMeta meta;
  std::vector<uint32> hashes;
  std::vector<HeightBlock> blocks;

  void Read(BinReaderRef_e rd) {
    rd.Read(hdr);
    if (hdr.id != HEI1_ID) {
      throw es::InvalidHeaderError(hdr.id);
    }
    rd.ReadContainer(data, hdr.numHBlocks * hdr.numWBlocks);
    rd.Read(meta);
    rd.ReadContainer(hashes);
    rd.ReadContainer(blocks);

    assert(meta.null0[0] == 0);
    assert(meta.null0[1] == 0);
    // assert(meta.null0[2] == 0);
    assert(meta.unk2[0] == 0);
    assert(meta.unk2[1] == 0);
    assert(meta.unk2[2] == 0);
    assert(meta.null1 == 0);
  }
};

struct Mask {
  std::string fileName;
  uint32 unk0[2];
  uint16 unk1[3];
  uint32 uncompressedSize;
  uint32 unk2;
  uint32 size;

  void Read(BinReaderRef_e rd) {
    rd.ReadContainer(fileName);
    rd.Read(unk0);
    rd.Read(unk1);
    rd.Read(uncompressedSize);
    rd.Read(unk2);
    rd.Read(size);
  }
};

struct Meta {
  uint32 null0[3];
  uint32 numMeshes;
  uint32 numTextures;
  uint32 null1[6];
  uint32 numLayouts;
  uint32 null2[3];
};

template <> void FByteswapper(Meta &id, bool) {
  FByteswapper(id.null0);
  FByteswapper(id.numMeshes);
  FByteswapper(id.numLayouts);
  FByteswapper(id.null1);
  FByteswapper(id.numTextures);
  FByteswapper(id.null2);
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != SBLA_ID) {
    if (id == SBLA_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  int32 metaSize;
  rd.Read(metaSize);

  if (metaSize < 1) {
    throw std::runtime_error("Expected metadata");
  }

  std::string inBuffer;
  std::string outBuffer;

  Height meta;
  rd.Push();
  try {
    rd.Read(meta);
  } catch (const es::InvalidHeaderError &e) {
    if (metaSize != 0x3c) {
      throw std::runtime_error("Unknown pack type.");
    }
    rd.Pop();

    Meta meta;
    rd.Read(meta);

    std::vector<HeiFile> meshes;
    std::vector<HeiFile> layouts;
    std::vector<HeiFile> textures;

    rd.ReadContainer(meshes, meta.numMeshes);
    rd.ReadContainer(layouts, meta.numLayouts);
    rd.ReadContainer(textures, meta.numTextures);

    assert(meta.null0[0] == 0);
    assert(meta.null0[1] == 0);
    assert(meta.null0[2] == 0);
    assert(meta.null1[0] == 0);
    assert(meta.null1[1] == 0);
    assert(meta.null1[2] == 0);
    assert(meta.null1[3] == 0);
    assert(meta.null1[4] == 0);
    assert(meta.null1[5] == 0);
    assert(meta.null2[0] == 0);
    assert(meta.null2[1] == 0);
    assert(meta.null2[2] == 0);

    auto ectx = ctx->ExtractContext();

    for (auto &m : meshes) {
      auto mName = ExtractMeshPack(rd, {}, ectx, inBuffer, outBuffer);
      hash::GetStringHash(m.hash0, mName);
    }

    for (auto &s : layouts) {
      ectx->NewFile(
          std::to_string(hash::GetStringHash(s.hash1 ? s.hash1 : s.hash0)) +
          ".lay");
      Extract(ectx, s.size, s.uncompressedSize, inBuffer, outBuffer, rd);
    }

    const char *dtex = rd.SwappedEndian() ? "XETD" : "DTEX";

    for (auto &df : textures) {
        if (!df.size) {
          continue;
        }

        rd.ReadContainer(inBuffer, df.size);

        ectx->NewFile(std::to_string(hash::GetStringHash(df.hash0)) +
                      ".dtex");
        ectx->SendData(dtex);
        ectx->SendData(inBuffer);
      }

    return;
  }

  auto ectx = ctx->ExtractContext();

  std::vector<HeiFile> meshes;
  std::vector<HeiFile> phys;
  std::vector<HeiFile> layouts;
  std::vector<HeiFile> fbData;
  std::vector<HeiFile> pvData;
  std::vector<HeiFile> masks;

  rd.ReadContainer(meshes, meta.meta.numMeshes);
  rd.ReadContainer(phys, meta.meta.numPhys);
  rd.ReadContainer(layouts, meta.meta.numLayouts);
  rd.ReadContainer(fbData, meta.meta.numFB);
  rd.ReadContainer(pvData, meta.meta.numPV);
  rd.ReadContainer(masks, meta.meta.numMasks);

  for (auto &m : meshes) {
    auto mName = ExtractMeshPack(rd, {}, ectx, inBuffer, outBuffer);
    hash::GetStringHash(m.hash0, mName);
  }

  for (auto &df : phys) {
    ectx->NewFile(std::to_string(hash::GetStringHash(df.hash0)) + ".phy");
    Extract(ectx, df.size, df.uncompressedSize, inBuffer, outBuffer, rd);
  }

  for (auto &s : layouts) {
    ectx->NewFile(
        std::to_string(hash::GetStringHash(s.hash1 ? s.hash1 : s.hash0)) +
        ".lay");
    Extract(ectx, s.size, s.uncompressedSize, inBuffer, outBuffer, rd);
  }

  for (auto &s : fbData) {
    ectx->NewFile(std::to_string(hash::GetStringHash(s.hash0)) + ".fb");
    Extract(ectx, s.size, s.uncompressedSize, inBuffer, outBuffer, rd);
  }

  for (auto &s : pvData) {
    ectx->NewFile(std::to_string(hash::GetStringHash(s.hash0)) + ".pv");
    Extract(ectx, s.size, s.uncompressedSize, inBuffer, outBuffer, rd);
  }

  for (auto &s : masks) {
    Mask mask;
    rd.Read(mask);
    ectx->NewFile(mask.fileName + ".mask");
    Extract(ectx, mask.size, mask.uncompressedSize, inBuffer, outBuffer, rd);
    hash::GetStringHash(s.hash0, mask.fileName);
  }
}
