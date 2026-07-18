#include "ChatProtocol.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace cmc::server::chat
{
namespace
{
bool isSafeIdentifier(std::string_view value, std::size_t maxBytes) noexcept
{
    if (value.empty() || value.size() > maxBytes)
        return false;

    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.';
    });
}

bool isValidUtf8(std::string_view value, std::size_t& codePointCount) noexcept
{
    codePointCount = 0;
    for (std::size_t index = 0; index < value.size();)
    {
        const auto leading      = static_cast<std::uint8_t>(value[index]);
        std::size_t width       = 0;
        std::uint32_t codePoint = 0;
        if (leading <= 0x7F)
        {
            width     = 1;
            codePoint = leading;
        }
        else if (leading >= 0xC2 && leading <= 0xDF)
        {
            width     = 2;
            codePoint = leading & 0x1FU;
        }
        else if (leading >= 0xE0 && leading <= 0xEF)
        {
            width     = 3;
            codePoint = leading & 0x0FU;
        }
        else if (leading >= 0xF0 && leading <= 0xF4)
        {
            width     = 4;
            codePoint = leading & 0x07U;
        }
        else
        {
            return false;
        }

        if (index + width > value.size())
            return false;
        for (std::size_t offset = 1; offset < width; ++offset)
        {
            const auto continuation = static_cast<std::uint8_t>(value[index + offset]);
            if ((continuation & 0xC0U) != 0x80U)
                return false;
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }

        const bool overlong = (width == 2 && codePoint < 0x80U) || (width == 3 && codePoint < 0x800U) ||
                              (width == 4 && codePoint < 0x10000U);
        if (overlong || codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            return false;
        if ((codePoint < 0x20U && codePoint != '\t') || codePoint == 0x7FU)
            return false;

        index += width;
        ++codePointCount;
    }
    return true;
}
}  // namespace

std::optional<ChannelKind> parseChannelKind(std::string_view value) noexcept
{
    if (value == "language")
        return ChannelKind::Language;
    if (value == "guild")
        return ChannelKind::Guild;
    if (value == "direct")
        return ChannelKind::Direct;
    return std::nullopt;
}

std::string_view channelKindName(ChannelKind kind) noexcept
{
    switch (kind)
    {
    case ChannelKind::Language:
        return "language";
    case ChannelKind::Guild:
        return "guild";
    case ChannelKind::Direct:
        return "direct";
    }
    return "language";
}

bool isSupportedLanguage(std::string_view language) noexcept
{
    constexpr std::array<std::string_view, 4> languages{"en", "zh", "ko", "ja"};
    return std::find(languages.begin(), languages.end(), language) != languages.end();
}

bool validateIdentity(const Identity& identity, std::string& error)
{
    if (!isSafeIdentifier(identity.userId, MAX_USER_ID_BYTES))
    {
        error = "userId must contain 1-64 ASCII letters, digits, '.', '_' or '-'";
        return false;
    }
    if (identity.displayName.empty() || identity.displayName.size() > MAX_DISPLAY_NAME_BYTES)
    {
        error = "displayName must contain 1-48 UTF-8 bytes";
        return false;
    }
    std::size_t displayNameCodePoints = 0;
    if (!isValidUtf8(identity.displayName, displayNameCodePoints) || displayNameCodePoints > 16)
    {
        error = "displayName must be valid UTF-8 without control characters and at most 16 characters";
        return false;
    }
    if (!isSupportedLanguage(identity.language))
    {
        error = "language must be one of en, zh, ko or ja";
        return false;
    }
    if (!identity.guildId.empty() && !isSafeIdentifier(identity.guildId, MAX_GUILD_ID_BYTES))
    {
        error = "guildId must contain at most 64 ASCII letters, digits, '.', '_' or '-'";
        return false;
    }
    return true;
}

bool validateChannel(const Identity& identity, const Channel& channel, std::string& error)
{
    switch (channel.kind)
    {
    case ChannelKind::Language:
        if (!isSupportedLanguage(channel.scope))
        {
            error = "language channel scope must be one of en, zh, ko or ja";
            return false;
        }
        return true;
    case ChannelKind::Guild:
        if (identity.guildId.empty())
        {
            error = "guild chat requires guild membership";
            return false;
        }
        if (!channel.scope.empty() && channel.scope != identity.guildId)
        {
            error = "guild channel scope does not match the authenticated guild";
            return false;
        }
        return true;
    case ChannelKind::Direct:
        if (!isSafeIdentifier(channel.scope, MAX_USER_ID_BYTES))
        {
            error = "direct channel scope must be a valid target userId";
            return false;
        }
        if (channel.scope == identity.userId)
        {
            error = "direct channel target must be another user";
            return false;
        }
        return true;
    }
    error = "unsupported channel";
    return false;
}

bool validateMessageText(std::string_view text, std::string& normalized, std::string& error)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        error = "message text cannot be empty";
        return false;
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    text            = text.substr(first, last - first + 1);
    if (text.size() > MAX_MESSAGE_BYTES)
    {
        error = "message text exceeds 768 UTF-8 bytes";
        return false;
    }

    std::size_t codePointCount = 0;
    if (!isValidUtf8(text, codePointCount))
    {
        error = "message text must be valid UTF-8 without control characters";
        return false;
    }
    if (codePointCount > MAX_MESSAGE_CODE_POINTS)
    {
        error = "message text exceeds 200 characters";
        return false;
    }

    normalized.assign(text);
    return true;
}

std::string roomKey(const Identity& identity, const Channel& channel)
{
    switch (channel.kind)
    {
    case ChannelKind::Language:
        return "language:" + channel.scope;
    case ChannelKind::Guild:
        return "guild:" + identity.guildId;
    case ChannelKind::Direct:
        return identity.userId < channel.scope ? "direct:" + identity.userId + ':' + channel.scope
                                               : "direct:" + channel.scope + ':' + identity.userId;
    }
    return {};
}

}  // namespace cmc::server::chat
