/*  DTEX2DDS
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
#include "spike/format/DDS.hpp"
#include "spike/io/binreader_stream.hpp"
#include "spike/io/binwritter_stream.hpp"
#include "spike/reflect/reflector.hpp"
#include "zlib.h"

std::string_view filters[]{
    ".dtex$",
};

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = DTEX2DDS_DESC " v" DTEX2DDS_VERSION ", " DTEX2DDS_COPYRIGHT
                            "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

static constexpr uint32 DTEX_ID = CompileFourCC("DTEX");
static constexpr uint32 DTEX_ID_BE = CompileFourCC("XETD");

struct Texture {
  uint32 format;
  uint32 unk;
  uint16 width;
  uint16 height;
  uint16 numMips; //??
  uint32 uncompressedSize;
  uint32 numStreams;

  void Read(BinReaderRef_e rd) {
    rd.Read(format);
    rd.Read(unk);
    rd.Read(width);
    rd.Read(height);
    rd.Read(numMips);
    rd.Read(uncompressedSize);
    rd.Read(numStreams);
  }
};

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != DTEX_ID) {
    /*if (id == DTEX_ID_BE) {
      rd.SwapEndian(true);
    } else {*/
    throw es::InvalidHeaderError(id);
    //}
  }

  if (rd.Tell() >= rd.GetSize()) {
    throw std::runtime_error("Empty texture");
  }

  std::string name;
  rd.ReadContainer(name);

  Texture tex;
  rd.Read(tex);

  DDS ddtex = {};
  ddtex = DDSFormat_DX10;
  ddtex.width = tex.width;
  ddtex.height = tex.height;

  switch (tex.format) {
  case CompileFourCC("DXT1"):
    ddtex.dxgiFormat = DXGI_FORMAT_BC1_UNORM;
    break;
  case CompileFourCC("DXT5"):
    ddtex.dxgiFormat = DXGI_FORMAT_BC3_UNORM;
    break;
  case 21:
    ddtex.dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    break;

  default:
    throw std::runtime_error("Unknown format: " + std::to_string(tex.format));
    break;
  }

  ddtex.NumMipmaps(tex.numMips);

  const uint32 sizetoWrite =
      ddtex.ToLegacy() ? ddtex.DDS_SIZE : ddtex.LEGACY_SIZE;
  BinWritterRef wr(ctx->NewFile(name + ".dds").str);

  wr.WriteBuffer(reinterpret_cast<const char *>(&ddtex), sizetoWrite);
  std::string inBuffer;
  std::string outBuffer;
  outBuffer.resize(tex.uncompressedSize);

  for (size_t i = 0; i < tex.numStreams; i++) {
    rd.ReadContainer(inBuffer);

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = inBuffer.size();
    infstream.next_in = reinterpret_cast<Bytef *>(&inBuffer[0]);
    infstream.avail_out = outBuffer.size();
    infstream.next_out = reinterpret_cast<Bytef *>(&outBuffer[0]);
    inflateInit(&infstream);
    int state = inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);

    if (state < 0) {
      throw std::runtime_error(infstream.msg);
    }

    wr.WriteBuffer(outBuffer.data() + 4 * 6, infstream.total_out);
  }
}
