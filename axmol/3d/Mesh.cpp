/****************************************************************************
 Copyright (c) 2014-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
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

#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshSkin.h"
#include "axmol/3d/Skeleton3D.h"
#include "axmol/3d/MeshVertexIndexData.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedQuality.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/3d/VertexInputBinding.h"
#include "axmol/2d/Light.h"
#include "axmol/scene/Scene.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/Director.h"
#include "axmol/base/Environment.h"
#include "axmol/renderer/TextureCache.h"
#include "axmol/renderer/Material.h"
#include "axmol/renderer/Technique.h"
#include "axmol/renderer/Pass.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/rhi/Buffer.h"
#include "axmol/rhi/Program.h"
#include "axmol/renderer/RenderConsts.h"
#include "axmol/math/Mat4.h"
#include "axmol/scene/Camera.h"

#include <array>
#include <algorithm>
#include <cmath>

using namespace std;

namespace ax
{

// Helpers

// sampler uniform names, only diffuse and normal texture are supported for now
std::string s_uniformSamplerName[] = {
    "",             // NTextureData::Usage::Unknown,
    "",             // NTextureData::Usage::None
    "",             // NTextureData::Usage::Diffuse
    "",             // NTextureData::Usage::Emissive
    "",             // NTextureData::Usage::Ambient
    "",             // NTextureData::Usage::Specular
    "",             // NTextureData::Usage::Shininess
    "u_normalTex",  // NTextureData::Usage::Normal
    "",             // NTextureData::Usage::Bump
    "",             // NTextureData::Usage::Transparency
    "",             // NTextureData::Usage::Reflection
};

// helpers
void Mesh::resetLightUniformValues()
{
    const auto& conf            = Environment::getInstance();
    constexpr int maxDirLight   = AX_MAX_DIRECTIONAL_LIGHT;
    constexpr int maxPointLight = AX_MAX_POINT_LIGHT;
    constexpr int maxSpotLight  = AX_MAX_SPOT_LIGHT;

    _dirLightUniformColorValues.assign(maxDirLight, Vec3::zero);
    _dirLightUniformDirValues.assign(maxDirLight, Vec3::zero);

    _pointLightUniformColorValues.assign(maxPointLight, Vec3::zero);
    _pointLightUniformPositionValues.assign(maxPointLight, Vec3::zero);
    _pointLightUniformRangeInverseValues.assign(maxPointLight, 0.0f);

    _spotLightUniformColorValues.assign(maxSpotLight, Vec3::zero);
    _spotLightUniformPositionValues.assign(maxSpotLight, Vec3::zero);

    // TODO It's strange that init _spotLightUniformDirValues to zeros will cause no light effects on iPhone6 and
    // iPhone6s, but works well on iPhoneX fix no light effects on iPhone6 and iPhone6s
    _spotLightUniformDirValues.assign(maxSpotLight, Vec3(FLT_EPSILON, 0.0f, 0.0f));
    _spotLightUniformInnerAngleCosValues.assign(maxSpotLight, 1.0f);
    _spotLightUniformOuterAngleCosValues.assign(maxSpotLight, 0.0f);
    _spotLightUniformRangeInverseValues.assign(maxSpotLight, 0.0f);
}

Mesh::Mesh()
    : _skin(nullptr)
    , _visible(true)
    , _instancing(false)
    , _instanceTransformBuffer(nullptr)
    , _instanceTransformDirty(false)
    , _instanceTransformBufferDirty(false)
    , _instanceCount(0)
    , _dynamicInstancing(false)
    , _instanceMatrixCache(nullptr)
    , meshIndexFormat(CustomCommand::IndexFormat::U_SHORT)
    , _meshIndexData(nullptr)
    , _blend(BlendFunc::ALPHA_NON_PREMULTIPLIED)
    , _blendDirty(true)
    , _material(nullptr)
    , _texFile("")
{}
Mesh::~Mesh()
{
    for (auto&& tex : _textures)
        AX_SAFE_RELEASE(tex.second);

    for (auto&& ins : _instances)
        AX_SAFE_RELEASE(ins);

    AX_SAFE_RELEASE(_skin);
    AX_SAFE_RELEASE(_meshIndexData);
    AX_SAFE_RELEASE(_material);
    AX_SAFE_RELEASE(_stylizedShadowMaterial);
    AX_SAFE_RELEASE(_instanceTransformBuffer);
    AX_SAFE_DELETE_ARRAY(_instanceMatrixCache);
}

void Mesh::enableInstancing(bool instance, int count)
{
    const int capacity = instance ? std::max(1, count) : 0;
    if (_instancing == instance && _instanceCount == capacity)
        return;

    _instancing                   = instance;
    _instanceCount                = capacity;
    _instanceTransformDirty       = instance;
    _instanceTransformBufferDirty = true;
}

void Mesh::setInstanceCount(int count)
{
    AXASSERT(_instancing, "Instancing should be enabled on this mesh.");

    const int capacity = std::max(1, count);
    if (_instanceCount != capacity)
    {
        _instanceCount                = capacity;
        _instanceTransformBufferDirty = true;
        _instanceTransformDirty       = true;
    }
}

void Mesh::addInstanceChild(Node* child)
{
    AX_SAFE_RETAIN(child);
    _instances.push_back(child);
    _instanceTransformDirty = true;

    if (_instances.size() > _instanceCount)
    {
        _instanceCount                = std::max(1, _instanceCount * 2);
        _instanceTransformBufferDirty = true;
    }
}

void Mesh::shrinkToFitInstances()
{
    if (_instanceCount > _instances.size())
    {
        _instanceCount                = static_cast<int>(_instances.size());
        _instanceTransformBufferDirty = true;
    }
}

void Mesh::rebuildInstances()
{
    _instanceTransformDirty = true;
}

void Mesh::setDynamicInstancing(bool dynamic)
{
    if (_dynamicInstancing == dynamic)
        return;

    _dynamicInstancing            = dynamic;
    _instanceTransformBufferDirty = true;
    _instanceTransformDirty       = true;
}

bool Mesh::prepareInstanceData(bool identityInstanceRequired)
{
    if (!_instancing && !identityInstanceRequired)
        return true;
    if (_instancing && _instances.empty())
        return false;

    const int capacity      = _instancing ? std::max(1, _instanceCount) : 1;
    const size_t bufferSize = static_cast<size_t>(capacity) * sizeof(Mat4);
    bool rebuilt            = false;
    if (!_instanceTransformBuffer || _instanceTransformBufferDirty ||
        _instanceTransformBuffer->getCapacity() < bufferSize)
    {
        AX_SAFE_RELEASE_NULL(_instanceTransformBuffer);
        AX_SAFE_DELETE_ARRAY(_instanceMatrixCache);

        const auto usage         = _dynamicInstancing ? rhi::BufferUsage::DYNAMIC : rhi::BufferUsage::STATIC;
        _instanceTransformBuffer = axdrv->createBuffer(bufferSize, rhi::BufferType::VERTEX, usage);
        if (!_instanceTransformBuffer)
            return false;

        _instanceMatrixCache = new float[static_cast<size_t>(capacity) * 16];
        for (int index = 0; index < capacity; ++index)
            std::copy_n(Mat4::identity.m, 16, _instanceMatrixCache + static_cast<size_t>(index) * 16);
        _instanceTransformBuffer->updateData(_instanceMatrixCache, bufferSize);

        _instanceTransformBufferDirty = false;
        rebuilt                       = true;
    }

    if (_instancing && (rebuilt || _instanceTransformDirty || _dynamicInstancing))
    {
        size_t matrixOffset = 0;
        for (const auto* instance : _instances)
        {
            const Mat4& matrix = instance->getNodeToParentTransform();
            std::copy_n(matrix.m, 16, _instanceMatrixCache + matrixOffset * 16);
            ++matrixOffset;
        }
        _instanceTransformBuffer->updateSubData(_instanceMatrixCache, 0, matrixOffset * sizeof(Mat4));
        _instanceTransformDirty = false;
    }

    return true;
}

rhi::Buffer* Mesh::getVertexBuffer() const
{
    return _meshIndexData->getVertexBuffer();
}

bool Mesh::hasVertexAttrib(shaderinfos::VertexKey attrib) const
{
    return _meshIndexData->getMeshVertexData()->hasVertexAttrib(attrib);
}

ssize_t Mesh::getMeshVertexAttribCount() const
{
    return _meshIndexData->getMeshVertexData()->getMeshVertexAttribCount();
}

const MeshVertexAttrib& Mesh::getMeshVertexAttribute(int idx)
{
    return _meshIndexData->getMeshVertexData()->getMeshVertexAttrib(idx);
}

int Mesh::getVertexSizeInBytes() const
{
    return static_cast<int>(_meshIndexData->getMeshVertexData()->getSizePerVertex());
}

Mesh* Mesh::create(const std::vector<float>& positions,
                   const std::vector<float>& normals,
                   const std::vector<float>& texs,
                   const IndexArray& indices)
{
    int perVertexSizeInFloat = 0;
    std::vector<float> vertices;
    tlx::pod_vector<MeshVertexAttrib> attribs;

    MeshVertexAttrib att;
    att.type = rhi::VertexElementType::FLOAT3;

    attribs.reserve(3);

    size_t hasNormal   = 0;
    size_t hasTexCoord = 0;
    if (!positions.empty())
    {
        perVertexSizeInFloat += 3;
        att.vertexAttrib = shaderinfos::VertexKey::VERTEX_ATTRIB_POSITION;
        attribs.emplace_back(att);
    }
    if (!normals.empty())
    {
        perVertexSizeInFloat += 3;
        att.vertexAttrib = shaderinfos::VertexKey::VERTEX_ATTRIB_NORMAL;
        attribs.emplace_back(att);
        hasNormal = 1;
    }
    if (!texs.empty())
    {
        perVertexSizeInFloat += 2;
        att.type         = rhi::VertexElementType::FLOAT2;
        att.vertexAttrib = shaderinfos::VertexKey::VERTEX_ATTRIB_TEX_COORD;
        attribs.emplace_back(att);
        hasTexCoord = 1;
    }

    // position, normal, texCoordinate into _vertexs
    size_t vertexNum = positions.size() / 3;
    vertices.reserve(positions.size() + hasNormal * 3 + hasTexCoord * 2);
    for (size_t i = 0; i < vertexNum; i++)
    {
        vertices.emplace_back(positions[i * 3]);
        vertices.emplace_back(positions[i * 3 + 1]);
        vertices.emplace_back(positions[i * 3 + 2]);

        if (hasNormal)
        {
            vertices.emplace_back(normals[i * 3]);
            vertices.emplace_back(normals[i * 3 + 1]);
            vertices.emplace_back(normals[i * 3 + 2]);
        }

        if (hasTexCoord)
        {
            vertices.emplace_back(texs[i * 2]);
            vertices.emplace_back(texs[i * 2 + 1]);
        }
    }
    return create(vertices, perVertexSizeInFloat, indices, attribs);
}

Mesh* Mesh::create(const std::vector<float>& vertices,
                   int /*perVertexSizeInFloat*/,
                   const IndexArray& indices,
                   const tlx::pod_vector<MeshVertexAttrib>& attribs)
{
    MeshData meshdata;
    meshdata.attribs = attribs;
    meshdata.vertex  = vertices;
    meshdata.subMeshIndices.emplace_back(indices);
    meshdata.subMeshIds.emplace_back("");
    auto meshvertexdata = MeshVertexData::create(meshdata, indices.format());
    auto indexData      = meshvertexdata->getMeshIndexDataByIndex(0);

    auto mesh = create("", indexData);
    mesh->setIndexFormat(indices.format());

    return mesh;
}

