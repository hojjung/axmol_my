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

#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/MeshSkin.h"
#if defined(AX_ENABLE_LEGACY_3D) && AX_ENABLE_LEGACY_3D
#    include "axmol/3d/Bundle3D.h"
#endif
#include "axmol/3d/MeshMaterial.h"
#include "axmol/3d/AttachNode.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
#    include "axmol/3d/GltfLoader.h"
#endif

#include "axmol/base/Director.h"
#include "axmol/base/text_utils.h"
#include "axmol/base/Utils.h"
#include "axmol/2d/Light.h"
#include "axmol/scene/Camera.h"
#include "axmol/base/Macros.h"
#include "axmol/platform/PlatformMacros.h"
#include "axmol/platform/FileUtils.h"
#include "axmol/renderer/TextureCache.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/Material.h"
#include "axmol/renderer/Technique.h"
#include "axmol/renderer/Pass.h"

#include <algorithm>

namespace ax
{

static MeshMaterial* getMeshRendererMaterialForAttribs(MeshVertexData* meshVertexData, bool usesLight);

MeshRenderer* MeshRenderer::create()
{
    auto mesh = new MeshRenderer();
    if (mesh->init())
    {
        mesh->autorelease();
        return mesh;
    }
    AX_SAFE_DELETE(mesh);
    return nullptr;
}

MeshRenderer* MeshRenderer::create(std::string_view modelPath)
{
    return create(modelPath, tlx::empty_sv);
}

MeshRenderer* MeshRenderer::create(std::string_view modelPath, std::string_view texturePath)
{
    AXASSERT(modelPath.length() >= 4, "Invalid filename.");

    auto meshRenderer = new MeshRenderer();
    if (meshRenderer->initWithFile(modelPath))
    {
        meshRenderer->setModelTexture(modelPath, texturePath);
        meshRenderer->_contentSize = meshRenderer->getBoundingBox().size;
        meshRenderer->autorelease();
        return meshRenderer;
    }
    AX_SAFE_DELETE(meshRenderer);
    return nullptr;
}

void MeshRenderer::createAsync(std::string_view modelPath,
                               const std::function<void(MeshRenderer*, void*)>& callback,
                               void* callbackparam)
{
    createAsync(modelPath, "", callback, callbackparam);
}

void MeshRenderer::createAsync(std::string_view modelPath,
                               std::string_view texturePath,
                               const std::function<void(MeshRenderer*, void*)>& callback,
                               void* callbackparam)
{
    MeshRenderer* meshRenderer = new MeshRenderer();
#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
    meshRenderer->_preferStylizedMaterial = GltfLoader::isGltfPath(modelPath);
    meshRenderer->_isImportedAssetRoot    = meshRenderer->_preferStylizedMaterial;
#endif
    if (meshRenderer->loadFromCache(modelPath))
    {
        meshRenderer->autorelease();
        meshRenderer->setModelTexture(modelPath, texturePath);
        callback(meshRenderer, callbackparam);
        return;
    }

    meshRenderer->_asyncLoadParam.afterLoadCallback = callback;
    meshRenderer->_asyncLoadParam.texPath           = texturePath;
    meshRenderer->_asyncLoadParam.modelPath         = modelPath;
    meshRenderer->_asyncLoadParam.modelFullPath     = FileUtils::getInstance()->fullPathForFilename(modelPath);
    meshRenderer->_asyncLoadParam.callbackParam     = callbackparam;
    meshRenderer->_asyncLoadParam.materialdatas     = new MaterialDatas();
    meshRenderer->_asyncLoadParam.meshdatas         = new MeshDatas();
    meshRenderer->_asyncLoadParam.nodeDatas         = new NodeDatas();

    auto director = Director::getInstance();
    director->runAsync([meshRenderer] {
        auto& loadParam  = meshRenderer->_asyncLoadParam;
        loadParam.result = meshRenderer->loadFromFile(loadParam.modelFullPath, loadParam.nodeDatas, loadParam.meshdatas,
                                                      loadParam.materialdatas);
    }, [meshRenderer] { meshRenderer->afterAsyncLoad(&meshRenderer->_asyncLoadParam); });
}

void MeshRenderer::afterAsyncLoad(void* param)
{
    MeshRenderer::AsyncLoadParam* asyncParam = (MeshRenderer::AsyncLoadParam*)param;
    autorelease();
    if (asyncParam)
    {
        if (asyncParam->result)
        {
            _meshes.clear();
            _meshVertexDatas.clear();
            AX_SAFE_RELEASE_NULL(_skeleton);
            removeAllAttachNode();

            // create in the main thread
            auto& meshdatas     = asyncParam->meshdatas;
            auto& materialdatas = asyncParam->materialdatas;
            auto& nodeDatas     = asyncParam->nodeDatas;
            if (initFrom(*nodeDatas, *meshdatas, *materialdatas))
            {
                auto meshdata = MeshDataCache::getInstance()->getMeshRenderData(asyncParam->modelPath);
                if (meshdata == nullptr)
                {
                    // add to cache
                    auto data             = new MeshDataCache::MeshRenderData();
                    data->materialdatas   = materialdatas;
                    data->nodedatas       = nodeDatas;
                    data->meshVertexDatas = _meshVertexDatas;
                    for (const auto mesh : _meshes)
                    {
                        data->programStates.pushBack(mesh->getProgramState());
                    }

                    MeshDataCache::getInstance()->addMeshRenderData(asyncParam->modelPath, data);

                    AX_SAFE_DELETE(meshdatas);
                    materialdatas = nullptr;
                    nodeDatas     = nullptr;
                }
            }
            AX_SAFE_DELETE(meshdatas);
            AX_SAFE_DELETE(materialdatas);
            AX_SAFE_DELETE(nodeDatas);

            setModelTexture(asyncParam->modelPath, asyncParam->texPath);
        }
        else
        {
            AXLOGW("file load failed: {}\n", asyncParam->modelPath);
        }
        asyncParam->afterLoadCallback(this, asyncParam->callbackParam);
    }
}

AABB MeshRenderer::getAABBRecursivelyImp(Node* node)
{
    AABB aabb;
    for (auto&& iter : node->getChildren())
    {
        aabb.merge(getAABBRecursivelyImp(iter));
    }

    MeshRenderer* meshRenderer = dynamic_cast<MeshRenderer*>(node);
    if (meshRenderer)
        aabb.merge(meshRenderer->getAABB());

    return aabb;
}

bool MeshRenderer::loadFromCache(std::string_view path)
{
    auto* meshDataCache = MeshDataCache::getInstance();
    auto* meshdata      = meshDataCache->getMeshRenderData(path);
    if (meshdata)
    {
        for (auto&& it : meshdata->meshVertexDatas)
        {
            _meshVertexDatas.pushBack(it);
        }
        _skeleton = Skeleton3D::create(meshdata->nodedatas->skeleton);
        AX_SAFE_RETAIN(_skeleton);

        const bool singleMesh = !_preferStylizedMaterial && meshdata->nodedatas->nodes.size() == 1;
        for (const auto& it : meshdata->nodedatas->nodes)
        {
            if (it)
            {
                if (!createNode(it, this, *(meshdata->materialdatas), singleMesh))
                    goto cache_load_failed;
            }
        }

        for (const auto& it : meshdata->nodedatas->skeleton)
        {
            if (it)
            {
                if (!createAttachMeshRendererNode(it, *(meshdata->materialdatas)))
                    goto cache_load_failed;
            }
        }

        if (!_preferStylizedMaterial)
        {
            for (ssize_t i = 0, size = _meshes.size(); i < size; ++i)
            {
                // cloning is needed in order to have one state per mesh
                auto ps       = meshdata->programStates.at(i);
                auto clonedPS = ps->clone();
                _meshes.at(i)->setProgramState(clonedPS);
                clonedPS->release();
            }
        }
        return true;

    cache_load_failed:
        AXLOGW("MeshRenderer: cached model '{}' could not recreate its material resources; reparsing source", path);
        _meshes.clear();
        _meshVertexDatas.clear();
        AX_SAFE_RELEASE_NULL(_skeleton);
        removeAllAttachNode();
        removeAllChildren();
        meshDataCache->removeMeshRenderData(path);
    }

    return false;
}

bool MeshRenderer::loadFromFile(std::string_view path,
                                NodeDatas* nodedatas,
                                MeshDatas* meshdatas,
                                MaterialDatas* materialdatas)
{
    std::string fullPath = FileUtils::getInstance()->fullPathForFilename(path);

    std::string ext = FileUtils::getPathExtension(path);
#if defined(AX_ENABLE_LEGACY_3D) && AX_ENABLE_LEGACY_3D
    if (ext == ".obj")
    {
        return Bundle3D::loadObj(*meshdatas, *materialdatas, *nodedatas, fullPath);
    }
    else if (ext == ".c3b" || ext == ".c3t")
    {
        // load from .c3b or .c3t
        auto bundle = Bundle3D::createBundle();
        if (!bundle->load(fullPath))
        {
            Bundle3D::destroyBundle(bundle);
            return false;
        }

        auto ret =
            bundle->loadMeshDatas(*meshdatas) && bundle->loadMaterials(*materialdatas) && bundle->loadNodes(*nodedatas);
        Bundle3D::destroyBundle(bundle);

        return ret;
    }
#endif
#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
    if (GltfLoader::isGltfPath(path))
    {
        _preferStylizedMaterial = true;
        _isImportedAssetRoot    = true;
        return GltfLoader::load(fullPath, *nodedatas, *meshdatas, *materialdatas);
    }
#endif
    return false;
}

MeshRenderer::MeshRenderer()
    : _skeleton(nullptr)
    , _blend(BlendFunc::ALPHA_NON_PREMULTIPLIED)
    , _lightMask(-1)
    , _aabbDirty(true)
    , _shaderUsingLight(false)
    , _forceDepthWrite(false)
    , _wireframe(false)
    , _castShadow(true)
    , _receiveShadow(true)
    , _skeletonPosePreparedForVisit(false)
    , _preferStylizedMaterial(false)
    , _isImportedAssetRoot(false)
    , _usingAutogeneratedProgram(true)
    , _transparentMaterialHint(false)
    , _meshTextureHint(0)
{}

MeshRenderer::~MeshRenderer()
{
    _meshes.clear();
    _meshVertexDatas.clear();
    AX_SAFE_RELEASE_NULL(_skeleton);
    removeAllAttachNode();
}

bool MeshRenderer::init()
{
    if (Node::init())
    {
        return true;
    }
    return false;
}

bool MeshRenderer::initWithFile(std::string_view path)
{
    _aabbDirty = true;
#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
    _preferStylizedMaterial = GltfLoader::isGltfPath(path);
    _isImportedAssetRoot    = _preferStylizedMaterial;
#else
    _preferStylizedMaterial = false;
    _isImportedAssetRoot    = false;
#endif
    _meshes.clear();
    _meshVertexDatas.clear();
    AX_SAFE_RELEASE_NULL(_skeleton);
    removeAllAttachNode();

    if (loadFromCache(path))
        return true;

    MeshDatas* meshdatas         = new MeshDatas();
    MaterialDatas* materialdatas = new MaterialDatas();
    NodeDatas* nodeDatas         = new NodeDatas();
    if (loadFromFile(path, nodeDatas, meshdatas, materialdatas))
    {
        if (initFrom(*nodeDatas, *meshdatas, *materialdatas))
        {
            // add to cache
            auto data             = new MeshDataCache::MeshRenderData();
            data->materialdatas   = materialdatas;
            data->nodedatas       = nodeDatas;
            data->meshVertexDatas = _meshVertexDatas;
            for (const auto mesh : _meshes)
            {
                data->programStates.pushBack(mesh->getProgramState());
            }

            MeshDataCache::getInstance()->addMeshRenderData(path, data);
            AX_SAFE_DELETE(meshdatas);
            _contentSize = getBoundingBox().size;
            return true;
        }
    }
    AX_SAFE_DELETE(meshdatas);
    AX_SAFE_DELETE(materialdatas);
    AX_SAFE_DELETE(nodeDatas);

    return false;
}

bool MeshRenderer::initFrom(const NodeDatas& nodeDatas, const MeshDatas& meshdatas, const MaterialDatas& materialdatas)
{
    for (const auto& it : meshdatas.meshDatas)
    {
        if (it)
        {
            //            Mesh* mesh = Mesh::create(*it);
            //            _meshes.pushBack(mesh);
            auto meshvertex = MeshVertexData::create(*it);
            _meshVertexDatas.pushBack(meshvertex);
        }
    }
    _skeleton = Skeleton3D::create(nodeDatas.skeleton);
    AX_SAFE_RETAIN(_skeleton);

    const bool singleMesh = !_preferStylizedMaterial && nodeDatas.nodes.size() == 1;
    for (const auto& it : nodeDatas.nodes)
    {
        if (it)
        {
            if (!createNode(it, this, materialdatas, singleMesh))
                return false;
        }
    }
    for (const auto& it : nodeDatas.skeleton)
    {
        if (it)
        {
            if (!createAttachMeshRendererNode(it, materialdatas))
                return false;
        }
    }
    genMaterial();

    return true;
}

MeshRenderer* MeshRenderer::createMeshRendererNode(NodeData* nodedata,
                                                   ModelData* modeldata,
                                                   const MaterialDatas& materialdatas)
{
    auto meshRenderer                     = new MeshRenderer();
    meshRenderer->_preferStylizedMaterial = _preferStylizedMaterial;

    meshRenderer->setName(nodedata->id);
    auto* meshIndexData = getMeshIndexData(modeldata->subMeshId);
    auto* mesh          = meshIndexData ? Mesh::create(nodedata->id, meshIndexData) : nullptr;
    if (!mesh)
    {
        delete meshRenderer;
        return nullptr;
    }

    if (_skeleton && !modeldata->bones.empty())
    {
        auto skin = MeshSkin::create(_skeleton, modeldata->bones, modeldata->invBindPose);
        mesh->setSkin(skin);
    }

    const NMaterialData* materialData = materialdatas.getMaterialData(modeldata->materialId);
    if (!materialData && !_preferStylizedMaterial && modeldata->materialId.empty() && !materialdatas.materials.empty())
        materialData = &materialdatas.materials.front();
    if (!applyMaterialData(mesh, materialData))
    {
        delete meshRenderer;
        return nullptr;
    }

    // set locale transform
    Vec3 pos;
    Quat qua;
    Vec3 scale;
    nodedata->transform.decompose(&scale, &qua, &pos);
    meshRenderer->setPosition3D(pos);
    meshRenderer->setRotationQuat(qua);
    meshRenderer->setScaleX(scale.x);
    meshRenderer->setScaleY(scale.y);
    meshRenderer->setScaleZ(scale.z);

    meshRenderer->addMesh(mesh);
    meshRenderer->autorelease();
    meshRenderer->genMaterial();

    return meshRenderer;
}
bool MeshRenderer::createAttachMeshRendererNode(NodeData* nodedata, const MaterialDatas& materialdatas)
{
    for (const auto& it : nodedata->modelNodeDatas)
    {
        if (it && getAttachNode(nodedata->id))
        {
            auto mesh = createMeshRendererNode(nodedata, it, materialdatas);
            if (!mesh)
                return false;
            getAttachNode(nodedata->id)->addChild(mesh);
        }
    }
    for (const auto& it : nodedata->children)
    {
        if (!createAttachMeshRendererNode(it, materialdatas))
            return false;
    }
    return true;
}

void MeshRenderer::collectMeshRenderers(Node* node, std::vector<MeshRenderer*>& renderers)
{
    if (!node)
        return;

    if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(node))
        renderers.emplace_back(meshRenderer);

