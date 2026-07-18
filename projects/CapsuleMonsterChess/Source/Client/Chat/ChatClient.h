#pragma once

#include "axmol/network/WebSocket.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cmc::client
{
inline constexpr int CHAT_SCHEMA_VERSION = 1;

enum class ChatChannelKind
{
    Language,
    Guild,
    Direct,
};

enum class ChatConnectionState
{
    Disconnected,
    Connecting,
    Connected,
};

struct ChatIdentity final
{
    std::string userId;
    std::string displayName;
    std::string language;
    std::string guildId;
};

struct ChatChannel final
{
    ChatChannelKind kind = ChatChannelKind::Language;
    std::string scope;
};

struct ChatMessage final
{
    std::uint64_t messageId = 0;
    ChatChannelKind channel = ChatChannelKind::Language;
    std::string scope;
    std::string senderId;
    std::string senderName;
    std::string senderLanguage;
    std::string text;
    std::int64_t timestampMs = 0;
};

[[nodiscard]] std::string normalizeChatLanguage(std::string_view languageCode);
[[nodiscard]] std::string_view chatFontPath(std::string_view language) noexcept;
[[nodiscard]] std::string chatRoomKey(std::string_view localUserId, const ChatChannel& channel);

class ChatClient final : private ax::network::WebSocket::Delegate
{
public:
    using StateCallback   = std::function<void(ChatConnectionState)>;
    using HistoryCallback = std::function<void(const ChatChannel&, std::vector<ChatMessage>)>;
    using MessageCallback = std::function<void(ChatMessage)>;
    using ErrorCallback   = std::function<void(std::string)>;

    ChatClient();
    ~ChatClient() override;

    ChatClient(const ChatClient&)            = delete;
    ChatClient& operator=(const ChatClient&) = delete;

    bool connect(std::string url, ChatIdentity identity);
    void disconnect();
    void selectChannel(ChatChannel channel);
    bool sendMessage(std::string_view text);

    void setStateCallback(StateCallback callback);
    void setHistoryCallback(HistoryCallback callback);
    void setMessageCallback(MessageCallback callback);
    void setErrorCallback(ErrorCallback callback);

    [[nodiscard]] ChatConnectionState state() const noexcept { return state_; }
    [[nodiscard]] const ChatIdentity& identity() const noexcept { return identity_; }
    [[nodiscard]] const ChatChannel& selectedChannel() const noexcept { return selectedChannel_; }

private:
    void onOpen(ax::network::WebSocket* socket) override;
    void onMessage(ax::network::WebSocket* socket, const ax::network::WebSocket::Data& data) override;
    void onClose(ax::network::WebSocket* socket, std::uint16_t code, std::string_view reason) override;
    void onError(ax::network::WebSocket* socket, const ax::network::WebSocket::ErrorCode& error) override;

    void sendHello();
    void sendSelection();
    void changeState(ChatConnectionState state);
    void reportError(std::string error);

    std::unique_ptr<ax::network::WebSocket> socket_;
    std::string url_;
    ChatIdentity identity_;
    ChatChannel selectedChannel_;
    ChatConnectionState state_ = ChatConnectionState::Disconnected;
    StateCallback stateCallback_;
    HistoryCallback historyCallback_;
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    bool serverReady_           = false;
    bool intentionalDisconnect_ = false;
};
}  // namespace cmc::client