Mesh* Mesh::create(std::string_view name, MeshIndexData* indexData, MeshSkin* skin)
{
    auto state = new Mesh();
    state->autorelease();
    state->bindMeshCommand();
    state->_name = name;
    state->setMeshIndexData(indexData);
    state->setSkin(skin);

    return state;
}

void Mesh::setVisible(bool visible)
{
    if (_visible != visible)
    {
        _visible = visible;
        if (_visibleChanged)
            _visibleChanged();
    }
}

bool Mesh::isVisible() const
{
    return _visible;
}

void Mesh::setTexture(std::string_view texPath)
{
    _texFile = texPath;
    auto tex = Director::getInstance()->getTextureCache()->addImage(texPath);
    setTexture(tex, NTextureData::Usage::Diffuse);
}

void Mesh::setTexture(Texture2D* tex)
{
    setTexture(tex, NTextureData::Usage::Diffuse);
}

void Mesh::setTexture(Texture2D* tex, NTextureData::Usage usage, bool cacheFileName)
{
    // Texture must be saved for future use
    // it doesn't matter if the material is already set or not
    // This functionality is added for compatibility issues
    if (tex == nullptr)
        tex = Director::getInstance()->getTextureCache()->getDummyTexture();
    AX_SAFE_RETAIN(tex);
    AX_SAFE_RELEASE(_textures[usage]);
    _textures[usage] = tex;

    if (usage == NTextureData::Usage::Diffuse)
    {
        if (_material)
        {
            auto technique = _material->_currentTechnique;
            for (auto&& pass : technique->_passes)
            {
                pass->setUniformTexture(0, tex->getRHITexture());
            }
        }

        bindMeshCommand();
        if (cacheFileName)
            _texFile = tex->getPath();
    }
    else if (usage == NTextureData::Usage::Normal)  // currently only diffuse and normal are supported
    {
        if (_material)
        {
            auto technique = _material->_currentTechnique;
            for (auto&& pass : technique->_passes)
            {
                pass->setUniformNormTexture(1, tex->getRHITexture());
            }
        }
    }
}