    for (auto* child : node->getChildren())
        collectMeshRenderers(child, renderers);
}

void MeshRenderer::collectMeshRenderers(const Node* node, std::vector<const MeshRenderer*>& renderers)
{
    if (!node)
        return;

    if (auto* meshRenderer = dynamic_cast<const MeshRenderer*>(node))
        renderers.emplace_back(meshRenderer);

    for (const auto* child : node->getChildren())
        collectMeshRenderers(child, renderers);
}

void MeshRenderer::setMaterial(Material* material)
{
    setMaterial(material, -1);
}

void MeshRenderer::setMaterial(Material* material, int meshIndex)
{
    AXASSERT(material, "Invalid Material");
    const ssize_t meshCount = getMeshCount();
    AXASSERT(meshIndex == -1 || (meshIndex >= 0 && meshIndex < meshCount), "Invalid meshIndex.");
    if (!material || (meshIndex != -1 && (meshIndex < 0 || meshIndex >= meshCount)))
        return;

    bool sourceMaterialApplied = false;
    const auto materialForMesh = [material, &sourceMaterialApplied](Mesh* mesh) -> Material* {
        if (auto* stylized = material->asStylizedMaterial())
        {
            const bool meshIsSkinned = mesh->getSkin() != nullptr;
            if (stylized->isSkinned() != meshIsSkinned)
                return stylized->cloneForSkinning(meshIsSkinned);
        }

        if (!sourceMaterialApplied)
        {
            sourceMaterialApplied = true;
            return material;
        }
        return material->clone();
    };

    if (_isImportedAssetRoot)
    {
        std::vector<MeshRenderer*> renderers;
        collectMeshRenderers(this, renderers);
        ssize_t flattenedIndex = 0;
        bool materialApplied   = false;
        for (auto* renderer : renderers)
        {
            for (auto* mesh : renderer->_meshes)
            {
                if (meshIndex == -1 || flattenedIndex == meshIndex)
                {
                    auto* resolvedMaterial = materialForMesh(mesh);
                    if (!resolvedMaterial)
                    {
                        AXLOGE("MeshRenderer: failed to create a stylized skinning variant");
                        return;
                    }
                    mesh->setMaterial(resolvedMaterial);
                    renderer->_usingAutogeneratedProgram = false;
                    if (meshIndex != -1)
                    {
                        materialApplied = true;
                        break;
                    }
                }
                ++flattenedIndex;
            }
            if (materialApplied)
                break;
        }
        _usingAutogeneratedProgram = false;
        return;
    }

    if (meshIndex == -1)
    {
        for (ssize_t i = 0, size = _meshes.size(); i < size; ++i)
        {
            auto* mesh             = _meshes.at(i);
            auto* resolvedMaterial = materialForMesh(mesh);
            if (!resolvedMaterial)
            {
                AXLOGE("MeshRenderer: failed to create a stylized skinning variant");
                return;
            }
            mesh->setMaterial(resolvedMaterial);
        }
    }
    else
    {
        auto mesh              = _meshes.at(meshIndex);
        auto* resolvedMaterial = materialForMesh(mesh);
        if (!resolvedMaterial)
        {
            AXLOGE("MeshRenderer: failed to create a stylized skinning variant");
            return;
        }
        mesh->setMaterial(resolvedMaterial);
    }

    _usingAutogeneratedProgram = false;
}

