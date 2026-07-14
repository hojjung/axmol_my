#include <doctest.h>

#include "axmol/3d/Animate3D.h"
#include "axmol/3d/Animation3D.h"
#include "axmol/3d/GltfLoader.h"
#include "axmol/3d/MeshDataCache.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/base/Director.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/renderer/TextureCache.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace ax;

namespace
{
class ScopedFixture final
{
public:
    explicit ScopedFixture(std::filesystem::path path) : _path(std::move(path)) {}
    ~ScopedFixture()
    {
        std::error_code error;
        std::filesystem::remove(_path, error);
    }

    const std::filesystem::path& path() const { return _path; }

private:
    std::filesystem::path _path;
};

void appendUint32(std::vector<uint8_t>& output, uint32_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
    output.push_back(static_cast<uint8_t>(value >> 16));
    output.push_back(static_cast<uint8_t>(value >> 24));
}

void appendUint16(std::vector<uint8_t>& output, uint16_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
}

void appendFloat(std::vector<uint8_t>& output, float value)
{
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    appendUint32(output, bits);
}

std::vector<uint8_t> readBinary(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return {};
    const auto size = stream.tellg();
    if (size <= 0)
        return {};
    std::vector<uint8_t> result(static_cast<size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(result.data()), size);
    return stream ? result : std::vector<uint8_t>{};
}

std::filesystem::path writeGlb(std::string_view name, std::string json, const std::vector<uint8_t>& binary)
{
    while (json.size() % 4 != 0)
        json.push_back(' ');

    std::vector<uint8_t> glb;
    glb.reserve(12 + 8 + json.size() + 8 + binary.size());
    appendUint32(glb, 0x46546C67U);
    appendUint32(glb, 2U);
    appendUint32(glb, static_cast<uint32_t>(12 + 8 + json.size() + 8 + binary.size()));
    appendUint32(glb, static_cast<uint32_t>(json.size()));
    appendUint32(glb, 0x4E4F534AU);
    glb.insert(glb.end(), json.begin(), json.end());
    appendUint32(glb, static_cast<uint32_t>(binary.size()));
    appendUint32(glb, 0x004E4942U);
    glb.insert(glb.end(), binary.begin(), binary.end());

    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    return path;
}

std::filesystem::path makeEmbeddedKtxFixture(bool overflowImageOffset)
{
    const auto ktxPath =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "platform" / "fixtures" / "stylized_albedo.ktx2";
    const std::vector<uint8_t> ktx = readBinary(ktxPath);

    std::vector<uint8_t> binary;
    for (float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F})
        appendFloat(binary, value);
    appendUint16(binary, 0);
    appendUint16(binary, 1);
    appendUint16(binary, 2);
    while (binary.size() % 4 != 0)
        binary.push_back(0);
    for (float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F})
        appendFloat(binary, value);
    for (float value : {0.25F, 0.75F, 0.5F, 0.5F, 0.75F, 0.25F})
        appendFloat(binary, value);
    const size_t imageOffset = binary.size();
    binary.insert(binary.end(), ktx.begin(), ktx.end());
    const size_t normalImageOffset = binary.size();
    binary.insert(binary.end(), ktx.begin(), ktx.end());

    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << R"("extensionsUsed":["KHR_texture_basisu","KHR_texture_transform"],)"
        << R"("extensionsRequired":["KHR_texture_basisu","KHR_texture_transform"],)"
        << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
        << R"("bufferViews":[)"
        << R"({"buffer":0,"byteOffset":0,"byteLength":36},)"
        << R"({"buffer":0,"byteOffset":36,"byteLength":6},)"
        << R"({"buffer":0,"byteOffset":44,"byteLength":24},)"
        << R"({"buffer":0,"byteOffset":68,"byteLength":24},)"
        << "{\"buffer\":0,\"byteOffset\":"
        << (overflowImageOffset ? "18446744073709551600" : std::to_string(imageOffset))
        << ",\"byteLength\":" << ktx.size() << "},"
        << "{\"buffer\":0,\"byteOffset\":" << normalImageOffset << ",\"byteLength\":" << ktx.size() << "}],"
        << R"("accessors":[)"
        << R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
        << R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},)"
        << R"({"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},)"
        << R"({"bufferView":3,"componentType":5126,"count":3,"type":"VEC2"}],)"
        << R"("images":[{"bufferView":4,"mimeType":"image/ktx2"},{"bufferView":5,"mimeType":"image/ktx2"},{"uri":"unused_fallback.png"}],)"
        << R"("samplers":[{"wrapS":33071,"wrapT":33648}],)"
        << R"("textures":[{"source":2,"sampler":0,"extensions":{"KHR_texture_basisu":{"source":0}}},{"source":2,"extensions":{"KHR_texture_basisu":{"source":1}}}],)"
        << R"("materials":[)"
        << R"({"alphaMode":"MASK","alphaCutoff":0.42,"doubleSided":true,"normalTexture":{"index":1},"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,0.8],"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0.125,0.25],"rotation":0.5,"scale":[2,3],"texCoord":1}}}}},)"
        << R"({"alphaMode":"BLEND","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,0.25]}},)"
        << R"({"pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1]}}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2,"TEXCOORD_1":3},"indices":1,"material":0}]}],)"
        << R"("nodes":[{"name":"EmbeddedKtxTriangle","mesh":0}],)"
        << R"("scenes":[{"nodes":[0]}],"scene":0})";

    return writeGlb(overflowImageOffset ? "axmol_gltf_ktx_overflow.glb" : "axmol_gltf_ktx_transform.glb", json.str(),
                    binary);
}