void Mesh::setTexture(std::string_view texPath, NTextureData::Usage usage)
{
    auto tex = Director::getInstance()->getTextureCache()->addImage(texPath);
    setTexture(tex, usage);
}

Texture2D* Mesh::getTexture() const
{
    return _textures.at(NTextureData::Usage::Diffuse);
}

Texture2D* Mesh::getTexture(NTextureData::Usage usage)
{
    return _textures[usage];
}

void Mesh::setMaterial(Material* material)
{
    if (_material != material)
    {
        AX_SAFE_RELEASE(_material);
        _material = material;
        AX_SAFE_RETAIN(_material);
    }
    _meshCommands.clear();

    if (_material)
    {
        for (auto&& technique : _material->getTechniques())
        {
            // allocate MeshCommand vector for technique
            // allocate MeshCommand for each pass
            auto& list = _meshCommands[technique->getName()];
            list.resize(technique->getPasses().size());

            int i = 0;
            for (auto&& pass : technique->getPasses())
            {
#ifdef _AX_DEBUG
                // make it crashed when missing attribute data
                if (_material->getTechnique()->getName().compare(technique->getName()) == 0)
                {
                    auto program        = pass->getProgramState()->getProgram();
                    auto& vertexInputs  = program->getActiveVertexInputs();
                    auto meshVertexData = _meshIndexData->getMeshVertexData();
                    auto attributeCount = meshVertexData->getMeshVertexAttribCount();
                    // AXASSERT(vertexInputs.size() <= attributeCount, "missing attribute data");
                }
#endif
                const auto* stylizedMaterial  = dynamic_cast<const StylizedMaterial*>(_material);
                const bool usesInstanceStream = stylizedMaterial ? !stylizedMaterial->isSkinned() : _instancing;
                auto vertexInputBinding = VertexInputBinding::fetch(_meshIndexData, pass, &list[i], usesInstanceStream);
                pass->setVertexInputBinding(vertexInputBinding);
                i += 1;
            }
        }

        _meshIndexData->setPrimitiveType(material->getPrimitiveType());
    }
    // Was the texture set before the ProgramState ? Set it
    for (auto&& tex : _textures)
        setTexture(tex.second, tex.first);

    if (_blendDirty)
        setBlendFunc(_blend);

    bindMeshCommand();
}

