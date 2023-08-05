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
#include "spike/io/binreader_stream.hpp"
#include <cinttypes>
#include <variant>

using StringHash = std::variant<std::string_view, uint32>;

namespace hash {
constexpr uint32 GetHash(std::string_view str) {
  if (str.empty()) {
    return 0;
  }

  uint32 retVal = 0x811C9DC5;

  for (auto c : str) {
    retVal = (retVal ^ (uint8(c) | 0x20)) * 0x1000193;
  }

  return 0x1000193 * (retVal ^ 0x2a);
}

static_assert(GetHash("ANY") == 3976557093);

void LoadStorage(const std::string &file);

StringHash GetStringHash(uint32 id);
StringHash GetStringHash(uint32 id, std::string name);
} // namespace hash

inline StringHash ReadStringHash(BinReaderRef_e rd) {
  uint32 id;
  rd.Read(id);
  return hash::GetStringHash(id);
}

namespace std {
inline std::string to_string(const StringHash &item) {
  return std::visit(
      [](auto &item) {
        if constexpr (std::is_same_v<std::string_view,
                                     std::decay_t<decltype(item)>>) {
          return std::string(item);
        } else {
          char buffer[16]{};
          snprintf(buffer, sizeof(buffer), "%" PRIX32, item);
          return std::string(buffer);
        }
      },
      item);
}
} // namespace std
