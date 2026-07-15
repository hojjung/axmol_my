#pragma once

#include "axmol/axmol.h"

#include <string>

namespace ax
{
class MeshRenderer;
}

namespace cmc::client
{
struct DragonFixtureStyle final
{
    ax::Color diffuseTint{1.0F, 0.9290111F, 0.75F, 1.0F};
    ax::Color rimColor{1.0F, 0.9F, 0.8F, 1.0F};
    float targetExtent = 4.15F;
    float yawDegrees   = -18.0F;
};

[[nodiscard]] ax::MeshRenderer* createDragonFixture(const DragonFixtureStyle& style, std::string& error);
}  // namespace cmc::client
