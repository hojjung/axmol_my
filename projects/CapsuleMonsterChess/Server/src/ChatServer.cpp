#include "ChatServer.h"

#include "ChatProtocol.h"

#include "Util/json.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cmc::server
{
namespace
{
namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
using Json          = nlohmann::json;
using Tcp           = asio::ip::tcp;

constexpr std::size_t MAX_COMMAND_BYTES = 4U * 1024U;
constexpr std::size_t MAX_RATE_MESSAGES = 5;
constexpr auto RATE_WINDOW              = std::chrono::seconds(10);

class ChatSession;

std::string makeError(std::string_view code, std::string_view message)
{
    return Json{{"type", "error"}, {"code", code}, {"message", message}}.dump();
}

bool readString(const Json& object, std::string_view key, std::string& value)
{
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_string())
        return false;
    value = iterator->get<std::string>();
    return true;
}

std::int64_t unixTimeMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
}  // namespace

class ChatHub final
{
public:
    void join(std::uint64_t sessionId, const std::shared_ptr<ChatSession>& session, chat::Identity identity);
    void leave(std::uint64_t sessionId);
    std::string select(std::uint64_t sessionId, const chat::Channel& channel);
    void publish(std::uint64_t sessionId, const chat::Channel& channel, std::string text);
    [[nodiscard]] std::size_t connectionCount() const;

private:
    struct Connection final
    {
        std::weak_ptr<ChatSession> session;
        chat::Identity identity;
        std::string activeRoom;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, Connection> connections_;
    std::unordered_map<std::string, std::deque<Json>> history_;
    std::uint64_t nextMessageId_ = 1;
};

namespace
{
class ChatSession final : public std::enable_shared_from_this<ChatSession>
{
public:
    ChatSession(Tcp::socket socket, std::shared_ptr<ChatHub> hub)
        : id_(nextSessionId_.fetch_add(1, std::memory_order_relaxed)), socket_(std::move(socket)), hub_(std::move(hub))
    {}

    ~ChatSession() { hub_->leave(id_); }

    void run()
    {
        requestParser_.emplace();
        requestParser_->body_limit(MAX_COMMAND_BYTES);
        http::async_read(socket_.next_layer(), readBuffer_, *requestParser_,
                         [self = shared_from_this()](const beast::error_code& error, std::size_t) {
            if (error)
            {
                self->fail(error, "HTTP handshake read");
                return;
            }
            self->routeHandshake(self->requestParser_->release());
        });
    }

    void enqueue(std::string payload)
    {
        asio::post(socket_.get_executor(), [self = shared_from_this(), payload = std::move(payload)]() mutable {
            const bool writeInProgress = !self->writeQueue_.empty();
            self->writeQueue_.emplace_back(std::move(payload));
            if (!writeInProgress)
                self->write();
        });
    }

private:
    void routeHandshake(http::request<http::string_body> request)
    {
        const std::string_view target{request.target().data(), request.target().size()};
        if (websocket::is_upgrade(request) && target == "/v1/chat")
        {
            socket_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
            socket_.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& response) { response.set(http::field::server, "cmc-chat-server/1"); }));
            socket_.read_message_max(MAX_COMMAND_BYTES);
            socket_.async_accept(request, [self = shared_from_this()](const beast::error_code& error) {
                if (error)
                {
                    self->fail(error, "WebSocket accept");
                    return;
                }
                self->read();
            });
            return;
        }