Material* MeshRenderer::getSubMeshMaterial(size_t subMeshIndex, size_t materialIndex)
{
    if (subMeshIndex < getChildren().size())
    {
        auto child = dynamic_cast<MeshRenderer*>(getChildren().at(subMeshIndex));
        if (child)
            return child->getMaterial(materialIndex);
    }
    return nullptr;
}

Material* MeshRenderer::getMaterial(int meshIndex) const
{
    auto* mesh = getMeshByIndex(meshIndex);
    return mesh ? mesh->getMaterial() : nullptr;
}

void MeshRenderer::setForceDepthWrite(bool value)
{
    if (!_isImportedAssetRoot)
    {
        _forceDepthWrite = value;
        return;
    }
    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        renderer->_forceDepthWrite = value;
}

void MeshRenderer::setLightMask(unsigned int mask)
{
    if (!_isImportedAssetRoot)
    {
        _lightMask = mask;
        return;
    }
    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        renderer->_lightMask = mask;
}

void MeshRenderer::setCastShadow(bool value)
{
    if (!_isImportedAssetRoot)
    {
        _castShadow = value;
        return;
    }
    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        renderer->_castShadow = value;
}

void MeshRenderer::setReceiveShadow(bool value)
{
    if (!_isImportedAssetRoot)
    {
        _receiveShadow = value;
        return;
    }
    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        renderer->_receiveShadow = value;
}

