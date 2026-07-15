#pragma once

#include "BattleApi.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>

namespace cmc::server
{
class HttpServer final
{
public:
    HttpServer(boost::asio::io_context& ioContext, std::uint16_t port, const BattleApi& api);

    void start();
    void stop();

private:
    void accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    const BattleApi& api_;
};
}  // namespace cmc::server
