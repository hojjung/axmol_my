#include "HttpServer.h"

#include "HttpCompression.h"

#include <boost/asio/socket_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cmc::server
{
namespace
{
namespace beast = boost::beast;
namespace http  = beast::http;
using Tcp       = boost::asio::ip::tcp;

constexpr std::uint64_t MAX_REQUEST_BODY_BYTES = 64U * 1024U;

http::response<http::string_body> makeResponse(const http::request<http::string_body>& request,
                                               unsigned int status,
                                               std::string body)
{
    http::response<http::string_body> response{static_cast<http::status>(status), request.version()};
    response.set(http::field::server, "cmc-battle-server/1");
    response.set(http::field::content_type, "application/json; charset=utf-8");
    response.set(http::field::vary, "Accept-Encoding");
    response.keep_alive(request.keep_alive());

    const auto acceptEncoding = request.find(http::field::accept_encoding);
    if (body.size() >= http_support::MIN_GZIP_RESPONSE_BYTES && acceptEncoding != request.end())
    {
        const auto header = acceptEncoding->value();
        if (http_support::acceptsGzip(std::string_view{header.data(), header.size()}))
        {
            if (std::optional<std::string> compressed = http_support::gzipCompress(body);
                compressed && compressed->size() < body.size())
            {
                body = std::move(*compressed);
                response.set(http::field::content_encoding, "gzip");
            }
        }
    }

    response.body() = std::move(body);
    response.prepare_payload();
    return response;
}

http::response<http::string_body> route(const http::request<http::string_body>& request, const BattleApi& api)
{
    const std::string_view target{request.target().data(), request.target().size()};
    if (target == "/healthz")
    {
        if (request.method() != http::verb::get)
            return makeResponse(request, 405, R"({"error":{"code":"METHOD_NOT_ALLOWED","message":"Use GET"}})");
        ApiResponse response = api.health();
        return makeResponse(request, response.status, std::move(response.body));
    }

    if (target == "/v1/battles:simulate")
    {
        if (request.method() != http::verb::post)
            return makeResponse(request, 405, R"({"error":{"code":"METHOD_NOT_ALLOWED","message":"Use POST"}})");
        ApiResponse response = api.simulate(request.body());
        return makeResponse(request, response.status, std::move(response.body));
    }

    return makeResponse(request, 404, R"({"error":{"code":"NOT_FOUND","message":"Route not found"}})");
}

class HttpSession final : public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(Tcp::socket socket, const BattleApi& api) : socket_(std::move(socket)), api_(api) {}

    void run() { read(); }

private:
    void read()
    {
        parser_.emplace();
        parser_->body_limit(MAX_REQUEST_BODY_BYTES);
        http::async_read(socket_, buffer_, *parser_,
                         [self = shared_from_this()](const beast::error_code& error, std::size_t) {
            if (error == http::error::end_of_stream)
            {
                self->close();
                return;
            }
            if (error)
                return;
            self->write(route(self->parser_->get(), self->api_));
        });
    }

    void write(http::response<http::string_body> response)
    {
        const bool closeAfterWrite = response.need_eof();
        auto sharedResponse        = std::make_shared<http::response<http::string_body>>(std::move(response));
        http::async_write(
            socket_, *sharedResponse,
            [self = shared_from_this(), sharedResponse, closeAfterWrite](const beast::error_code& error, std::size_t) {
            if (error)
                return;
            if (closeAfterWrite)
            {
                self->close();
                return;
            }
            self->parser_.reset();
            self->read();
        });
    }

    void close()
    {
        beast::error_code ignored;
        socket_.shutdown(Tcp::socket::shutdown_send, ignored);
    }

    Tcp::socket socket_;
    beast::flat_buffer buffer_;
    std::optional<http::request_parser<http::string_body>> parser_;
    const BattleApi& api_;
};
}  // namespace

HttpServer::HttpServer(boost::asio::io_context& ioContext, std::uint16_t port, const BattleApi& api)
    : acceptor_(ioContext), api_(api)
{
    const Tcp::endpoint endpoint{boost::asio::ip::make_address("0.0.0.0"), port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

void HttpServer::start()
{
    accept();
}

void HttpServer::stop()
{
    beast::error_code ignored;
    acceptor_.cancel(ignored);
    acceptor_.close(ignored);
}

void HttpServer::accept()
{
    acceptor_.async_accept([this](const beast::error_code& error, Tcp::socket socket) {
        if (!error)
            std::make_shared<HttpSession>(std::move(socket), api_)->run();
        if (acceptor_.is_open())
            accept();
    });
}
}  // namespace cmc::server