std::filesystem::path makeSharedImageSamplerFixture()
{
    const auto ktxPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "platform" / "fixtures" /
                         "stylized_albedo_base.ktx2";
    const std::vector<uint8_t> ktx = readBinary(ktxPath);

    std::vector<uint8_t> binary;
    for (float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F})
        appendFloat(binary, value);
    appendUint16(binary, 0);
    appendUint16(binary, 1);
    appendUint16(binary, 2);
    while (binary.size() % 4 != 0)
        binary.push_back(0);
    for (float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F})
        appendFloat(binary, value);
    const size_t imageOffset = binary.size();
    binary.insert(binary.end(), ktx.begin(), ktx.end());

    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << R"("extensionsUsed":["KHR_texture_basisu"],"extensionsRequired":["KHR_texture_basisu"],)"
        << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":24},)"
        << "{\"buffer\":0,\"byteOffset\":" << imageOffset << ",\"byteLength\":" << ktx.size() << "}],"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],)"
        << R"("images":[{"uri":"unused_fallback.png"},{"bufferView":3,"mimeType":"image/ktx2"}],)"
        << R"("samplers":[{"magFilter":9728,"minFilter":9984,"wrapS":33071,"wrapT":10497},{"magFilter":9729,"minFilter":9987,"wrapS":33648,"wrapT":33071}],)"
        << R"("textures":[{"source":0,"sampler":0,"extensions":{"KHR_texture_basisu":{"source":1}}},{"source":0,"sampler":1,"extensions":{"KHR_texture_basisu":{"source":1}}}],)"
        << R"("materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},{"pbrMetallicRoughness":{"baseColorTexture":{"index":1}}}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2},"indices":1,"material":0}]},{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2},"indices":1,"material":1}]}],)"
        << R"("nodes":[{"mesh":0},{"mesh":1}],"scenes":[{"nodes":[0,1]}],"scene":0})";

    return writeGlb("axmol_gltf_shared_image_samplers.glb", json.str(), binary);
}

std::filesystem::path makeOversizedAccessorFixture()
{
    const auto path = std::filesystem::temp_directory_path() / "axmol_gltf_oversized_accessor.gltf";
    std::ofstream stream(path, std::ios::trunc);
    stream
        << R"({"asset":{"version":"2.0"},"accessors":[{"componentType":5126,"count":300000000,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    return path;
}

std::filesystem::path makeSkinFixture(size_t jointCount)
{
    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << R"("buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],)"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
        << R"("nodes":[)";
    for (size_t joint = 0; joint < jointCount; ++joint)
    {
        if (joint != 0)
            json << ',';
        json << "{\"name\":\"Joint" << joint << "\"}";
    }
    if (jointCount != 0)
        json << ',';
    json << R"({"name":"TriangleNode","mesh":0}],"skins":[{"joints":[)";
    for (size_t joint = 0; joint < jointCount; ++joint)
    {
        if (joint != 0)
            json << ',';
        json << joint;
    }
    json << "]}],\"scenes\":[{\"nodes\":[" << jointCount << "]}],\"scene\":0}";

    const auto path =
        std::filesystem::temp_directory_path() / ("axmol_gltf_skin_" + std::to_string(jointCount) + ".gltf");
    std::ofstream stream(path, std::ios::trunc);
    stream << json.str();
    return path;
}