Material* Mesh::getMaterial() const
{
    return _material;
}

void Mesh::draw(Renderer* renderer,
                float globalZOrder,
                const Mat4& transform,
                uint32_t flags,
                unsigned int lightMask,
                const Vec4& color,
                bool forceDepthWrite,
                bool wireframe,
                Scene* ownerScene)
{
    if (!isVisible())
        return;

    bool isTransparent = (_material->isTransparent() || color.w < 1.f);
    float globalZ      = isTransparent ? 0 : globalZOrder;
    if (isTransparent)
        flags |= Node::FLAGS_RENDER_AS_3D;

    if (isTransparent && !forceDepthWrite)
        _material->getStateBlock().setDepthWrite(false);
    else
        _material->getStateBlock().setDepthWrite(true);

    // set default uniforms for Mesh
    // 'u_color' and others
    const auto scene              = ownerScene ? ownerScene : Director::getInstance()->getRunningScene();
    auto technique                = _material->_currentTechnique;
    auto* stylizedMaterial        = dynamic_cast<StylizedMaterial*>(_material);
    const bool usesInstanceStream = stylizedMaterial ? !_skin : _instancing;
    if (usesInstanceStream && !prepareInstanceData(stylizedMaterial && !_instancing))
        return;
    for (const auto pass : technique->_passes)
    {
        if (const auto diffuse = _textures.find(NTextureData::Usage::Diffuse); diffuse != _textures.end())
            pass->setUniformTexture(0, diffuse->second->getRHITexture());
        if (const auto normal = _textures.find(NTextureData::Usage::Normal); normal != _textures.end())
            pass->setUniformNormTexture(1, normal->second->getRHITexture());

        pass->setUniformColor(&color, sizeof(color));

        if (_skin)
            pass->setUniformMatrixPalette(_skin->getMatrixPalette(), _skin->getMatrixPaletteSizeInBytes());

        if (stylizedMaterial)
        {
            setStylizedLightUniforms(pass, scene, *stylizedMaterial, lightMask, transform);
        }
        else if (scene && !scene->getLights().empty())
        {
            setLightUniforms(pass, scene, color, lightMask);
        }
    }
    auto& commands = _meshCommands[technique->getName()];

    for (auto&& command : commands)
    {
        command.init(globalZ, transform);
        command.setSkipBatching(isTransparent);
        command.setTransparent(isTransparent);
        command.set3D(!_material->isForce2DQueue());
        command.setWireframe(wireframe);
        if (usesInstanceStream)
        {
            const int drawCount = _instancing ? static_cast<int>(_instances.size()) : 1;
            command.setDrawType(CustomCommand::DrawType::ELEMENT_INSTANCED);
            command.setInstanceBuffer(_instanceTransformBuffer, drawCount);
        }
        else
        {
            command.setDrawType(CustomCommand::DrawType::ELEMENT);
            command.setInstanceBuffer(nullptr, 0);
        }
    }

    _meshIndexData->setPrimitiveType(_material->_drawPrimitive);
    _material->draw(commands.data(), globalZ, getVertexBuffer(), getIndexBuffer(), getPrimitiveType(), getIndexFormat(),
                    static_cast<unsigned int>(getIndexCount()), transform);
}

bool Mesh::ensureStylizedShadowMaterial(const StylizedMaterial& sourceMaterial, uint64_t resourceGeneration)
{
    const uint32_t programType = sourceMaterial.getShadowProgramType();
    if (_stylizedShadowMaterial && _stylizedShadowProgramType == programType &&
        _stylizedShadowGeneration == resourceGeneration)
    {
        _stylizedShadowMaterial->getStateBlock().setCullFace(!sourceMaterial._desc.doubleSided);
        return true;
    }

    AX_SAFE_RELEASE_NULL(_stylizedShadowMaterial);
    auto* program = axpm->getBuiltinProgram(programType);
    if (!program)
        return false;

    auto* programState = new rhi::ProgramState(program);
    auto* material     = Material::createWithProgramState(programState);
    programState->release();
    if (!material)
        return false;

    material->retain();
    material->getStateBlock().setDepthTest(true);
    material->getStateBlock().setDepthWrite(true);
    material->getStateBlock().setCullFace(!sourceMaterial._desc.doubleSided);
    material->getStateBlock().setCullFaceSide(CullFaceSide::BACK);

    auto* pass    = material->getTechnique()->getPassByIndex(0);
    auto* binding = VertexInputBinding::fetch(_meshIndexData, pass, &_stylizedShadowCommands[0], !_skin);
    pass->setVertexInputBinding(binding);

    _stylizedShadowMaterial    = material;
    _stylizedShadowProgramType = programType;
    _stylizedShadowGeneration  = resourceGeneration;
    return true;
}

