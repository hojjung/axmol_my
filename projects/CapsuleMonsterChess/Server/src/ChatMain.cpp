#include "ChatServer.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
std::uint16_t readPort()
{
    constexpr std::uint16_t defaultPort = 8090;
    const char* value                   = std::getenv("PORT");
    if (value == nullptr)
        return defaultPort;

    unsigned int port = 0;
    const std::string_view text(value);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || port == 0 || port > 65535)
        throw std::runtime_error("PORT must be an integer between 1 and 65535");
    return static_cast<std::uint16_t>(port);
}

unsigned int readThreadCount()
{
    const unsigned int fallback = std::clamp(std::thread::hardware_concurrency(), 1U, 8U);
    const char* value           = std::getenv("CMC_CHAT_THREADS");
    if (value == nullptr)
        return fallback;

    unsigned int count = 0;
    const std::string_view text(value);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), count);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || count == 0 || count > 32)
        throw std::runtime_error("CMC_CHAT_THREADS must be an integer between 1 and 32");
    return count;
}
}  // namespace

int main()
{
    try
    {
        const std::uint16_t port       = readPort();
        const unsigned int threadCount = readThreadCount();
        boost::asio::io_context ioContext(static_cast<int>(threadCount));
        cmc::server::ChatServer server(ioContext, port);

        boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            server.stop();
            ioContext.stop();
        });

        server.start();
        std::cout << "cmc_chat_server listening on 0.0.0.0:" << port << " with " << threadCount << " threads\n";

        std::vector<std::thread> workers;
        workers.reserve(threadCount > 0 ? threadCount - 1 : 0);
        for (unsigned int index = 1; index < threadCount; ++index)
            workers.emplace_back([&ioContext] { ioContext.run(); });
        ioContext.run();
        for (std::thread& worker : workers)
            worker.join();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "cmc_chat_server startup failed: " << error.what() << '\n';
        return 1;
    }
}