std::filesystem::path makeSkinAttributeFixture(bool secondarySet, uint8_t firstJoint)
{
    std::vector<uint8_t> binary;
    for (float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F})
        appendFloat(binary, value);
    appendUint16(binary, 0);
    appendUint16(binary, 1);
    appendUint16(binary, 2);
    while (binary.size() % 4 != 0)
        binary.push_back(0);

    binary.insert(binary.end(), {firstJoint, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
        appendFloat(binary, 1.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
    }

    const int attributeSet = secondarySet ? 1 : 0;
    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":12},{"buffer":0,"byteOffset":56,"byteLength":48}],)"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"}],)"
        << "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"JOINTS_" << attributeSet << "\":2,\"WEIGHTS_"
        << attributeSet << "\":3},\"indices\":1}]}],"
        << R"("nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";

    return writeGlb(secondarySet ? "axmol_gltf_secondary_skin_set.glb" : "axmol_gltf_invalid_joint.glb", json.str(),
                    binary);
}

std::filesystem::path makeSkinPaletteFixture(bool reuseMeshWithSmallerSkin)
{
    std::vector<uint8_t> binary;
    for (float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F})
        appendFloat(binary, value);
    appendUint16(binary, 0);
    appendUint16(binary, 1);
    appendUint16(binary, 2);
    while (binary.size() % 4 != 0)
        binary.push_back(0);
    binary.insert(binary.end(), {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
        appendFloat(binary, 1.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
    }

    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":12},{"buffer":0,"byteOffset":56,"byteLength":48}],)"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":2,"WEIGHTS_0":3},"indices":1}]}],)"
        << R"("nodes":[{"name":"Joint0"},{"name":"Joint1"},{"name":"BoundMeshNode","mesh":0,"skin":0})";
    if (reuseMeshWithSmallerSkin)
        json << R"(,{"name":"SmallPaletteNode","mesh":0,"skin":1})";
    json << R"(],"skins":[{"joints":[0)";
    if (reuseMeshWithSmallerSkin)
        json << ",1";
    json << R"(]})";
    if (reuseMeshWithSmallerSkin)
        json << R"(,{"joints":[0]})";
    json << R"(],"scenes":[{"nodes":[0,1,2)";
    if (reuseMeshWithSmallerSkin)
        json << ",3";
    json << R"(]}],"scene":0})";

    return writeGlb(
        reuseMeshWithSmallerSkin ? "axmol_gltf_shared_mesh_skin_palette.glb" : "axmol_gltf_skin_palette.glb",
        json.str(), binary);
}

std::filesystem::path makeSkinnedAnimationFixture()
{
    std::vector<uint8_t> binary;
    for (float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F})
        appendFloat(binary, value);
    appendUint16(binary, 0);
    appendUint16(binary, 1);
    appendUint16(binary, 2);
    while (binary.size() % 4 != 0)
        binary.push_back(0);
    binary.insert(binary.end(), 12, 0);
    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
        appendFloat(binary, 1.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
        appendFloat(binary, 0.0F);
    }
    for (size_t index = 0; index < 16; ++index)
        appendFloat(binary, Mat4::identity.m[index]);
    appendFloat(binary, 0.0F);
    appendFloat(binary, 2.0F);
    for (float value : {5.0F, 6.0F, 7.0F, 9.0F, 10.0F, 11.0F})
        appendFloat(binary, value);

    std::ostringstream json;
    json
        << R"({"asset":{"version":"2.0"},)"
        << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":12},{"buffer":0,"byteOffset":56,"byteLength":48},{"buffer":0,"byteOffset":104,"byteLength":64},{"buffer":0,"byteOffset":168,"byteLength":8},{"buffer":0,"byteOffset":176,"byteLength":24}],)"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":4,"componentType":5126,"count":1,"type":"MAT4"},{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[2]},{"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":2,"WEIGHTS_0":3},"indices":1}]}],)"
        << R"("nodes":[{"name":"Joint","translation":[5,6,7],"rotation":[0,0,0.70710678,0.70710678],"scale":[2,3,4]},{"name":"SkinnedTriangle","mesh":0,"skin":0,"translation":[20,30,40]}],)"
        << R"("skins":[{"inverseBindMatrices":4,"skeleton":0,"joints":[0]}],)"
        << R"("animations":[{"name":"MoveJoint","samplers":[{"input":5,"output":6,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],)"
        << R"("scenes":[{"nodes":[0,1]}],"scene":0})";
    return writeGlb("axmol_gltf_skinned_linear_animation.glb", json.str(), binary);
}

std::array<std::filesystem::path, 2> makeExternalKtxFixture()
{
    const auto sourcePath =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "platform" / "fixtures" / "stylized_albedo.ktx2";
    const std::vector<uint8_t> ktx = readBinary(sourcePath);
    const auto directory           = std::filesystem::temp_directory_path();
    const auto imagePath           = directory / "axmol_gltf_external_albedo.ktx2";
    const auto gltfPath            = directory / "axmol_gltf_external_basis.gltf";

    {
        std::ofstream image(imagePath, std::ios::binary | std::ios::trunc);
        image.write(reinterpret_cast<const char*>(ktx.data()), static_cast<std::streamsize>(ktx.size()));
    }
    {
        std::ofstream gltf(gltfPath, std::ios::trunc);
        gltf
            << R"({"asset":{"version":"2.0"},)"
            << R"("extensionsUsed":["KHR_texture_basisu"],"extensionsRequired":["KHR_texture_basisu"],)"
            << R"("buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],)"
            << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            << R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            << R"("images":[{"uri":"unused_fallback.png"},{"uri":"axmol_gltf_external_albedo.ktx2"}],)"
            << R"("textures":[{"source":0,"extensions":{"KHR_texture_basisu":{"source":1}}}],)"
            << R"("materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)"
            << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            << R"("nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    }
    return {gltfPath, imagePath};
}

void checkTriangle(const MeshData& mesh)
{
    REQUIRE(mesh.attribCount == 3);
    CHECK(mesh.attribs[0].vertexAttrib == shaderinfos::VertexKey::VERTEX_ATTRIB_POSITION);
    CHECK(mesh.attribs[1].vertexAttrib == shaderinfos::VertexKey::VERTEX_ATTRIB_TEX_COORD);
    CHECK(mesh.attribs[2].vertexAttrib == shaderinfos::VertexKey::VERTEX_ATTRIB_NORMAL);

    const std::array<float, 24> expectedVertices{
        0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
    };
    REQUIRE(mesh.vertex.size() == expectedVertices.size());
    for (size_t i = 0; i < expectedVertices.size(); ++i)
        CHECK(mesh.vertex[i] == doctest::Approx(expectedVertices[i]));

    REQUIRE(mesh.subMeshIndices.size() == 1);
    const IndexArray& indices = mesh.subMeshIndices.front();
    REQUIRE(indices.size() == 3);
    REQUIRE(indices.format() == rhi::IndexFormat::U_SHORT);
    CHECK(indices.at<uint16_t>(0) == 0);
    CHECK(indices.at<uint16_t>(1) == 1);
    CHECK(indices.at<uint16_t>(2) == 2);
}