void MeshRenderer::setWireframe(bool value)
{
    if (!_isImportedAssetRoot)
    {
        _wireframe = value;
        return;
    }
    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        renderer->_wireframe = value;
}

void MeshRenderer::genMaterial(bool useLight)
{
    _shaderUsingLight = useLight;

#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
    if (_preferStylizedMaterial)
    {
        for (auto&& mesh : _meshes)
        {
            if (mesh->getStylizedMaterial())
                continue;

            StylizedMaterialDesc desc;
            desc.baseTexture = mesh->getTexture(NTextureData::Usage::Diffuse);
            auto* material   = StylizedMaterial::create(desc, mesh->getSkin() != nullptr);
            AXASSERT(material, "stylized material should not be null.");
            if (!material)
                continue;

            if (auto* oldMaterial = mesh->getMaterial())
            {
                material->setStateBlock(oldMaterial->getStateBlock());
                material->setTransparent(oldMaterial->isTransparent());
            }
            mesh->setMaterial(material);
        }
        return;
    }
#endif

    std::unordered_map<const MeshVertexData*, MeshMaterial*> materials;
    for (auto&& meshVertexData : _meshVertexDatas)
    {
        auto material = getMeshRendererMaterialForAttribs(meshVertexData, useLight);
        AXASSERT(material, "material should cannot be null.");
        materials[meshVertexData] = material;
    }

    for (auto&& mesh : _meshes)
    {
        auto material = materials[mesh->getMeshIndexData()->getMeshVertexData()];
        material->setTransparent(_transparentMaterialHint);
        // keep original state block if exist
        auto oldmaterial = mesh->getMaterial();
        if (oldmaterial)
        {
            material->setStateBlock(oldmaterial->getStateBlock());
            material->setTransparent(oldmaterial->isTransparent());
        }

        if (material->getReferenceCount() == 1)
            mesh->setMaterial(material);
        else
            mesh->setMaterial(material->clone());
    }
}

