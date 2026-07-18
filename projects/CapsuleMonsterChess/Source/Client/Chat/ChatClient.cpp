#include "Client/Chat/ChatClient.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace cmc::client
{
namespace
{
std::string_view channelName(ChatChannelKind kind) noexcept
{
    switch (kind)
    {
    case ChatChannelKind::Language:
        return "language";
    case ChatChannelKind::Guild:
        return "guild";
    case ChatChannelKind::Direct:
        return "direct";
    }
    return "language";
}

bool parseChannelKind(std::string_view value, ChatChannelKind& kind) noexcept
{
    if (value == "language")
        kind = ChatChannelKind::Language;
    else if (value == "guild")
        kind = ChatChannelKind::Guild;
    else if (value == "direct")
        kind = ChatChannelKind::Direct;
    else
        return false;
    return true;
}

bool readString(const rapidjson::Value& object, const char* key, std::string& output)
{
    const auto member = object.FindMember(key);
    if (member == object.MemberEnd() || !member->value.IsString())
        return false;
    output.assign(member->value.GetString(), member->value.GetStringLength());
    return true;
}

bool parseMessage(const rapidjson::Value& object, ChatMessage& output)
{
    std::string channel;
    const auto id        = object.FindMember("messageId");
    const auto timestamp = object.FindMember("timestampMs");
    if (!object.IsObject() || id == object.MemberEnd() || !id->value.IsUint64() || timestamp == object.MemberEnd() ||
        !timestamp->value.IsInt64() || !readString(object, "channel", channel) ||
        !parseChannelKind(channel, output.channel) || !readString(object, "scope", output.scope) ||
        !readString(object, "senderId", output.senderId) || !readString(object, "senderName", output.senderName) ||
        !readString(object, "language", output.senderLanguage) || !readString(object, "text", output.text))
    {
        return false;
    }
    output.messageId   = id->value.GetUint64();
    output.timestampMs = timestamp->value.GetInt64();
    return true;
}

template <typename WriteBody>
std::string writeCommand(std::string_view type, WriteBody&& writeBody)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("schemaVersion");
    writer.Int(CHAT_SCHEMA_VERSION);
    writer.Key("type");
    writer.String(type.data(), static_cast<rapidjson::SizeType>(type.size()));
    writeBody(writer);
    writer.EndObject();
    return {buffer.GetString(), buffer.GetSize()};
}
}  // namespace

std::string normalizeChatLanguage(std::string_view languageCode)
{
    std::string normalized;
    normalized.reserve(2);
    for (char character : languageCode)
    {
        if (character == '-' || character == '_')
            break;
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        if (normalized.size() == 2)
            break;
    }
    if (normalized == "zh" || normalized == "ko" || normalized == "ja")
        return normalized;
    return "en";
}

std::string_view chatFontPath(std::string_view language) noexcept
{
    if (language == "zh")
        return "fonts/NotoSansSC-Regular.otf";
    if (language == "ko")
        return "fonts/NotoSansKR-Regular.ttf";
    if (language == "ja")
        return "fonts/NotoSansJP-Regular.ttf";
    return "fonts/NotoSans-Regular.ttf";
}

std::string chatRoomKey(std::string_view localUserId, const ChatChannel& channel)
{
    switch (channel.kind)
    {
    case ChatChannelKind::Language:
        return "language:" + channel.scope;
    case ChatChannelKind::Guild:
        return "guild:" + channel.scope;
    case ChatChannelKind::Direct:
        return localUserId < channel.scope ? "direct:" + std::string(localUserId) + ':' + channel.scope
                                           : "direct:" + channel.scope + ':' + std::string(localUserId);
    }
    return {};
}

ChatClient::ChatClient() = default;

ChatClient::~ChatClient()
{
    intentionalDisconnect_ = true;
    socket_.reset();
}

bool ChatClient::connect(std::string url, ChatIdentity identity)
{
    disconnect();
    if (url.empty() || identity.userId.empty() || identity.displayName.empty())
    {
        reportError("Chat URL and identity are required");
        return false;
    }

    url_                   = std::move(url);
    identity_              = std::move(identity);
    identity_.language     = normalizeChatLanguage(identity_.language);
    selectedChannel_       = {ChatChannelKind::Language, identity_.language};
    serverReady_           = false;
    intentionalDisconnect_ = false;
    socket_                = std::make_unique<ax::network::WebSocket>();
    changeState(ChatConnectionState::Connecting);
    if (!socket_->open(this, url_))
    {
        socket_.reset();
        changeState(ChatConnectionState::Disconnected);
        reportError("Failed to start the chat WebSocket connection");
        return false;
    }
    return true;
}

void ChatClient::disconnect()
{
    intentionalDisconnect_ = true;
    serverReady_           = false;
    socket_.reset();
    changeState(ChatConnectionState::Disconnected);
}

void ChatClient::selectChannel(ChatChannel channel)
{
    if (channel.kind == ChatChannelKind::Language)
        channel.scope = normalizeChatLanguage(channel.scope);
    else if (channel.kind == ChatChannelKind::Guild)
        channel.scope = identity_.guildId;
    selectedChannel_ = std::move(channel);
    if (serverReady_)
        sendSelection();
}

