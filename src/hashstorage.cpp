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
#include "spike/app_context.hpp"
#include "spike/io/stat.hpp"
#include "spike/master_printer.hpp"
#include <cassert>
#include <map>
#include <memory>
#include <mutex>

static std::map<uint32, std::string_view> FILES;
static es::MappedFile mappedFile;
static std::mutex accessMutex;

static struct Stats {
  // maybe use atomic
  size_t numCalls = 0;
  size_t numHits = 0;
  std::vector<std::unique_ptr<std::string>> STRINGS;
} STATS;

void AppFinishContext() {
  for (auto &s : STATS.STRINGS) {
    PrintWarning("Unused hash: ", *s);
  }

  PrintInfo("String calls: ", STATS.numCalls, ", string hits: ", STATS.numHits);
}

void hash::LoadStorage(const std::string &file) {
  mappedFile = es::MappedFile(file);
  std::string_view totalMap(static_cast<const char *>(mappedFile.data),
                            mappedFile.fileSize);

  while (!totalMap.empty()) {
    size_t found = totalMap.find_first_of("\r\n");

    if (found != totalMap.npos) {
      auto sub = totalMap.substr(0, found);

      if (sub.empty()) {
        continue;
      }

      uint32 hash = GetHash(sub);

      {
        std::lock_guard lg(accessMutex);
        if (FILES.contains(hash) && FILES.at(hash) != sub) {
          PrintError("String colision: ", FILES.at(hash), " vs: ", sub);
        } else {
          FILES.emplace(hash, sub);
        }
      }

      totalMap.remove_prefix(found + 1);

      if (totalMap.front() == '\n') {
        totalMap.remove_prefix(1);
      }
    }
  }
}

StringHash hash::GetStringHash(uint32 id) {
  std::lock_guard lg(accessMutex);
  STATS.numCalls++;
  if (FILES.contains(id)) {
    STATS.numHits++;
    return FILES.at(id);
  }

  return id;
}

StringHash hash::GetStringHash(uint32 id, std::string name) {
  if (name.empty()) [[unlikely]] {
    return 0u;
  }
  std::lock_guard lg(accessMutex);
  if (FILES.contains(id)) {
    auto str = FILES.at(id);
    assert(std::equal(str.begin(), str.end(), name.begin(), name.end(),
                      [](char a, char b) { return tolower(a) == tolower(b); }));
    return str;
  }

  assert(GetHash(name) == id);

  STATS.STRINGS.emplace_back(std::make_unique<std::string>(std::move(name)));
  auto &nameRef = STATS.STRINGS.back();
  return FILES.emplace(id, *nameRef).first->second;
}
