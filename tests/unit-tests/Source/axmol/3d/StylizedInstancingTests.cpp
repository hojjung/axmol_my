#include <doctest.h>

#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/MeshSkin.h"
#include "axmol/3d/MeshVertexIndexData.h"
#include "axmol/3d/Skeleton3D.h"
#include "axmol/renderer/CustomCommand.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/rhi/Buffer.h"
#include "axmol/rhi/GraphicsCore.h"
#include "axmol/rhi/Program.h"
#include "axmol/rhi/VertexLayout.h"

using namespace ax;

namespace
{
class TestMesh final : public Mesh
{
public:
    using Mesh::prepareInstanceData;

    void setBounds(const AABB& bounds) { _aabb = bounds; }
    bool isTransformDirty() const { return _instanceTransformDirty; }
    const float* getInstanceMatrices() const { return _instanceMatrixCache; }
    rhi::Buffer* getInstanceTransformBuffer() const { return _instanceTransformBuffer; }
};

class TestMeshRenderer final : public MeshRenderer
{
public:
    void addTestMesh(Mesh* mesh)
    {
        _meshes.pushBack(mesh);
        _aabbDirty = true;
    }
};
}  // namespace

TEST_CASE("Stylized static programs share one instance matrix input")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    for (const uint32_t type : {rhi::ProgramType::STYLIZED_3D, rhi::ProgramType::STYLIZED_CUTOUT_3D,
                                rhi::ProgramType::STYLIZED_SHADOW_3D, rhi::ProgramType::STYLIZED_SHADOW_CUTOUT_3D})
    {
        auto* program = axpm->getBuiltinProgram(type);
        REQUIRE(program != nullptr);
        CHECK(program->getVertexInputDesc(rhi::VertexInputKind::INSTANCE) != nullptr);
    }

    for (const uint32_t type :
         {rhi::ProgramType::STYLIZED_SKIN_3D, rhi::ProgramType::STYLIZED_SKIN_CUTOUT_3D,
          rhi::ProgramType::STYLIZED_SHADOW_SKIN_3D, rhi::ProgramType::STYLIZED_SHADOW_SKIN_CUTOUT_3D})
    {
        auto* program = axpm->getBuiltinProgram(type);
        REQUIRE(program != nullptr);
        CHECK(program->getVertexInputDesc(rhi::VertexInputKind::INSTANCE) == nullptr);
    }
}

TEST_CASE("Instance matrix layout uses a dedicated 64-byte per-instance stream")
{
    const rhi::VertexInputDesc position{"POSITION", 0, 1, 0};
    const rhi::VertexInputDesc instance{"TEXCOORD", 3, 4, 0};
    rhi::VertexLayoutDesc layout;
    layout.startLayout(2);
    layout.addAttrib(rhi::VERTEX_INPUT_NAME_POSITION, &position, rhi::VertexElementType::FLOAT3, 0, false);
    layout.addAttrib(rhi::VERTEX_INPUT_NAME_INSTANCE, &instance, rhi::VertexElementType::MAT4, 0, false, 1);
    layout.endLayout(12);

    CHECK(layout.getStride() == 12);
    CHECK(layout.getInstanceStride() == sizeof(Mat4));
    REQUIRE(layout.getBindings().size() == 2);
    CHECK(layout.getBindings()[1].offset == 0);
    CHECK(layout.getBindings()[1].instanceStepRate == 1);
}

TEST_CASE("Custom command updates draw count when reusing an instance buffer")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    auto* buffer = axdrv->createBuffer(sizeof(Mat4) * 8, rhi::BufferType::VERTEX, rhi::BufferUsage::STATIC);
    REQUIRE(buffer != nullptr);

    CustomCommand command;
    command.setInstanceBuffer(buffer, 1);
    command.setInstanceBuffer(buffer, 7);
    CHECK(command.getInstanceCount() == 7);
    CHECK(command.getInstanceCapacity() >= 7);

    CustomCommand copy = command;
    CHECK(copy.getInstanceBuffer() == buffer);
    CHECK(copy.getInstanceCount() == 7);

    command.setInstanceBuffer(nullptr, 0);
    CHECK(command.getInstanceBuffer() == nullptr);
    CHECK(command.getInstanceCount() == 0);
    buffer->release();
}

