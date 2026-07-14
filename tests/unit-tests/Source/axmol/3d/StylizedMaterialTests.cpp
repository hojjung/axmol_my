#include <doctest.h>

#include "axmol/3d/Mesh.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/renderer/Material.h"

using namespace ax;

namespace
{
class ShadowStateProbeMesh final : public Mesh
{
public:
    bool updateShadowMaterial(const StylizedMaterial& material) { return ensureStylizedShadowMaterial(material, 1); }

    Material* getShadowMaterial() const { return _stylizedShadowMaterial; }
};
}  // namespace

TEST_CASE("Stylized material defaults match the two-band reference")
{
    const StylizedMaterialDesc material;

    CHECK(material.isValid());
    CHECK_FALSE(material.isAlphaCutout());
    CHECK(material.bandThreshold == doctest::Approx(0.7F));
    CHECK(material.bandSoftness == doctest::Approx(0.25F));
    CHECK(material.rimStart == doctest::Approx(0.7F));
    CHECK(material.rimEnd == doctest::Approx(1.0F));
    CHECK_FALSE(material.doubleSided);
}

TEST_CASE("Stylized double-sided state disables main and shadow culling")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    StylizedMaterialDesc desc;
    desc.doubleSided = true;
    auto* material   = StylizedMaterial::create(desc);
    REQUIRE(material != nullptr);

    const std::vector<float> positions = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const std::vector<float> normals   = {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    const std::vector<float> texCoords = {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    const IndexArray indices           = {uint16_t{0}, uint16_t{1}, uint16_t{2}};
    auto* sourceMesh                   = Mesh::create(positions, normals, texCoords, indices);
    REQUIRE(sourceMesh != nullptr);

    auto* mesh = new ShadowStateProbeMesh();
    mesh->setMeshIndexData(sourceMesh->getMeshIndexData());
    mesh->setMaterial(material);
    CHECK_FALSE(material->getStateBlock().isCullFaceEnabled());
    CHECK(material->getStateBlock().getCullFaceSide() == CullFaceSide::BACK);

    REQUIRE(mesh->updateShadowMaterial(*material));
    REQUIRE(mesh->getShadowMaterial() != nullptr);
    CHECK_FALSE(mesh->getShadowMaterial()->getStateBlock().isCullFaceEnabled());

    desc.doubleSided = false;
    REQUIRE(material->setDescription(desc));
    CHECK(material->getStateBlock().isCullFaceEnabled());
    REQUIRE(mesh->updateShadowMaterial(*material));
    CHECK(mesh->getShadowMaterial()->getStateBlock().isCullFaceEnabled());
    mesh->release();
}

TEST_CASE("Stylized material rejects ambiguous band and rim ranges")
{
    StylizedMaterialDesc material;

    material.bandSoftness = -0.01F;
    CHECK_FALSE(material.isValid());

    material          = {};
    material.rimStart = material.rimEnd;
    CHECK_FALSE(material.isValid());

    material             = {};
    material.alphaCutoff = 0.5F;
    CHECK(material.isValid());
    CHECK(material.isAlphaCutout());
}

TEST_CASE("Stylized rim is zero in full shadow and monotonic across penumbra")
{
    constexpr float kEightBitTolerance = 1.0F / 255.0F;
    constexpr float kFresnel           = 0.92F;
    constexpr float kLightFacing       = 0.8F;
    constexpr float kMainLuminance     = 1.25F;
    constexpr float kRimStart          = 0.7F;
    constexpr float kRimEnd            = 1.0F;
    constexpr float kIntensity         = 0.9F;

    const float fullShadow =
        StylizedMaterial::evaluateRimMask(kFresnel, kLightFacing, 0.0F, kMainLuminance, kRimStart, kRimEnd, kIntensity);
    CHECK(fullShadow <= kEightBitTolerance);

    float previous =
        StylizedMaterial::evaluateRimMask(kFresnel, kLightFacing, 1.0F, kMainLuminance, kRimStart, kRimEnd, kIntensity);
    for (int step = 1; step <= 16; ++step)
    {
        const float visibility = 1.0F - static_cast<float>(step) / 16.0F;
        const float current    = StylizedMaterial::evaluateRimMask(kFresnel, kLightFacing, visibility, kMainLuminance,
                                                                   kRimStart, kRimEnd, kIntensity);
        CHECK(current <= previous);
        previous = current;
    }
    CHECK(previous <= kEightBitTolerance);
}
