#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace axasset
{

enum class TextureEncoding
{
    Etc1s,
    Uastc,
};

struct Options
{
    std::filesystem::path input;
    std::filesystem::path output;
    std::string gltfpackExecutable;
    TextureEncoding textureEncoding{TextureEncoding::Etc1s};
    bool force{};
    bool keepIntermediate{};
};

enum class ParseResult
{
    Run,
    Help,
    Error,
};

[[nodiscard]] ParseResult parseArguments(int argc, const char* const* argv, Options& options, std::string& error);
[[nodiscard]] std::vector<std::string> buildGltfpackCommand(std::string executable,
                                                            const std::filesystem::path& input,
                                                            const std::filesystem::path& output,
                                                            TextureEncoding textureEncoding);
void printUsage();
[[nodiscard]] int runPipeline(const Options& options);

}  // namespace axasset
