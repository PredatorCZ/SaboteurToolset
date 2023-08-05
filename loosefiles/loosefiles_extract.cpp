/*  LooseFiles
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

#include "project.h"
#include "spike/app_context.hpp"
#include "spike/io/binreader_stream.hpp"
#include "spike/master_printer.hpp"
#include "spike/reflect/reflector.hpp"

std::string_view filters[]{
    "loosefiles_*.pack$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = LooseFiles_DESC " v" LooseFiles_VERSION ", " LooseFiles_COPYRIGHT
                              "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

void AppProcessFile(AppContext *ctx) {
  BinReaderRef rd(ctx->GetStream());
  auto ectx = ctx->ExtractContext();
  std::string bufferStr;

  const size_t filesSize = rd.GetSize();

  while (rd.Tell() < filesSize) {
    uint32 hash;
    rd.Read(hash);
    uint32 dataSize;
    rd.Read(dataSize);
    char name[120];
    rd.Read(name);
    ectx->NewFile(name);
    rd.ReadContainer(bufferStr, dataSize);
    rd.ApplyPadding(16);
    ectx->SendData(bufferStr);
  }
}
