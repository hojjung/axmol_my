/****************************************************************************
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "axmol/3d/GltfLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <meshoptimizer.h>

#include "axmol/base/Data.h"
#include "axmol/base/Logging.h"
#include "axmol/base/Utils.h"
#include "axmol/platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ax
{
namespace
{

constexpr size_t MAX_RUNTIME_VERTEX_STRIDE = 256;
constexpr size_t MAX_STYLIZED_JOINT_COUNT  = 60;
constexpr size_t MAX_RUNTIME_MESH_COUNT    = static_cast<size_t>(std::numeric_limits<int>::max());
constexpr size_t MAX_RHI_BUFFER_BYTES      = static_cast<size_t>(std::numeric_limits<uint32_t>::max());

const char* resultName(cgltf_result result)
{
    switch (result)
    {
    case cgltf_result_success:
        return "success";
    case cgltf_result_data_too_short:
        return "data_too_short";
    case cgltf_result_unknown_format:
        return "unknown_format";
    case cgltf_result_invalid_json:
        return "invalid_json";
    case cgltf_result_invalid_gltf:
        return "invalid_gltf";
    case cgltf_result_invalid_options:
        return "invalid_options";
    case cgltf_result_file_not_found:
        return "file_not_found";
    case cgltf_result_io_error:
        return "io_error";
    case cgltf_result_out_of_memory:
        return "out_of_memory";
    case cgltf_result_legacy_gltf:
        return "legacy_gltf";
    default:
        return "unknown_error";
    }
}

void* allocate(const cgltf_memory_options* memory, size_t size)
{
    if (memory->alloc_func)
        return memory->alloc_func(memory->user_data, size);
    return std::malloc(size);
}

void deallocate(const cgltf_memory_options* memory, void* data)
{
    if (memory->free_func)
        memory->free_func(memory->user_data, data);
    else
        std::free(data);
}

cgltf_result readFile(const cgltf_memory_options* memory,
                      const cgltf_file_options*,
                      const char* path,
                      cgltf_size* size,
                      void** data)
{
    Data fileData = FileUtils::getInstance()->getDataFromFile(path);
    if (fileData.isNull())
        return cgltf_result_file_not_found;

    const size_t actualSize = fileData.size();
    if (*size != 0 && actualSize < *size)
        return cgltf_result_data_too_short;

    void* copy = allocate(memory, actualSize);
    if (!copy)
        return cgltf_result_out_of_memory;

    std::memcpy(copy, fileData.data(), actualSize);
    *size = actualSize;
    *data = copy;
    return cgltf_result_success;
}

void releaseFile(const cgltf_memory_options* memory, const cgltf_file_options*, void* data)
{
    deallocate(memory, data);
}

bool checkedMultiply(size_t lhs, size_t rhs, size_t& result)
{
    if (lhs != 0 && rhs > std::numeric_limits<size_t>::max() / lhs)
        return false;
    result = lhs * rhs;
    return true;
}

bool checkedRuntimeVectorSize(size_t count, size_t elementsPerItem, size_t elementSize, size_t& elementCount)
{
    size_t byteCount = 0;
    return checkedMultiply(count, elementsPerItem, elementCount) && elementCount <= MAX_RUNTIME_MESH_COUNT &&
           checkedMultiply(elementCount, elementSize, byteCount) && byteCount <= MAX_RHI_BUFFER_BYTES;
}

bool decodeMeshopt(cgltf_data& data)
{
    for (cgltf_size index = 0; index < data.buffer_views_count; ++index)
    {
        cgltf_buffer_view& view = data.buffer_views[index];
        if (!view.has_meshopt_compression)
            continue;

        const cgltf_meshopt_compression& compression = view.meshopt_compression;
        if (!compression.buffer || !compression.buffer->data || compression.offset > compression.buffer->size ||
            compression.size > compression.buffer->size - compression.offset || compression.stride == 0 ||
            compression.stride > MAX_RUNTIME_VERTEX_STRIDE)
        {
            AXLOGE("glTF meshopt buffer view {} has invalid bounds", index);
            return false;
        }

        size_t decodedSize = 0;
        if (!checkedMultiply(compression.count, compression.stride, decodedSize) || decodedSize > MAX_RHI_BUFFER_BYTES)
        {
            AXLOGE("glTF meshopt buffer view {} decoded size overflow", index);
            return false;
        }

        void* decoded = allocate(&data.memory, decodedSize);
        if (!decoded)
        {
            AXLOGE("glTF meshopt buffer view {} allocation failed ({} bytes)", index, decodedSize);
            return false;
        }

        const auto* source = static_cast<const unsigned char*>(compression.buffer->data) + compression.offset;
        int decodeResult   = -1;
        switch (compression.mode)
        {
        case cgltf_meshopt_compression_mode_attributes:
            decodeResult =
                meshopt_decodeVertexBuffer(decoded, compression.count, compression.stride, source, compression.size);
            break;
        case cgltf_meshopt_compression_mode_triangles:
            decodeResult =
                meshopt_decodeIndexBuffer(decoded, compression.count, compression.stride, source, compression.size);
            break;
        case cgltf_meshopt_compression_mode_indices:
            decodeResult =
                meshopt_decodeIndexSequence(decoded, compression.count, compression.stride, source, compression.size);
            break;
        default:
            break;
        }

        if (decodeResult != 0)
        {
            deallocate(&data.memory, decoded);
            AXLOGE("glTF meshopt buffer view {} decode failed ({})", index, decodeResult);
            return false;
        }

        switch (compression.filter)
        {
        case cgltf_meshopt_compression_filter_none:
            break;
        case cgltf_meshopt_compression_filter_octahedral:
            meshopt_decodeFilterOct(decoded, compression.count, compression.stride);
            break;
        case cgltf_meshopt_compression_filter_quaternion:
            meshopt_decodeFilterQuat(decoded, compression.count, compression.stride);
            break;
        case cgltf_meshopt_compression_filter_exponential:
            meshopt_decodeFilterExp(decoded, compression.count, compression.stride);
            break;
        default:
            deallocate(&data.memory, decoded);
            AXLOGE("glTF meshopt buffer view {} uses an unsupported filter", index);
            return false;
        }

        // cgltf_free() releases buffer_view.data through data.memory.free_func.
        view.data = decoded;
    }
    return true;
}

bool validateRequiredExtensions(const cgltf_data& data)
{
    static constexpr std::array<std::string_view, 4> supported = {
        "EXT_meshopt_compression",
        "KHR_mesh_quantization",
        "KHR_texture_basisu",
        "KHR_texture_transform",
    };

    for (cgltf_size i = 0; i < data.extensions_required_count; ++i)
    {
        const std::string_view extension = data.extensions_required[i] ? data.extensions_required[i] : "";
        if (std::find(supported.begin(), supported.end(), extension) == supported.end())
        {
            AXLOGE("glTF requires unsupported extension '{}'", extension);
            return false;
        }
    }
    return true;
}

bool validateSkinJointCounts(const cgltf_data& data)
{
    for (cgltf_size skinIndex = 0; skinIndex < data.skins_count; ++skinIndex)
    {
        if (data.skins[skinIndex].joints_count > MAX_STYLIZED_JOINT_COUNT)
        {
            AXLOGE("glTF skin {} has {} joints; stylized shaders support at most {}", skinIndex,
                   data.skins[skinIndex].joints_count, MAX_STYLIZED_JOINT_COUNT);
            return false;
        }
    }
    return true;
}

bool validateSkinJointIndices(const cgltf_data& data)
{
    for (cgltf_size nodeIndex = 0; nodeIndex < data.nodes_count; ++nodeIndex)
    {
        const cgltf_node& node = data.nodes[nodeIndex];
        if (!node.mesh || !node.skin)
            continue;

        const auto skinIndex = static_cast<size_t>(node.skin - data.skins);
        const auto meshIndex = static_cast<size_t>(node.mesh - data.meshes);
        for (cgltf_size primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; ++primitiveIndex)
        {
            const cgltf_primitive& primitive    = node.mesh->primitives[primitiveIndex];
            const cgltf_accessor* jointAccessor = nullptr;
            for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
            {
                const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
                if (attribute.type == cgltf_attribute_type_joints && attribute.index == 0)
                {
                    jointAccessor = attribute.data;
                    break;
                }
            }
            if (!jointAccessor)
                continue;

            for (cgltf_size vertex = 0; vertex < jointAccessor->count; ++vertex)
            {
                cgltf_uint joints[4]{};
                if (!cgltf_accessor_read_uint(jointAccessor, vertex, joints, 4))
                {
                    AXLOGE("glTF node {} mesh {} primitive {} has an unreadable JOINTS_0 accessor", nodeIndex,
                           meshIndex, primitiveIndex);
                    return false;
                }
                for (cgltf_uint joint : joints)
                {
                    if (joint >= node.skin->joints_count)
                    {
                        AXLOGE(
                            "glTF node {} mesh {} primitive {} uses joint index {} outside skin {} palette "
                            "[0, {})",
                            nodeIndex, meshIndex, primitiveIndex, joint, skinIndex, node.skin->joints_count);
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

class ParsedAsset final
{
public:
    ~ParsedAsset()
    {
        if (_data)
            cgltf_free(_data);
    }

    bool open(std::string_view path)
    {
        _fullPath = FileUtils::getInstance()->fullPathForFilename(path);
        _source   = FileUtils::getInstance()->getDataFromFile(_fullPath);
        if (_source.isNull())
        {
            AXLOGE("glTF file not found: {}", path);
            return false;
        }

        _options.file.read    = readFile;
        _options.file.release = releaseFile;

        cgltf_result result = cgltf_parse(&_options, _source.data(), _source.size(), &_data);
        if (result != cgltf_result_success)
        {
            AXLOGE("glTF parse failed for '{}': {}", path, resultName(result));
            return false;
        }

        result = cgltf_load_buffers(&_options, _data, _fullPath.c_str());
        if (result != cgltf_result_success)
        {
            AXLOGE("glTF buffer load failed for '{}': {}", path, resultName(result));
            return false;
        }

        result = cgltf_validate(_data);
        if (result != cgltf_result_success)
        {
            AXLOGE("glTF validation failed for '{}': {}", path, resultName(result));
            return false;
        }

        if (!validateRequiredExtensions(*_data) || !validateSkinJointCounts(*_data) || !decodeMeshopt(*_data) ||
            !validateSkinJointIndices(*_data))
            return false;

        return true;
    }

    cgltf_data& data() const { return *_data; }
    const std::string& fullPath() const { return _fullPath; }

private:
    cgltf_options _options{};
    cgltf_data* _data = nullptr;
    Data _source;
    std::string _fullPath;
};

std::string nodeName(const cgltf_data& data, const cgltf_node* node)
{
    const size_t index = static_cast<size_t>(node - data.nodes);
    if (!node->name || node->name[0] == '\0')
        return "node_" + std::to_string(index);

    bool duplicate = false;
    for (cgltf_size i = 0; i < data.nodes_count; ++i)
    {
        if (&data.nodes[i] != node && data.nodes[i].name && std::strcmp(data.nodes[i].name, node->name) == 0)
        {
            duplicate = true;
            break;
        }
    }
    return duplicate ? std::string(node->name) + "_" + std::to_string(index) : std::string(node->name);
}

Mat4 localTransform(const cgltf_node& node)
{
    float values[16];
    cgltf_node_transform_local(&node, values);
    return Mat4(values);
}

const cgltf_accessor* findAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int index = 0)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
    {
        const cgltf_attribute& attribute = primitive.attributes[i];
        if (attribute.type == type && attribute.index == index)
            return attribute.data;
    }
    return nullptr;
}

bool unpackFloats(const cgltf_accessor* accessor, size_t components, std::vector<float>& output)
{
    if (!accessor || cgltf_num_components(accessor->type) != components)
        return false;

    size_t floatCount = 0;
    if (!checkedRuntimeVectorSize(accessor->count, components, sizeof(float), floatCount))
        return false;
    output.resize(floatCount);
    return cgltf_accessor_unpack_floats(accessor, output.data(), output.size()) == output.size();
}

bool unpackColor(const cgltf_accessor* accessor, size_t vertexCount, std::vector<float>& output)
{
    if (!accessor || accessor->count != vertexCount)
        return false;

    const size_t components = cgltf_num_components(accessor->type);
    if (components != 3 && components != 4)
        return false;

    std::vector<float> source;
    if (!unpackFloats(accessor, components, source))
        return false;

    size_t floatCount = 0;
    if (!checkedRuntimeVectorSize(vertexCount, 4, sizeof(float), floatCount))
        return false;
    output.resize(floatCount);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        output[i * 4 + 0] = source[i * components + 0];
        output[i * 4 + 1] = source[i * components + 1];
        output[i * 4 + 2] = source[i * components + 2];
        output[i * 4 + 3] = components == 4 ? source[i * components + 3] : 1.0F;
    }
    return true;
}

bool unpackJoints(const cgltf_accessor* accessor, size_t vertexCount, std::vector<float>& output)
{
    if (!accessor || accessor->count != vertexCount || cgltf_num_components(accessor->type) != 4)
        return false;

    size_t floatCount = 0;
    if (!checkedRuntimeVectorSize(vertexCount, 4, sizeof(float), floatCount))
        return false;
    output.resize(floatCount);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        cgltf_uint joints[4]{};
        if (!cgltf_accessor_read_uint(accessor, i, joints, 4))
            return false;
        for (size_t component = 0; component < 4; ++component)
            output[i * 4 + component] = static_cast<float>(joints[component]);
    }
    return true;
}

bool readIndices(const cgltf_primitive& primitive, size_t vertexCount, std::vector<uint32_t>& output)
{
    const size_t indexCount = primitive.indices ? primitive.indices->count : vertexCount;
    size_t checkedCount     = 0;
    if (!checkedRuntimeVectorSize(indexCount, 1, sizeof(uint32_t), checkedCount) ||
        vertexCount > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        return false;

    if (!primitive.indices)
    {
        output.resize(checkedCount);
        for (size_t i = 0; i < vertexCount; ++i)
            output[i] = static_cast<uint32_t>(i);
        return true;
    }

    output.resize(checkedCount);
    for (size_t i = 0; i < output.size(); ++i)
    {
        const cgltf_size value = cgltf_accessor_read_index(primitive.indices, i);
        if (value >= vertexCount || value > std::numeric_limits<uint32_t>::max())
            return false;
        output[i] = static_cast<uint32_t>(value);
    }
    return true;
}

std::vector<float> generateNormals(const std::vector<float>& positions, const std::vector<uint32_t>& indices)
{
    const size_t vertexCount = positions.size() / 3;
    std::vector<float> normals(vertexCount * 3, 0.0F);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const uint32_t i0 = indices[i + 0];
        const uint32_t i1 = indices[i + 1];
        const uint32_t i2 = indices[i + 2];
        const Vec3 p0(positions[i0 * 3 + 0], positions[i0 * 3 + 1], positions[i0 * 3 + 2]);
        const Vec3 p1(positions[i1 * 3 + 0], positions[i1 * 3 + 1], positions[i1 * 3 + 2]);
        const Vec3 p2(positions[i2 * 3 + 0], positions[i2 * 3 + 1], positions[i2 * 3 + 2]);
        Vec3 face;
        Vec3::cross(p1 - p0, p2 - p0, &face);
        for (uint32_t vertex : {i0, i1, i2})
        {
            normals[vertex * 3 + 0] += face.x;
            normals[vertex * 3 + 1] += face.y;
            normals[vertex * 3 + 2] += face.z;
        }
    }

    for (size_t i = 0; i < vertexCount; ++i)
    {
        Vec3 normal(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
        if (normal.lengthSquared() > 1.0e-12F)
            normal.normalize();
        else
            normal.set(0.0F, 1.0F, 0.0F);
        normals[i * 3 + 0] = normal.x;
        normals[i * 3 + 1] = normal.y;
        normals[i * 3 + 2] = normal.z;
    }
    return normals;
}

struct VertexStream
{
    shaderinfos::VertexKey key;
    rhi::VertexElementType type;
    size_t components;
    std::vector<float> values;
};

int baseColorTexCoord(const cgltf_primitive& primitive)
{
    if (!primitive.material || !primitive.material->has_pbr_metallic_roughness)
        return 0;

    const cgltf_texture_view& view = primitive.material->pbr_metallic_roughness.base_color_texture;
    if (!view.texture)
        return 0;
    return view.has_transform && view.transform.has_texcoord ? view.transform.texcoord : view.texcoord;
}

bool buildPrimitive(const cgltf_primitive& primitive, std::string_view id, MeshData& meshData)
{
    if (primitive.type != cgltf_primitive_type_triangles || primitive.has_draco_mesh_compression)
    {
        AXLOGE("glTF primitive '{}' must be uncompressed triangles (Draco is not a runtime dependency)", id);
        return false;
    }

    const cgltf_accessor* positionAccessor = findAttribute(primitive, cgltf_attribute_type_position);
    if (!positionAccessor || positionAccessor->count == 0)
    {
        AXLOGE("glTF primitive '{}' has no POSITION accessor", id);
        return false;
    }

    const size_t vertexCount     = positionAccessor->count;
    size_t runtimeStrideInFloats = 8;  // POSITION + TEXCOORD + NORMAL
    if (findAttribute(primitive, cgltf_attribute_type_color))
        runtimeStrideInFloats += 4;
    if (findAttribute(primitive, cgltf_attribute_type_weights) || findAttribute(primitive, cgltf_attribute_type_joints))
        runtimeStrideInFloats += 8;
    size_t maximumVertexFloatCount = 0;
    if (!checkedRuntimeVectorSize(vertexCount, runtimeStrideInFloats, sizeof(float), maximumVertexFloatCount))
    {
        AXLOGE("glTF primitive '{}' exceeds portable MeshData/RHI vertex-buffer limits", id);
        return false;
    }

    std::vector<VertexStream> streams;
    streams.reserve(6);

    VertexStream position{shaderinfos::VertexKey::VERTEX_ATTRIB_POSITION, rhi::VertexElementType::FLOAT3, 3, {}};
    if (!unpackFloats(positionAccessor, 3, position.values))
        return false;

    std::vector<uint32_t> indices;
    if (!readIndices(primitive, vertexCount, indices) || indices.size() % 3 != 0)
        return false;
    streams.emplace_back(std::move(position));

    if (const cgltf_accessor* colorAccessor = findAttribute(primitive, cgltf_attribute_type_color))
    {
        VertexStream color{shaderinfos::VertexKey::VERTEX_ATTRIB_COLOR, rhi::VertexElementType::FLOAT4, 4, {}};
        if (!unpackColor(colorAccessor, vertexCount, color.values))
            return false;
        streams.emplace_back(std::move(color));
    }

    const int textureCoordinate = baseColorTexCoord(primitive);
    if (textureCoordinate < 0)
    {
        AXLOGE("glTF primitive '{}' uses an invalid negative base-color texCoord", id);
        return false;
    }

    VertexStream uv{shaderinfos::VertexKey::VERTEX_ATTRIB_TEX_COORD, rhi::VertexElementType::FLOAT2, 2, {}};
    if (const cgltf_accessor* uvAccessor = findAttribute(primitive, cgltf_attribute_type_texcoord, textureCoordinate))
    {
        if (uvAccessor->count != vertexCount || !unpackFloats(uvAccessor, 2, uv.values))
            return false;
    }
    else if (primitive.material && primitive.material->has_pbr_metallic_roughness &&
             primitive.material->pbr_metallic_roughness.base_color_texture.texture)
    {
        AXLOGE("glTF primitive '{}' is missing TEXCOORD_{} required by its base-color texture", id, textureCoordinate);
        return false;
    }
    else
    {
        // The four stylized variants always consume TEXCOORD0. A constant UV
        // keeps untextured glTF primitives on the same compact shader path.
        size_t floatCount = 0;
        if (!checkedRuntimeVectorSize(vertexCount, 2, sizeof(float), floatCount))
            return false;
        uv.values.resize(floatCount, 0.0F);
    }
    streams.emplace_back(std::move(uv));

    VertexStream normal{shaderinfos::VertexKey::VERTEX_ATTRIB_NORMAL, rhi::VertexElementType::FLOAT3, 3, {}};
    if (const cgltf_accessor* normalAccessor = findAttribute(primitive, cgltf_attribute_type_normal))
    {
        if (normalAccessor->count != vertexCount || !unpackFloats(normalAccessor, 3, normal.values))
            return false;
    }
    else
    {
        normal.values = generateNormals(streams.front().values, indices);
    }
    streams.emplace_back(std::move(normal));

    for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
    {
        const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
        if ((attribute.type == cgltf_attribute_type_joints || attribute.type == cgltf_attribute_type_weights) &&
            attribute.index != 0)
        {
            AXLOGE(
                "glTF primitive '{}' uses JOINTS_{}/WEIGHTS_{}; stylized skinning supports exactly four "
                "influences from set 0",
                id, attribute.index, attribute.index);
            return false;
        }
    }

    const cgltf_accessor* weightAccessor = findAttribute(primitive, cgltf_attribute_type_weights);
    const cgltf_accessor* jointAccessor  = findAttribute(primitive, cgltf_attribute_type_joints);
    if ((weightAccessor == nullptr) != (jointAccessor == nullptr))
    {
        AXLOGE("glTF primitive '{}' must provide JOINTS_0 and WEIGHTS_0 together", id);
        return false;
    }
    if (weightAccessor)
    {
        VertexStream weights{shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_WEIGHT, rhi::VertexElementType::FLOAT4, 4, {}};
        VertexStream joints{shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_INDEX, rhi::VertexElementType::FLOAT4, 4, {}};
        if (weightAccessor->count != vertexCount || !unpackFloats(weightAccessor, 4, weights.values) ||
            !unpackJoints(jointAccessor, vertexCount, joints.values))
            return false;

        if (const auto invalidJoint =
                std::find_if(joints.values.begin(), joints.values.end(),
                             [](float joint) { return joint >= static_cast<float>(MAX_STYLIZED_JOINT_COUNT); });
            invalidJoint != joints.values.end())
        {
            AXLOGE("glTF primitive '{}' uses joint index {}; stylized shaders support indices 0..{}", id,
                   static_cast<uint32_t>(*invalidJoint), MAX_STYLIZED_JOINT_COUNT - 1);
            return false;
        }

        for (size_t vertex = 0; vertex < vertexCount; ++vertex)
        {
            float* value      = weights.values.data() + vertex * 4;
            const float total = value[0] + value[1] + value[2] + value[3];
            if (total > 1.0e-8F)
            {
                const float inverse = 1.0F / total;
                for (size_t component = 0; component < 4; ++component)
                    value[component] *= inverse;
            }
            else
            {
                value[0] = 1.0F;
                value[1] = value[2] = value[3] = 0.0F;
            }
        }
        streams.emplace_back(std::move(weights));
        streams.emplace_back(std::move(joints));
    }

    size_t strideInFloats = 0;
    for (const VertexStream& stream : streams)
    {
        strideInFloats += stream.components;
        meshData.attribs.emplace_back(MeshVertexAttrib{stream.type, stream.key});
    }

    size_t vertexFloatCount = 0;
    if (!checkedRuntimeVectorSize(vertexCount, strideInFloats, sizeof(float), vertexFloatCount) ||
        streams.size() > MAX_RUNTIME_MESH_COUNT || indices.size() > MAX_RUNTIME_MESH_COUNT)
    {
        AXLOGE("glTF primitive '{}' exceeds portable MeshData/RHI buffer limits", id);
        return false;
    }
    meshData.vertex.resize(vertexFloatCount);
    for (size_t vertex = 0; vertex < vertexCount; ++vertex)
    {
        size_t write = vertex * strideInFloats;
        for (const VertexStream& stream : streams)
        {
            const float* source = stream.values.data() + vertex * stream.components;
            std::copy_n(source, stream.components, meshData.vertex.data() + write);
            write += stream.components;
        }
    }

    uint32_t maximumIndex = 0;
    for (uint32_t index : indices)
        maximumIndex = std::max(maximumIndex, index);

    IndexArray indexArray(maximumIndex <= std::numeric_limits<uint16_t>::max() ? rhi::IndexFormat::U_SHORT
                                                                               : rhi::IndexFormat::U_INT);
    indexArray.resize(indices.size());
    if (indexArray.format() == rhi::IndexFormat::U_SHORT)
    {
        for (size_t i = 0; i < indices.size(); ++i)
            indexArray.at<uint16_t>(i) = static_cast<uint16_t>(indices[i]);
    }
    else
    {
        for (size_t i = 0; i < indices.size(); ++i)
            indexArray.at<uint32_t>(i) = indices[i];
    }

    const std::vector<float>& positions = streams.front().values;
    Vec3 minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max());
    Vec3 maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest());
    for (size_t vertex = 0; vertex < vertexCount; ++vertex)
    {
        const Vec3 point(positions[vertex * 3 + 0], positions[vertex * 3 + 1], positions[vertex * 3 + 2]);
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }

    meshData.vertexSizeInFloat = static_cast<int>(meshData.vertex.size());
    meshData.attribCount       = static_cast<int>(meshData.attribs.size());
    meshData.numIndex          = static_cast<int>(indices.size());
    meshData.subMeshIds.emplace_back(id);
    meshData.subMeshIndices.emplace_back(std::move(indexArray));
    meshData.subMeshAABB.emplace_back(minimum, maximum);
    return true;
}

rhi::SamplerAddressMode addressMode(cgltf_wrap_mode mode)
{
    switch (mode)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        return rhi::SamplerAddressMode::CLAMP_TO_EDGE;
    case cgltf_wrap_mode_mirrored_repeat:
        return rhi::SamplerAddressMode::MIRROR;
    default:
        return rhi::SamplerAddressMode::REPEAT;
    }
}

rhi::SamplerFilter minFilter(cgltf_filter_type filter)
{
    switch (filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_nearest_mipmap_linear:
        return rhi::SamplerFilter::MIN_NEAREST;
    default:
        return rhi::SamplerFilter::MIN_LINEAR;
    }
}

rhi::SamplerFilter magFilter(cgltf_filter_type filter)
{
    return filter == cgltf_filter_type_nearest ? rhi::SamplerFilter::MAG_NEAREST : rhi::SamplerFilter::MAG_LINEAR;
}

rhi::SamplerFilter mipFilter(cgltf_filter_type filter)
{
    switch (filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_linear:
        return rhi::SamplerFilter::MIP_DEFAULT;
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_linear_mipmap_nearest:
        return rhi::SamplerFilter::MIP_NEAREST;
    default:
        // glTF's undefined minFilter defaults to LINEAR_MIPMAP_LINEAR.
        return rhi::SamplerFilter::MIP_LINEAR;
    }
}

std::string samplerCacheSuffix(const NTextureData& texture)
{
    return "#sampler-" + std::to_string(static_cast<uint32_t>(texture.minFilter)) + '-' +
           std::to_string(static_cast<uint32_t>(texture.magFilter)) + '-' +
           std::to_string(static_cast<uint32_t>(texture.mipFilter)) + '-' +
           std::to_string(static_cast<uint32_t>(texture.wrapS)) + '-' +
           std::to_string(static_cast<uint32_t>(texture.wrapT)) + "-cs" +
           std::to_string(static_cast<uint32_t>(texture.colorSpace));
}

const cgltf_image* preferredImage(const cgltf_texture& texture)
{
    if (texture.has_basisu && texture.basisu_image)
        return texture.basisu_image;
    return texture.image;
}

std::string imageCacheKey(const cgltf_data& data, const cgltf_image& image, std::string_view gltfPath)
{
    const size_t imageIndex = static_cast<size_t>(&image - data.images);
    return std::string(gltfPath) + "#gltf-image-" + std::to_string(imageIndex);
}

std::string externalImagePath(const cgltf_image& image, std::string_view gltfPath)
{
    if (!image.uri || std::strncmp(image.uri, "data:", 5) == 0)
        return {};

    std::string uri(image.uri);
    cgltf_decode_uri(uri.data());
    uri.resize(std::strlen(uri.c_str()));
    if (FileUtils::getInstance()->isAbsolutePath(uri))
        return uri;

    std::string result = FileUtils::getPathDirName(gltfPath);
    if (!result.empty() && result.back() != '/')
        result.push_back('/');
    result.append(uri);
    const std::string fullPath = FileUtils::getInstance()->fullPathForFilename(result);
    return fullPath.empty() ? result : fullPath;
}

std::shared_ptr<const Data> copyImageData(const cgltf_image& image)
{
    auto result = std::make_shared<Data>();
    if (image.buffer_view)
    {
        const cgltf_buffer_view& view = *image.buffer_view;
        if (!view.buffer || !view.buffer->data || view.offset > view.buffer->size ||
            view.size > view.buffer->size - view.offset || view.size == 0 ||
            view.size > static_cast<size_t>(std::numeric_limits<ssize_t>::max()))
            return nullptr;
        const uint8_t* source = cgltf_buffer_view_data(&view);
        if (!source || result->copy(source, static_cast<ssize_t>(view.size)) <= 0)
            return nullptr;
        return result;
    }

    if (!image.uri || std::strncmp(image.uri, "data:", 5) != 0)
        return nullptr;

    const char* comma = std::strchr(image.uri, ',');
    if (!comma || comma == image.uri ||
        std::string_view(image.uri, static_cast<size_t>(comma - image.uri)).find(";base64") == std::string_view::npos)
        return nullptr;

    auto decoded = utils::base64Decode(comma + 1);
    if (decoded.empty() || decoded.size() > static_cast<size_t>(std::numeric_limits<ssize_t>::max()) ||
        result->copy(decoded.data(), static_cast<ssize_t>(decoded.size())) <= 0)
        return nullptr;
    return result;
}

bool addTexture(const cgltf_data& data,
                const cgltf_texture_view& view,
                NTextureData::Usage usage,
                std::string_view gltfPath,
                std::unordered_map<const cgltf_image*, std::shared_ptr<const Data>>& embeddedImages,
                NMaterialData& output)
{
    if (!view.texture)
        return true;

    const cgltf_image* image = preferredImage(*view.texture);
    if (!image)
    {
        AXLOGE("glTF texture has no supported image source");
        return false;
    }

    NTextureData texture;
    texture.id         = "gltf_texture_" + std::to_string(output.textures.size());
    texture.type       = usage;
    texture.colorSpace = usage == NTextureData::Usage::Diffuse || usage == NTextureData::Usage::Emissive
                             ? rhi::ColorSpace::Srgb
                             : rhi::ColorSpace::Linear;
    texture.minFilter =
        view.texture->sampler ? minFilter(view.texture->sampler->min_filter) : rhi::SamplerFilter::MIN_LINEAR;
    texture.magFilter =
        view.texture->sampler ? magFilter(view.texture->sampler->mag_filter) : rhi::SamplerFilter::MAG_LINEAR;
    texture.mipFilter =
        view.texture->sampler ? mipFilter(view.texture->sampler->min_filter) : rhi::SamplerFilter::MIP_LINEAR;
    texture.wrapS =
        view.texture->sampler ? addressMode(view.texture->sampler->wrap_s) : rhi::SamplerAddressMode::REPEAT;
    texture.wrapT =
        view.texture->sampler ? addressMode(view.texture->sampler->wrap_t) : rhi::SamplerAddressMode::REPEAT;
    texture.texCoord     = view.has_transform && view.transform.has_texcoord ? view.transform.texcoord : view.texcoord;
    texture.hasTransform = view.has_transform;
    if (view.has_transform)
    {
        texture.offset   = Vec2(view.transform.offset[0], view.transform.offset[1]);
        texture.scale    = Vec2(view.transform.scale[0], view.transform.scale[1]);
        texture.rotation = view.transform.rotation;
    }

    texture.filename = externalImagePath(*image, gltfPath);
    if (!texture.filename.empty())
    {
        texture.cacheKey = texture.filename + samplerCacheSuffix(texture);
    }
    else
    {
        texture.cacheKey    = imageCacheKey(data, *image, gltfPath) + samplerCacheSuffix(texture);
        auto [it, inserted] = embeddedImages.try_emplace(image);
        if (inserted)
            it->second = copyImageData(*image);
        texture.embeddedData = it->second;
        if (!texture.embeddedData || texture.embeddedData->isNull())
        {
            AXLOGE("glTF embedded image '{}' could not be decoded", texture.cacheKey);
            return false;
        }
    }

    output.textures.emplace_back(std::move(texture));
    return true;
}

bool buildMaterials(const cgltf_data& data, std::string_view gltfPath, MaterialDatas& output)
{
    std::unordered_map<const cgltf_image*, std::shared_ptr<const Data>> embeddedImages;
    output.materials.reserve(data.materials_count);
    for (cgltf_size i = 0; i < data.materials_count; ++i)
    {
        const cgltf_material& source = data.materials[i];
        NMaterialData material;
        material.id          = "gltf_material_" + std::to_string(i);
        material.doubleSided = source.double_sided;
        if (source.has_pbr_metallic_roughness)
        {
            const auto& color  = source.pbr_metallic_roughness.base_color_factor;
            material.baseColor = Color(color[0], color[1], color[2], color[3]);
            if (!addTexture(data, source.pbr_metallic_roughness.base_color_texture, NTextureData::Usage::Diffuse,
                            gltfPath, embeddedImages, material))
                return false;
        }
        switch (source.alpha_mode)
        {
        case cgltf_alpha_mode_mask:
            material.alphaMode   = NMaterialData::AlphaMode::Mask;
            material.alphaCutoff = source.alpha_cutoff;
            break;
        case cgltf_alpha_mode_blend:
            material.alphaMode = NMaterialData::AlphaMode::Blend;
            break;
        default:
            material.alphaMode = NMaterialData::AlphaMode::Opaque;
            break;
        }
        output.materials.emplace_back(std::move(material));
    }
    return true;
}

std::string materialId(const cgltf_data& data, const cgltf_material* material)
{
    if (!material)
        return {};
    return "gltf_material_" + std::to_string(static_cast<size_t>(material - data.materials));
}

std::vector<Mat4> inverseBindMatrices(const cgltf_skin& skin)
{
    std::vector<Mat4> result(skin.joints_count, Mat4::identity);
    if (!skin.inverse_bind_matrices || skin.inverse_bind_matrices->count < skin.joints_count ||
        cgltf_num_components(skin.inverse_bind_matrices->type) != 16)
        return result;

    for (size_t i = 0; i < result.size(); ++i)
    {
        float matrix[16];
        if (cgltf_accessor_read_float(skin.inverse_bind_matrices, i, matrix, 16))
            result[i] = Mat4(matrix);
    }
    return result;
}

NodeData* cloneRenderNode(const cgltf_data& data,
                          const cgltf_node& source,
                          const std::unordered_map<const cgltf_primitive*, std::string>& primitiveIds)
{
    auto node       = std::make_unique<NodeData>();
    node->id        = nodeName(data, &source);
    node->transform = localTransform(source);

    if (source.mesh)
    {
        const std::vector<Mat4> inverseBind = source.skin ? inverseBindMatrices(*source.skin) : std::vector<Mat4>{};
        for (cgltf_size i = 0; i < source.mesh->primitives_count; ++i)
        {
            const cgltf_primitive& primitive = source.mesh->primitives[i];
            const auto id                    = primitiveIds.find(&primitive);
            if (id == primitiveIds.end())
                continue;

            auto model        = std::make_unique<ModelData>();
            model->subMeshId  = id->second;
            model->materialId = materialId(data, primitive.material);
            if (source.skin)
            {
                model->skinningInSkeletonSpace = true;
                model->bones.reserve(source.skin->joints_count);
                for (cgltf_size joint = 0; joint < source.skin->joints_count; ++joint)
                    model->bones.emplace_back(nodeName(data, source.skin->joints[joint]));
                model->invBindPose = inverseBind;
            }
            node->modelNodeDatas.emplace_back(model.release());
        }
    }

    for (cgltf_size i = 0; i < source.children_count; ++i)
        node->children.emplace_back(cloneRenderNode(data, *source.children[i], primitiveIds));
    return node.release();
}

NodeData* cloneSkeletonNode(const cgltf_data& data,
                            const cgltf_node& source,
                            const std::unordered_set<const cgltf_node*>& skeletonNodes)
{
    auto node       = std::make_unique<NodeData>();
    node->id        = nodeName(data, &source);
    node->transform = localTransform(source);
    for (cgltf_size i = 0; i < source.children_count; ++i)
    {
        if (skeletonNodes.contains(source.children[i]))
            node->children.emplace_back(cloneSkeletonNode(data, *source.children[i], skeletonNodes));
    }
    return node.release();
}

void buildNodes(const cgltf_data& data,
                const std::unordered_map<const cgltf_primitive*, std::string>& primitiveIds,
                NodeDatas& output)
{
    std::unordered_set<const cgltf_node*> skeletonNodes;
    for (cgltf_size skinIndex = 0; skinIndex < data.skins_count; ++skinIndex)
    {
        const cgltf_skin& skin = data.skins[skinIndex];
        for (cgltf_size joint = 0; joint < skin.joints_count; ++joint)
        {
            for (const cgltf_node* node = skin.joints[joint]; node; node = node->parent)
                skeletonNodes.emplace(node);
        }
    }
    for (const cgltf_node* node : skeletonNodes)
    {
        if (!node->parent || !skeletonNodes.contains(node->parent))
            output.skeleton.emplace_back(cloneSkeletonNode(data, *node, skeletonNodes));
    }

    if (data.scene)
    {
        for (cgltf_size i = 0; i < data.scene->nodes_count; ++i)
            output.nodes.emplace_back(cloneRenderNode(data, *data.scene->nodes[i], primitiveIds));
    }
    else
    {
        for (cgltf_size i = 0; i < data.nodes_count; ++i)
        {
            if (!data.nodes[i].parent)
                output.nodes.emplace_back(cloneRenderNode(data, data.nodes[i], primitiveIds));
        }
    }
}

bool buildMeshes(const cgltf_data& data,
                 MeshDatas& output,
                 std::unordered_map<const cgltf_primitive*, std::string>& primitiveIds)
{
    for (cgltf_size meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex)
    {
        const cgltf_mesh& mesh = data.meshes[meshIndex];
        for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
        {
            const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
            const std::string id =
                "gltf_mesh_" + std::to_string(meshIndex) + "_primitive_" + std::to_string(primitiveIndex);
            auto meshData = std::make_unique<MeshData>();
            if (!buildPrimitive(primitive, id, *meshData))
                return false;
            primitiveIds.emplace(&primitive, id);
            output.meshDatas.emplace_back(meshData.release());
        }
    }
    return !output.meshDatas.empty();
}

bool loadAnimationData(const cgltf_data& data, std::string_view animationName, Animation3DData& output)
{
    const cgltf_animation* animation = nullptr;
    if (animationName.empty() && data.animations_count > 0)
        animation = &data.animations[0];
    else
    {
        for (cgltf_size i = 0; i < data.animations_count; ++i)
        {
            if (data.animations[i].name && animationName == data.animations[i].name)
            {
                animation = &data.animations[i];
                break;
            }
        }
    }
    if (!animation)
        return false;

    output._usesAbsoluteLocalTransforms = true;
    std::unordered_set<const cgltf_node*> animatedNodes;

    for (cgltf_size i = 0; i < animation->channels_count; ++i)
    {
        const cgltf_animation_channel& channel = animation->channels[i];
        if (!channel.target_node || !channel.sampler || !channel.sampler->input || !channel.sampler->output)
            return false;
        if (channel.sampler->interpolation != cgltf_interpolation_type_linear)
        {
            AXLOGE("glTF animation '{}' uses unsupported STEP/CUBICSPLINE interpolation",
                   animation->name ? animation->name : "");
            return false;
        }

        const cgltf_accessor& input  = *channel.sampler->input;
        const cgltf_accessor& values = *channel.sampler->output;
        if (cgltf_num_components(input.type) != 1 || input.count != values.count)
            return false;

        std::vector<float> times;
        if (!unpackFloats(&input, 1, times))
            return false;
        if (times.empty() ||
            std::adjacent_find(times.begin(), times.end(), [](float left, float right) { return left >= right; }) !=
                times.end() ||
            std::any_of(times.begin(), times.end(), [](float time) { return !std::isfinite(time) || time < 0.0F; }))
            return false;
        const std::string target = nodeName(data, channel.target_node);
        animatedNodes.emplace(channel.target_node);

        switch (channel.target_path)
        {
        case cgltf_animation_path_type_translation:
        case cgltf_animation_path_type_scale:
        {
            std::vector<float> vectors;
            if (!unpackFloats(&values, 3, vectors))
                return false;
            auto& keys = channel.target_path == cgltf_animation_path_type_translation ? output._translationKeys[target]
                                                                                      : output._scaleKeys[target];
            keys.reserve(times.size());
            for (size_t key = 0; key < times.size(); ++key)
                keys.emplace_back(times[key], Vec3(vectors[key * 3 + 0], vectors[key * 3 + 1], vectors[key * 3 + 2]));
            break;
        }
        case cgltf_animation_path_type_rotation:
        {
            std::vector<float> quaternions;
            if (!unpackFloats(&values, 4, quaternions))
                return false;
            auto& keys = output._rotationKeys[target];
            keys.reserve(times.size());
            for (size_t key = 0; key < times.size(); ++key)
            {
                Quat rotation(quaternions[key * 4 + 0], quaternions[key * 4 + 1], quaternions[key * 4 + 2],
                              quaternions[key * 4 + 3]);
                rotation.normalize();
                keys.emplace_back(times[key], rotation);
            }
            break;
        }
        default:
            continue;
        }

        if (!times.empty())
            output._totalTime = std::max(output._totalTime, times.back());
    }

    for (const cgltf_node* targetNode : animatedNodes)
    {
        const std::string target = nodeName(data, targetNode);
        Vec3 translation;
        Quat rotation;
        Vec3 scale;
        localTransform(*targetNode).decompose(&scale, &rotation, &translation);

        if (!output._translationKeys.contains(target))
            output._translationKeys[target].emplace_back(0.0F, translation);
        if (!output._rotationKeys.contains(target))
            output._rotationKeys[target].emplace_back(0.0F, rotation);
        if (!output._scaleKeys.contains(target))
            output._scaleKeys[target].emplace_back(0.0F, scale);
    }

    if (output._totalTime > 0.0F)
    {
        const float inverseDuration = 1.0F / output._totalTime;
        const auto normalizeTimes   = [inverseDuration](auto& curves) {
            for (auto& [name, keys] : curves)
            {
                (void)name;
                for (auto& key : keys)
                    key._time *= inverseDuration;
            }
        };
        normalizeTimes(output._translationKeys);
        normalizeTimes(output._rotationKeys);
        normalizeTimes(output._scaleKeys);
    }
    return true;
}

}  // namespace

bool GltfLoader::isGltfPath(std::string_view path)
{
    std::string extension = FileUtils::getPathExtension(path);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".gltf" || extension == ".glb";
}

bool GltfLoader::load(std::string_view path, NodeDatas& nodeDatas, MeshDatas& meshDatas, MaterialDatas& materialDatas)
{
    nodeDatas.resetData();
    meshDatas.resetData();
    materialDatas.resetData();

    ParsedAsset asset;
    if (!asset.open(path))
        return false;
    std::unordered_map<const cgltf_primitive*, std::string> primitiveIds;
    if (!buildMeshes(asset.data(), meshDatas, primitiveIds))
    {
        meshDatas.resetData();
        return false;
    }
    if (!buildMaterials(asset.data(), asset.fullPath(), materialDatas))
    {
        meshDatas.resetData();
        materialDatas.resetData();
        return false;
    }
    buildNodes(asset.data(), primitiveIds, nodeDatas);
    if (nodeDatas.nodes.empty())
    {
        nodeDatas.resetData();
        meshDatas.resetData();
        materialDatas.resetData();
        return false;
    }
    return true;
}

bool GltfLoader::loadAnimation(std::string_view path, std::string_view animationName, Animation3DData& animationData)
{
    animationData.resetData();
    ParsedAsset asset;
    if (!asset.open(path) || !loadAnimationData(asset.data(), animationName, animationData))
    {
        animationData.resetData();
        return false;
    }
    return true;
}

}  // namespace ax