bool ChatClient::sendMessage(std::string_view text)
{
    if (!socket_ || state_ != ChatConnectionState::Connected || !serverReady_ || text.empty())
        return false;

    socket_->send(writeCommand("send", [text](auto& writer) {
        writer.Key("text");
        writer.String(text.data(), static_cast<rapidjson::SizeType>(text.size()));
    }));
    return true;
}

void ChatClient::setStateCallback(StateCallback callback)
{
    stateCallback_ = std::move(callback);
}

void ChatClient::setHistoryCallback(HistoryCallback callback)
{
    historyCallback_ = std::move(callback);
}

void ChatClient::setMessageCallback(MessageCallback callback)
{
    messageCallback_ = std::move(callback);
}

void ChatClient::setErrorCallback(ErrorCallback callback)
{
    errorCallback_ = std::move(callback);
}

void ChatClient::onOpen(ax::network::WebSocket* socket)
{
    if (socket != socket_.get())
        return;
    sendHello();
}

void ChatClient::onMessage(ax::network::WebSocket* socket, const ax::network::WebSocket::Data& data)
{
    if (socket != socket_.get() || data.isBinary)
        return;

    rapidjson::Document document;
    document.Parse(data.bytes, data.len);
    if (document.HasParseError() || !document.IsObject())
    {
        reportError("Chat server returned invalid JSON");
        return;
    }

    std::string type;
    if (!readString(document, "type", type))
    {
        reportError("Chat server response is missing a type");
        return;
    }
    if (type == "ready")
    {
        serverReady_ = true;
        changeState(ChatConnectionState::Connected);
        sendSelection();
        return;
    }
    if (type == "error")
    {
        std::string message;
        reportError(readString(document, "message", message) ? std::move(message) : "Unknown chat server error");
        return;
    }
    if (type == "message")
    {
        ChatMessage message;
        if (!parseMessage(document, message))
            reportError("Chat message response is invalid");
        else if (messageCallback_)
            messageCallback_(std::move(message));
        return;
    }
    if (type == "history")
    {
        ChatChannel channel;
        std::string channelNameValue;
        if (!readString(document, "channel", channelNameValue) || !parseChannelKind(channelNameValue, channel.kind) ||
            !readString(document, "scope", channel.scope))
        {
            reportError("Chat history channel is invalid");
            return;
        }
        const auto messagesMember = document.FindMember("messages");
        if (messagesMember == document.MemberEnd() || !messagesMember->value.IsArray())
        {
            reportError("Chat history messages are invalid");
            return;
        }

        std::vector<ChatMessage> messages;
        messages.reserve(messagesMember->value.Size());
        for (const rapidjson::Value& value : messagesMember->value.GetArray())
        {
            ChatMessage message;
            if (!parseMessage(value, message))
            {
                reportError("Chat history contains an invalid message");
                return;
            }
            messages.emplace_back(std::move(message));
        }
        if (historyCallback_)
            historyCallback_(channel, std::move(messages));
    }
}

void ChatClient::onClose(ax::network::WebSocket* socket, std::uint16_t, std::string_view)
{
    if (socket != socket_.get())
        return;
    serverReady_ = false;
    changeState(ChatConnectionState::Disconnected);
    if (!intentionalDisconnect_)
        reportError("Chat connection closed; reconnecting shortly");
}

void ChatClient::onError(ax::network::WebSocket* socket, const ax::network::WebSocket::ErrorCode& error)
{
    if (socket != socket_.get())
        return;
    serverReady_ = false;
    changeState(ChatConnectionState::Disconnected);
    reportError("Chat network error " + std::to_string(static_cast<int>(error)));
}

void ChatClient::sendHello()
{
    if (!socket_)
        return;
    socket_->send(writeCommand("hello", [this](auto& writer) {
        const auto writeString = [&writer](const char* key, const std::string& value) {
            writer.Key(key);
            writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
        };
        writeString("userId", identity_.userId);
        writeString("displayName", identity_.displayName);
        writeString("language", identity_.language);
        writeString("guildId", identity_.guildId);
    }));
}

void ChatClient::sendSelection()
{
    if (!socket_ || !serverReady_)
        return;
    socket_->send(writeCommand("select", [this](auto& writer) {
        const std::string_view kind = channelName(selectedChannel_.kind);
        writer.Key("channel");
        writer.String(kind.data(), static_cast<rapidjson::SizeType>(kind.size()));
        writer.Key("scope");
        writer.String(selectedChannel_.scope.data(), static_cast<rapidjson::SizeType>(selectedChannel_.scope.size()));
    }));
}

void ChatClient::changeState(ChatConnectionState state)
{
    if (state_ == state)
        return;
    state_ = state;
    if (stateCallback_)
        stateCallback_(state_);
}

void ChatClient::reportError(std::string error)
{
    if (errorCallback_)
        errorCallback_(std::move(error));
}
}  // namespace cmc::client
