#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace cmc::server::http_support
{
inline constexpr std::size_t MIN_GZIP_RESPONSE_BYTES = 1024;

[[nodiscard]] bool acceptsGzip(std::string_view acceptEncoding) noexcept;
[[nodiscard]] std::optional<std::string> gzipCompress(std::string_view input) noexcept;
}  // namespace cmc::server::http_support
