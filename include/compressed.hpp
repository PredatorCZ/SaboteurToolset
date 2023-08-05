/*  SaboteurToolset
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

#pragma once
#include "spike/app_context.hpp"
#include "spike/io/binreader_stream.hpp"
#include "zlib.h"

void ExtractZlib(AppExtractContext *ectx, uint32 compSize, uint32 uncompSize,
                 std::string &inData, std::string &outData,
                 int32 wbits = MAX_WBITS) {
  outData.resize(uncompSize);
  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  infstream.avail_in = compSize;
  infstream.next_in = reinterpret_cast<Bytef *>(&inData[0]);
  infstream.avail_out = outData.size();
  infstream.next_out = reinterpret_cast<Bytef *>(&outData[0]);
  inflateInit2(&infstream, wbits);
  int state = inflate(&infstream, Z_FINISH);
  inflateEnd(&infstream);

  if (state < 0) {
    throw std::runtime_error(infstream.msg);
  }

  if (infstream.total_out < uncompSize) {
    outData.resize(infstream.total_out);
  }

  ectx->SendData(outData);
}

struct SEGS {
  uint32 id;
  uint16 version;
  uint16 numChunks;
  uint32 uncompressedSize;
  uint32 compressedSize;
};

template <> void FByteswapper(SEGS &id, bool) {
  FByteswapper(id.id);
  FByteswapper(id.version);
  FByteswapper(id.numChunks);
  FByteswapper(id.compressedSize);
  FByteswapper(id.uncompressedSize);
}

struct SEGSChunk {
  uint16 compressedSize;
  uint16 uncompressedSize;
  uint32 offset;
};

template <> void FByteswapper(SEGSChunk &id, bool) {
  FByteswapper(id.uncompressedSize);
  FByteswapper(id.compressedSize);
  FByteswapper(id.offset);
}

void Extract(AppExtractContext *ectx, uint32 compSize, uint32 uncompSize,
             std::string &inData, std::string &outData, BinReaderRef_e rd) {
  uint32 segs;
  rd.Push();
  rd.Read(segs);
  rd.Pop();

  if (segs == CompileFourCC("sges")) {
    rd.SetRelativeOrigin(rd.Tell(), false);
    SEGS hdr;
    rd.Read(hdr);
    std::vector<SEGSChunk> chunks;
    rd.ReadContainer(chunks, hdr.numChunks);

    for (auto &c : chunks) {
      rd.Seek(c.offset - 1);
      uint32 uncompSize =
          c.uncompressedSize == 0 ? 0x10000 : c.uncompressedSize;
      rd.ReadContainer(inData, c.compressedSize);
      ExtractZlib(ectx, c.compressedSize, uncompSize, inData, outData,
                  -MAX_WBITS);
    }

    rd.ResetRelativeOrigin();
    rd.Pop();
    rd.Skip(compSize);

    return;
  }

  rd.ReadContainer(inData, compSize);
  if (compSize != uncompSize) {
    ExtractZlib(ectx, compSize, uncompSize, inData, outData);
  } else {
    ectx->SendData(inData);
  }
}
