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

#include "hashstorage.hpp"

static constexpr uint32 MP_ID = CompileFourCC("00PM");
static constexpr uint32 MP_ID_BE = CompileFourCC("MP00");

struct alignas(8) FileId {
  uint32 crc; // Different hash, or crc?
  uint32 index;

  bool operator<(FileId o) const {
    return reinterpret_cast<const uint64 &>(*this) <
           reinterpret_cast<const uint64 &>(o);
  }
};

template <> void FByteswapper(FileId &id, bool) {
  FByteswapper(id.crc);
  FByteswapper(id.index);
}

struct File {
  FileId id;
  uint32 size;
  uint64 offset;

  void Read(BinReaderRef_e rd) {
    rd.Read(id);
    rd.Read(size);
    rd.Read(offset);
  }
};

struct FileRange {
  mutable bool used;
  uint32 offset;
};

inline std::map<uint32, FileRange> LoadMegaPack(BinReaderRef_e rd) {
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

  std::map<uint32, FileRange> retVal;

  for (auto &f : files) {
    retVal.emplace(f.id.index, FileRange(false, f.offset));
  }

  return retVal;
}
