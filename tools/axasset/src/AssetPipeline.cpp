#include "AssetPipeline.h"

#include "AxAssetConfig.h"
#include "EmbeddedTexturePreprocessor.h"
#include "Process.h"

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#    include <Windows.h>
#else
#    include <unistd.h>
#endif

namespace axasset
{
namespace
{

constexpr int kInputError      = 66;
constexpr int kPipelineError   = 70;
constexpr int kOutputError     = 73;
constexpr int kExecutableError = 127;

enum class ValidationResult
{
    Success,
    InputError,
    OutputError,
};

class TemporaryDirectory
{
public:
    TemporaryDirectory() = default;
    explicit TemporaryDirectory(std::filesystem::path path) : _path(std::move(path)) {}

    TemporaryDirectory(const TemporaryDirectory&)            = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    TemporaryDirectory(TemporaryDirectory&& other) noexcept : _path(std::move(other._path)), _keep(other._keep)
    {
        other._path.clear();
    }

    TemporaryDirectory& operator=(TemporaryDirectory&& other) noexcept
    {
        if (this == &other)
            return *this;
        cleanup();
        _path = std::move(other._path);
        _keep = other._keep;
        other._path.clear();
        return *this;
    }

    ~TemporaryDirectory() { cleanup(); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }
    void keep() noexcept { _keep = true; }

private:
    void cleanup() noexcept
    {
        if (_path.empty() || _keep)
            return;
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }

    std::filesystem::path _path;
    bool _keep{};
};

[[nodiscard]] uint64_t processId() noexcept
{
#if defined(_WIN32)
    return static_cast<uint64_t>(GetCurrentProcessId());
#else
    return static_cast<uint64_t>(getpid());
#endif
}

[[nodiscard]] std::optional<TemporaryDirectory> createTemporaryDirectory(const std::filesystem::path& parent,
                                                                         std::string_view prefix,
                                                                         std::string& error)
{
    static std::atomic<uint64_t> sequence{};
    const auto timestamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

    for (uint64_t attempt = 0; attempt < 128; ++attempt)
    {
        const auto token =
            timestamp ^ (processId() << 32U) ^ sequence.fetch_add(1, std::memory_order_relaxed) ^ attempt;
        const auto path = parent / (std::string(prefix) + std::to_string(token));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec))
            return TemporaryDirectory(path);
        if (ec && ec != std::errc::file_exists)
        {
            error = "cannot create temporary directory " + path.string() + ": " + ec.message();
            return std::nullopt;
        }
    }

    error = "cannot allocate a unique temporary directory under " + parent.string();
    return std::nullopt;
}

[[nodiscard]] std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

[[nodiscard]] bool hasGlbExporter(Assimp::Exporter& exporter)
{
    for (size_t index = 0; index < exporter.GetExportFormatCount(); ++index)
    {
        const aiExportFormatDesc* description = exporter.GetExportFormatDescription(index);
        if (description && description->id && std::string_view(description->id) == "glb2")
            return true;
    }
    return false;
}

[[nodiscard]] std::string pathForProcess(const std::filesystem::path& path)
{
#if defined(_WIN32)
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
#else
    return path.native();
#endif
}

[[nodiscard]] ValidationResult validatePaths(const Options& options,
                                             std::filesystem::path& input,
                                             std::filesystem::path& output,
                                             std::string& error)
{
    std::error_code ec;
    input = std::filesystem::weakly_canonical(options.input, ec);
    if (ec || !std::filesystem::is_regular_file(input, ec))
    {
        error = "input is not a readable regular file: " + options.input.string();
        return ValidationResult::InputError;
    }

    output = std::filesystem::absolute(options.output, ec).lexically_normal();
    if (ec)
    {
        error = "cannot resolve output path: " + options.output.string();
        return ValidationResult::OutputError;
    }

    if (lowercase(output.extension().string()) != ".glb")
    {
        error = "output must use the .glb extension: " + output.string();
        return ValidationResult::OutputError;
    }

    const auto outputParent = output.parent_path();
    if (!std::filesystem::is_directory(outputParent, ec) || ec)
    {
        error = "output directory does not exist: " + outputParent.string();
        return ValidationResult::OutputError;
    }

    const bool outputExists = std::filesystem::exists(output, ec);
    if (ec)
    {
        error = "cannot inspect output path: " + ec.message();
        return ValidationResult::OutputError;
    }

    if (outputExists)
    {
        if (!std::filesystem::is_regular_file(output, ec) || ec)
        {
            error = "existing output is not a regular file: " + output.string();
            return ValidationResult::OutputError;
        }
        const bool sameFile = std::filesystem::equivalent(input, output, ec);
        if (ec)
        {
            error = "cannot compare input and output paths: " + ec.message();
            return ValidationResult::OutputError;
        }
        if (sameFile)
        {
            error = "input and output must be different files";
            return ValidationResult::OutputError;
        }
        if (!options.force)
        {
            error = "output already exists; pass --force to replace it: " + output.string();
            return ValidationResult::OutputError;
        }
    }
    else if (input == output)
    {
        error = "input and output must be different files";
        return ValidationResult::OutputError;
    }

    return ValidationResult::Success;
}