        const bool healthRoute = target == "/healthz" && request.method() == http::verb::get;
        auto response          = std::make_shared<http::response<http::string_body>>(
            healthRoute ? http::status::ok : http::status::not_found, request.version());
        response->set(http::field::server, "cmc-chat-server/1");
        response->set(http::field::content_type, "application/json; charset=utf-8");
        response->keep_alive(false);
        response->body() = healthRoute
                               ? Json{{"status", "ok"},
                                      {"service", "cmc-chat-server"},
                                      {"schemaVersion", chat::CHAT_SCHEMA_VERSION},
                                      {"connections", hub_->connectionCount()}}
                                     .dump()
                               : R"({"error":{"code":"NOT_FOUND","message":"Use GET /healthz or WebSocket /v1/chat"}})";
        response->prepare_payload();
        http::async_write(socket_.next_layer(), *response,
                          [self = shared_from_this(), response](const beast::error_code&, std::size_t) {
            beast::error_code ignored;
            self->socket_.next_layer().socket().shutdown(Tcp::socket::shutdown_send, ignored);
        });
    }

    void read()
    {
        socket_.async_read(readBuffer_, [self = shared_from_this()](const beast::error_code& error, std::size_t) {
            if (error == websocket::error::closed)
                return;
            if (error)
            {
                self->fail(error, "WebSocket read");
                return;
            }

            if (!self->socket_.got_text())
            {
                self->enqueue(makeError("BINARY_NOT_SUPPORTED", "Chat commands must be UTF-8 JSON text"));
            }
            else
            {
                const std::string payload = beast::buffers_to_string(self->readBuffer_.data());
                self->handle(payload);
            }
            self->readBuffer_.consume(self->readBuffer_.size());
            self->read();
        });
    }

    void handle(std::string_view payload)
    {
        const Json command = Json::parse(payload, nullptr, false);
        if (command.is_discarded() || !command.is_object())
        {
            enqueue(makeError("INVALID_JSON", "Command must be a JSON object"));
            return;
        }

        const auto schema = command.find("schemaVersion");
        if (schema == command.end() || !schema->is_number_integer() || schema->get<int>() != chat::CHAT_SCHEMA_VERSION)
        {
            enqueue(makeError("SCHEMA_MISMATCH", "Unsupported or missing schemaVersion"));
            return;
        }

        std::string type;
        if (!readString(command, "type", type))
        {
            enqueue(makeError("INVALID_COMMAND", "Command type is required"));
            return;
        }

        if (type == "hello")
        {
            handleHello(command);
            return;
        }
        if (!identity_)
        {
            enqueue(makeError("HELLO_REQUIRED", "Send hello before selecting a channel"));
            return;
        }
        if (type == "select")
        {
            handleSelect(command);
            return;
        }
        if (type == "send")
        {
            handleSend(command);
            return;
        }
        enqueue(makeError("UNKNOWN_COMMAND", "Supported commands are hello, select and send"));
    }

    void handleHello(const Json& command)
    {
        if (identity_)
        {
            enqueue(makeError("ALREADY_IDENTIFIED", "hello can only be sent once per connection"));
            return;
        }

        chat::Identity candidate;
        if (!readString(command, "userId", candidate.userId) ||
            !readString(command, "displayName", candidate.displayName) ||
            !readString(command, "language", candidate.language))
        {
            enqueue(makeError("INVALID_IDENTITY", "hello requires userId, displayName and language strings"));
            return;
        }
        const auto guild = command.find("guildId");
        if (guild != command.end())
        {
            if (!guild->is_string())
            {
                enqueue(makeError("INVALID_IDENTITY", "guildId must be a string"));
                return;
            }
            candidate.guildId = guild->get<std::string>();
        }

        std::string error;
        if (!chat::validateIdentity(candidate, error))
        {
            enqueue(makeError("INVALID_IDENTITY", error));
            return;
        }

        identity_ = std::move(candidate);
        hub_->join(id_, shared_from_this(), *identity_);
        enqueue(Json{{"type", "ready"},
                     {"schemaVersion", chat::CHAT_SCHEMA_VERSION},
                     {"userId", identity_->userId},
                     {"language", identity_->language},
                     {"guildId", identity_->guildId}}
                    .dump());
    }

    void handleSelect(const Json& command)
    {
        std::string kindName;
        chat::Channel candidate;
        if (!readString(command, "channel", kindName))
        {
            enqueue(makeError("INVALID_CHANNEL", "select requires a channel string"));
            return;
        }
        const std::optional<chat::ChannelKind> kind = chat::parseChannelKind(kindName);
        if (!kind)
        {
            enqueue(makeError("INVALID_CHANNEL", "channel must be language, guild or direct"));
            return;
        }
        candidate.kind   = *kind;
        const auto scope = command.find("scope");
        if (scope != command.end())
        {
            if (!scope->is_string())
            {
                enqueue(makeError("INVALID_CHANNEL", "channel scope must be a string"));
                return;
            }
            candidate.scope = scope->get<std::string>();
        }
        if (candidate.kind == chat::ChannelKind::Guild && candidate.scope.empty())
            candidate.scope = identity_->guildId;

        std::string error;
        if (!chat::validateChannel(*identity_, candidate, error))
        {
            enqueue(makeError("INVALID_CHANNEL", error));
            return;
        }

        channel_ = std::move(candidate);
        enqueue(hub_->select(id_, *channel_));
    }

    void handleSend(const Json& command)
    {
        if (!channel_)
        {
            enqueue(makeError("CHANNEL_REQUIRED", "Select a channel before sending messages"));
            return;
        }

        std::string text;
        if (!readString(command, "text", text))
        {
            enqueue(makeError("INVALID_MESSAGE", "send requires a text string"));
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        while (!recentMessages_.empty() && now - recentMessages_.front() >= RATE_WINDOW)
            recentMessages_.pop_front();
        if (recentMessages_.size() >= MAX_RATE_MESSAGES)
        {
            enqueue(makeError("RATE_LIMITED", "Send at most 5 messages per 10 seconds"));
            return;
        }

        std::string normalized;
        std::string error;
        if (!chat::validateMessageText(text, normalized, error))
        {
            enqueue(makeError("INVALID_MESSAGE", error));
            return;
        }

        recentMessages_.push_back(now);
        hub_->publish(id_, *channel_, std::move(normalized));
    }

    void write()
    {
        socket_.text(true);
        socket_.async_write(asio::buffer(writeQueue_.front()),
                            [self = shared_from_this()](const beast::error_code& error, std::size_t) {
            if (error)
            {
                self->fail(error, "WebSocket write");
                return;
            }
            self->writeQueue_.pop_front();
            if (!self->writeQueue_.empty())
                self->write();
        });
    }

    void fail(const beast::error_code& error, std::string_view operation)
    {
        if (error != asio::error::operation_aborted && error != asio::error::eof &&
            error != asio::error::connection_reset && error != asio::error::bad_descriptor &&
            error != websocket::error::closed)
            std::cerr << "cmc_chat_server " << operation << " failed: " << error.message() << '\n';
        hub_->leave(id_);
    }

    inline static std::atomic<std::uint64_t> nextSessionId_{1};

    const std::uint64_t id_;
    websocket::stream<beast::tcp_stream> socket_;
    std::shared_ptr<ChatHub> hub_;
    beast::flat_buffer readBuffer_;
    std::optional<http::request_parser<http::string_body>> requestParser_;
    std::deque<std::string> writeQueue_;
    std::optional<chat::Identity> identity_;
    std::optional<chat::Channel> channel_;
    std::deque<std::chrono::steady_clock::time_point> recentMessages_;
};
}  // namespace