void Mesh::drawStylizedShadow(Renderer& renderer,
                              const StylizedMaterial& sourceMaterial,
                              const std::array<int, 2>& cascadeQueueIds,
                              const std::array<Mat4, 2>& lightViewProjection,
                              uint8_t cascadeCount,
                              const Mat4& transform,
                              const Vec4& color,
                              uint64_t resourceGeneration)
{
    if (!isVisible() || cascadeCount == 0 || !ensureStylizedShadowMaterial(sourceMaterial, resourceGeneration))
        return;

    const bool usesInstanceStream = !_skin;
    if (usesInstanceStream && !prepareInstanceData(!_instancing))
        return;

    auto* pass        = _stylizedShadowMaterial->getTechnique()->getPassByIndex(0);
    const auto toVec4 = [](const Color& value) { return Vec4{value.r, value.g, value.b, value.a}; };
    const std::array<Vec4, 7> materialValues = {
        toVec4(sourceMaterial._desc.baseColor),
        toVec4(sourceMaterial._desc.highlightColor),
        toVec4(sourceMaterial._desc.shadowColor),
        toVec4(sourceMaterial._desc.diffuseTint),
        toVec4(sourceMaterial._desc.rimColor),
        Vec4{sourceMaterial._desc.bandThreshold, sourceMaterial._desc.bandSoftness, sourceMaterial._desc.rimStart,
             sourceMaterial._desc.rimEnd},
        Vec4{sourceMaterial._desc.rimIntensity, sourceMaterial._desc.alphaCutoff, 0.0F, 0.0F},
    };
    pass->setUniformStylizedMaterial(materialValues.data(), sizeof(materialValues));
    const float uvCosine                  = std::cos(sourceMaterial._desc.uvRotation);
    const float uvSine                    = std::sin(sourceMaterial._desc.uvRotation);
    const std::array<Vec4, 2> uvTransform = {
        Vec4{sourceMaterial._desc.uvOffset.x, sourceMaterial._desc.uvOffset.y, sourceMaterial._desc.uvScale.x,
             sourceMaterial._desc.uvScale.y},
        Vec4{uvCosine, uvSine, 0.0F, 0.0F},
    };
    pass->setUniformStylizedUvTransform(uvTransform.data(), sizeof(uvTransform));
    pass->setUniformColor(&color, sizeof(color));
    if (_skin)
        pass->setUniformMatrixPalette(_skin->getMatrixPalette(), _skin->getMatrixPaletteSizeInBytes());

    Texture2D* texture = sourceMaterial._desc.baseTexture;
    if (auto textureIt = _textures.find(NTextureData::Usage::Diffuse); textureIt != _textures.end())
        texture = textureIt->second;
    if (!texture)
        texture = Director::getInstance()->getTextureCache()->getWhiteTexture();
    pass->setUniformTexture(0, texture->getRHITexture());

    for (uint8_t cascade = 0; cascade < std::min<uint8_t>(cascadeCount, 2); ++cascade)
    {
        if (cascadeQueueIds[cascade] < 0)
            continue;
        auto& command = _stylizedShadowCommands[cascade];
        if (usesInstanceStream)
        {
            const int drawCount = _instancing ? static_cast<int>(_instances.size()) : 1;
            command.setDrawType(CustomCommand::DrawType::ELEMENT_INSTANCED);
            command.setInstanceBuffer(_instanceTransformBuffer, drawCount);
        }
        else
        {
            command.setDrawType(CustomCommand::DrawType::ELEMENT);
            command.setInstanceBuffer(nullptr, 0);
        }
        renderer.pushGroup(cascadeQueueIds[cascade]);
        _stylizedShadowMaterial->draw(&command, 0.0F, getVertexBuffer(), getIndexBuffer(), getPrimitiveType(),
                                      getIndexFormat(), static_cast<unsigned int>(getIndexCount()), transform);
        renderer.popGroup();

        command.setTransparent(false);
        command.set3D(true);
        command.setWireframe(false);
        command.setSkipBatching(false);
        command.setViewProjectionOverride(lightViewProjection[cascade]);
    }
}

void Mesh::setSkin(MeshSkin* skin)
{
    if (_skin != skin)
    {
        AX_SAFE_RETAIN(skin);
        AX_SAFE_RELEASE(_skin);
        _skin = skin;
        calculateAABB();
    }
}

void Mesh::setMeshIndexData(MeshIndexData* subMesh)
{
    if (_meshIndexData != subMesh)
    {
        AX_SAFE_RETAIN(subMesh);
        AX_SAFE_RELEASE(_meshIndexData);
        _meshIndexData = subMesh;
        calculateAABB();
        bindMeshCommand();
    }
}

void Mesh::setProgramState(rhi::ProgramState* programState)
{
    auto material = Material::createWithProgramState(programState);
    if (_material)
    {
        material->setStateBlock(_material->getStateBlock());
    }
    setMaterial(material);
}

rhi::ProgramState* Mesh::getProgramState() const
{
    return _material ? _material->_currentTechnique->_passes.at(0)->getProgramState() : nullptr;
}

void Mesh::calculateAABB()
{
    if (_meshIndexData)
    {
        _aabb = _meshIndexData->getAABB();
        if (_skin)
        {
            // get skin root
            Bone3D* root = nullptr;
            Mat4 invBindPose;
            if (_skin->_skinBones.size())
            {
                root = _skin->_skinBones.at(0);
                while (root)
                {
                    auto parent           = root->getParentBone();
                    bool parentInSkinBone = false;
                    for (const auto& bone : _skin->_skinBones)
                    {
                        if (bone == parent)
                        {
                            parentInSkinBone = true;
                            break;
                        }
                    }
                    if (!parentInSkinBone)
                        break;
                    root = parent;
                }
            }

            if (root)
            {
                _aabb.transform(root->getWorldMat() * _skin->getInvBindPose(root));
            }
        }
    }
}

