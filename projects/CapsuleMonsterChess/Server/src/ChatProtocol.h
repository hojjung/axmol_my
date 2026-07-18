#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace cmc::server::chat
{
inline constexpr int CHAT_SCHEMA_VERSION             = 1;
inline constexpr std::size_t MAX_USER_ID_BYTES       = 64;
inline constexpr std::size_t MAX_DISPLAY_NAME_BYTES  = 48;
inline constexpr std::size_t MAX_GUILD_ID_BYTES      = 64;
inline constexpr std::size_t MAX_MESSAGE_BYTES       = 768;
inline constexpr std::size_t MAX_MESSAGE_CODE_POINTS = 200;
inline constexpr std::size_t MAX_HISTORY_PER_CHANNEL = 50;

enum class ChannelKind
{
    Language,
    Guild,
    Direct,
};

struct Identity final
{
    std::string userId;
    std::string displayName;
    std::string language;
    std::string guildId;
};

struct Channel final
{
    ChannelKind kind = ChannelKind::Language;
    std::string scope;
};

[[nodiscard]] std::optional<ChannelKind> parseChannelKind(std::string_view value) noexcept;
[[nodiscard]] std::string_view channelKindName(ChannelKind kind) noexcept;
[[nodiscard]] bool isSupportedLanguage(std::string_view language) noexcept;
[[nodiscard]] bool validateIdentity(const Identity& identity, std::string& error);
[[nodiscard]] bool validateChannel(const Identity& identity, const Channel& channel, std::string& error);
[[nodiscard]] bool validateMessageText(std::string_view text, std::string& normalized, std::string& error);
[[nodiscard]] std::string roomKey(const Identity& identity, const Channel& channel);

}  // namespace cmc::server::chat