[[nodiscard]] bool replaceOutput(const std::filesystem::path& staged,
                                 const std::filesystem::path& output,
                                 bool force,
                                 std::string& error)
{
    std::error_code ec;
    const bool outputExists = std::filesystem::exists(output, ec);
    if (ec)
    {
        error = "cannot inspect output before replacement: " + ec.message();
        return false;
    }
    if (!outputExists)
    {
        std::filesystem::rename(staged, output, ec);
        if (!ec)
            return true;
        error = "cannot move staged output into place: " + ec.message();
        return false;
    }

    if (!force)
    {
        error = "output was created while conversion was running; refusing to overwrite it";
        return false;
    }

    std::string tempError;
    auto backupDirectory = createTemporaryDirectory(output.parent_path(), ".axasset-backup-", tempError);
    if (!backupDirectory)
    {
        error = std::move(tempError);
        return false;
    }
    const auto backup = backupDirectory->path() / output.filename();

    std::filesystem::rename(output, backup, ec);
    if (ec)
    {
        error = "cannot preserve existing output before replacement: " + ec.message();
        return false;
    }

    std::filesystem::rename(staged, output, ec);
    if (!ec)
        return true;

    const std::string moveError = ec.message();
    ec.clear();
    std::filesystem::rename(backup, output, ec);
    if (ec)
    {
        backupDirectory->keep();
        error = "cannot install staged output (" + moveError +
                ") and could not restore the previous output; backup: " + backup.string();
        return false;
    }

    error = "cannot install staged output; previous output was restored: " + moveError;
    return false;
}

[[nodiscard]] std::string resolveGltfpack(const Options& options)
{
    if (!options.gltfpackExecutable.empty())
        return options.gltfpackExecutable;
    if (const char* environment = std::getenv("AX_GLTFPACK"); environment && *environment)
        return environment;
    if (std::string_view configured = AXASSET_CONFIGURED_GLTFPACK; !configured.empty())
        return std::string(configured);
    return "gltfpack";
}

}  // namespace

ParseResult parseArguments(int argc, const char* const* argv, Options& options, std::string& error)
{
    std::vector<std::string_view> positional;
    bool positionalOnly = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (!positionalOnly && argument == "--")
        {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly && (argument == "--help" || argument == "-h"))
            return ParseResult::Help;
        if (!positionalOnly && (argument == "--force" || argument == "-f"))
        {
            options.force = true;
            continue;
        }
        if (!positionalOnly && argument == "--keep-intermediate")
        {
            options.keepIntermediate = true;
            continue;
        }
        if (!positionalOnly && argument == "--uastc")
        {
            options.textureEncoding = TextureEncoding::Uastc;
            continue;
        }
        if (!positionalOnly && (argument == "--output" || argument == "-o"))
        {
            if (++index >= argc)
            {
                error = std::string(argument) + " requires a .glb path";
                return ParseResult::Error;
            }
            options.output = argv[index];
            continue;
        }
        if (!positionalOnly && argument == "--gltfpack")
        {
            if (++index >= argc)
            {
                error = "--gltfpack requires an executable path";
                return ParseResult::Error;
            }
            options.gltfpackExecutable = argv[index];
            continue;
        }
        if (!positionalOnly && !argument.empty() && argument.front() == '-')
        {
            error = "unknown option: " + std::string(argument);
            return ParseResult::Error;
        }
        positional.push_back(argument);
    }

    if (positional.size() != 1)
    {
        error = "exactly one input asset is required";
        return ParseResult::Error;
    }
    if (options.output.empty())
    {
        error = "--output is required";
        return ParseResult::Error;
    }

    options.input = positional.front();
    return ParseResult::Run;
}

std::vector<std::string> buildGltfpackCommand(std::string executable,
                                              const std::filesystem::path& input,
                                              const std::filesystem::path& output,
                                              TextureEncoding textureEncoding)
{
    std::vector<std::string> command = {std::move(executable),  "-i", pathForProcess(input), "-o",
                                        pathForProcess(output), "-cc"};
    command.emplace_back(textureEncoding == TextureEncoding::Uastc ? "-tu" : "-tc");
    command.emplace_back("-kn");
    command.emplace_back("-km");
    return command;
}

