/*  MegaPack
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

#include "megapack.hpp"
#include "project.h"
#include "spike/app_context.hpp"
#include "spike/except.hpp"
#include "spike/io/binreader_stream.hpp"
#include "spike/reflect/reflector.hpp"

std::string_view filters[]{
    ".kilopack$",
    ".kiloPack$",
    ".megapack$",
    ".megaPack$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = MegaPack_DESC " v" MegaPack_VERSION ", " MegaPack_COPYRIGHT
                            "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "saboteur_strings.txt");
  return true;
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != MP_ID) {
    if (id == MP_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  std::vector<File> files;
  rd.ReadContainer(files);
  // std::vector<FileId> fileIds;
  // rd.ReadContainer(fileIds, files.size());

  auto ectx = ctx->ExtractContext();
  std::string bufferStr;

  for (auto &f : files) {
    rd.Seek(f.offset);
    rd.ReadContainer(bufferStr, f.size);

    static constexpr uint32 SBLA_ID = CompileFourCC("ALBS");
    static constexpr uint32 SBLA_ID_BE = CompileFourCC("SBLA");

    uint32 id;
    memcpy(&id, bufferStr.data(), 4);
    const char *ext = ".dat";

    if (id == SBLA_ID || id == SBLA_ID_BE) {
      ext = ".pack";
    }

    ectx->NewFile(std::to_string(hash::GetStringHash(f.id.index)) + ext);
    ectx->SendData(bufferStr);
  }
}