bool hasSkinnedModel(const NodeData& node)
{
    if (std::any_of(node.modelNodeDatas.begin(), node.modelNodeDatas.end(), [](const ModelData* model) {
        return model && !model->bones.empty() && model->bones.size() == model->invBindPose.size();
    }))
        return true;
    return std::any_of(node.children.begin(), node.children.end(),
                       [](const NodeData* child) { return child && hasSkinnedModel(*child); });
}
}  // namespace

TEST_CASE("glTF loader builds static triangle and generated normals")
{
    const std::filesystem::path fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "minimal.gltf";

    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(fixture.string(), nodes, meshes, materials));
    REQUIRE(meshes.meshDatas.size() == 1);
    REQUIRE(nodes.nodes.size() == 1);

    const MeshData& mesh = *meshes.meshDatas.front();
    checkTriangle(mesh);
    CHECK(nodes.nodes.front()->id == "TriangleNode");
    REQUIRE(nodes.nodes.front()->modelNodeDatas.size() == 1);
    CHECK(nodes.nodes.front()->modelNodeDatas.front()->subMeshId == "gltf_mesh_0_primitive_0");
}

TEST_CASE("glTF loader decodes EXT_meshopt vertex and triangle streams")
{
    const std::filesystem::path fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "meshopt.gltf";

    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(fixture.string(), nodes, meshes, materials));
    REQUIRE(meshes.meshDatas.size() == 1);
    REQUIRE(nodes.nodes.size() == 1);

    checkTriangle(*meshes.meshDatas.front());
    CHECK(nodes.nodes.front()->id == "MeshoptTriangle");
}

TEST_CASE("glTF loader preserves embedded KHR_texture_basisu material and texture transform")
{
    ScopedFixture fixture(makeEmbeddedKtxFixture(false));

    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
    REQUIRE(meshes.meshDatas.size() == 1);
    REQUIRE(materials.materials.size() == 3);

    const NMaterialData& mask = materials.materials[0];
    CHECK(mask.alphaMode == NMaterialData::AlphaMode::Mask);
    CHECK(mask.alphaCutoff == doctest::Approx(0.42F));
    CHECK(mask.doubleSided);
    CHECK(mask.baseColor.r == doctest::Approx(0.5F));
    CHECK(mask.baseColor.g == doctest::Approx(0.6F));
    CHECK(mask.baseColor.b == doctest::Approx(0.7F));
    CHECK(mask.baseColor.a == doctest::Approx(0.8F));

    const NTextureData* texture = mask.getTextureData(NTextureData::Usage::Diffuse);
    REQUIRE(texture != nullptr);
    CHECK(texture->filename.empty());
    CHECK(texture->cacheKey.find("#gltf-image-0") != std::string::npos);
    CHECK(texture->cacheKey.find("-cs1") != std::string::npos);
    CHECK(texture->colorSpace == rhi::ColorSpace::Srgb);
    REQUIRE(texture->embeddedData != nullptr);
    REQUIRE(texture->embeddedData->size() >= 12);
    const std::array<uint8_t, 12> ktx2Signature = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A,
    };
    CHECK(std::memcmp(texture->embeddedData->data(), ktx2Signature.data(), ktx2Signature.size()) == 0);
    CHECK(texture->wrapS == rhi::SamplerAddressMode::CLAMP_TO_EDGE);
    CHECK(texture->wrapT == rhi::SamplerAddressMode::MIRROR);
    CHECK(texture->hasTransform);
    CHECK(texture->texCoord == 1);
    CHECK(texture->offset.x == doctest::Approx(0.125F));
    CHECK(texture->offset.y == doctest::Approx(0.25F));
    CHECK(texture->scale.x == doctest::Approx(2.0F));
    CHECK(texture->scale.y == doctest::Approx(3.0F));
    CHECK(texture->rotation == doctest::Approx(0.5F));
    CHECK(mask.textures.size() == 1);
    CHECK(mask.getTextureData(NTextureData::Usage::Normal) == nullptr);

    const MeshData& mesh = *meshes.meshDatas.front();
    REQUIRE(mesh.vertex.size() == 24);
    CHECK(mesh.vertex[3] == doctest::Approx(0.25F));
    CHECK(mesh.vertex[4] == doctest::Approx(0.75F));
    CHECK(mesh.vertex[11] == doctest::Approx(0.5F));
    CHECK(mesh.vertex[12] == doctest::Approx(0.5F));
    CHECK(mesh.vertex[19] == doctest::Approx(0.75F));
    CHECK(mesh.vertex[20] == doctest::Approx(0.25F));

    CHECK(materials.materials[1].alphaMode == NMaterialData::AlphaMode::Blend);
    CHECK_FALSE(materials.materials[1].doubleSided);
    CHECK(materials.materials[1].baseColor.a == doctest::Approx(0.25F));
    CHECK(materials.materials[2].alphaMode == NMaterialData::AlphaMode::Opaque);
}

TEST_CASE("glTF loader prefers external KHR_texture_basisu source over fallback")
{
    const auto paths = makeExternalKtxFixture();
    ScopedFixture gltfFixture(paths[0]);
    ScopedFixture imageFixture(paths[1]);

    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(gltfFixture.path().string(), nodes, meshes, materials));
    REQUIRE(materials.materials.size() == 1);
    const NTextureData* texture = materials.materials.front().getTextureData(NTextureData::Usage::Diffuse);
    REQUIRE(texture != nullptr);
    CHECK(texture->filename == imageFixture.path().string());
    CHECK(texture->cacheKey.starts_with(texture->filename + "#sampler-"));
    CHECK(texture->embeddedData == nullptr);
}