bool MeshRenderer::createNode(NodeData* nodedata, Node* root, const MaterialDatas& materialdatas, bool singleMesh)
{
    const auto applyNodeTransform = [nodedata](Node& target) {
        Vec3 position;
        Quat rotation;
        Vec3 scale;
        nodedata->transform.decompose(&scale, &rotation, &position);
        target.setPosition3D(position);
        target.setRotationQuat(rotation);
        target.setScaleX(scale.x);
        target.setScaleY(scale.y);
        target.setScaleZ(scale.z);
    };

    Node* node = nullptr;
    for (const auto& it : nodedata->modelNodeDatas)
    {
        if (it)
        {
            if (!it->bones.empty() || singleMesh)
            {
                const bool usesSkeletonSpace = it->skinningInSkeletonSpace && !it->bones.empty();
                if (singleMesh && root != nullptr && !usesSkeletonSpace)
                    root->setName(nodedata->id);
                auto* meshIndexData = getMeshIndexData(it->subMeshId);
                auto* mesh          = meshIndexData ? Mesh::create(nodedata->id, meshIndexData) : nullptr;
                if (mesh)
                {
                    if (_skeleton && it->bones.size())
                    {
                        auto skin = MeshSkin::create(_skeleton, it->bones, it->invBindPose);
                        mesh->setSkin(skin);
                    }
                    mesh->_visibleChanged = std::bind(&MeshRenderer::onAABBDirty, this);

                    const NMaterialData* materialData = materialdatas.getMaterialData(it->materialId);
                    if (!materialData && !_preferStylizedMaterial && it->materialId.empty() &&
                        !materialdatas.materials.empty())
                        materialData = &materialdatas.materials.front();
                    if (!applyMaterialData(mesh, materialData))
                        return false;
                    _meshes.pushBack(mesh);

                    if (usesSkeletonSpace)
                    {
                        if (!node)
                        {
                            node = Node::create();
                            if (!node)
                                return false;
                            node->setName(nodedata->id);
                            applyNodeTransform(*node);
                            if (root)
                                root->addChild(node);
                        }
                    }
                    else
                    {
                        applyNodeTransform(*this);
                        node = this;
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                auto mesh = createMeshRendererNode(nodedata, it, materialdatas);
                if (mesh)
                {
                    if (root)
                    {
                        root->addChild(mesh);
                    }
                }
                else
                {
                    return false;
                }
                node = mesh;
            }
        }
    }
    if (nodedata->modelNodeDatas.size() == 0)
    {
        node = Node::create();
        if (node)
        {
            node->setName(nodedata->id);

            applyNodeTransform(*node);

            if (root)
            {
                root->addChild(node);
            }
        }
    }

    const bool singleChildMesh = !_preferStylizedMaterial && nodedata->children.size() == 1;
    for (const auto& it : nodedata->children)
    {
        if (!createNode(it, node, materialdatas, singleChildMesh))
            return false;
    }
    return true;
}

MeshIndexData* MeshRenderer::getMeshIndexData(std::string_view indexId) const
{
    for (auto&& it : _meshVertexDatas)
    {
        auto index = it->getMeshIndexDataById(indexId);
        if (index)
            return index;
    }
    return nullptr;
}

void MeshRenderer::addMesh(Mesh* mesh)
{
    auto meshVertex = mesh->getMeshIndexData()->_vertexData;
    _meshVertexDatas.pushBack(meshVertex);
    _meshes.pushBack(mesh);
}

Texture2D* MeshRenderer::setMeshTexture(Mesh* mesh, std::string_view texPath, NTextureData::Usage usage)
{
    auto tex = _director->getTextureCache()->addImage(texPath);
    mesh->setTexture(texPath, usage);
    if (tex)
        ++_meshTextureHint;
    return tex;
}

Texture2D* MeshRenderer::setMeshTexture(Mesh* mesh, const NTextureData& textureData)
{
    auto* cache        = _director->getTextureCache();
    Texture2D* texture = textureData.cacheKey.empty() ? nullptr : cache->getTextureForKey(textureData.cacheKey);
    if (!texture && textureData.embeddedData && !textureData.embeddedData->isNull())
        texture = cache->addImage(textureData.embeddedData, textureData.cacheKey, textureData.colorSpace);
    if (!texture && !textureData.filename.empty())
    {
        // glTF sampler state belongs to a texture binding, not the image. Load
        // external encoded bytes under the sampler-qualified key so two glTF
        // textures that share an image cannot mutate one Texture2D last-wins.
        auto encoded = std::make_shared<const Data>(FileUtils::getInstance()->getDataFromFile(textureData.filename));
        if (!encoded->isNull())
            texture = cache->addImage(encoded, textureData.cacheKey, textureData.colorSpace);
    }
    if (!texture)
        return nullptr;

    Texture2D::TexParams texParams{};
    texParams.minFilter    = textureData.minFilter;
    texParams.magFilter    = textureData.magFilter;
    texParams.mipFilter    = textureData.mipFilter;
    texParams.sAddressMode = textureData.wrapS;
    texParams.tAddressMode = textureData.wrapT;
    texture->setTexParameters(texParams);
    mesh->setTexture(texture, textureData.type, false);
    ++_meshTextureHint;

    return texture;
}

bool MeshRenderer::applyMaterialData(Mesh* mesh, const NMaterialData* materialData)
{
    const NTextureData* diffuseTexture = nullptr;
    Texture2D* diffuse                 = nullptr;
    if (materialData)
    {
        diffuseTexture = materialData->getTextureData(NTextureData::Usage::Diffuse);
        if (diffuseTexture)
        {
            diffuse = setMeshTexture(mesh, *diffuseTexture);
            if (_preferStylizedMaterial && !diffuse)
            {
                AXLOGE("MeshRenderer: failed to create required glTF base-color texture '{}'",
                       diffuseTexture->cacheKey);
                return false;
            }
        }

        if (!_preferStylizedMaterial)
        {
            if (const NTextureData* normalTexture = materialData->getTextureData(NTextureData::Usage::Normal))
                setMeshTexture(mesh, *normalTexture);
            _transparentMaterialHint = materialData->getTextureData(NTextureData::Usage::Transparency) != nullptr;
        }
    }

    if (!_preferStylizedMaterial)
        return true;

    StylizedMaterialDesc desc;
    desc.baseTexture = diffuse;
    if (materialData)
    {
        desc.baseColor   = materialData->baseColor;
        desc.doubleSided = materialData->doubleSided;
        if (materialData->alphaMode == NMaterialData::AlphaMode::Mask)
            desc.alphaCutoff = materialData->alphaCutoff;
    }
    if (diffuseTexture)
    {
        desc.uvOffset   = diffuseTexture->offset;
        desc.uvScale    = diffuseTexture->scale;
        desc.uvRotation = diffuseTexture->rotation;
    }

    auto* material = StylizedMaterial::create(desc, mesh->getSkin() != nullptr);
    AXASSERT(material, "stylized material should not be null.");
    if (!material)
        return false;
    material->setTransparent(materialData && materialData->alphaMode == NMaterialData::AlphaMode::Blend);
    mesh->setMaterial(material);
    return true;
}

void MeshRenderer::setModelTexture(std::string_view modelPath, std::string_view texturePath)
{
    if (!texturePath.empty())
        setTexture(texturePath);
#if defined(AX_ENABLE_GLTF) && AX_ENABLE_GLTF
    else if (_preferStylizedMaterial)
    {
        // An absent glTF base-color texture is intentional and uses the
        // stylized material's white fallback; do not probe a legacy .png.
        return;
    }
#endif
    else if (!_meshTextureHint)
    {
        auto pos = modelPath.find_last_of('.');
        if (pos != std::string_view::npos)
        {
            std::string modelTexPath{modelPath};
            modelTexPath.resize(pos);
            modelTexPath.append(".png"sv);
            setTexture(modelTexPath);
        }
    }
}

void MeshRenderer::enableInstancing(MeshMaterial::InstanceMaterialType instanceMat, int count)
{
    switch (instanceMat)
    {
    case MeshMaterial::InstanceMaterialType::UNLIT_INSTANCE:
    {
        auto mat = MeshMaterial::createBuiltInMaterial(MeshMaterial::MaterialType::UNLIT_INSTANCE, false);
        enableInstancing(mat, count);
    }
    }
}

void MeshRenderer::enableInstancing(MeshMaterial* instanceMat, int count)
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
    {
        for (auto* mesh : renderer->_meshes)
        {
            mesh->enableInstancing(true, MAX(1, count));
            mesh->setMaterial(instanceMat);
        }
        renderer->_aabbDirty = true;
    }
}

void MeshRenderer::disableInstancing()
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
    {
        for (auto* mesh : renderer->_meshes)
            mesh->enableInstancing(false, 0);
        renderer->_aabbDirty = true;
    }
}

