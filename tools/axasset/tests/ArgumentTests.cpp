#include "AssetPipeline.h"
#include "EmbeddedTexturePreprocessor.h"

#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

[[nodiscard]] bool contains(const std::vector<std::string>& arguments, const std::string& value)
{
    return std::find(arguments.begin(), arguments.end(), value) != arguments.end();
}

[[nodiscard]] bool testDefaultEtc1s()
{
    const char* arguments[] = {"axasset", "--output", "output.glb", "input.fbx"};
    axasset::Options options;
    std::string error;
    if (axasset::parseArguments(4, arguments, options, error) != axasset::ParseResult::Run || !error.empty() ||
        options.textureEncoding != axasset::TextureEncoding::Etc1s)
        return false;

    const auto command =
        axasset::buildGltfpackCommand("gltfpack", "intermediate.glb", "output.glb", options.textureEncoding);
    return contains(command, "-tc") && !contains(command, "-tu");
}

[[nodiscard]] bool testExplicitUastc()
{
    const char* arguments[] = {"axasset", "--uastc", "-o", "output.glb", "input.fbx"};
    axasset::Options options;
    std::string error;
    if (axasset::parseArguments(5, arguments, options, error) != axasset::ParseResult::Run || !error.empty() ||
        options.textureEncoding != axasset::TextureEncoding::Uastc)
        return false;

    const auto command =
        axasset::buildGltfpackCommand("gltfpack", "intermediate.glb", "output.glb", options.textureEncoding);
    return contains(command, "-tu") && !contains(command, "-tc");
}

[[nodiscard]] bool testRawEmbeddedTextureNormalization()
{
    aiScene scene;
    scene.mNumTextures = 1;
    scene.mTextures    = new aiTexture*[1];
    scene.mTextures[0] = new aiTexture();
    aiTexture& texture = *scene.mTextures[0];
    texture.mWidth     = 1;
    texture.mHeight    = 1;
    texture.pcData     = new aiTexel[1];
    texture.pcData[0]  = aiTexel{30, 20, 10, 255};

    std::string error;
    if (!axasset::prepareEmbeddedTexturesForGltfpack(scene, error) || !error.empty() || texture.mHeight != 0 ||
        std::string_view(texture.achFormatHint) != "png" || texture.mWidth < 8)
        return false;

    static constexpr std::array<uint8_t, 8> signature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return std::memcmp(texture.pcData, signature.data(), signature.size()) == 0;
}

}  // namespace

int main()
{
    if (!testDefaultEtc1s())
    {
        std::cerr << "axasset argument test failed: default ETC1S mode\n";
        return 1;
    }
    if (!testExplicitUastc())
    {
        std::cerr << "axasset argument test failed: explicit all-texture UASTC mode\n";
        return 1;
    }
    if (!testRawEmbeddedTextureNormalization())
    {
        std::cerr << "axasset argument test failed: embedded texture PNG normalization\n";
        return 1;
    }
    std::cout << "axasset argument tests passed\n";
    return 0;
}