TEST_CASE("glTF loader separates shared images by sampler state and preserves min mag mip filters")
{
    ScopedFixture fixture(makeSharedImageSamplerFixture());
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
    REQUIRE(materials.materials.size() == 2);

    const NTextureData* nearest = materials.materials[0].getTextureData(NTextureData::Usage::Diffuse);
    const NTextureData* linear  = materials.materials[1].getTextureData(NTextureData::Usage::Diffuse);
    REQUIRE(nearest != nullptr);
    REQUIRE(linear != nullptr);
    CHECK(nearest->embeddedData == linear->embeddedData);
    CHECK(nearest->cacheKey != linear->cacheKey);
    CHECK(nearest->minFilter == rhi::SamplerFilter::MIN_NEAREST);
    CHECK(nearest->magFilter == rhi::SamplerFilter::MAG_NEAREST);
    CHECK(nearest->mipFilter == rhi::SamplerFilter::MIP_NEAREST);
    CHECK(nearest->wrapS == rhi::SamplerAddressMode::CLAMP_TO_EDGE);
    CHECK(nearest->wrapT == rhi::SamplerAddressMode::REPEAT);
    CHECK(linear->minFilter == rhi::SamplerFilter::MIN_LINEAR);
    CHECK(linear->magFilter == rhi::SamplerFilter::MAG_LINEAR);
    CHECK(linear->mipFilter == rhi::SamplerFilter::MIP_LINEAR);
    CHECK(linear->wrapS == rhi::SamplerAddressMode::MIRROR);
    CHECK(linear->wrapT == rhi::SamplerAddressMode::CLAMP_TO_EDGE);

    if (axdrv)
    {
        auto* meshCache             = MeshDataCache::getInstance();
        auto* textureCache          = Director::getInstance()->getTextureCache();
        const std::string modelPath = fixture.path().string();
        meshCache->removeMeshRenderData(modelPath);
        textureCache->removeTextureForKey(nearest->cacheKey);
        textureCache->removeTextureForKey(linear->cacheKey);

        auto* renderer = new MeshRenderer();
        REQUIRE(renderer->initWithFile(modelPath));
        Texture2D* nearestTexture = textureCache->getTextureForKey(nearest->cacheKey);
        Texture2D* linearTexture  = textureCache->getTextureForKey(linear->cacheKey);
        REQUIRE(nearestTexture != nullptr);
        REQUIRE(linearTexture != nullptr);
        CHECK(nearestTexture != linearTexture);
        CHECK_FALSE(nearestTexture->hasMipmaps());
        CHECK_FALSE(linearTexture->hasMipmaps());

        Texture2D::TexParams nearestParams{};
        nearestParams.minFilter     = nearest->minFilter;
        nearestParams.magFilter     = nearest->magFilter;
        nearestParams.mipFilter     = nearest->mipFilter;
        nearestParams.sAddressMode  = nearest->wrapS;
        nearestParams.tAddressMode  = nearest->wrapT;
        const auto effectiveNearest = Texture2D::resolveTexParameters(nearestParams, nearestTexture->hasMipmaps());
        CHECK(effectiveNearest.minFilter == nearest->minFilter);
        CHECK(effectiveNearest.magFilter == nearest->magFilter);
        CHECK(effectiveNearest.mipFilter == rhi::SamplerFilter::MIP_DEFAULT);
        CHECK(effectiveNearest.sAddressMode == nearest->wrapS);
        CHECK(effectiveNearest.tAddressMode == nearest->wrapT);
        renderer->release();

        textureCache->removeTextureForKey(nearest->cacheKey);
        textureCache->removeTextureForKey(linear->cacheKey);
        meshCache->removeMeshRenderData(modelPath);
    }
}