void MeshRenderer::setDynamicInstancing(bool dynamic)
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            mesh->setDynamicInstancing(dynamic);
}

void MeshRenderer::addInstanceChild(Node* child, bool active)
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    bool added = false;
    for (auto* renderer : renderers)
    {
        for (auto* mesh : renderer->_meshes)
        {
            mesh->addInstanceChild(child);
            added = true;
        }
        renderer->_aabbDirty = true;
    }
    if (!added)
        return;
    if (active)
        addChild(child);
    else
        child->setParent(this);
}

void MeshRenderer::shrinkToFitInstances()
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            mesh->shrinkToFitInstances();
}

void MeshRenderer::rebuildInstances()
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
    {
        for (auto* mesh : renderer->_meshes)
            mesh->rebuildInstances();
        renderer->_aabbDirty = true;
    }
}

void MeshRenderer::setTexture(std::string_view texFile)
{
    auto tex = _director->getTextureCache()->addImage(texFile);
    setTexture(tex);
}

void MeshRenderer::setTexture(Texture2D* texture)
{
    if (!_isImportedAssetRoot)
    {
        for (auto* mesh : _meshes)
            mesh->setTexture(texture);
        return;
    }

    std::vector<MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            mesh->setTexture(texture);
}
AttachNode* MeshRenderer::getAttachNode(std::string_view boneName)
{
    auto it = _attachments.find(boneName);
    if (it != _attachments.end())
        return it->second;

    if (_skeleton)
    {
        auto bone = _skeleton->getBoneByName(boneName);
        if (bone)
        {
            auto attachNode = AttachNode::create(bone);
            addChild(attachNode);
            _attachments.emplace(boneName, attachNode);  // _attachments[boneName] = attachNode;
            return attachNode;
        }
    }

    return nullptr;
}

void MeshRenderer::removeAttachNode(std::string_view boneName)
{
    auto it = _attachments.find(boneName);
    if (it != _attachments.end())
    {
        removeChild(it->second);
        _attachments.erase(it);
    }
}

void MeshRenderer::removeAllAttachNode()
{
    for (auto&& it : _attachments)
    {
        removeChild(it.second);
    }
    _attachments.clear();
}

void MeshRenderer::visit(ax::Renderer* renderer, const ax::Mat4& parentTransform, uint32_t parentFlags)
{
    // quick return if not visible. children won't be drawn.
    if (!_visible)
    {
        return;
    }

    uint32_t flags = processParentFlags(parentTransform, parentFlags);
    flags |= FLAGS_RENDER_AS_3D;

    bool visibleByCamera       = isVisitableByVisitingCamera();
    auto* scene                = getScene();
    auto* stylizedRenderer     = scene ? StylizedRenderer::get(*scene) : nullptr;
    const bool hasLocalMeshes  = !_meshes.empty();
    const bool shadowCandidate = stylizedRenderer && stylizedRenderer->isFrameActive() && _castShadow;
    if (_skeleton && (visibleByCamera || shadowCandidate))
    {
        _skeleton->updateBoneMatrix();
        _skeletonPosePreparedForVisit = true;
    }
    const AABB* worldBounds = hasLocalMeshes && (visibleByCamera || shadowCandidate) ? &getLocalAABB() : nullptr;
    if (hasLocalMeshes && visibleByCamera)
    {
        auto* camera = Camera::getVisitingCamera();
        if (camera && worldBounds && !worldBounds->isEmpty())
            visibleByCamera = camera->isVisibleInFrustum(worldBounds);
    }

    const uint8_t shadowCascadeMask =
        shadowCandidate && worldBounds ? stylizedRenderer->getShadowCascadeMask(*worldBounds) : 0;
    const bool visibleInShadow = shadowCascadeMask != 0;

    const auto visitSelf = [&]() {
        if (hasLocalMeshes && visibleByCamera)
            this->draw(renderer, _modelViewTransform, flags);

        if (visibleInShadow)
        {
            Color color(getDisplayedColor());
            color.a = getDisplayedOpacity() / 255.0F;
            stylizedRenderer->submitShadowCaster(*this, *renderer, shadowCascadeMask, _modelViewTransform,
                                                 Vec4{color.r, color.g, color.b, color.a});
        }
    };

    int i = 0;

    if (!_children.empty())
    {
        sortAllChildren();
        // draw children zOrder < 0
        for (auto size = _children.size(); i < size; i++)
        {
            auto node = _children.at(i);

            if (node && node->getLocalZOrder() < 0)
                node->visit(renderer, _modelViewTransform, flags);
            else
                break;
        }
        // self draw
        visitSelf();

        for (auto it = _children.cbegin() + i, itCend = _children.cend(); it != itCend; ++it)
            (*it)->visit(renderer, _modelViewTransform, flags);
    }
    else
    {
        visitSelf();
    }

    _skeletonPosePreparedForVisit = false;
}

