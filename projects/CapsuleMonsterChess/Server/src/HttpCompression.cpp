#include "HttpCompression.h"

#include <zlib.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cmc::server::http_support
{
namespace
{
std::string_view trim(std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right) noexcept
{
    const auto asciiLower = [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? static_cast<unsigned char>(character + ('a' - 'A')) : character;
    };
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [asciiLower](unsigned char a, unsigned char b) { return asciiLower(a) == asciiLower(b); });
}

int parseQuality(std::string_view value) noexcept
{
    value = trim(value);
    if (value.empty())
        return -1;

    if (value.front() == '1')
    {
        if (value.size() == 1)
            return 1000;
        if (value[1] != '.' || value.size() > 5)
            return -1;
        return std::all_of(value.begin() + 2, value.end(), [](char character) { return character == '0'; }) ? 1000 : -1;
    }

    if (value.front() != '0')
        return -1;
    if (value.size() == 1)
        return 0;
    if (value[1] != '.' || value.size() > 5)
        return -1;

    int quality = 0;
    int scale   = 100;
    for (char character : value.substr(2))
    {
        if (character < '0' || character > '9')
            return -1;
        quality += (character - '0') * scale;
        scale /= 10;
    }
    return quality;
}

struct EncodingPreference final
{
    std::string_view coding;
    int quality = 1000;
};

EncodingPreference parsePreference(std::string_view value) noexcept
{
    const std::size_t firstSeparator = value.find(';');
    EncodingPreference preference{trim(value.substr(0, firstSeparator)), 1000};
    if (firstSeparator == std::string_view::npos)
        return preference;

    value.remove_prefix(firstSeparator + 1);
    while (!value.empty())
    {
        const std::size_t separator      = value.find(';');
        const std::string_view parameter = trim(value.substr(0, separator));
        const std::size_t equals         = parameter.find('=');
        if (equals != std::string_view::npos && equalsIgnoreCase(trim(parameter.substr(0, equals)), "q"))
        {
            const int quality  = parseQuality(parameter.substr(equals + 1));
            preference.quality = quality >= 0 ? quality : 0;
        }

        if (separator == std::string_view::npos)
            break;
        value.remove_prefix(separator + 1);
    }
    return preference;
}
}  // namespace

bool acceptsGzip(std::string_view acceptEncoding) noexcept
{
    int gzipQuality     = -1;
    int wildcardQuality = -1;

    while (!acceptEncoding.empty())
    {
        const std::size_t separator         = acceptEncoding.find(',');
        const EncodingPreference preference = parsePreference(acceptEncoding.substr(0, separator));
        if (equalsIgnoreCase(preference.coding, "gzip"))
            gzipQuality = std::max(gzipQuality, preference.quality);
        else if (preference.coding == "*")
            wildcardQuality = std::max(wildcardQuality, preference.quality);

        if (separator == std::string_view::npos)
            break;
        acceptEncoding.remove_prefix(separator + 1);
    }

    return gzipQuality >= 0 ? gzipQuality > 0 : wildcardQuality > 0;
}

std::optional<std::string> gzipCompress(std::string_view input) noexcept
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return std::nullopt;

    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return std::nullopt;

    const uLong bound = deflateBound(&stream, static_cast<uLong>(input.size()));
    if (bound > std::numeric_limits<uInt>::max())
    {
        deflateEnd(&stream);
        return std::nullopt;
    }

    bool streamActive    = true;
    const auto endStream = [&stream, &streamActive] {
        if (streamActive)
        {
            deflateEnd(&stream);
            streamActive = false;
        }
    };

    try
    {
        std::string output(static_cast<std::size_t>(bound), '\0');
        stream.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        stream.avail_in  = static_cast<uInt>(input.size());
        stream.next_out  = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int result                 = deflate(&stream, Z_FINISH);
        const std::size_t compressedSize = static_cast<std::size_t>(stream.total_out);
        endStream();
        if (result != Z_STREAM_END)
            return std::nullopt;

        output.resize(compressedSize);
        return output;
    }
    catch (...)
    {
        endStream();
        return std::nullopt;
    }
}
}  // namespace cmc::server::http_support
