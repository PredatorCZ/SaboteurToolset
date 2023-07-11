/*  AnimPack
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

#include "datas/app_context.hpp"
#include "datas/binreader_stream.hpp"
#include "datas/except.hpp"
#include "datas/master_printer.hpp"
#include "datas/stat.hpp"
#include "nlohmann/json.hpp"
#include "project.h"
#include "zlib.h"
#include <cassert>
#include <cctype>
#include <fstream>
#include "hashstorage.hpp"

static AppInfo_s appInfo{
    .filteredLoad = true,
    .header = AnimPack_DESC " v" AnimPack_VERSION ", " AnimPack_COPYRIGHT
                            "Lukas Cone",
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "saboteur_strings.txt");
  return true;
}

static constexpr uint32 AP0L_ID = CompileFourCC("L0PA");
static constexpr uint32 AP0L_ID_BE = CompileFourCC("AP0L");

static constexpr uint32 ANIM_ID = CompileFourCC("MINA");
static constexpr uint32 SEQC_ID = CompileFourCC("CQES");
static constexpr uint32 TRAN_ID = CompileFourCC("NART");
static constexpr uint32 EDGE_ID = CompileFourCC("EGDE");
static constexpr uint32 DIST_ID = CompileFourCC("TSID");
static constexpr uint32 BANK_ID = CompileFourCC("KNAB");
static constexpr uint32 ADD1_ID = CompileFourCC("1DDA");
static constexpr uint32 ALPH_ID = CompileFourCC("HPLA");
static constexpr uint32 SSP0_ID = CompileFourCC("0PSS");
static constexpr uint32 INTV_ID = CompileFourCC("VTNI");
static constexpr uint32 ANMA_ID = CompileFourCC("AMNA");

void to_json(nlohmann::json &j, const StringHash &item) {
  std::visit([&j](auto &item) { j = item; }, item);
}

namespace nlohmann {
template <> struct adl_serializer<StringHash> {
  static void to_json(json &j, const StringHash &value) { ::to_json(j, value); }

  static void from_json(const json &, StringHash &) {}
};
} // namespace nlohmann

struct ANIMStruct0 {
  uint32 unk[10];
};

template <> void FByteswapper(ANIMStruct0 &id, bool) { FByteswapper(id.unk); }

struct ANIMStruct1 {
  uint32 unk2;
  float unk0[2];
  uint32 unk1;
};

struct ANIM {
  bool streamed;
  bool unk1;
  bool unk4;
  float duration;
  float unk0[8];
  std::vector<uint32> bones;
  std::vector<ANIMStruct0> unk2;
  std::vector<ANIMStruct1> unk3;
  StringHash id;
};

void to_json(nlohmann::json &j, const ANIMStruct0 &item) { j = item.unk; }

void to_json(nlohmann::json &j, const ANIMStruct1 &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
      {"unk2", item.unk2},
  };
}

void to_json(nlohmann::json &j, const ANIM &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"bones", item.bones},
      {"duration", item.duration},
      {"unk0", item.unk0},
      {"streamed", item.streamed},
      {"unk1", item.unk1},
      {"unk4", item.unk4},
      {"unk2", item.unk2},
      {"unk3", item.unk3},
  };
}

void ReadNull8(BinReaderRef rd) {
  uint8 null;
  rd.Read(null);

  assert(null == 0);
}

void ReadNull32(BinReaderRef rd) {
  uint32 null;
  rd.Read(null);

  assert(null == 0);
}

bool ReadBool(BinReaderRef rd) {
  uint8 null;
  rd.Read(null);

  assert(null < 2);
  return null;
}

void ProcessANIM(BinReaderRef_e rd, AppExtractContext *ectx,
                 nlohmann::json &json) {
  std::vector<ANIM> anims;
  rd.ReadContainerLambda(anims, [](BinReaderRef_e rd, ANIM &item) {
    uint32 id;
    rd.Read(id);
    item.unk4 = ReadBool(rd);
    item.streamed = ReadBool(rd);
    std::string name;
    rd.ReadContainer(name);
    rd.Read(item.duration);

    item.id = hash::GetStringHash(id, std::move(name));

    if (!item.streamed) {
      rd.ReadContainer(item.bones);
    }

    rd.Read(item.unk0);
    item.unk1 = ReadBool(rd);
    rd.ReadContainer(item.unk2);

    rd.ReadContainerLambda(item.unk3, [](BinReaderRef_e rd, ANIMStruct1 &item) {
      rd.Read(item.unk2);
      ReadNull8(rd);
      rd.Read(item.unk0);
      rd.Read(item.unk1);
    });
  });

  uint32 numAnims;
  rd.Read(numAnims);
  uint32 hkSize;
  rd.Read(hkSize);

  ectx->NewFile("animations.hkx");
  char buffer[0x40000];
  const size_t numBlocks = hkSize / sizeof(buffer);
  const size_t restSize = hkSize % sizeof(buffer);

  for (size_t b = 0; b < numBlocks; b++) {
    rd.ReadBuffer(buffer, sizeof(buffer));
    ectx->SendData({buffer, sizeof(buffer)});
  }

  if (restSize) {
    rd.ReadBuffer(buffer, restSize);
    ectx->SendData({buffer, restSize});
  }

  uint32 constInt;
  rd.Read(constInt);

  json = anims;
}

struct INTVStruct0 {
  StringHash id;
  uint32 unk;
  std::vector<StringHash> animIds;
};

void to_json(nlohmann::json &j, const INTVStruct0 &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"unk", item.unk},
      {"animIds", item.animIds},
  };
}

void ReadStringHashes(BinReaderRef_e rd, std::vector<StringHash> &items) {
  rd.ReadContainerLambda(items, [](BinReaderRef_e rd, StringHash &item) {
    item = ReadStringHash(rd);
  });
}

void ProcessINTV(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<INTVStruct0> data;
  rd.ReadContainerLambda(data, [](BinReaderRef_e rd, INTVStruct0 &item) {
    item.id = ReadStringHash(rd);
    rd.Read(item.unk);
    ReadStringHashes(rd, item.animIds);
  });

  json = data;
}

struct SEQCStruct0 {
  uint32 unk0;
  int32 unk1;
  std::vector<StringHash> anims;
  std::vector<StringHash> tags;
};

struct SEQC {
  StringHash id;
  std::vector<SEQCStruct0> unk0;
  float unk1[5];
  uint32 unk2;
  float unk3[4];
  std::vector<std::array<float, 2>> unk4;
  std::vector<std::array<float, 2>> unk5;
  std::vector<std::array<float, 2>> unk6;
  bool isLooped;
  bool unk7;
};

template <> void FByteswapper(std::array<float, 2> &id, bool) {
  FByteswapper(id[0]);
  FByteswapper(id[1]);
}

void to_json(nlohmann::json &j, const SEQCStruct0 &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
      {"anims", item.anims},
      {"tags", item.tags},
  };
}

void to_json(nlohmann::json &j, const SEQC &item) {
  j = nlohmann::json{
      {"id", item.id},     {"unk0", item.unk0},
      {"unk1", item.unk1}, {"unk2", item.unk2},
      {"unk3", item.unk3}, {"unk4", item.unk4},
      {"unk5", item.unk5}, {"unk6", item.unk6},
      {"unk7", item.unk7}, {"isLooped", item.isLooped},
  };
}

void ProcessSEQC(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<SEQC> sequences;
  rd.ReadContainerLambda(sequences, [](BinReaderRef_e rd, SEQC &item) {
    item.id = ReadStringHash(rd);
    rd.ReadContainerLambda(item.unk0, [](BinReaderRef_e rd, SEQCStruct0 &item) {
      rd.Read(item.unk0);
      rd.Read(item.unk1);
      ReadStringHashes(rd, item.anims);
      ReadStringHashes(rd, item.tags);
    });
    rd.Read(item.unk1);
    rd.Read(item.unk2);
    rd.Read(item.unk3);
    uint8 numItems[3];
    rd.Read(numItems);
    rd.ReadContainer(item.unk4, numItems[0]);
    rd.ReadContainer(item.unk4, numItems[1]);
    rd.ReadContainer(item.unk4, numItems[2]);
    item.isLooped = ReadBool(rd);
    item.unk7 = ReadBool(rd);
  });

  json = sequences;
}

struct TRANSStruct {
  StringHash from;
  StringHash fromTag;
  StringHash to;
  StringHash toTag;
  uint32 unk0[3];
  StringHash tag;
  bool hasTag;

  uint32 unk1[16];
  uint8 unk2;
  uint32 unk3[3];

  std::string name;
};

struct TRAN {
  StringHash id;
  std::vector<TRANSStruct> items;
};

void to_json(nlohmann::json &j, const TRANSStruct &item) {
  j = nlohmann::json{
      {"from", item.from},   {"fromTag", item.fromTag}, {"to", item.to},
      {"toTag", item.toTag}, {"unk0", item.unk0},       {"tag", item.tag},
      {"name", item.name},
  };

  if (item.hasTag) {
    j["unk1"] = item.unk1;
    j["unk2"] = item.unk2;
  } else {
    j["unk3"] = item.unk3;
  }
}

void to_json(nlohmann::json &j, const TRAN &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"items", item.items},
  };
}

void ParseTRANName(const std::string &name) {
  std::string curName;

  for (auto c : name) {
    if (std::isalnum(c) || c == '_') {
      curName.push_back(c);
    } else if (!curName.empty()) {
      uint32 hash = hash::GetHash(curName);
      hash::GetStringHash(hash, std::move(curName));
    }
  }
}

void ProcessTRAN(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<TRAN> transitions;
  rd.ReadContainerLambda(transitions, [](BinReaderRef_e rd, TRAN &item) {
    item.id = ReadStringHash(rd);
    rd.ReadContainerLambda(item.items,
                           [](BinReaderRef_e rd, TRANSStruct &item) {
                             item.from = ReadStringHash(rd);
                             item.fromTag = ReadStringHash(rd);
                             item.to = ReadStringHash(rd);
                             item.toTag = ReadStringHash(rd);
                             rd.Read(item.unk0);
                             uint32 tag;
                             rd.Read(tag);
                             item.tag = hash::GetStringHash(tag);
                             item.hasTag = tag != 0;
                             if (item.hasTag) {
                               rd.Read(item.unk1);
                               rd.Read(item.unk2);
                             } else {
                               rd.Read(item.unk3);
                             }
                             rd.ReadContainer<uint16>(item.name);
                             item.name = item.name.c_str();
                             ParseTRANName(item.name);
                           });
  });

  json = transitions;
}

void ProcessEDGE(BinReaderRef_e rd) {
  std::vector<uint32> items;
  rd.ReadContainer(items, 5030);
}

void ProcessDIST(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<float> items;
  rd.ReadContainer(items, 24);

  json = items;
}

struct BANKStruct {
  StringHash unk0;
  StringHash unk1;
  std::vector<StringHash> unk2;
};

struct BANK {
  StringHash id;
  StringHash parent;
  std::vector<BANKStruct> items;
};

void to_json(nlohmann::json &j, const BANKStruct &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
      {"unk2", item.unk2},
  };
}

void to_json(nlohmann::json &j, const BANK &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"parent", item.parent},
      {"items", item.items},
  };
}

void ProcessBANK(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<BANK> banks;
  rd.ReadContainerLambda(banks, [](BinReaderRef_e rd, BANK &item) {
    uint32 id;
    rd.Read(id);
    std::string name;
    rd.ReadContainer<uint16>(name);
    name = name.c_str();
    item.parent = ReadStringHash(rd);
    item.id = hash::GetStringHash(id, std::move(name));

    rd.ReadContainerLambda(item.items, [](BinReaderRef_e rd, BANKStruct &item) {
      item.unk0 = ReadStringHash(rd);
      item.unk1 = ReadStringHash(rd);
      ReadStringHashes(rd, item.unk2);
    });
  });

  json = banks;
}

struct ADD1 {
  StringHash unk0;
  StringHash unk1;
  uint32 unk2;
};

void to_json(nlohmann::json &j, const ADD1 &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
      {"unk2", item.unk2},
  };
}

void ProcessADD1(BinReaderRef_e rd, nlohmann::json &json) {
  std::vector<ADD1> adds;

  rd.ReadContainerLambda(adds, [](BinReaderRef_e rd, ADD1 &item) {
    item.unk0 = ReadStringHash(rd);
    item.unk1 = ReadStringHash(rd);
    rd.Read(item.unk2);
  });

  json = adds;
}

struct ALPH {
  std::vector<StringHash> unk0;
  std::vector<StringHash> unk1;
};

void to_json(nlohmann::json &j, const ALPH &item) {
  j = nlohmann::json{
      {"unk0", item.unk0},
      {"unk1", item.unk1},
  };
}

void ProcessALPH(BinReaderRef_e rd, nlohmann::json &json) {
  ALPH main;
  ReadStringHashes(rd, main.unk0);
  ReadStringHashes(rd, main.unk1);

  json = main;
}

struct SSPStruct0 {
  StringHash ids[17];
};

struct SSPStruct1 {
  StringHash id;
  uint32 unk;
};

struct SSPStruct2 {
  StringHash id;
  uint32 size;
  uint32 offset; // relative to ap0lanim
  std::vector<StringHash> bones;
};

struct SSPStruct3 {
  StringHash id;
  uint32 count;
  uint32 begin;
};

struct SSP0 {
  std::vector<SSPStruct0> groups;
  std::vector<SSPStruct1> unk0;
  std::vector<SSPStruct3> unk1;
  std::vector<SSPStruct2> unk2;
  std::vector<SSPStruct2> unk3;
};

void to_json(nlohmann::json &j, const SSPStruct0 &item) { j = item.ids; }

void to_json(nlohmann::json &j, const SSPStruct1 &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"unk", item.unk},
  };
}

void to_json(nlohmann::json &j, const SSPStruct2 &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"bones", item.bones},
  };
}

void to_json(nlohmann::json &j, const SSPStruct3 &item) {
  j = nlohmann::json{
      {"id", item.id},
      {"count", item.count},
      {"begin", item.begin},
  };
}

void to_json(nlohmann::json &j, const SSP0 &item) {
  j = nlohmann::json{
      {"groups", item.groups}, {"unk0", item.unk0}, {"unk1", item.unk1},
      {"unk2", item.unk2},     {"unk3", item.unk3},
  };
}

void ProcessSSP0(BinReaderRef_e rd, nlohmann::json &json,
                 AppExtractContext *ectx) {
  SSP0 main;
  uint32 hack;
  rd.Read(hack);
  rd.Skip(-4);
  if (hack == 8) {
    rd.ReadContainerLambda(main.groups,
                           [](BinReaderRef_e rd, SSPStruct0 &item) {
                             for (auto &i : item.ids) {
                               i = ReadStringHash(rd);
                             }
                           });
  }

  rd.ReadContainerLambda(main.unk0, [](BinReaderRef_e rd, SSPStruct1 &item) {
    item.id = ReadStringHash(rd);
    rd.Read(item.unk);
  });

  rd.ReadContainerLambda(main.unk1, [](BinReaderRef_e rd, SSPStruct3 &item) {
    item.id = ReadStringHash(rd);
    rd.Read(item.count);
    rd.Read(item.begin);
  });

  uint32 numAnims0;
  uint32 numAnims1;
  rd.Read(numAnims0);
  rd.Read(numAnims1);

  main.unk2.resize(numAnims0);
  main.unk3.resize(numAnims1);

  for (SSPStruct2 &item : main.unk2) {
    item.id = ReadStringHash(rd);
    rd.Read(item.size);
    rd.Read(item.offset);
  }

  for (SSPStruct2 &item : main.unk3) {
    item.id = ReadStringHash(rd);
    rd.Read(item.size);
    rd.Read(item.offset);
  }

  rd.Push();
  std::string buffer;
  auto Extract = [rd, ectx, &buffer](std::vector<SSPStruct2> &items) {
    for (auto &item : items) {
      rd.Seek(item.offset);
      uint64 id;
      rd.Read(id);
      rd.ReadContainer(buffer);
      ReadStringHashes(rd, item.bones);

      std::visit(
          [ectx](auto &item) {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(item)>>) {
              char buffer[32]{};
              snprintf(buffer, sizeof(buffer), "%" PRIX32 ".hkx", item);
              ectx->NewFile(buffer);
            } else {
              ectx->NewFile(std::string(item) + ".hkx");
            }
          },
          item.id);

      ectx->SendData(buffer);
    }
  };

  Extract(main.unk2);
  Extract(main.unk3);

  rd.Pop();

  json = main;
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != AP0L_ID) {
    if (id == AP0L_ID_BE) {
      rd.SwapEndian(true);
    } else {
      throw es::InvalidHeaderError(id);
    }
  }

  const size_t fileSize = rd.GetSize();
  nlohmann::json main;
  main["version"] = 1;
  auto ectx = ctx->ExtractContext();

  while (rd.Tell() < fileSize) {
    uint32 id_;
    rd.Read(id_);

    switch (id_) {
    case ANIM_ID:
      ProcessANIM(rd, ectx, main["anims"]);
      break;
    case INTV_ID:
      ProcessINTV(rd, main["interruptions"]);
      break;
    case SEQC_ID:
      ProcessSEQC(rd, main["sequences"]);
      break;
    case TRAN_ID:
      ProcessTRAN(rd, main["transitions"]);
      break;
    case EDGE_ID:
      ProcessEDGE(rd);
      break;
    case DIST_ID:
      ProcessDIST(rd, main["dists"]);
      break;
    case BANK_ID:
      ProcessBANK(rd, main["banks"]);
      break;
    case ADD1_ID:
      ProcessADD1(rd, main["adds"]);
      break;
    case ALPH_ID:
      ProcessALPH(rd, main["alpha"]);
      break;
    case SSP0_ID:
      ProcessSSP0(rd, main["ssp"], ectx);
      break;
    case ANMA_ID: {
      uint32 size;
      rd.Read(size); // spl2
      rd.Read(size);
      rd.Skip(size);
      rd.Read(size);
      rd.Skip(size * 4);
      break;
    }
    default:
      throw std::runtime_error("Undefined block type");
    }
  }

  std::string tempFile = es::GetTempFilename();
  {
    std::fstream str(tempFile, std::ios::trunc | std::ios::out | std::ios::in);
    ectx->NewFile("animations.json");
    str << std::setw(2) << main;

    const size_t strSize = str.tellp();
    str.seekg(0);
    char buffer[0x40000];
    const size_t numBlocks = strSize / sizeof(buffer);
    const size_t restSize = strSize % sizeof(buffer);

    for (size_t b = 0; b < numBlocks; b++) {
      str.read(buffer, sizeof(buffer));
      ectx->SendData({buffer, sizeof(buffer)});
    }

    if (restSize) {
      str.read(buffer, restSize);
      ectx->SendData({buffer, restSize});
    }
  }
  es::RemoveFile(tempFile);
}