TEST_CASE("Static stylized identity and real instance data use the same buffer contract")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    auto* mesh = new TestMesh();
    CHECK_FALSE(mesh->isTransformDirty());
    REQUIRE(mesh->prepareInstanceData(true));
    REQUIRE(mesh->getInstanceTransformBuffer() != nullptr);
    CHECK(mesh->getInstanceTransformBuffer()->getCapacity() == sizeof(Mat4));
    REQUIRE(mesh->getInstanceMatrices() != nullptr);
    for (size_t index = 0; index < 16; ++index)
        CHECK(mesh->getInstanceMatrices()[index] == doctest::Approx(Mat4::identity.m[index]));

    auto* child = Node::create();
    child->setPosition3D(Vec3{3.0F, 4.0F, 5.0F});
    mesh->enableInstancing(true, 1);
    mesh->addInstanceChild(child);
    REQUIRE(mesh->prepareInstanceData(false));
    CHECK(mesh->getInstanceMatrices()[12] == doctest::Approx(3.0F));
    CHECK(mesh->getInstanceMatrices()[13] == doctest::Approx(4.0F));
    CHECK(mesh->getInstanceMatrices()[14] == doctest::Approx(5.0F));
    mesh->release();
}

TEST_CASE("Instanced renderer bounds include child transforms")
{
    auto* renderer = new TestMeshRenderer();
    REQUIRE(renderer->init());
    auto* mesh = new TestMesh();
    mesh->setBounds(AABB{Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 1.0F, 1.0F}});
    mesh->enableInstancing(true, 1);
    renderer->addTestMesh(mesh);
    mesh->release();

    auto* child = Node::create();
    child->setPosition3D(Vec3{100.0F, 2.0F, -3.0F});
    renderer->addInstanceChild(child);
    const AABB& bounds = renderer->getAABB();
    CHECK(bounds._min.x == doctest::Approx(100.0F));
    CHECK(bounds._max.x == doctest::Approx(101.0F));
    CHECK(bounds._min.y == doctest::Approx(2.0F));
    CHECK(bounds._max.z == doctest::Approx(-2.0F));
    renderer->release();
}

TEST_CASE("Skinned renderer bounds follow the current bone pose")
{
    NodeData rootBoneData;
    rootBoneData.id        = "root";
    rootBoneData.transform = Mat4::identity;
    const std::vector<NodeData*> skeletonData{&rootBoneData};
    auto* skeleton = Skeleton3D::create(skeletonData);
    REQUIRE(skeleton != nullptr);

    const std::vector<std::string> boneNames{"root"};
    const std::vector<Mat4> inverseBindPoses{Mat4::identity};
    auto* skin = MeshSkin::create(skeleton, boneNames, inverseBindPoses);
    REQUIRE(skin != nullptr);

    auto* meshIndexData = new MeshIndexData();
    meshIndexData->setAABB(AABB{Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 1.0F, 1.0F}});
    auto* mesh = new TestMesh();
    mesh->setMeshIndexData(meshIndexData);
    mesh->setSkin(skin);
    meshIndexData->release();

    auto* renderer = new TestMeshRenderer();
    REQUIRE(renderer->init());
    renderer->addTestMesh(mesh);
    mesh->release();

    auto* rootBone = skeleton->getBoneByName("root");
    REQUIRE(rootBone != nullptr);
    float rotation[] = {0.0F, 0.0F, 0.0F, 1.0F};
    float scale[]    = {1.0F, 1.0F, 1.0F};

    float firstTranslation[] = {100.0F, 2.0F, -3.0F};
    rootBone->setAnimationValue(firstTranslation, rotation, scale);
    skeleton->updateBoneMatrix();
    const AABB& firstBounds = renderer->getAABB();
    CHECK(firstBounds._min.x == doctest::Approx(100.0F));
    CHECK(firstBounds._max.x == doctest::Approx(101.0F));
    CHECK(firstBounds._min.y == doctest::Approx(2.0F));
    CHECK(firstBounds._max.z == doctest::Approx(-2.0F));

    float secondTranslation[] = {200.0F, -4.0F, 7.0F};
    rootBone->setAnimationValue(secondTranslation, rotation, scale);
    skeleton->updateBoneMatrix();
    const AABB& secondBounds = renderer->getAABB();
    CHECK(secondBounds._min.x == doctest::Approx(200.0F));
    CHECK(secondBounds._max.x == doctest::Approx(201.0F));
    CHECK(secondBounds._min.y == doctest::Approx(-4.0F));
    CHECK(secondBounds._max.z == doctest::Approx(8.0F));

    renderer->release();
}
