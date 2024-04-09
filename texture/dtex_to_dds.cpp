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
  uint16 numMips;
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

struct InnerTexture {
  uint32 mipIndex;
  uint32 width;
  uint32 height;
  uint32 null1; // depth?
  uint32 const0;
  uint32 mipSize;
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

  std::string inBuffer;
  std::string outBuffer;
  outBuffer.resize(tex.uncompressedSize);
  uint32 processedBytes = 0;

  for (size_t i = 0; i < tex.numStreams; i++) {
    rd.ReadContainer(inBuffer);

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = inBuffer.size();
    infstream.next_in = reinterpret_cast<Bytef *>(&inBuffer[0]);
    infstream.avail_out = outBuffer.size() - processedBytes;
    infstream.next_out =
        reinterpret_cast<Bytef *>(&outBuffer[0]) + processedBytes;
    inflateInit(&infstream);
    int state = inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
    processedBytes += infstream.total_out;

    if (state < 0) {
      throw std::runtime_error(infstream.msg);
    }
  }

  NewTexelContext *tctx = ctx->NewImage({
      .width = tex.width,
      .height = tex.height,
      .baseFormat =
          {
              .type =
                  [&] {
                    switch (tex.format) {
                    case CompileFourCC("DXT1"):
                      return TexelInputFormatType::BC1;
                    case CompileFourCC("DXT5"):
                      return TexelInputFormatType::BC3;
                    case 21:
                      return TexelInputFormatType::RGBA8;
                    default:
                      throw std::runtime_error("Unknown format: " +
                                               std::to_string(tex.format));
                    }

                    return TexelInputFormatType::INVALID;
                  }(),
          },
      .numMipmaps = uint8(tex.numMips),
  });

  const char *texData = outBuffer.data();
  const uint8 numMips = tctx->ShouldDoMipmaps() ? tex.numMips : 1;

  for (uint8 m = 0; m < numMips; m++) {
    const InnerTexture *datat = reinterpret_cast<const InnerTexture *>(texData);
    texData += sizeof(InnerTexture);
    tctx->SendRasterData(texData, TexelInputLayout{
                                      .mipMap = m,
                                  });

    texData += datat->mipSize;

    // assert(datat->const0 == 1);
    // assert(datat->null1 == 0);
  }
}