void printUsage()
{
    std::cout << "Usage: axasset [options] INPUT\n"
                 "\n"
                 "Convert an Assimp-supported source asset into a meshopt + KTX2 GLB.\n"
                 "\n"
                 "Options:\n"
                 "  -o, --output PATH       Required .glb output path\n"
                 "      --gltfpack PATH     gltfpack executable (then AX_GLTFPACK, configured path, PATH)\n"
                 "      --uastc             Encode every texture as UASTC (-tu); default is ETC1S (-tc)\n"
                 "  -f, --force             Replace an existing output transactionally\n"
                 "      --keep-intermediate Keep the Assimp GLB for diagnosis\n"
                 "  -h, --help              Show this help\n";
}

int runPipeline(const Options& options)
{
    std::filesystem::path input;
    std::filesystem::path output;
    std::string error;
    const ValidationResult validation = validatePaths(options, input, output, error);
    if (validation != ValidationResult::Success)
    {
        std::cerr << "axasset: " << error << '\n';
        return validation == ValidationResult::InputError ? kInputError : kOutputError;
    }

    std::error_code tempError;
    const auto systemTemp = std::filesystem::temp_directory_path(tempError);
    if (tempError)
    {
        std::cerr << "axasset: cannot locate the host temporary directory: " << tempError.message() << '\n';
        return kPipelineError;
    }

    auto intermediateDirectory = createTemporaryDirectory(systemTemp, "axasset-intermediate-", error);
    if (!intermediateDirectory)
    {
        std::cerr << "axasset: " << error << '\n';
        return kPipelineError;
    }
    const auto intermediate = intermediateDirectory->path() / "assimp.glb";

    auto stagingDirectory = createTemporaryDirectory(output.parent_path(), ".axasset-staging-", error);
    if (!stagingDirectory)
    {
        std::cerr << "axasset: " << error << '\n';
        return kOutputError;
    }
    const auto staged = stagingDirectory->path() / output.filename();

    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    constexpr unsigned int importFlags =
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_LimitBoneWeights |
        aiProcess_GenSmoothNormals | aiProcess_ValidateDataStructure | aiProcess_EmbedTextures;
    aiScene* scene = const_cast<aiScene*>(importer.ReadFile(pathForProcess(input), importFlags));
    if (!scene)
    {
        std::cerr << "axasset: Assimp import failed: " << importer.GetErrorString() << '\n';
        return kInputError;
    }
    if (!scene->HasMeshes())
    {
        std::cerr << "axasset: Assimp imported no renderable meshes\n";
        return kInputError;
    }
    if (!prepareEmbeddedTexturesForGltfpack(*scene, error))
    {
        std::cerr << "axasset: " << error << '\n';
        return kInputError;
    }

    Assimp::Exporter exporter;
    if (!hasGlbExporter(exporter))
    {
        std::cerr << "axasset: this Assimp package was built without the glTF2 binary exporter (glb2)\n";
        return kPipelineError;
    }
    if (exporter.Export(scene, "glb2", pathForProcess(intermediate)) != AI_SUCCESS)
    {
        std::cerr << "axasset: Assimp GLB export failed: " << exporter.GetErrorString() << '\n';
        return kPipelineError;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(intermediate, ec) || ec ||
        std::filesystem::file_size(intermediate, ec) == 0 || ec)
    {
        std::cerr << "axasset: Assimp reported success without producing a non-empty GLB\n";
        return kPipelineError;
    }
    if (options.keepIntermediate)
    {
        intermediateDirectory->keep();
        std::cout << "axasset: intermediate retained at " << intermediate << '\n';
    }

    const std::vector<std::string> command =
        buildGltfpackCommand(resolveGltfpack(options), intermediate, staged, options.textureEncoding);

    std::cout << "axasset: optimizing " << input << " -> " << output << '\n';
    const ProcessResult process = runProcess(command);
    if (process.exitCode != 0)
    {
        if (!process.error.empty())
            std::cerr << "axasset: gltfpack: " << process.error << '\n';
        std::cerr << "axasset: gltfpack exited with status " << process.exitCode << '\n';
        return process.exitCode == kExecutableError ? kExecutableError : process.exitCode;
    }

    ec.clear();
    if (!std::filesystem::is_regular_file(staged, ec) || ec || std::filesystem::file_size(staged, ec) == 0 || ec)
    {
        std::cerr << "axasset: gltfpack reported success without producing a non-empty GLB\n";
        return kPipelineError;
    }

    if (!replaceOutput(staged, output, options.force, error))
    {
        std::cerr << "axasset: " << error << '\n';
        return kOutputError;
    }

    std::cout << "axasset: wrote " << output << '\n';
    return 0;
}

}  // namespace axasset