TEST_CASE("glTF loader rejects accessor storage beyond portable MeshData and RHI limits")
{
    ScopedFixture fixture(makeOversizedAccessorFixture());
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader rejects overflowing embedded image buffer view")
{
    ScopedFixture fixture(makeEmbeddedKtxFixture(true));
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader enforces stylized skin joint limit")
{
    {
        ScopedFixture fixture(makeSkinFixture(60));
        NodeDatas nodes;
        MeshDatas meshes;
        MaterialDatas materials;
        CHECK(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
    }
    {
        ScopedFixture fixture(makeSkinFixture(61));
        NodeDatas nodes;
        MeshDatas meshes;
        MaterialDatas materials;
        CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
    }
}

TEST_CASE("glTF loader rejects secondary skin influence sets")
{
    ScopedFixture fixture(makeSkinAttributeFixture(true, 0));
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader rejects joint index outside shader palette")
{
    ScopedFixture fixture(makeSkinAttributeFixture(false, 60));
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader rejects joint index outside the bound skin palette")
{
    ScopedFixture fixture(makeSkinPaletteFixture(false));
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader validates a shared mesh against every bound skin palette")
{
    ScopedFixture fixture(makeSkinPaletteFixture(true));
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    CHECK_FALSE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
}

TEST_CASE("glTF loader builds a four-weight skinned mesh and LINEAR animation")
{
    ScopedFixture fixture(makeSkinnedAnimationFixture());
    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(fixture.path().string(), nodes, meshes, materials));
    REQUIRE(meshes.meshDatas.size() == 1);
    const MeshData& mesh = *meshes.meshDatas.front();
    REQUIRE(mesh.attribs.size() == 5);
    CHECK(mesh.attribs[3].vertexAttrib == shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_WEIGHT);
    CHECK(mesh.attribs[4].vertexAttrib == shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_INDEX);

    REQUIRE(nodes.skeleton.size() == 1);
    CHECK(nodes.skeleton.front()->id == "Joint");
    REQUIRE(nodes.nodes.size() == 2);
    const NodeData* skinnedNode = nodes.nodes[1];
    Vec3 skinnedScale;
    Quat skinnedRotation;
    Vec3 skinnedTranslation;
    skinnedNode->transform.decompose(&skinnedScale, &skinnedRotation, &skinnedTranslation);
    CHECK(skinnedTranslation.x == doctest::Approx(20.0F));
    CHECK(skinnedTranslation.y == doctest::Approx(30.0F));
    CHECK(skinnedTranslation.z == doctest::Approx(40.0F));
    REQUIRE(skinnedNode->modelNodeDatas.size() == 1);
    const ModelData* model = skinnedNode->modelNodeDatas.front();
    CHECK(model->skinningInSkeletonSpace);
    REQUIRE(model->bones.size() == 1);
    CHECK(model->bones.front() == "Joint");
    REQUIRE(model->invBindPose.size() == 1);
    for (size_t index = 0; index < 16; ++index)
        CHECK(model->invBindPose.front().m[index] == doctest::Approx(Mat4::identity.m[index]));

    if (axdrv)
    {
        const std::string modelPath = fixture.path().string();
        auto* meshCache             = MeshDataCache::getInstance();
        meshCache->removeMeshRenderData(modelPath);
        auto* renderer = new MeshRenderer();
        REQUIRE(renderer->initWithFile(modelPath));
        CHECK(renderer->getMeshCount() == 1);
        REQUIRE(renderer->getMesh() != nullptr);
        REQUIRE(renderer->getMesh()->getSkin() != nullptr);
        auto* replacementMaterial = StylizedMaterial::create();
        REQUIRE(replacementMaterial != nullptr);
        CHECK_FALSE(replacementMaterial->isSkinned());
        renderer->setMaterial(replacementMaterial);
        auto* appliedMaterial = dynamic_cast<StylizedMaterial*>(renderer->getMaterial());
        REQUIRE(appliedMaterial != nullptr);
        CHECK(appliedMaterial->isSkinned());
        CHECK(renderer->getPosition3D().isZero());
        auto* importedNode = renderer->getChildByName("SkinnedTriangle");
        REQUIRE(importedNode != nullptr);
        CHECK(dynamic_cast<MeshRenderer*>(importedNode) == nullptr);
        CHECK(importedNode->getPosition3D().x == doctest::Approx(20.0F));
        CHECK(importedNode->getPosition3D().y == doctest::Approx(30.0F));
        CHECK(importedNode->getPosition3D().z == doctest::Approx(40.0F));
        renderer->release();
        meshCache->removeMeshRenderData(modelPath);
    }

    Animation3DData animation;
    REQUIRE(GltfLoader::loadAnimation(fixture.path().string(), "MoveJoint", animation));
    CHECK(animation._usesAbsoluteLocalTransforms);
    CHECK(animation._totalTime == doctest::Approx(2.0F));
    const auto keys = animation._translationKeys.find("Joint");
    REQUIRE(keys != animation._translationKeys.end());
    REQUIRE(keys->second.size() == 2);
    CHECK(keys->second[0]._time == doctest::Approx(0.0F));
    CHECK(keys->second[1]._time == doctest::Approx(1.0F));
    CHECK(keys->second[1]._key.x == doctest::Approx(9.0F));
    CHECK(keys->second[1]._key.y == doctest::Approx(10.0F));
    CHECK(keys->second[1]._key.z == doctest::Approx(11.0F));

    const auto rotations = animation._rotationKeys.find("Joint");
    REQUIRE(rotations != animation._rotationKeys.end());
    REQUIRE(rotations->second.size() == 1);
    CHECK(rotations->second.front()._key.z == doctest::Approx(0.70710678F));
    CHECK(rotations->second.front()._key.w == doctest::Approx(0.70710678F));

    const auto scales = animation._scaleKeys.find("Joint");
    REQUIRE(scales != animation._scaleKeys.end());
    REQUIRE(scales->second.size() == 1);
    CHECK(scales->second.front()._key.x == doctest::Approx(2.0F));
    CHECK(scales->second.front()._key.y == doctest::Approx(3.0F));
    CHECK(scales->second.front()._key.z == doctest::Approx(4.0F));

    auto* root = new Node();
    REQUIRE(root->init());
    auto* joint = new Node();
    REQUIRE(joint->init());
    joint->setName("Joint");
    joint->setNodeToParentTransform(nodes.skeleton.front()->transform);
    root->addChild(joint);
    joint->release();

    auto* compiledAnimation = new Animation3D();
    REQUIRE(compiledAnimation->init(animation));
    auto* action = new Animate3D();
    REQUIRE(action->init(compiledAnimation));
    action->setQuality(Animate3DQuality::QUALITY_HIGH);
    action->startWithTarget(root);
    action->update(0.5F);

    Vec3 evaluatedScale;
    Quat evaluatedRotation;
    Vec3 evaluatedTranslation;
    joint->getNodeToParentTransform().decompose(&evaluatedScale, &evaluatedRotation, &evaluatedTranslation);
    CHECK(evaluatedTranslation.x == doctest::Approx(7.0F));
    CHECK(evaluatedTranslation.y == doctest::Approx(8.0F));
    CHECK(evaluatedTranslation.z == doctest::Approx(9.0F));
    CHECK(evaluatedScale.x == doctest::Approx(2.0F));
    CHECK(evaluatedScale.y == doctest::Approx(3.0F));
    CHECK(evaluatedScale.z == doctest::Approx(4.0F));
    CHECK(std::abs(evaluatedRotation.z * 0.70710678F + evaluatedRotation.w * 0.70710678F) == doctest::Approx(1.0F));

    action->update(1.0F);
    joint->getNodeToParentTransform().decompose(&evaluatedScale, &evaluatedRotation, &evaluatedTranslation);
    CHECK(evaluatedTranslation.x == doctest::Approx(9.0F));
    CHECK(evaluatedTranslation.y == doctest::Approx(10.0F));
    CHECK(evaluatedTranslation.z == doctest::Approx(11.0F));

    action->stop();
    action->release();
    compiledAnimation->release();
    root->release();
}

TEST_CASE("glTF animation keeps imported local transforms separate from renderer placement")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    const std::string modelPath =
        (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "minimal.gltf").string();
    auto* meshCache = MeshDataCache::getInstance();
    meshCache->removeMeshRenderData(modelPath);

    auto* renderer = new MeshRenderer();
    REQUIRE(renderer->initWithFile(modelPath));
    renderer->setPosition3D(Vec3{100.0F, 200.0F, 300.0F});
    auto* importedNode = renderer->getChildByName("TriangleNode");
    REQUIRE(importedNode != nullptr);
    auto* importedRenderer = dynamic_cast<MeshRenderer*>(importedNode);
    REQUIRE(importedRenderer != nullptr);
    REQUIRE(renderer->getMeshCount() == 1);
    REQUIRE(renderer->getMeshes().size() == 1);
    CHECK(renderer->getMesh() == importedRenderer->getMesh());
    CHECK(renderer->getMeshByIndex(0) == importedRenderer->getMeshByIndex(0));
    CHECK(renderer->getMaterial(0) == importedRenderer->getMaterial(0));

    const AABB initialAssetBounds = renderer->getAABB();
    const AABB initialChildBounds = importedRenderer->getAABB();
    CHECK(initialAssetBounds._min.x == doctest::Approx(initialChildBounds._min.x));
    CHECK(initialAssetBounds._max.x == doctest::Approx(initialChildBounds._max.x));

    constexpr unsigned short testCameraMask = 0x0002;
    renderer->setCameraMask(testCameraMask);
    renderer->setLightMask(0x1234U);
    renderer->setForceDepthWrite(true);
    renderer->setWireframe(true);
    renderer->setCastShadow(false);
    renderer->setReceiveShadow(false);
    CHECK(importedRenderer->getCameraMask() == testCameraMask);
    CHECK(importedRenderer->getLightMask() == 0x1234U);
    CHECK(importedRenderer->isForceDepthWrite());
    CHECK(importedRenderer->isWireframe());
    CHECK_FALSE(importedRenderer->isCastingShadow());
    CHECK_FALSE(importedRenderer->isReceivingShadow());
    renderer->setCullFaceEnabled(false);
    CHECK_FALSE(importedRenderer->getMaterial()->getStateBlock().isCullFaceEnabled());

    Animation3DData data;
    data._totalTime                       = 2.0F;
    data._usesAbsoluteLocalTransforms     = true;
    data._translationKeys["TriangleNode"] = {
        {0.0F, Vec3{1.0F, 2.0F, 3.0F}},
        {1.0F, Vec3{5.0F, 6.0F, 7.0F}},
    };
    data._rotationKeys["TriangleNode"] = {{0.0F, Quat::identity}};
    data._scaleKeys["TriangleNode"]    = {{0.0F, Vec3::one}};

    auto* compiledAnimation = new Animation3D();
    REQUIRE(compiledAnimation->init(data));
    auto* action = new Animate3D();
    REQUIRE(action->init(compiledAnimation));
    action->setQuality(Animate3DQuality::QUALITY_HIGH);
    action->startWithTarget(renderer);
    action->update(0.5F);

    CHECK(renderer->getPosition3D().x == doctest::Approx(100.0F));
    CHECK(renderer->getPosition3D().y == doctest::Approx(200.0F));
    CHECK(renderer->getPosition3D().z == doctest::Approx(300.0F));
    Vec3 importedScale;
    Quat importedRotation;
    Vec3 importedTranslation;
    importedNode->getNodeToParentTransform().decompose(&importedScale, &importedRotation, &importedTranslation);
    CHECK(importedTranslation.x == doctest::Approx(3.0F));
    CHECK(importedTranslation.y == doctest::Approx(4.0F));
    CHECK(importedTranslation.z == doctest::Approx(5.0F));
    Vec3 worldScale;
    Quat worldRotation;
    Vec3 worldTranslation;
    importedNode->getNodeToWorldTransform().decompose(&worldScale, &worldRotation, &worldTranslation);
    CHECK(worldTranslation.x == doctest::Approx(103.0F));
    CHECK(worldTranslation.y == doctest::Approx(204.0F));
    CHECK(worldTranslation.z == doctest::Approx(305.0F));
    const AABB animatedAssetBounds = renderer->getAABB();
    const AABB animatedChildBounds = importedRenderer->getAABB();
    CHECK(animatedAssetBounds._min.x == doctest::Approx(animatedChildBounds._min.x));
    CHECK(animatedAssetBounds._max.x == doctest::Approx(animatedChildBounds._max.x));
    CHECK(animatedAssetBounds._min.x == doctest::Approx(initialAssetBounds._min.x + 3.0F));

    action->stop();
    action->release();
    compiledAnimation->release();
    renderer->release();
    meshCache->removeMeshRenderData(modelPath);
}

TEST_CASE("glTF loader accepts an opt-in local axasset skinned fixture")
{
    const char* path = std::getenv("AX_GLTF_LOCAL_FIXTURE");
    if (!path || path[0] == '\0')
        return;

    NodeDatas nodes;
    MeshDatas meshes;
    MaterialDatas materials;
    REQUIRE(GltfLoader::load(path, nodes, meshes, materials));
    CHECK_FALSE(meshes.meshDatas.empty());
    CHECK(std::any_of(nodes.nodes.begin(), nodes.nodes.end(),
                      [](const NodeData* node) { return node && hasSkinnedModel(*node); }));

    if (axdrv)
    {
        const std::string modelPath(path);
        auto* meshCache = MeshDataCache::getInstance();
        meshCache->removeMeshRenderData(modelPath);
        auto* renderer = new MeshRenderer();
        REQUIRE(renderer->initWithFile(modelPath));
        renderer->release();
        meshCache->removeMeshRenderData(modelPath);
    }
}

TEST_CASE("MeshRenderer recreates cached embedded KTX2 without retaining unused normal data")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    ScopedFixture fixture(makeEmbeddedKtxFixture(false));
    const std::string modelPath        = fixture.path().string();
    auto* meshCache                    = MeshDataCache::getInstance();
    auto* textureCache                 = Director::getInstance()->getTextureCache();
    const std::string normalTextureKey = modelPath + "#gltf-image-1";
    meshCache->removeMeshRenderData(modelPath);
    textureCache->removeTextureForKey(normalTextureKey);

    auto* first = new MeshRenderer();
    REQUIRE(first->initWithFile(modelPath));
    auto* cachedModel = meshCache->getMeshRenderData(modelPath);
    REQUIRE(cachedModel != nullptr);
    REQUIRE(cachedModel->materialdatas != nullptr);
    const NTextureData* source =
        cachedModel->materialdatas->materials.front().getTextureData(NTextureData::Usage::Diffuse);
    REQUIRE(source != nullptr);
    CHECK(source->colorSpace == rhi::ColorSpace::Srgb);
    REQUIRE(source->embeddedData != nullptr);
    CHECK_FALSE(source->embeddedData->isNull());
    CHECK(cachedModel->materialdatas->materials.front().textures.size() == 1);
    CHECK(cachedModel->materialdatas->materials.front().getTextureData(NTextureData::Usage::Normal) == nullptr);
    CHECK(textureCache->getTextureForKey(normalTextureKey) == nullptr);
    const std::string textureKey = source->cacheKey;

    auto* firstNode = dynamic_cast<MeshRenderer*>(first->getChildByName("EmbeddedKtxTriangle"));
    REQUIRE(firstNode != nullptr);
    REQUIRE(firstNode->getMeshCount() == 1);
    auto* firstMesh     = firstNode->getMeshByIndex(0);
    auto* firstMaterial = dynamic_cast<StylizedMaterial*>(firstMesh->getMaterial());
    REQUIRE(firstMaterial != nullptr);
    CHECK(firstMaterial->getDescription().doubleSided);
    CHECK_FALSE(firstMaterial->getStateBlock().isCullFaceEnabled());
    CHECK(firstMesh->getTexture(NTextureData::Usage::Normal) == nullptr);
    auto* firstTexture = textureCache->getTextureForKey(textureKey);
    REQUIRE(firstTexture != nullptr);
    CHECK(firstTexture->getWidth() == 24);
    CHECK(firstTexture->getHeight() == 24);
    CHECK(firstTexture->getRHITexture()->getColorSpace() == rhi::ColorSpace::Srgb);

    textureCache->removeTextureForKey(textureKey);
    first->release();

    auto* second = new MeshRenderer();
    REQUIRE(second->initWithFile(modelPath));
    auto* secondNode = dynamic_cast<MeshRenderer*>(second->getChildByName("EmbeddedKtxTriangle"));
    REQUIRE(secondNode != nullptr);
    REQUIRE(secondNode->getMeshCount() == 1);
    auto* recreated = textureCache->getTextureForKey(textureKey);
    REQUIRE(recreated != nullptr);
    CHECK(recreated->getWidth() == 24);
    CHECK(recreated->getHeight() == 24);
    CHECK(recreated->getRHITexture()->getColorSpace() == rhi::ColorSpace::Srgb);
    CHECK(secondNode->getMeshByIndex(0)->getTexture(NTextureData::Usage::Normal) == nullptr);
    second->release();

    textureCache->removeTextureForKey(textureKey);
    textureCache->removeTextureForKey(normalTextureKey);
    meshCache->removeMeshRenderData(modelPath);
}