void ChatHub::join(std::uint64_t sessionId, const std::shared_ptr<ChatSession>& session, chat::Identity identity)
{
    std::lock_guard lock(mutex_);
    connections_.insert_or_assign(sessionId, Connection{session, std::move(identity), {}});
}

void ChatHub::leave(std::uint64_t sessionId)
{
    std::lock_guard lock(mutex_);
    connections_.erase(sessionId);
}

std::string ChatHub::select(std::uint64_t sessionId, const chat::Channel& channel)
{
    std::lock_guard lock(mutex_);
    const auto connection = connections_.find(sessionId);
    if (connection == connections_.end())
        return makeError("SESSION_CLOSED", "Chat session is no longer registered");

    connection->second.activeRoom = chat::roomKey(connection->second.identity, channel);
    Json messages                 = Json::array();
    if (const auto history = history_.find(connection->second.activeRoom); history != history_.end())
    {
        for (const Json& message : history->second)
            messages.push_back(message);
    }
    return Json{{"type", "history"},
                {"schemaVersion", chat::CHAT_SCHEMA_VERSION},
                {"channel", chat::channelKindName(channel.kind)},
                {"scope", channel.scope},
                {"messages", std::move(messages)}}
        .dump();
}

void ChatHub::publish(std::uint64_t sessionId, const chat::Channel& channel, std::string text)
{
    std::vector<std::shared_ptr<ChatSession>> recipients;
    std::string payload;
    {
        std::lock_guard lock(mutex_);
        const auto sender = connections_.find(sessionId);
        if (sender == connections_.end())
            return;

        const std::string key = chat::roomKey(sender->second.identity, channel);
        if (sender->second.activeRoom != key)
            return;

        Json message{{"messageId", nextMessageId_++},
                     {"channel", chat::channelKindName(channel.kind)},
                     {"scope", channel.scope},
                     {"senderId", sender->second.identity.userId},
                     {"senderName", sender->second.identity.displayName},
                     {"language", sender->second.identity.language},
                     {"text", std::move(text)},
                     {"timestampMs", unixTimeMilliseconds()}};
        auto& roomHistory = history_[key];
        roomHistory.emplace_back(message);
        if (roomHistory.size() > chat::MAX_HISTORY_PER_CHANNEL)
            roomHistory.pop_front();

        message["type"]          = "message";
        message["schemaVersion"] = chat::CHAT_SCHEMA_VERSION;
        payload                  = message.dump();

        for (auto iterator = connections_.begin(); iterator != connections_.end();)
        {
            std::shared_ptr<ChatSession> session = iterator->second.session.lock();
            if (!session)
            {
                iterator = connections_.erase(iterator);
                continue;
            }

            bool shouldDeliver = iterator->second.activeRoom == key;
            if (channel.kind == chat::ChannelKind::Direct)
            {
                const std::string& userId = iterator->second.identity.userId;
                shouldDeliver             = userId == sender->second.identity.userId || userId == channel.scope;
            }
            if (shouldDeliver)
                recipients.emplace_back(std::move(session));
            ++iterator;
        }
    }

    for (const auto& recipient : recipients)
        recipient->enqueue(payload);
}

