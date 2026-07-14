#include "EmbeddedTexturePreprocessor.h"

#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace axasset
{
namespace
{

constexpr uint32_t kMaxTextureDimension  = 65535;
constexpr size_t kMaxDecodedTextureBytes = 256U * 1024U * 1024U;

void appendBigEndian(std::vector<uint8_t>& output, uint32_t value)
{
    output.push_back(static_cast<uint8_t>(value >> 24U));
    output.push_back(static_cast<uint8_t>(value >> 16U));
    output.push_back(static_cast<uint8_t>(value >> 8U));
    output.push_back(static_cast<uint8_t>(value));
}

uint32_t updateCrc(uint32_t crc, const uint8_t* data, size_t size)
{
    for (size_t index = 0; index < size; ++index)
    {
        crc ^= data[index];
        for (uint32_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}

void appendChunk(std::vector<uint8_t>& output, const std::array<uint8_t, 4>& type, const std::vector<uint8_t>& data)
{
    appendBigEndian(output, static_cast<uint32_t>(data.size()));
    output.insert(output.end(), type.begin(), type.end());
    output.insert(output.end(), data.begin(), data.end());

    uint32_t crc = updateCrc(0xFFFFFFFFU, type.data(), type.size());
    crc          = updateCrc(crc, data.data(), data.size()) ^ 0xFFFFFFFFU;
    appendBigEndian(output, crc);
}

uint32_t adler32(const std::vector<uint8_t>& data)
{
    constexpr uint32_t modulus = 65521U;
    uint32_t a                 = 1;
    uint32_t b                 = 0;
    for (uint8_t value : data)
    {
        a = (a + value) % modulus;
        b = (b + a) % modulus;
    }
    return (b << 16U) | a;
}

bool encodePng(const uint8_t* rgba, uint32_t width, uint32_t height, std::vector<uint8_t>& output)
{
    if (!rgba || width == 0 || height == 0 || width > kMaxTextureDimension || height > kMaxTextureDimension)
        return false;

    const size_t rowBytes = static_cast<size_t>(width) * 4U;
    if (rowBytes > std::numeric_limits<size_t>::max() - 1U ||
        static_cast<size_t>(height) > kMaxDecodedTextureBytes / (rowBytes + 1U))
        return false;

    std::vector<uint8_t> filtered;
    filtered.reserve((rowBytes + 1U) * height);
    for (uint32_t row = 0; row < height; ++row)
    {
        filtered.push_back(0);
        const uint8_t* source = rgba + static_cast<size_t>(row) * rowBytes;
        filtered.insert(filtered.end(), source, source + rowBytes);
    }

    std::vector<uint8_t> zlib;
    zlib.reserve(filtered.size() + filtered.size() / 65535U * 5U + 16U);
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    for (size_t offset = 0; offset < filtered.size();)
    {
        const size_t blockSize = std::min<size_t>(65535U, filtered.size() - offset);
        const bool finalBlock  = offset + blockSize == filtered.size();
        const uint16_t length  = static_cast<uint16_t>(blockSize);
        const uint16_t inverse = static_cast<uint16_t>(~length);
        zlib.push_back(finalBlock ? 0x01 : 0x00);
        zlib.push_back(static_cast<uint8_t>(length));
        zlib.push_back(static_cast<uint8_t>(length >> 8U));
        zlib.push_back(static_cast<uint8_t>(inverse));
        zlib.push_back(static_cast<uint8_t>(inverse >> 8U));
        zlib.insert(zlib.end(), filtered.begin() + static_cast<ptrdiff_t>(offset),
                    filtered.begin() + static_cast<ptrdiff_t>(offset + blockSize));
        offset += blockSize;
    }
    appendBigEndian(zlib, adler32(filtered));

    output.assign({0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});
    std::vector<uint8_t> header;
    header.reserve(13);
    appendBigEndian(header, width);
    appendBigEndian(header, height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    appendChunk(output, {'I', 'H', 'D', 'R'}, header);
    appendChunk(output, {'I', 'D', 'A', 'T'}, zlib);
    appendChunk(output, {'I', 'E', 'N', 'D'}, {});
    return true;
}

std::string_view textureFormat(const aiTexture& texture)
{
    const char* begin = texture.achFormatHint;
    const char* end   = std::find(begin, begin + sizeof(texture.achFormatHint), '\0');
    return std::string_view(begin, static_cast<size_t>(end - begin));
}

bool isDirectGltfpackInput(const aiTexture& texture)
{
    const std::string_view format = textureFormat(texture);
    return format == "png" || format == "jpg" || format == "jpeg" || format == "kx2" || format == "ktx2";
}

bool replaceWithPng(aiTexture& texture, size_t textureIndex, std::string& error)
{
    std::vector<uint8_t> rgba;
    uint32_t width  = texture.mWidth;
    uint32_t height = texture.mHeight;

    if (texture.mHeight == 0)
    {
        if (!texture.pcData || texture.mWidth == 0 || texture.mWidth > static_cast<unsigned int>(INT_MAX))
        {
            error = "embedded texture " + std::to_string(textureIndex) + " has an invalid compressed payload";
            return false;
        }

        int decodedWidth  = 0;
        int decodedHeight = 0;
        int channels      = 0;
        stbi_uc* decoded =
            stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(texture.pcData), static_cast<int>(texture.mWidth),
                                  &decodedWidth, &decodedHeight, &channels, 4);
        if (!decoded || decodedWidth <= 0 || decodedHeight <= 0)
        {
            error = "embedded texture " + std::to_string(textureIndex) + " format '" +
                    std::string(textureFormat(texture)) + "' cannot be normalized to PNG";
            stbi_image_free(decoded);
            return false;
        }
        width  = static_cast<uint32_t>(decodedWidth);
        height = static_cast<uint32_t>(decodedHeight);
        if (width > kMaxTextureDimension || height > kMaxTextureDimension ||
            static_cast<size_t>(width) > kMaxDecodedTextureBytes / 4U / height)
        {
            error = "embedded texture " + std::to_string(textureIndex) + " exceeds the host texture limit";
            stbi_image_free(decoded);
            return false;
        }
        const size_t byteCount = static_cast<size_t>(width) * height * 4U;
        rgba.assign(decoded, decoded + byteCount);
        stbi_image_free(decoded);
    }
    else
    {
        if (!texture.pcData || width == 0 || height == 0 || width > kMaxTextureDimension ||
            height > kMaxTextureDimension || static_cast<size_t>(width) > kMaxDecodedTextureBytes / 4U / height)
        {
            error = "embedded texture " + std::to_string(textureIndex) + " has invalid raw dimensions";
            return false;
        }
        rgba.resize(static_cast<size_t>(width) * height * 4U);
        for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel)
        {
            rgba[pixel * 4U + 0U] = texture.pcData[pixel].r;
            rgba[pixel * 4U + 1U] = texture.pcData[pixel].g;
            rgba[pixel * 4U + 2U] = texture.pcData[pixel].b;
            rgba[pixel * 4U + 3U] = texture.pcData[pixel].a;
        }
    }

    std::vector<uint8_t> png;
    if (!encodePng(rgba.data(), width, height, png) || png.size() > std::numeric_limits<unsigned int>::max())
    {
        error = "embedded texture " + std::to_string(textureIndex) + " PNG encoding failed";
        return false;
    }

    const size_t texelCount = (png.size() + sizeof(aiTexel) - 1U) / sizeof(aiTexel);
    auto* replacement       = new aiTexel[texelCount]{};
    std::memcpy(replacement, png.data(), png.size());
    delete[] texture.pcData;
    texture.pcData  = replacement;
    texture.mWidth  = static_cast<unsigned int>(png.size());
    texture.mHeight = 0;
    std::memset(texture.achFormatHint, 0, sizeof(texture.achFormatHint));
    std::memcpy(texture.achFormatHint, "png", 3);
    return true;
}

}  // namespace

bool prepareEmbeddedTexturesForGltfpack(aiScene& scene, std::string& error)
{
    for (size_t index = 0; index < scene.mNumTextures; ++index)
    {
        aiTexture* texture = scene.mTextures[index];
        if (!texture)
        {
            error = "embedded texture " + std::to_string(index) + " is null";
            return false;
        }
        if (texture->mHeight == 0 && isDirectGltfpackInput(*texture))
            continue;
        if (!replaceWithPng(*texture, index, error))
            return false;
    }
    return true;
}

}  // namespace axasset
