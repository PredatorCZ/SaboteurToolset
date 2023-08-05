/*  ShadersExtract
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

#include "hashstorage.hpp"
#include "nlohmann/json.hpp"
#include "project.h"
#include "spike/app_context.hpp"
#include "spike/except.hpp"
#include "spike/io/binreader_stream.hpp"
#include "spike/master_printer.hpp"
#include "spike/reflect/reflector.hpp"

std::string_view filters[]{
    "*.shaders$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = ShadersExtract_DESC " v" ShadersExtract_VERSION
                                  ", " ShadersExtract_COPYRIGHT "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "saboteur_strings.txt");
  return true;
}

static constexpr uint32 SHDR_ID = CompileFourCC("RDHS");
static constexpr uint32 SHDR_ID_BE = CompileFourCC("SHDR");

static constexpr uint32 PSHD_ID = CompileFourCC("DHSP");
static constexpr uint32 VSHD_ID = CompileFourCC("DHSV");

void to_json(nlohmann::json &j, const StringHash &item) {
  j = std::to_string(item);
}

namespace nlohmann {
template <> struct adl_serializer<StringHash> {
  static void to_json(json &j, const StringHash &value) { ::to_json(j, value); }

  static void from_json(const json &, StringHash &) {}
};
} // namespace nlohmann

void ExtractShader(AppExtractContext *ectx, BinReaderRef_e rd,
                   std::string &buffer, const char *ext) {
  uint32 dummy;
  rd.Read(dummy);

  if (dummy != PSHD_ID && dummy != VSHD_ID) {
    throw es::InvalidHeaderError(dummy);
  }

  rd.Read(dummy);
  assert(dummy == 0);
  rd.Read(dummy);
  assert(dummy == 1);
  rd.Read(dummy);
  assert(dummy == 1);

  std::string name = std::to_string(ReadStringHash(rd));
  ectx->NewFile(name + ext);

  rd.ReadContainer(buffer);
  ectx->SendData(buffer);

  struct VarItem {
    std::string name;
    uint32 type;
  };

  std::vector<VarItem> vars;

  rd.ReadContainerLambda(vars, [](BinReaderRef_e rd, auto &item) {
    rd.ReadContainer(item.name);
    item.name = item.name.c_str();
    rd.Read(item.type);
    assert(item.type < 2);
  });

  if (!vars.empty()) {
    nlohmann::json main;

    for (auto &v : vars) {
      main[v.name] = v.type;
    }

    ectx->NewFile(name + ".json");
    ectx->SendData(main.dump(2));
  }
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != SHDR_ID) {
    if (id == SHDR_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  auto ectx = ctx->ExtractContext("shaders");
  uint32 numItems;
  rd.Read(numItems);
  std::string buffer;

  for (uint32 i = 0; i < numItems; i++) {
    ExtractShader(ectx, rd, buffer, ".psh");
  }

  rd.Read(numItems);
  for (uint32 i = 0; i < numItems; i++) {
    ExtractShader(ectx, rd, buffer, ".vsh");
  }
}
