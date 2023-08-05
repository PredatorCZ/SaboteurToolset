/*  LuaPack
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
#include "spike/except.hpp"
#include "spike/io/binreader_stream.hpp"
#include "spike/master_printer.hpp"
#include "spike/reflect/reflector.hpp"
#include <cassert>

std::string_view filters[]{
    "*.luap$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header =
        LuaPack_DESC " v" LuaPack_VERSION ", " LuaPack_COPYRIGHT "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

struct File {
  uint32 id0;
  uint32 id1;
  uint32 offset;
  uint32 compressedSize;
  uint32 uncompressedSize;
};

struct File_ : File {
  void Read(BinReaderRef rd) {
    rd.Read<File>(*this);
    uint8 null;
    rd.Read(null);
    assert(null < 2);
  }
};

static constexpr uint32 LUA_ID = CompileFourCC("\x1BLua");

struct Lua {
  uint32 id;
  uint8 version;
  uint8 format;
  uint8 endian;
  uint8 intSize;
  uint8 size_tSize;
  uint8 instructionSize;
  uint8 numberSize;
  uint8 internalFlag;
};

void AppProcessFile(AppContext *ctx) {
  BinReaderRef rd(ctx->GetStream());
  rd.Push();
  uint32 numFiles;
  rd.Read(numFiles);
  rd.Pop();

  if (numFiles > 0x1000) {
    FByteswapper(numFiles);

    if (numFiles > 0x1000) {
      throw es::InvalidHeaderError(numFiles);
    }
    rd.SwapEndian(true);
  }

  // TODO folder gen

  std::vector<File_> files;
  rd.ReadContainer(files);

  auto ectx = ctx->ExtractContext();
  std::string bufferStr;

  for (auto &f : files) {
    rd.Seek(f.offset);
    assert(f.compressedSize == f.uncompressedSize);
    rd.Push();
    Lua hdr;
    rd.Read(hdr);

    assert(hdr.id == LUA_ID);

    std::string sourceName;
    rd.ReadContainer(sourceName);
    rd.Pop();

    AFileInfo sourcePath(sourceName);
    auto foundRoot = sourcePath.GetFullPath().find("cripts/");

    if (foundRoot != std::string::npos) {
      sourceName = sourcePath.GetFullPath().substr(foundRoot - 1);
    }
    ectx->NewFile(sourceName);
    rd.ReadContainer(bufferStr, f.compressedSize);
    ectx->SendData(bufferStr);
  }
}
