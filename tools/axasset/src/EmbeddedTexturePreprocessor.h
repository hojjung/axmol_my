#pragma once

#include <string>

struct aiScene;

namespace axasset
{

[[nodiscard]] bool prepareEmbeddedTexturesForGltfpack(aiScene& scene, std::string& error);

}  // namespace axasset