void Mesh::bindMeshCommand()
{
    if (_material && _meshIndexData)
    {
        auto& stateBlock = _material->getStateBlock();
        if (const auto* stylized = dynamic_cast<const StylizedMaterial*>(_material))
            stateBlock.setCullFace(!stylized->_desc.doubleSided);
        else
            stateBlock.setCullFace(true);
        stateBlock.setDepthTest(true);
        if (_blend.src != rhi::BlendFactor::ONE && _blend.dst != rhi::BlendFactor::ONE)
            stateBlock.setBlend(true);
    }
}

void Mesh::setLightUniforms(Pass* pass, Scene* scene, const Vec4& color, unsigned int lightmask)
{
    AXASSERT(pass, "Invalid Pass");
    AXASSERT(scene, "Invalid scene");

    const auto& conf            = Environment::getInstance();
    constexpr int maxDirLight   = AX_MAX_DIRECTIONAL_LIGHT;
    constexpr int maxPointLight = AX_MAX_POINT_LIGHT;
    constexpr int maxSpotLight  = AX_MAX_SPOT_LIGHT;
    auto& lights                = scene->getLights();

    auto bindings = pass->getVertexAttributeBinding();

    if (bindings && bindings->hasAttribute(shaderinfos::VertexKey::VERTEX_ATTRIB_NORMAL))
    {
        resetLightUniformValues();

        int enabledDirLightNum   = 0;
        int enabledPointLightNum = 0;
        int enabledSpotLightNum  = 0;
        Vec3 ambientColor;
        for (const auto& light : lights)
        {
            bool useLight = light->isEnabled() && ((unsigned int)light->getLightFlag() & lightmask);
            if (useLight)
            {
                float intensity = light->getIntensity();
                switch (light->getLightType())
                {
                case LightType::DIRECTIONAL:
                {
                    if (enabledDirLightNum < maxDirLight)
                    {
                        auto dirLight = static_cast<DirectionLight*>(light);
                        Vec3 dir      = dirLight->getDirectionInWorld();
                        dir.normalize();
                        const Color32& col = dirLight->getDisplayedColor();
                        _dirLightUniformColorValues[enabledDirLightNum].set(
                            col.r / 255.0f * intensity, col.g / 255.0f * intensity, col.b / 255.0f * intensity);
                        _dirLightUniformDirValues[enabledDirLightNum] = dir;
                        ++enabledDirLightNum;
                    }
                }
                break;
                case LightType::POINT:
                {
                    if (enabledPointLightNum < maxPointLight)
                    {
                        auto pointLight    = static_cast<PointLight*>(light);
                        Mat4 mat           = pointLight->getNodeToWorldTransform();
                        const Color32& col = pointLight->getDisplayedColor();
                        _pointLightUniformColorValues[enabledPointLightNum].set(
                            col.r / 255.0f * intensity, col.g / 255.0f * intensity, col.b / 255.0f * intensity);
                        _pointLightUniformPositionValues[enabledPointLightNum].set(mat.m[12], mat.m[13], mat.m[14]);
                        _pointLightUniformRangeInverseValues[enabledPointLightNum] = 1.0f / pointLight->getRange();
                        ++enabledPointLightNum;
                    }
                }
                break;
                case LightType::SPOT:
                {
                    if (enabledSpotLightNum < maxSpotLight)
                    {
                        auto spotLight = static_cast<SpotLight*>(light);
                        Vec3 dir       = spotLight->getDirectionInWorld();
                        dir.normalize();
                        Mat4 mat           = light->getNodeToWorldTransform();
                        const Color32& col = spotLight->getDisplayedColor();
                        _spotLightUniformColorValues[enabledSpotLightNum].set(
                            col.r / 255.0f * intensity, col.g / 255.0f * intensity, col.b / 255.0f * intensity);
                        _spotLightUniformPositionValues[enabledSpotLightNum].set(mat.m[12], mat.m[13], mat.m[14]);
                        _spotLightUniformDirValues[enabledSpotLightNum]           = dir;
                        _spotLightUniformInnerAngleCosValues[enabledSpotLightNum] = spotLight->getCosInnerAngle();
                        _spotLightUniformOuterAngleCosValues[enabledSpotLightNum] = spotLight->getCosOuterAngle();
                        _spotLightUniformRangeInverseValues[enabledSpotLightNum]  = 1.0f / spotLight->getRange();
                        ++enabledSpotLightNum;
                    }
                }
                break;
                case LightType::AMBIENT:
                {
                    auto ambLight      = static_cast<AmbientLight*>(light);
                    const Color32& col = ambLight->getDisplayedColor();
                    ambientColor.add(col.r / 255.0f * intensity, col.g / 255.0f * intensity,
                                     col.b / 255.0f * intensity);
                }
                break;
                default:
                    break;
                }
            }
        }
        if (0 < maxDirLight)
        {
            pass->setUniformDirLightColor(&_dirLightUniformColorValues[0],
                                          _dirLightUniformColorValues.size() * sizeof(_dirLightUniformColorValues[0]));
            pass->setUniformDirLightDir(&_dirLightUniformDirValues[0],
                                        _dirLightUniformDirValues.size() * sizeof(_dirLightUniformDirValues[0]));
        }

        if (0 < maxPointLight)
        {
            pass->setUniformPointLightColor(
                &_pointLightUniformColorValues[0],
                _pointLightUniformColorValues.size() * sizeof(_pointLightUniformColorValues[0]));
            pass->setUniformPointLightPosition(
                &_pointLightUniformPositionValues[0],
                _pointLightUniformPositionValues.size() * sizeof(_pointLightUniformPositionValues[0]));
            pass->setUniformPointLightRangeInverse(
                &_pointLightUniformRangeInverseValues[0],
                _pointLightUniformRangeInverseValues.size() * sizeof(_pointLightUniformRangeInverseValues[0]));
        }

        if (0 < maxSpotLight)
        {
            pass->setUniformSpotLightColor(
                &_spotLightUniformColorValues[0],
                _spotLightUniformColorValues.size() * sizeof(_spotLightUniformColorValues[0]));
            pass->setUniformSpotLightPosition(
                &_spotLightUniformPositionValues[0],
                _spotLightUniformPositionValues.size() * sizeof(_spotLightUniformPositionValues[0]));
            pass->setUniformSpotLightDir(&_spotLightUniformDirValues[0],
                                         _spotLightUniformDirValues.size() * sizeof(_spotLightUniformDirValues[0]));
            pass->setUniformSpotLightInnerAngleCos(
                &_spotLightUniformInnerAngleCosValues[0],
                _spotLightUniformInnerAngleCosValues.size() * sizeof(_spotLightUniformInnerAngleCosValues[0]));
            pass->setUniformSpotLightOuterAngleCos(
                &_spotLightUniformOuterAngleCosValues[0],
                _spotLightUniformOuterAngleCosValues.size() * sizeof(_spotLightUniformOuterAngleCosValues[0]));
            pass->setUniformSpotLightRangeInverse(
                &_spotLightUniformRangeInverseValues[0],
                _spotLightUniformRangeInverseValues.size() * sizeof(_spotLightUniformRangeInverseValues[0]));
        }

        auto ambientLightColor = Vec3(ambientColor.x, ambientColor.y, ambientColor.z);
        pass->setUniformAmbientLigthColor(&ambientLightColor, sizeof(ambientLightColor));
    }
    else  // normal does not exist
    {
        Vec3 ambient(0.0f, 0.0f, 0.0f);
        bool hasAmbient = false;
        for (const auto& light : lights)
        {
            if (light->getLightType() == LightType::AMBIENT)
            {
                bool useLight = light->isEnabled() && ((unsigned int)light->getLightFlag() & lightmask);
                if (useLight)
                {
                    hasAmbient         = true;
                    const Color32& col = light->getDisplayedColor();
                    ambient.x += col.r * light->getIntensity();
                    ambient.y += col.g * light->getIntensity();
                    ambient.z += col.b * light->getIntensity();
                }
            }
        }
        if (hasAmbient)
        {
            ambient.x /= 255.f;
            ambient.y /= 255.f;
            ambient.z /= 255.f;
            // override the uniform value of u_color using the calculated color
            auto fcolor = Vec4(color.x * ambient.x, color.y * ambient.y, color.z * ambient.z, color.w);
            pass->setUniformColor(&fcolor, sizeof(fcolor));
        }
    }
}

