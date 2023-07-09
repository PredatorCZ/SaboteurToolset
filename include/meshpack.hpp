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
#include "compressed.hpp"
#include "datas/except.hpp"

static constexpr uint32 MSHA_ID = CompileFourCC("AHSM");

struct MSHA {
  uint32 id;
  uint32 uncompressedSize0;
  uint32 uncompressedSize1;
  uint32 compressedSize0;
  uint32 compressedSize1;
  char name[0x100];
};

template <> void FByteswapper(MSHA &id, bool) {
  FByteswapper(id.id);
  FByteswapper(id.uncompressedSize0);
  FByteswapper(id.uncompressedSize1);
  FByteswapper(id.compressedSize0);
  FByteswapper(id.compressedSize1);
}

std::string ExtractMeshPack(BinReaderRef_e rd, const std::string &curPath,
                            AppExtractContext *ectx, std::string &inBuffer,
                            std::string &outBuffer) {
  MSHA msha;
  rd.Read(msha);

  if (msha.id != MSHA_ID) {
    throw es::InvalidHeaderError(msha.id);
  }

  std::string fileName = curPath + msha.name;

  if (msha.compressedSize0) {
    ectx->NewFile(fileName + ".msh");
    const char *mesh = rd.SwappedEndian() ? "HSEM" : "MESH";
    ectx->SendData(mesh);
    Extract(ectx, msha.compressedSize0, msha.uncompressedSize0, inBuffer,
            outBuffer, rd);
  }

  if (msha.compressedSize1) {
    ectx->NewFile(fileName + ".dat");
    Extract(ectx, msha.compressedSize1, msha.uncompressedSize1, inBuffer,
            outBuffer, rd);
  }

  return msha.name;
}