void MeshRenderer::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    if (_skeleton && !_skeletonPosePreparedForVisit)
        _skeleton->updateBoneMatrix();

    Color color(getDisplayedColor());
    color.a = getDisplayedOpacity() / 255.0f;

    // check light and determine the shader used
    auto* scene = getScene();

    // Don't override ProgramState if using manually set Material
    if (_usingAutogeneratedProgram && scene)
    {
        const auto lights = scene->getLights();
        bool usingLight   = false;
        for (const auto light : lights)
        {
            usingLight = light->isEnabled() && ((static_cast<unsigned int>(light->getLightFlag()) & _lightMask) > 0);
            if (usingLight)
                break;
        }
        if (usingLight != _shaderUsingLight)
        {
            genMaterial(usingLight);
        }
    }

    for (auto&& mesh : _meshes)
    {
        if (auto* material = mesh->getStylizedMaterial())
        {
            if (auto* stylizedRenderer = scene ? StylizedRenderer::get(*scene) : nullptr)
                stylizedRenderer->configureMaterial(*material, _receiveShadow);
            else
                material->setMainShadowEnabled(false);
        }
        mesh->draw(renderer, _globalZOrder, transform, flags, _lightMask, Vec4(color.r, color.g, color.b, color.a),
                   _forceDepthWrite, _wireframe, scene);
    }
}

bool MeshRenderer::setProgramState(rhi::ProgramState* programState, bool ownPS /* = false*/)
{
    if (Node::setProgramState(programState, ownPS))
    {
        std::vector<MeshRenderer*> renderers;
        if (_isImportedAssetRoot)
            collectMeshRenderers(this, renderers);
        else
            renderers.emplace_back(this);
        for (auto* meshRenderer : renderers)
            for (auto* mesh : meshRenderer->_meshes)
                mesh->setProgramState(programState);
        return true;
    }
    return false;
}

void MeshRenderer::setBlendFunc(const BlendFunc& blendFunc)
{
    if (_blend.src != blendFunc.src || _blend.dst != blendFunc.dst)
    {
        _blend = blendFunc;
        std::vector<MeshRenderer*> renderers;
        if (_isImportedAssetRoot)
            collectMeshRenderers(this, renderers);
        else
            renderers.emplace_back(this);
        for (auto* meshRenderer : renderers)
            for (auto* mesh : meshRenderer->_meshes)
                mesh->setBlendFunc(blendFunc);
    }
}

const BlendFunc& MeshRenderer::getBlendFunc() const
{
    return _blend;
}

AABB MeshRenderer::getAABBRecursively()
{
    return getAABBRecursivelyImp(this);
}

const AABB& MeshRenderer::getAABB() const
{
    if (!_isImportedAssetRoot)
        return getLocalAABB();

    // Imported node animation can change child world transforms every frame,
    // so the asset aggregate is rebuilt from the child renderers instead of
    // reusing the local-mesh cache.
    _assetAabb = getLocalAABB();
    for (auto* child : getChildren())
        _assetAabb.merge(getAABBRecursivelyImp(child));
    return _assetAabb;
}

const AABB& MeshRenderer::getLocalAABB() const
{
    Mat4 nodeToWorldTransform(getNodeToWorldTransform());
    const bool hasInstancedMesh = std::any_of(_meshes.begin(), _meshes.end(),
                                              [](const Mesh* mesh) { return mesh->isVisible() && mesh->_instancing; });
    const bool hasSkinnedMesh =
        std::any_of(_meshes.begin(), _meshes.end(), [](const Mesh* mesh) { return mesh->isVisible() && mesh->_skin; });

    // If nodeToWorldTransform matrix isn't changed, we don't need to transform aabb.
    if (!hasInstancedMesh && !hasSkinnedMesh &&
        memcmp(_nodeToWorldTransform.m, nodeToWorldTransform.m, sizeof(Mat4)) == 0 && !_aabbDirty)
    {
        return _aabb;
    }
    else
    {
        _aabb.reset();
        if (_meshes.size())
        {
            Mat4 transform(nodeToWorldTransform);
            for (const auto& it : _meshes)
            {
                if (!it->isVisible())
                    continue;

                if (it->_skin && it->_meshIndexData)
                {
                    // A skinned vertex is a convex combination of its bone-
                    // transformed bind position. The union of the bind AABB
                    // transformed by every skin bone is therefore a
                    // conservative current-pose bound without touching all
                    // vertices on the CPU.
                    const AABB& bindBounds = it->_meshIndexData->getAABB();
                    AABB skinnedBounds;
                    for (ssize_t boneIndex = 0; boneIndex < it->_skin->getBoneCount(); ++boneIndex)
                    {
                        auto* bone = it->_skin->getBoneByIndex(static_cast<unsigned int>(boneIndex));
                        if (!bone)
                            continue;

                        AABB boneBounds = bindBounds;
                        boneBounds.transform(bone->getWorldMat() * it->_skin->getInvBindPose(bone));
                        skinnedBounds.merge(boneBounds);
                    }
                    _aabb.merge(skinnedBounds.isEmpty() ? bindBounds : skinnedBounds);
                }
                else if (it->_instancing)
                {
                    for (const auto* instance : it->_instances)
                    {
                        AABB instanceBounds = it->getAABB();
                        instanceBounds.transform(instance->getNodeToParentTransform());
                        _aabb.merge(instanceBounds);
                    }
                }
                else
                {
                    _aabb.merge(it->getAABB());
                }
            }

            _aabb.transform(transform);
            _nodeToWorldTransform = nodeToWorldTransform;
            _aabbDirty            = false;
        }
    }

    return _aabb;
}