void Mesh::setStylizedLightUniforms(Pass* pass,
                                    Scene* scene,
                                    const StylizedMaterial& material,
                                    unsigned int lightmask,
                                    const Mat4& transform)
{
    AXASSERT(pass, "Invalid Pass");

    struct PointCandidate
    {
        float score = 0.0F;
        Vec4 positionAndInverseRange{};
        Vec4 color{};
    };

    std::array<Vec4, 9> lightData{};
    lightData[0].set(0.0F, -1.0F, 0.0F, 0.0F);

    if (const auto* camera = Camera::getVisitingCamera())
    {
        const auto cameraTransform = camera->getNodeToWorldTransform();
        lightData[3].set(cameraTransform.m[12], cameraTransform.m[13], cameraTransform.m[14], 1.0F);
        Vec3 cameraForward;
        cameraTransform.getForwardVector(&cameraForward);
        if (cameraForward.lengthSquared() > 1.0e-8F)
            cameraForward.normalize();
        else
            cameraForward.set(0.0F, 0.0F, -1.0F);
        lightData[4].set(cameraForward.x, cameraForward.y, cameraForward.z, 0.0F);
    }
    else
    {
        lightData[3].set(transform.m[12], transform.m[13], transform.m[14] + 1.0F, 1.0F);
        lightData[4].set(0.0F, 0.0F, -1.0F, 0.0F);
    }

    if (!scene)
    {
        pass->setUniformStylizedLighting(lightData.data(), sizeof(lightData));
        return;
    }

    std::array<PointCandidate, 2> pointCandidates{};
    const Vec3 objectPosition{transform.m[12], transform.m[13], transform.m[14]};
    const auto* stylizedRenderer = StylizedRenderer::get(*scene);
    if (stylizedRenderer &&
        stylizedRenderer->fillStylizedLightData(lightData, lightmask, objectPosition, material.getMaximumPointLights()))
    {
        pass->setUniformStylizedLighting(lightData.data(), sizeof(lightData));
        return;
    }

    const auto linearChannel = [](uint8_t channel) {
        const float value = static_cast<float>(channel) * (1.0F / 255.0F);
        return value <= 0.04045F ? value * (1.0F / 12.92F) : std::pow((value + 0.055F) * (1.0F / 1.055F), 2.4F);
    };
    const auto linearColor = [&linearChannel](const BaseLight& light) {
        const Color32& color  = light.getDisplayedColor();
        const float intensity = light.getIntensity();
        return Vec3{linearChannel(color.r) * intensity, linearChannel(color.g) * intensity,
                    linearChannel(color.b) * intensity};
    };
    DirectionLight* selectedMainLight = stylizedRenderer ? stylizedRenderer->resolveMainLight(lightmask) : nullptr;

    if (!selectedMainLight)
    {
        for (auto* light : scene->getLights())
        {
            if (light->getLightType() == LightType::DIRECTIONAL && light->isEnabled() &&
                (static_cast<unsigned int>(light->getLightFlag()) & lightmask) != 0U)
            {
                selectedMainLight = static_cast<DirectionLight*>(light);
                break;
            }
        }
    }

    if (selectedMainLight)
    {
        Vec3 direction = selectedMainLight->getDirectionInWorld();
        direction.normalize();
        const Vec3 color = linearColor(*selectedMainLight);
        lightData[0].set(direction.x, direction.y, direction.z, 1.0F);
        lightData[1].set(color.x, color.y, color.z, 1.0F);
    }

    for (auto* light : scene->getLights())
    {
        if (!light->isEnabled() || !(static_cast<unsigned int>(light->getLightFlag()) & lightmask))
            continue;

        const Vec3 color = linearColor(*light);

        switch (light->getLightType())
        {
        case LightType::DIRECTIONAL:
            if (light == selectedMainLight)
            {
                auto* directional = static_cast<DirectionLight*>(light);
                Vec3 direction    = directional->getDirectionInWorld();
                direction.normalize();
                lightData[0].set(direction.x, direction.y, direction.z, 1.0F);
                lightData[1].set(color.x, color.y, color.z, 1.0F);
            }
            break;

        case LightType::AMBIENT:
            lightData[2].x += color.x;
            lightData[2].y += color.y;
            lightData[2].z += color.z;
            break;

        case LightType::POINT:
        {
            if (material.getMaximumPointLights() == 0)
                break;

            auto* point       = static_cast<PointLight*>(light);
            const float range = point->getRange();
            if (range <= 0.0F)
                break;

            const auto pointTransform = point->getNodeToWorldTransform();
            const Vec3 pointPosition{pointTransform.m[12], pointTransform.m[13], pointTransform.m[14]};
            const Vec3 toLight                    = pointPosition - objectPosition;
            const float normalizedDistanceSquared = toLight.lengthSquared() / (range * range);
            if (normalizedDistanceSquared >= 1.0F)
                break;

            const float attenuation = 1.0F - normalizedDistanceSquared;
            const float luminance   = color.x * 0.2126F + color.y * 0.7152F + color.z * 0.0722F;
            PointCandidate candidate;
            candidate.score = luminance * attenuation * attenuation;
            candidate.positionAndInverseRange.set(pointPosition.x, pointPosition.y, pointPosition.z, 1.0F / range);
            candidate.color.set(color.x, color.y, color.z, 1.0F);

            for (auto& selected : pointCandidates)
            {
                if (candidate.score > selected.score)
                    std::swap(candidate, selected);
            }
            break;
        }

        default:
            break;
        }
    }

    const uint8_t maximumPointLights =
        stylizedRenderer ? std::min<uint8_t>(material.getMaximumPointLights(),
                                             stylizedRenderer->getResolvedQuality().maximumPointLights)
                         : material.getMaximumPointLights();
    for (uint8_t index = 0; index < maximumPointLights; ++index)
    {
        lightData[5 + index * 2] = pointCandidates[index].positionAndInverseRange;
        lightData[6 + index * 2] = pointCandidates[index].color;
    }

    pass->setUniformStylizedLighting(lightData.data(), sizeof(lightData));
}

