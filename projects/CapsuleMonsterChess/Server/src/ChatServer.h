#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>

namespace boost::asio
{
class io_context;
}

namespace cmc::server
{
class ChatHub;

class ChatServer final
{
public:
    ChatServer(boost::asio::io_context& ioContext, std::uint16_t port);
    ~ChatServer();

    ChatServer(const ChatServer&)            = delete;
    ChatServer& operator=(const ChatServer&) = delete;

    void start();
    void stop();
    [[nodiscard]] std::uint16_t port() const;

private:
    void accept();

    boost::asio::io_context& ioContext_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<ChatHub> hub_;
};
}  // namespace cmc::server
