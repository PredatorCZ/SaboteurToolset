/*  MESH2GLTF
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
#include "datas/binwritter_stream.hpp"
#include "datas/except.hpp"
#include "datas/master_printer.hpp"
#include "datas/matrix44.hpp"
#include "datas/reflector.hpp"
#include "datas/stat.hpp"
#include "datas/vectors.hpp"
#include "gltf.hpp"
#include "hashstorage.hpp"
#include "project.h"
#include "uni/model.hpp"
#include "uni/rts.hpp"
#include <cassert>

std::string_view filters[]{
    ".msh$",
};

static AppInfo_s appInfo{
    .header = MESH2GLTF_DESC " v" MESH2GLTF_VERSION ", " MESH2GLTF_COPYRIGHT
                             "Lukas Cone",
    .filters = filters,
};

AppInfo_s *AppInitModule() { return &appInfo; }

bool AppInitContext(const std::string &dataFolder) {
  hash::LoadStorage(dataFolder + "names.txt");
  return true;
}

static constexpr uint32 MESH_ID = CompileFourCC("MESH");
static constexpr uint32 MESH_ID_BE = CompileFourCC("HSEM");

template <> void FByteswapper(uni::RTSValue &id, bool) {
  FByteswapper(id.translation);
  FByteswapper(id.rotation);
  FByteswapper(id.scale);
}

void ReadNull8(BinReaderRef rd) {
  uint8 null;
  rd.Read(null);

  assert(null == 0);
}

void ReadNull16(BinReaderRef rd) {
  uint16 null;
  rd.Read(null);

  assert(null == 0);
}

void ReadNull32(BinReaderRef rd) {
  uint32 null;
  rd.Read(null);

  assert(null == 0);
}

struct BBOX_ {
  Vector min;
  Vector4 max;

  void Read(BinReaderRef_e rd) {
    rd.Read(min);
    rd.Read(max);
  }
};

struct BBOX {
  Vector4 min;
  Vector4 max;

  void Read(BinReaderRef_e rd) {
    rd.Read(min);
    rd.Read(max);
  }
};

struct Bone {
  StringHash boneName0;
  StringHash boneName1;
  uint32 unk0;
  BBOX bbox;

  void Read(BinReaderRef_e rd) {
    boneName0 = ReadStringHash(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    boneName1 = ReadStringHash(rd);
    ReadNull32(rd);
    rd.Read(unk0);
    rd.Read(bbox);
  }
};

struct MESHSkeleton {
  std::vector<uint8> boneIds;
  std::vector<es::Matrix44> localTMS;
  std::vector<es::Matrix44> ibms;
  std::vector<uni::RTSValue> transforms;
  std::vector<Bone> bones;
  std::vector<int16> parentIds;

  void Read(BinReaderRef_e rd) {
    uint32 numBones2;
    uint32 numBones3;
    uint32 numBones4;
    uint32 numUnkBones0;
    uint32 numUnkBones1;

    rd.Read(numUnkBones0);
    ReadNull32(rd);
    ReadNull32(rd);
    rd.Read(numBones2);
    rd.Read(numUnkBones1);
    rd.Read(numBones3);
    ReadNull32(rd);
    rd.Read(numBones4);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);

    assert(numBones2 == numBones3);
    assert(numBones2 == numBones4);

    rd.ReadContainer(boneIds, numBones2);

    for (uint32 i = 0; i < numUnkBones0; i++) {
      ReadNull8(rd);
    }

    rd.ReadContainer(localTMS, numBones2);
    ibms.resize(numBones2);
    rd.ReadContainer(bones, numBones2);
    rd.ReadContainer(transforms, numBones2);
    rd.ReadContainer(parentIds, numBones2);

    for (uint32 i = 0; i < numBones2; i++) {
      ReadNull32(rd);
    }

    if (numUnkBones1) {
      ReadNull16(rd);
    }
  }
};

void GenerateIBMS(MESHSkeleton &item, size_t boneId,
                  std::vector<std::vector<uint32>> &children) {
  int parentId = item.parentIds.at(boneId);

  if (parentId < 0) {
    item.ibms.at(boneId) = item.localTMS.at(boneId);
  } else {
    item.ibms.at(boneId) = item.ibms.at(parentId) * item.localTMS.at(boneId);
  }

  for (auto &c : children.at(boneId)) {
    GenerateIBMS(item, c, children);
  }
};

void GenerateIBMS(MESHSkeleton &item) {
  std::vector<uint32> rootNodes;
  std::vector<std::vector<uint32>> children;
  children.resize(item.boneIds.size());

  for (size_t i = 0; i < item.ibms.size(); i++) {
    int parentId = item.parentIds.at(i);

    if (parentId < 0) {
      rootNodes.push_back(i);
    } else {
      children.at(parentId).push_back(i);
    }
  }

  for (auto r : rootNodes) {
    GenerateIBMS(item, r, children);
  }

  for (auto &m : item.ibms) {
    m = -m;
  }
}

struct BoneRemap {
  es::Matrix44 ibm;
  uint32 boneId;

  void Read(BinReaderRef_e rd) {
    rd.Read(ibm);
    rd.Read(boneId);
  }
};

struct Stream {
  uint32 numVertices;
  uint32 format;
  uint32 vertexBufferOffset;
  uint32 vertexBufferSize;
  uint32 vertexBufferStride;
  uint32 indexBufferOffset;
  uint32 indexBufferSize;
  uint32 unk0;
  uint32 faceType;
  uint32 numIndices;

  gltf::Attributes attributes;
  size_t indexBegin;

  void Read(BinReaderRef_e rd) {
    for (size_t i = 0; i < 6; i++) {
      ReadNull32(rd);
    }

    rd.Read(numVertices);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    rd.Read(format);

    for (size_t i = 0; i < 11; i++) {
      ReadNull32(rd);
    }
    rd.Read(vertexBufferOffset);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    rd.Read(vertexBufferSize);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    rd.Read(vertexBufferStride);
    ReadNull32(rd);
    rd.Read(indexBufferOffset);
    rd.Read(indexBufferSize);
    rd.Read(unk0);
    rd.Read(faceType);
    rd.Read(numIndices);
    ReadNull32(rd);

    assert(faceType == 1);
  }
};

struct Primitive {
  BBOX bbox;
  uint32 streamIndex;
  uint32 indexOffset;
  uint32 numFaces;
  uint32 numIndices;

  void Read(BinReaderRef_e rd) {
    ReadNull32(rd);
    int32 const0;
    rd.Read(const0);
    assert(const0 == -1);

    for (size_t i = 0; i < 10; i++) {
      ReadNull32(rd);
    }

    rd.Read(bbox);
    rd.Read(streamIndex);
    ReadNull32(rd);
    rd.Read(indexOffset);
    rd.Read(numFaces);
    rd.Read(numIndices);
  }
};

struct Drawcall {
  uint32 primitiveIndex;
  StringHash material;
  uint32 unk1;

  void Read(BinReaderRef_e rd) {
    rd.Read(primitiveIndex);
    material = ReadStringHash(rd);
    ReadNull32(rd);
    rd.Read(unk1);
  }
};

struct MESH {
  BBOX_ bbox;
  StringHash name;
  uint32 unk0;
  uint32 numBones0;
  uint32 numBoneRemaps;
  uint16 numStreams;
  uint16 numPrimitives;
  uint32 numDrawCalls;

  void Read(BinReaderRef_e rd) {
    for (size_t i = 0; i < 19; i++) {
      ReadNull32(rd);
    }

    rd.Read(bbox);

    for (size_t i = 0; i < 11; i++) {
      ReadNull32(rd);
    }

    name = ReadStringHash(rd);

    for (size_t i = 0; i < 8; i++) {
      ReadNull32(rd);
    }

    rd.Read(unk0);

    for (size_t i = 0; i < 4; i++) {
      ReadNull32(rd);
    }

    rd.Read(numBones0);
    rd.Read(numBoneRemaps);
    ReadNull32(rd);
    rd.Read(numStreams);
    rd.Read(numPrimitives);
    ReadNull32(rd);
    ReadNull32(rd);
    ReadNull32(rd);
    rd.Read(numDrawCalls);
    ReadNull32(rd);
    ReadNull32(rd);

    assert(numBones0 != 0);
  }
};

struct Proxy : uni::PrimitiveDescriptor {
  const char *buffer = nullptr;
  size_t stride = 0;
  size_t offset = 0;
  size_t index = 0;
  Usage_e usage;
  uni::FormatDescr type;

  Proxy() = default;
  Proxy(uni::FormatType fmtType, uni::DataType dtType, Usage_e usage_)
      : usage{usage_}, type{fmtType, dtType} {}

  const char *RawBuffer() const { return buffer; }
  size_t Stride() const { return stride; }
  size_t Offset() const { return offset; }
  size_t Index() const { return index; }
  Usage_e Usage() const { return usage; }
  uni::FormatDescr Type() const { return type; }
  uni::BBOX UnpackData() const { return {}; }
  UnpackDataType_e UnpackDataType() const { return UnpackDataType_e::None; };
};

struct IndexProxy : uni::IndexArray {
  std::vector<uint16> indices;
  const char *RawIndexBuffer() const override {
    return reinterpret_cast<const char *>(indices.data());
  }
  size_t IndexSize() const override { return 2; }
  size_t NumIndices() const override { return indices.size(); }
};

static const Proxy Vertex_Position(uni::FormatType::FLOAT,
                                   uni::DataType::R16G16B16A16,
                                   Proxy::Usage_e::Position);

static const Proxy Vertex_BoneWeights(uni::FormatType::UNORM,
                                      uni::DataType::R8G8B8A8,
                                      Proxy::Usage_e::BoneWeights);
static const Proxy Vertex_BoneIndices(uni::FormatType::UINT,
                                      uni::DataType::R8G8B8A8,
                                      Proxy::Usage_e::BoneIndices);
static const Proxy Vertex_UV(uni::FormatType::FLOAT, uni::DataType::R16G16,
                             Proxy::Usage_e::TextureCoordiante);
static const Proxy Vertex_Normal(uni::FormatType::FLOAT,
                                 uni::DataType::R32G32B32,
                                 Proxy::Usage_e::Normal);
static const Proxy Vertex_Tangent(uni::FormatType::UNORM,
                                  uni::DataType::R8G8B8A8,
                                  Proxy::Usage_e::Tangent);
static const Proxy Vertex_Color(uni::FormatType::UNORM, uni::DataType::R8G8B8A8,
                                Proxy::Usage_e::VertexColor);

template <std::same_as<Proxy>... T>
std::vector<Proxy> BuildVertices(T... items) {
  size_t offset = 0;
  static constexpr size_t fmtStrides[]{0,  128, 96, 64, 64, 48, 32, 32, 32,
                                       32, 32,  32, 24, 16, 16, 16, 16, 8};
  uint8 indices[0x10]{};

  auto NewDesc = [&](Proxy item) {
    item.offset = offset;
    item.index = indices[uint8(item.usage)]++;
    offset += fmtStrides[uint8(item.type.compType)] / 8;
    return item;
  };

  return std::vector<Proxy>{NewDesc(items)...};
}

enum VertexFormat_e {
  PositionType_HalfFloat = 2,
  SkinType_None = 0,
  SkinType_4Bone = 1,
};

struct VertexFormat {
  VertexFormat_e positionType : 2;
  VertexFormat_e skinType : 2;
  uint32 numVertexColors : 4;
  uint32 numTexCoords : 4;
  uint32 useNormal : 1;
  uint32 useTangent : 1;
  uint32 reserved : 10;
  uint32 constTag : 8;

  void Swap();
};

static const std::map<uint32, std::vector<Proxy>> proxies{
    {
        0x1b001102,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001112,
        BuildVertices(Vertex_Position, Vertex_Color, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001202,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001302,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_UV,
                      Vertex_Normal),
    },
    {
        0x1b001402,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_UV,
                      Vertex_UV, Vertex_Normal),
    },
    {
        0x1b003102,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_Normal,
                      Vertex_Tangent),
    },
    {
        0x1b003112,
        BuildVertices(Vertex_Position, Vertex_Color, Vertex_UV, Vertex_Normal,
                      Vertex_Tangent),
    },
    {
        0x1b003202,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_Normal,
                      Vertex_Tangent),
    },
    {
        0x1b003302,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_UV,
                      Vertex_Normal, Vertex_Tangent),
    },
    {
        0x1b003402,
        BuildVertices(Vertex_Position, Vertex_UV, Vertex_UV, Vertex_UV,
                      Vertex_UV, Vertex_Normal, Vertex_Tangent),
    },

    {
        0x1b001106,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001206,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001306,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_UV, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b001116,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_Color, Vertex_UV, Vertex_Normal),
    },
    {
        0x1b003106,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_Normal, Vertex_Tangent),
    },
    {
        0x1b003206,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_UV, Vertex_Normal, Vertex_Tangent),
    },
    {
        0x1b003306,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_UV, Vertex_UV, Vertex_UV, Vertex_Normal,
                      Vertex_Tangent),
    },
    {
        0x1b003116,
        BuildVertices(Vertex_Position, Vertex_BoneWeights, Vertex_BoneIndices,
                      Vertex_Color, Vertex_UV, Vertex_Normal, Vertex_Tangent),
    },

};

void ProcessStream(Stream &str, BinReaderRef_e rd, GLTFModel &main) {
  rd.Seek(str.indexBufferOffset);
  IndexProxy indices;
  rd.ReadContainer(indices.indices, str.numIndices);
  auto &stream = main.GetIndexStream();
  str.indexBegin = stream.wr.Tell();
  stream.wr.WriteContainer(indices.indices);

  rd.Seek(str.vertexBufferOffset);
  std::string buffer;
  rd.ReadContainer(buffer, str.vertexBufferSize);

  auto format = proxies.find(str.format);

  if (es::IsEnd(proxies, format)) {
    PrintError("Undefined format ", std::hex, str.format);
    return;
  }

  auto formats = format->second;
  std::vector<UCVector4> joints;
  std::vector<UCVector4> weights;

  for (auto &f : formats) {
    f.buffer = buffer.data() + f.offset;
    f.stride = str.vertexBufferStride;

    switch (f.usage) {
    case Proxy::Usage_e::Position:
      main.WritePositions(str.attributes, f, str.numVertices);
      break;

    case Proxy::Usage_e::Normal:
      str.attributes["NORMAL"] = main.WriteNormals16(f, str.numVertices);
      break;

    case Proxy::Usage_e::TextureCoordiante:
      main.WriteTexCoord(str.attributes, f, str.numVertices);
      break;
    case Proxy::Usage_e::VertexColor:
      main.WriteVertexColor(str.attributes, f, str.numVertices);
      break;

    case Proxy::Usage_e::BoneWeights: {
      uni::FormatCodec::fvec sampled;
      f.Codec().Sample(sampled, f.RawBuffer(), str.numVertices, f.Stride());
      f.Resample(sampled);

      for (auto &v : sampled) {
        auto pure = v;
        pure *= 0xff;
        pure = Vector4A16(_mm_round_ps(pure._data, _MM_ROUND_NEAREST));
        auto comp = pure.Convert<uint8>();
        weights.emplace_back(comp);
      }

      break;
    }

    case Proxy::Usage_e::BoneIndices: {
      uni::FormatCodec::ivec sampled;
      f.Codec().Sample(sampled, f.RawBuffer(), str.numVertices, f.Stride());

      for (auto &v : sampled) {
        joints.emplace_back(v.Convert<uint8>());
      }

      break;
    }

    default:
      break;
    }
  }

  if (!joints.empty()) {
    for (size_t v = 0; v < str.numVertices; v++) {
      for (size_t e = 0; e < 4; e++) {
        if (weights.at(v)[e] == 0) {
          joints.at(v)[e] = 0;
        }
      }
    }

    {
      auto &stream = main.GetVt4();
      auto [acc, index] = main.NewAccessor(stream, 4);
      acc.count = str.numVertices;
      acc.componentType = gltf::Accessor::ComponentType::UnsignedByte;
      acc.normalized = true;
      acc.type = gltf::Accessor::Type::Vec4;
      stream.wr.WriteContainer(weights);
      str.attributes["WEIGHTS_0"] = index;
    }

    {
      auto &stream = main.GetVt4();
      auto [acc, index] = main.NewAccessor(stream, 4);
      acc.count = str.numVertices;
      acc.componentType = gltf::Accessor::ComponentType::UnsignedByte;
      acc.type = gltf::Accessor::Type::Vec4;
      stream.wr.WriteContainer(joints);
      str.attributes["JOINTS_0"] = index;
    }
  }
}

void ProcessMesh(BinReaderRef_e rd, AppContext *ctx, MESH &hdr, GLTFModel &main,
                 MESHSkeleton &skeleton) {
  auto bufferStream =
      ctx->RequestFile(ctx->workingFile.ChangeExtension(".dat"));
  BinReaderRef strRd(*bufferStream.Get());
  std::vector<BoneRemap> boneRemaps;

  if (hdr.numBoneRemaps) {
    uint32 unk0;
    rd.Read(unk0);
    assert(hdr.numBoneRemaps == unk0);
    ReadNull32(rd);
    rd.ReadContainer(boneRemaps, hdr.numBoneRemaps);
  }

  std::vector<Stream> streams;
  rd.ReadContainer(streams, hdr.numStreams);
  std::vector<Primitive> primitives;
  rd.ReadContainer(primitives, hdr.numPrimitives);
  std::vector<Drawcall> drawCalls;
  rd.ReadContainer(drawCalls, hdr.numDrawCalls);

  for (auto &s : streams) {
    ProcessStream(s, strRd, main);
  }

  gltf::Mesh &mesh = main.meshes.emplace_back();

  for (auto &p : primitives) {
    Stream &str = streams.at(p.streamIndex);
    gltf::Primitive &prim = mesh.primitives.emplace_back();
    prim.attributes = str.attributes;
    prim.indices = main.accessors.size();

    gltf::Accessor &acc = main.accessors.emplace_back();
    acc.bufferView = main.GetIndexStream().slot;
    acc.byteOffset = str.indexBegin + p.indexOffset * 2;
    acc.componentType = gltf::Accessor::ComponentType::UnsignedShort;
    acc.count = p.numIndices;
    acc.type = gltf::Accessor::Type::Scalar;
  }

  if (!skeleton.boneIds.empty()) {
    if (!boneRemaps.empty()) {
      auto &skins = main.skins.emplace_back();

      auto &ibmStream = main.SkinStream();
      auto [acc, id] = main.NewAccessor(ibmStream, 16);
      skins.inverseBindMatrices = id;
      acc.componentType = gltf::Accessor::ComponentType::Float;
      acc.count = boneRemaps.size();
      acc.type = gltf::Accessor::Type::Mat4;

      for (auto &b : boneRemaps) {
        size_t boneId = skeleton.boneIds.at(b.boneId);
        skins.joints.emplace_back(boneId);
        ibmStream.wr.Write(skeleton.ibms.at(boneId));
        // ibmStream.wr.Write(b.ibm); unreliable
      }
    }
  }

  main.scenes.front().nodes.emplace_back(main.nodes.size());

  auto &node = main.nodes.emplace_back();
  node.mesh = 0;

  if (!skeleton.boneIds.empty()) {
    if (!boneRemaps.empty()) {
      node.skin = 0;
    }
  }

  main.extensionsRequired.emplace_back("KHR_mesh_quantization");
  main.extensionsUsed.emplace_back("KHR_mesh_quantization");
}

void AppProcessFile(AppContext *ctx) {
  BinReaderRef_e rd(ctx->GetStream());
  uint32 id;
  rd.Read(id);

  if (id != MESH_ID) {
    /*if (id == MESH_ID_BE) {
      rd.SwapEndian(true);
    } else {*/
    throw es::InvalidHeaderError(id);
    //}
  }

  MESH hdr;
  rd.Read(hdr);
  MESHSkeleton skeleton;
  GLTFModel main;

  if (hdr.numBones0 > 1) {
    rd.Read(skeleton);
    GenerateIBMS(skeleton);

    for (size_t b = 0; b < skeleton.bones.size(); b++) {
      auto &node = main.nodes.emplace_back();

      node.name = std::to_string(skeleton.bones.at(b).boneName0);
      auto &tm = skeleton.transforms.at(b);
      memcpy(node.translation.data(), &tm.translation,
             sizeof(node.translation));
      memcpy(node.rotation.data(), &tm.rotation, sizeof(node.rotation));
      memcpy(node.scale.data(), &tm.scale, sizeof(node.scale));
    }

    for (size_t b = 0; b < skeleton.bones.size(); b++) {
      auto parentId = skeleton.parentIds.at(b);

      if (parentId < 0) {
        main.scenes.front().nodes.emplace_back(b);
        continue;
      }

      main.nodes.at(parentId).children.push_back(b);
    }
  } else {
    assert(hdr.numBoneRemaps == 0);
  }

  if (hdr.numStreams > 0) {
    ProcessMesh(rd, ctx, hdr, main, skeleton);
  }

  auto &str = ctx->NewFile(ctx->workingFile.ChangeExtension(".glb")).str;
  main.FinishAndSave(str, std::string(ctx->workingFile.GetFolder()));
}