void Mesh::setBlendFunc(const BlendFunc& blendFunc)
{
    // Blend must be saved for future use
    // it doesn't matter if the material is already set or not
    // This functionality is added for compatibility issues
    if (_blend != blendFunc)
    {
        _blendDirty = true;
        _blend      = blendFunc;
    }

    if (_material)
    {
        // TODO set blend to Pass
        _material->getStateBlock().setBlendFunc(blendFunc);
        bindMeshCommand();
    }
}

const BlendFunc& Mesh::getBlendFunc() const
{
    // return _material->_currentTechnique->_passes.at(0)->getBlendFunc();
    return _blend;
}

CustomCommand::PrimitiveType Mesh::getPrimitiveType() const
{
    return _meshIndexData->getPrimitiveType();
}

ssize_t Mesh::getIndexCount() const
{
    return _meshIndexData->getIndexBuffer()->getSize() / IndexArray::formatToStride(meshIndexFormat);
}

CustomCommand::IndexFormat Mesh::getIndexFormat() const
{
    return meshIndexFormat;
}

void Mesh::setIndexFormat(CustomCommand::IndexFormat indexFormat)
{
    meshIndexFormat = indexFormat;
}

rhi::Buffer* Mesh::getIndexBuffer() const
{
    return _meshIndexData->getIndexBuffer();
}
}  // namespace ax
