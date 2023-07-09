/*  MaterialsExtract
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
#include "nlohmann/json.hpp"
#include "project.h"

std::string_view filters[]{
    "*.materials$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = MaterialsExtract_DESC
    " v" MaterialsExtract_VERSION ", " MaterialsExtract_COPYRIGHT "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "names.txt");
  return true;
}

static constexpr uint32 WSAO_ID = CompileFourCC("OASW");
static constexpr uint32 WSAO_ID_BE = CompileFourCC("WSAO");

static constexpr uint32 WSST_ID = CompileFourCC("TSSW");
static constexpr uint32 WSCP_ID = CompileFourCC("PCSW");
static constexpr uint32 WSPP_ID = CompileFourCC("PPSW");
static constexpr uint32 WSVP_ID = CompileFourCC("PVSW");
static constexpr uint32 WSTX_ID = CompileFourCC("XTSW");
static constexpr uint32 WSPA_ID = CompileFourCC("APSW");
static constexpr uint32 WSMA_ID = CompileFourCC("AMSW");

void to_json(nlohmann::json &j, const StringHash &item) {
  j = std::to_string(item);
}

namespace nlohmann {
template <> struct adl_serializer<StringHash> {
  static void to_json(json &j, const StringHash &value) { ::to_json(j, value); }

  static void from_json(const json &, StringHash &) {}
};
} // namespace nlohmann

struct Header {
  uint32 numMaterials;
  uint32 numWSMA;
  uint32 numWSST0;
  uint32 numST0Subitems;
  uint32 numWSST1;
  uint32 numST1Subitems;
  uint32 numWSVP;
  uint32 numVPSubitems;
  uint32 unk1[2];
  uint32 numWSPP;
  uint32 numPPSubitems;
  uint32 unk2[2];
  uint32 numWSCP;
  uint32 numCPSubitems;
  uint32 numWSTX;
  uint32 unk;
  uint32 numWSPA;
};

template <> void FByteswapper(Header &id, bool) {
  FByteswapper(id.numMaterials);
  FByteswapper(id.numWSMA);
  FByteswapper(id.numWSST0);
  FByteswapper(id.numST0Subitems);
  FByteswapper(id.numWSST1);
  FByteswapper(id.numST1Subitems);
  FByteswapper(id.numWSVP);
  FByteswapper(id.numVPSubitems);
  FByteswapper(id.unk1);
  FByteswapper(id.numWSPP);
  FByteswapper(id.numPPSubitems);
  FByteswapper(id.unk2);
  FByteswapper(id.numWSCP);
  FByteswapper(id.numCPSubitems);
  FByteswapper(id.numWSTX);
  FByteswapper(id.unk);
  FByteswapper(id.numWSPA);
}

struct WSSTItem {
  uint32 unk0;
  uint32 unk1;
};

template <> void FByteswapper(WSSTItem &id, bool) {
  FByteswapper(id.unk0);
  FByteswapper(id.unk1);
}

void to_json(nlohmann::json &j, const WSSTItem &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
  };
}

std::array<std::vector<std::vector<WSSTItem>>, 2> ProcessWSST(BinReaderRef_e rd,
                                                              Header &main) {
  std::array<std::vector<std::vector<WSSTItem>>, 2> items;
  items[0].resize(main.numWSST0);
  items[1].resize(main.numWSST1);

  for (auto &i : items[0]) {
    rd.ReadContainer(i);
  }

  for (auto &i : items[1]) {
    rd.ReadContainer(i);
  }

  return items;
}

struct WSCPItem {
  uint32 id;
  float data[4];
};

template <> void FByteswapper(WSCPItem &id, bool) {
  FByteswapper(id.id);
  FByteswapper(id.data);
}

void to_json(nlohmann::json &j, const WSCPItem &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"data", item.data},
  };
}

std::vector<std::vector<WSCPItem>> ProcessWSCP(BinReaderRef_e rd,
                                               Header &main) {
  std::vector<std::vector<WSCPItem>> items;
  items.resize(main.numWSCP);

  for (auto &i : items) {
    rd.ReadContainer(i);
  }

  return items;
}

struct WSxPItem {
  std::vector<std::array<float, 4>> items;
  uint64 null0;
};

template <> void FByteswapper(WSxPItem &id, bool) { FByteswapper(id.null0); }

template <> void FByteswapper(std::array<float, 4> &id, bool) {
  FByteswapper(id[0]);
  FByteswapper(id[1]);
  FByteswapper(id[2]);
  FByteswapper(id[3]);
}

void to_json(nlohmann::json &j, const WSxPItem &item) { j = item.items; }

std::vector<WSxPItem> ProcessWSPP(BinReaderRef_e rd, Header &main) {
  std::vector<WSxPItem> items;
  items.resize(main.numWSPP);

  for (auto &i : items) {
    rd.ReadContainer(i.items);
    rd.Read(i.null0);

    assert(i.null0 == 0);
  }

  return items;
}

std::vector<WSxPItem> ProcessWSVP(BinReaderRef_e rd, Header &main) {
  std::vector<WSxPItem> items;
  items.resize(main.numWSVP);

  for (auto &i : items) {
    rd.ReadContainer(i.items);
    rd.Read(i.null0);

    assert(i.null0 == 0);
  }

  return items;
}

std::vector<StringHash> ProcessWSTX(BinReaderRef_e rd, Header &main) {
  std::vector<StringHash> items;
  items.resize(main.numWSTX);

  for (auto &i : items) {
    i = ReadStringHash(rd);
  }

  return items;
}

struct WSPAItem {
  StringHash id;
  uint32 flags;
  int32 st0Index;
  int32 st1Index;
  uint32 constantPropertyIndex;
  uint32 pixelPropertyIndex;
  uint32 vertexPropertyIndex;
  StringHash pixelShader;
  StringHash vertexShader;
};

std::vector<WSPAItem> ProcessWSPA(BinReaderRef_e rd, Header &main) {
  std::vector<WSPAItem> items;
  items.resize(main.numWSPA);

  for (auto &i : items) {
    i.id = ReadStringHash(rd);
    rd.Read(i.flags);
    rd.Read(i.st0Index);
    rd.Read(i.st1Index);
    rd.Read(i.constantPropertyIndex);
    rd.Read(i.pixelPropertyIndex);
    rd.Read(i.vertexPropertyIndex);
    i.pixelShader = ReadStringHash(rd);
    i.vertexShader = ReadStringHash(rd);
  }

  return items;
}

struct WSMAItem {
  StringHash uid;
  std::vector<StringHash> textures;
  uint32 index;
  uint32 numTextures;
  uint32 textureBegin;
  uint32 renderPassIndex;
};

struct Material : WSMAItem {
  Material(WSMAItem &item) : WSMAItem(item) {}
  StringHash renderPass;
  uint32 flags;
  std::vector<WSSTItem> st0;
  std::vector<WSSTItem> st1;
  std::vector<WSCPItem> constantProperties;
  WSxPItem pixelProperties;
  WSxPItem vertexProperties;
  StringHash pixelShader;
  StringHash vertexShader;
};

void to_json(nlohmann::json &j, const Material &item) {
  j = nlohmann::json{
      {"uid", item.uid},
      {"textures", item.textures},
      {"index", item.index},
      {"renderPass", item.renderPass},
      {"flags", item.flags},
      {"st0", item.st0},
      {"st1", item.st1},
      {"constantProperties", item.constantProperties},
      {"pixelProperties", item.pixelProperties},
      {"vertexProperties", item.vertexProperties},
      {"pixelShader", item.pixelShader},
      {"vertexShader", item.vertexShader},
  };
}

std::map<uint32, WSMAItem> ProcessWSMA(BinReaderRef_e rd, Header &main) {
  std::map<uint32, WSMAItem> items;

  for (size_t i = 0; i < main.numWSMA; i++) {
    WSMAItem item;
    item.uid = ReadStringHash(rd);
    std::vector<uint32> identifiers;
    rd.ReadContainer(identifiers);

    rd.Read(item.index);
    rd.Read(item.numTextures);
    rd.Read(item.textureBegin);
    rd.Read(item.renderPassIndex);

    for (auto i : identifiers) {
      items.emplace(i, item);
    }
  }

  return items;
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != WSAO_ID) {
    if (id == WSAO_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  Header hdr;
  rd.Read(hdr);
  const size_t fileSize = rd.GetSize();

  std::map<uint32, WSMAItem> materials;
  std::vector<StringHash> textures;
  std::vector<WSPAItem> renderPasses;
  std::array<std::vector<std::vector<WSSTItem>>, 2> sts;
  std::vector<WSxPItem> pixelProps;
  std::vector<WSxPItem> vertexProps;
  std::vector<std::vector<WSCPItem>> constProps;

  while (rd.Tell() < fileSize) {
    uint32 id_;
    rd.Read(id_);

    switch (id_) {
    case WSST_ID:
      sts = ProcessWSST(rd, hdr);
      break;
    case WSCP_ID:
      constProps = ProcessWSCP(rd, hdr);
      break;
    case WSPP_ID:
      pixelProps = ProcessWSPP(rd, hdr);
      break;
    case WSVP_ID:
      vertexProps = ProcessWSVP(rd, hdr);
      break;
    case WSTX_ID:
      textures = ProcessWSTX(rd, hdr);
      break;
    case WSPA_ID:
      renderPasses = ProcessWSPA(rd, hdr);
      break;
    case WSMA_ID:
      materials = ProcessWSMA(rd, hdr);
      break;
    default:
      throw std::runtime_error("Undefined block type");
    }
  }

  auto ectx = ctx->ExtractContext("materials");
  std::stringstream stream;

  if (ectx->RequiresFolders()) {
    for (size_t i = 0; i < 10; i++) {
      char buf[2]{char('0' + i), 0};
      ectx->AddFolderPath(buf);
    }
    for (size_t i = 0; i < 6; i++) {
      char buf[2]{char('A' + i), 0};
      ectx->AddFolderPath(buf);
    }

    ectx->GenerateFolders();
  }

  for (auto &[mid, m_] : materials) {
    Material m(m_);
    auto beginIt = std::next(textures.begin(), m.textureBegin);
    std::copy(beginIt, std::next(beginIt, m.numTextures),
              std::back_inserter(m.textures));
    auto &render = renderPasses.at(m.renderPassIndex);
    m.renderPass = render.id;
    if (render.st0Index >= 0) {
      m.st0 = sts.at(0).at(render.st0Index);
    }
    if (render.st1Index >= 0) {
      m.st1 = sts.at(1).at(render.st1Index);
    }
    m.constantProperties = constProps.at(render.constantPropertyIndex);
    m.pixelProperties = pixelProps.at(render.pixelPropertyIndex);
    m.vertexProperties = vertexProps.at(render.vertexPropertyIndex);
    m.flags = render.flags;
    m.pixelShader = render.pixelShader;
    m.vertexShader = render.vertexShader;

    nlohmann::json main = m;
    main["version"] = 1;
    stream << std::setw(2) << main;

    auto uid = std::to_string(hash::GetStringHash(mid));
    std::string path;
    path.push_back(uid.front());
    path.push_back('/');
    path.append(uid);

    ectx->NewFile(path);
    ectx->SendData(std::move(stream).str());
  }
}
