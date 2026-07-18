#include "Client/Rendering/UnitPreviewRenderer.h"

#include "Client/Content/MonsterCatalog.h"

#include "axmol/3d/Animate3D.h"
#include "axmol/3d/Animation3D.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/renderer/Material.h"
#include "axmol/scene/Camera.h"

#include <algorithm>
#include <unordered_set>

namespace
{
using namespace ax;

bool configurePreviewMaterials(MeshRenderer& renderer)
{
    const auto meshCount = renderer.getMeshCount();
    if (meshCount <= 0)
        return false;

    std::unordered_set<Material*> configured;
    configured.reserve(static_cast<std::size_t>(meshCount));

    for (int index = 0; index < meshCount; ++index)
    {
        auto* material = renderer.getMaterial(index);
        if (!material || !configured.emplace(material).second)
            continue;

        material->setForce2DQueue(true);
        auto* stylized = dynamic_cast<StylizedMaterial*>(material);
        if (!stylized)
            continue;

        auto description              = stylized->getDescription();
        description.minimumBrightness = std::max(description.minimumBrightness, 0.24F);
        description.unlitStrength     = std::max(description.unlitStrength, 0.58F);
        description.rimColor          = Color{0.80F, 0.92F, 1.0F, 1.0F};
        description.rimStart          = 0.34F;
        description.rimEnd            = 0.92F;
        description.rimPower          = 1.8F;
        description.rimIntensity      = 0.42F;
        if (!stylized->setDescription(description))
            return false;
    }
    return !configured.empty();
}
}  // namespace

namespace cmc::client
{
Node* createUnitPreview(const MonsterCatalogEntry& entry, const Size& viewportSize, std::string& error)
{
    error.clear();
    if (entry.modelId.empty())
    {
        error = "Unit model path is empty";
        return nullptr;
    }

    auto* renderer = MeshRenderer::create(entry.modelId);
    if (!renderer || renderer->getMeshCount() <= 0)
    {
        error = "Unit GLB load failed: " + entry.modelId;
        return nullptr;
    }

    const AABB bounds = renderer->getAABBRecursively();
    if (bounds.isEmpty())
    {
        error = "Unit GLB bounds are empty: " + entry.modelId;
        return nullptr;
    }

    const Vec3 size = bounds._max - bounds._min;
    if (size.x <= 1.0e-4F || size.y <= 1.0e-4F)
    {
        error = "Unit GLB bounds are degenerate: " + entry.modelId;
        return nullptr;
    }

    if (!configurePreviewMaterials(*renderer))
    {
        error = "Unit GLB material setup failed: " + entry.modelId;
        return nullptr;
    }

    const float scale =
        std::min(viewportSize.width * 0.78F / size.x, viewportSize.height * 0.82F / size.y);
    const Vec3 center = (bounds._min + bounds._max) * 0.5F;

    auto* root = Node::create();
    root->setContentSize(viewportSize);
    renderer->setScale(scale);
    renderer->setPosition3D(
        {viewportSize.width * 0.5F - center.x * scale,
         viewportSize.height * 0.47F - center.y * scale,
         -center.z * scale});
    renderer->setCameraMask(static_cast<unsigned short>(CameraFlag::DEFAULT));
    renderer->setCastShadow(false);
    renderer->setReceiveShadow(false);
    root->addChild(renderer);

    if (auto* animation = Animation3D::create(entry.modelId); animation && animation->getDuration() > 0.0F)
        renderer->runAction(RepeatForever::create(Animate3D::create(animation)));

    return root;
}
}  // namespace cmc::client