Action* MeshRenderer::runAction(Action* action)
{
    setForceDepthWrite(true);
    return Node::runAction(action);
}

Rect MeshRenderer::getBoundingBox() const
{
    AABB aabb = getAABB();
    Rect ret(aabb._min.x, aabb._min.y, (aabb._max.x - aabb._min.x), (aabb._max.y - aabb._min.y));
    return ret;
}

void MeshRenderer::setCullFace(CullFaceSide side)
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            mesh->getMaterial()->getStateBlock().setCullFaceSide(side);
}

void MeshRenderer::setCullFaceEnabled(bool enable)
{
    std::vector<MeshRenderer*> renderers;
    if (_isImportedAssetRoot)
        collectMeshRenderers(this, renderers);
    else
        renderers.emplace_back(this);
    for (auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            mesh->getMaterial()->getStateBlock().setCullFace(enable);
}

Mesh* MeshRenderer::getMeshByIndex(int index) const
{
    const ssize_t meshCount = getMeshCount();
    AXASSERT(index >= 0 && index < meshCount, "Invalid index.");
    if (index < 0 || index >= meshCount)
        return nullptr;

    if (!_isImportedAssetRoot)
        return _meshes.at(index);

    std::vector<const MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    ssize_t flattenedIndex = index;
    for (const auto* renderer : renderers)
    {
        if (flattenedIndex < renderer->_meshes.size())
            return renderer->_meshes.at(flattenedIndex);
        flattenedIndex -= renderer->_meshes.size();
    }
    return nullptr;
}

ssize_t MeshRenderer::getMeshCount() const
{
    if (!_isImportedAssetRoot)
        return _meshes.size();

    std::vector<const MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    ssize_t meshCount = 0;
    for (const auto* renderer : renderers)
        meshCount += renderer->_meshes.size();
    return meshCount;
}

const Vector<Mesh*>& MeshRenderer::getMeshes() const
{
    if (!_isImportedAssetRoot)
        return _meshes;

    _flattenedMeshView.clear();
    std::vector<const MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (const auto* renderer : renderers)
        for (auto* mesh : renderer->_meshes)
            _flattenedMeshView.pushBack(mesh);
    return _flattenedMeshView;
}

/**get Mesh by Name */
Mesh* MeshRenderer::getMeshByName(std::string_view name) const
{
    if (!_isImportedAssetRoot)
    {
        for (const auto& mesh : _meshes)
        {
            if (mesh->getName() == name)
                return mesh;
        }
        return nullptr;
    }

    std::vector<const MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (const auto* renderer : renderers)
        for (const auto& mesh : renderer->_meshes)
            if (mesh->getName() == name)
                return mesh;
    return nullptr;
}

std::vector<Mesh*> MeshRenderer::getMeshArrayByName(std::string_view name) const
{
    std::vector<Mesh*> meshes;
    if (!_isImportedAssetRoot)
    {
        for (const auto& mesh : _meshes)
            if (mesh->getName() == name)
                meshes.emplace_back(mesh);
        return meshes;
    }

    std::vector<const MeshRenderer*> renderers;
    collectMeshRenderers(this, renderers);
    for (const auto* renderer : renderers)
        for (const auto& mesh : renderer->_meshes)
            if (mesh->getName() == name)
                meshes.emplace_back(mesh);
    return meshes;
}

Mesh* MeshRenderer::getMesh() const
{
    return getMeshCount() > 0 ? getMeshByIndex(0) : nullptr;
}

static MeshMaterial* getMeshRendererMaterialForAttribs(MeshVertexData* meshVertexData, bool usesLight)
{
    bool textured = meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_TEX_COORD);
    bool hasSkin  = meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_INDEX) &&
                   meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_BLEND_WEIGHT);
    bool hasNormal       = meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_NORMAL);
    bool hasTangentSpace = meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_TANGENT) &&
                           meshVertexData->hasVertexAttrib(shaderinfos::VertexKey::VERTEX_ATTRIB_BINORMAL);
    MeshMaterial::MaterialType type;
    if (textured)
    {
        if (hasTangentSpace)
        {
            type =
                hasNormal && usesLight ? MeshMaterial::MaterialType::BUMPED_DIFFUSE : MeshMaterial::MaterialType::UNLIT;
        }
        else
        {
            type = hasNormal && usesLight ? MeshMaterial::MaterialType::DIFFUSE : MeshMaterial::MaterialType::UNLIT;
        }
    }
    else
    {
        type = hasNormal && usesLight ? MeshMaterial::MaterialType::DIFFUSE_NOTEX
                                      : MeshMaterial::MaterialType::UNLIT_NOTEX;
    }

    return MeshMaterial::createBuiltInMaterial(type, hasSkin);
}

}  // namespace ax