std::size_t ChatHub::connectionCount() const
{
    std::lock_guard lock(mutex_);
    return connections_.size();
}

ChatServer::ChatServer(asio::io_context& ioContext, std::uint16_t port)
    : ioContext_(ioContext), acceptor_(ioContext), hub_(std::make_shared<ChatHub>())
{
    const Tcp::endpoint endpoint{asio::ip::make_address("0.0.0.0"), port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);
}

ChatServer::~ChatServer() = default;

void ChatServer::start()
{
    accept();
}

void ChatServer::stop()
{
    beast::error_code ignored;
    acceptor_.cancel(ignored);
    acceptor_.close(ignored);
}

std::uint16_t ChatServer::port() const
{
    beast::error_code error;
    const auto endpoint = acceptor_.local_endpoint(error);
    return error ? 0 : endpoint.port();
}

void ChatServer::accept()
{
    acceptor_.async_accept(asio::make_strand(ioContext_), [this](const beast::error_code& error, Tcp::socket socket) {
        if (!error)
            std::make_shared<ChatSession>(std::move(socket), hub_)->run();
        else if (error != asio::error::operation_aborted)
            std::cerr << "cmc_chat_server accept failed: " << error.message() << '\n';
        if (acceptor_.is_open())
            accept();
    });
}
}  // namespace cmc::server
