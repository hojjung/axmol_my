#include "AxmolRuntime.h"
#include "BattleApi.h"
#include "ChatProtocol.h"
#include "ChatServer.h"
#include "HttpCompression.h"
#include "ServerMonsterCatalog.h"

#include "Util/json.hpp"
#include "axmol/base/CustomEvent.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/Scheduler.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <zlib.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
using Json = nlohmann::json;

int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }
}

Json makeRequest(const cmc::server::ServerMonsterCatalog& catalog)
{
    Json playerUnits = Json::array();
    Json enemyUnits  = Json::array();
    for (int index = 0; index < 10; ++index)
    {
        playerUnits.push_back(
            {{"runtimeUnitId", 101 + index}, {"catalogUnitId", 49}, {"x", index / 5}, {"z", index % 5}});
        enemyUnits.push_back(
            {{"runtimeUnitId", 201 + index}, {"catalogUnitId", 49}, {"x", 8 + index / 5}, {"z", index % 5}});
    }

    return Json{{"schemaVersion", 1},
                {"requestId", "test-request-1"},
                {"battleId", "test-battle-1"},
                {"uid", "firebase-user-1"},
                {"seed", 0x434D43 + 3 * 31 + 3},
                {"tableVersion", catalog.tableVersion()},
                {"contentHash", catalog.contentHash()},
                {"playerUnits", std::move(playerUnits)},
                {"enemyUnits", std::move(enemyUnits)}};
}

std::optional<std::string> decompressGzip(std::string_view input)
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return std::nullopt;

    z_stream stream{};
    stream.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK)
        return std::nullopt;

    std::string output;
    std::array<char, 4096> chunk{};
    int result = Z_OK;
    while (result == Z_OK)
    {
        stream.next_out  = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        result           = inflate(&stream, Z_NO_FLUSH);
        output.append(chunk.data(), chunk.size() - stream.avail_out);
    }
    inflateEnd(&stream);
    return result == Z_STREAM_END ? std::optional<std::string>{std::move(output)} : std::nullopt;
}

void testHttpCompression()
{
    using cmc::server::http_support::acceptsGzip;

    expect(acceptsGzip("gzip"), "gzip content coding is accepted");
    expect(acceptsGzip("br, GZip; q=0.5"), "gzip matching is case insensitive");
    expect(acceptsGzip("br, *;q=0.25"), "wildcard content coding accepts gzip");
    expect(!acceptsGzip("br"), "unsupported content codings do not enable gzip");
    expect(!acceptsGzip("gzip;q=0, *;q=1"), "explicit gzip rejection overrides wildcard");
    expect(!acceptsGzip("gzip;q=invalid"), "malformed gzip quality is rejected");
    expect(!acceptsGzip(""), "empty Accept-Encoding does not enable gzip");

    const std::string source                    = R"({"events":[)" + std::string(64 * 1024, 'x') + R"(]})";
    const std::optional<std::string> compressed = cmc::server::http_support::gzipCompress(source);
    expect(compressed.has_value(), "gzip compression succeeds");
    if (!compressed)
        return;

    expect(compressed->size() < source.size(), "gzip reduces a repetitive battle payload");
    expect(compressed->size() >= 2 && static_cast<unsigned char>((*compressed)[0]) == 0x1F &&
               static_cast<unsigned char>((*compressed)[1]) == 0x8B,
           "gzip output contains the RFC 1952 magic bytes");
    expect(decompressGzip(*compressed) == source, "gzip payload round-trips through zlib");
}

void testChatProtocol()
{
    using cmc::server::chat::Channel;
    using cmc::server::chat::ChannelKind;
    using cmc::server::chat::Identity;

    const Identity identity{"user-17", "테스터", "ko", "guild-9"};
    std::string error;
    expect(cmc::server::chat::validateIdentity(identity, error), "Chat accepts a valid CJK identity");
    expect(cmc::server::chat::roomKey(identity, Channel{ChannelKind::Language, "ko"}) == "language:ko",
           "Language chat is partitioned by language");
    expect(cmc::server::chat::roomKey(identity, Channel{ChannelKind::Guild, "guild-9"}) == "guild:guild-9",
           "Guild chat is partitioned by authenticated guild");
    expect(cmc::server::chat::roomKey(identity, Channel{ChannelKind::Direct, "user-3"}) ==
               "direct:user-17:user-3",
           "Direct chat room keys are stable for both participants");

    const Identity peer{"user-3", "Peer", "en", ""};
    expect(cmc::server::chat::roomKey(peer, Channel{ChannelKind::Direct, "user-17"}) ==
               "direct:user-17:user-3",
           "Direct chat participants resolve the same room key");

    error.clear();
    expect(!cmc::server::chat::validateChannel(identity, Channel{ChannelKind::Language, "fr"}, error),
           "Unsupported language channels are rejected");
    error.clear();
    expect(!cmc::server::chat::validateChannel(peer, Channel{ChannelKind::Guild, ""}, error),
           "Guild chat rejects users without a guild");
    error.clear();
    expect(!cmc::server::chat::validateChannel(identity, Channel{ChannelKind::Direct, "user-17"}, error),
           "Direct chat rejects self targets");

    std::string normalized;
    error.clear();
    expect(cmc::server::chat::validateMessageText("  안녕하세요  ", normalized, error) &&
               normalized == "안녕하세요",
           "Chat normalizes valid UTF-8 message whitespace");
    error.clear();
    expect(!cmc::server::chat::validateMessageText("\xF0\x28\x8C\x28", normalized, error),
           "Chat rejects malformed UTF-8");
    error.clear();
    expect(!cmc::server::chat::validateMessageText(std::string(201, 'x'), normalized, error),
           "Chat enforces the character limit");
}

class TestChatClient final
{
public:
    explicit TestChatClient(std::uint16_t port) : socket_(ioContext_)
    {
        boost::asio::ip::tcp::resolver resolver(ioContext_);
        const auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
        boost::asio::connect(socket_.next_layer(), endpoints);
        socket_.handshake("127.0.0.1", "/v1/chat");
    }

    void send(const Json& command)
    {
        const std::string payload = command.dump();
        socket_.write(boost::asio::buffer(payload));
    }

    Json receive()
    {
        boost::beast::flat_buffer buffer;
        socket_.read(buffer);
        return Json::parse(boost::beast::buffers_to_string(buffer.data()));
    }

    void close()
    {
        boost::system::error_code ignored;
        socket_.close(boost::beast::websocket::close_code::normal, ignored);
    }

private:
    boost::asio::io_context ioContext_;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> socket_;
};

Json helloCommand(std::string userId, std::string displayName, std::string language, std::string guildId)
{
    return Json{{"schemaVersion", cmc::server::chat::CHAT_SCHEMA_VERSION},
                {"type", "hello"},
                {"userId", std::move(userId)},
                {"displayName", std::move(displayName)},
                {"language", std::move(language)},
                {"guildId", std::move(guildId)}};
}

Json selectCommand(std::string channel, std::string scope)
{
    return Json{{"schemaVersion", cmc::server::chat::CHAT_SCHEMA_VERSION},
                {"type", "select"},
                {"channel", std::move(channel)},
                {"scope", std::move(scope)}};
}

Json sendCommand(std::string text)
{
    return Json{{"schemaVersion", cmc::server::chat::CHAT_SCHEMA_VERSION},
                {"type", "send"},
                {"text", std::move(text)}};
}

void testChatWebSocketIntegration()
{
    boost::asio::io_context serverContext;
    cmc::server::ChatServer server(serverContext, 0);
    server.start();
    std::thread serverThread([&serverContext] { serverContext.run(); });

    try
    {
        TestChatClient alice(server.port());
        TestChatClient bob(server.port());
        alice.send(helloCommand("alice", "앨리스", "ko", "knights"));
        bob.send(helloCommand("bob", "Bob", "en", "knights"));
        expect(alice.receive().value("type", "") == "ready", "Alice chat WebSocket handshake becomes ready");
        expect(bob.receive().value("type", "") == "ready", "Bob chat WebSocket handshake becomes ready");

        alice.send(selectCommand("language", "ko"));
        bob.send(selectCommand("language", "ko"));
        expect(alice.receive().value("type", "") == "history", "Language channel returns Alice history");
        expect(bob.receive().value("type", "") == "history", "Language channel returns Bob history");
        alice.send(sendCommand("한국어 채널 메시지"));
        const Json aliceLanguage = alice.receive();
        const Json bobLanguage   = bob.receive();
        expect(aliceLanguage.value("text", "") == "한국어 채널 메시지",
               "Language sender receives the broadcast");
        expect(bobLanguage.value("text", "") == "한국어 채널 메시지",
               "Language peer receives the broadcast");

        alice.send(selectCommand("guild", "knights"));
        bob.send(selectCommand("guild", "knights"));
        alice.receive();
        bob.receive();
        bob.send(sendCommand("Guild hello"));
        expect(alice.receive().value("channel", "") == "guild", "Guild member receives guild chat");
        expect(bob.receive().value("channel", "") == "guild", "Guild sender receives guild chat");

        alice.send(selectCommand("direct", "bob"));
        alice.receive();
        alice.send(sendCommand("private hello"));
        expect(alice.receive().value("channel", "") == "direct", "Direct sender receives private chat");
        const Json bobDirect = bob.receive();
        expect(bobDirect.value("channel", "") == "direct" && bobDirect.value("text", "") == "private hello",
               "Direct recipient receives private chat while viewing another channel");

        bob.send(selectCommand("direct", "alice"));
        const Json directHistory = bob.receive();
        expect(directHistory.value("type", "") == "history" && directHistory.at("messages").size() == 1,
               "Direct history is shared by both participants");
        alice.close();
        bob.close();
    }
    catch (const std::exception& error)
    {
        expect(false, error.what());
    }

    server.stop();
    serverContext.stop();
    serverThread.join();
}

void testAxmolHeadlessPrimitives()
{
    ax::EventDispatcher dispatcher;
    dispatcher.setEnabled(true);
    int dispatched = 0;
    auto* listener = new ax::CustomEventListener();
    expect(listener->init("cmc.test.event", [&dispatched](ax::CustomEvent*) { ++dispatched; }),
           "CustomEventListener initializes");
    dispatcher.addEventListenerWithFixedPriority(listener, 1);
    listener->release();
    dispatcher.dispatchCustomEvent("cmc.test.event");
    expect(dispatched == 1, "Headless EventDispatcher dispatches CustomEvent");

    struct TickTarget final
    {
        void update(float) { ++ticks; }
        int ticks = 0;
    } target;

    ax::Scheduler scheduler;
    scheduler.scheduleUpdate(&target, 0, false);
    scheduler.update(0.016F);
    expect(target.ticks == 1, "Headless Scheduler runs update selector");
    scheduler.unscheduleUpdate(&target);
    scheduler.update(0.016F);
    expect(target.ticks == 1, "Headless Scheduler cancels update selector");

    int removedEventCalls = 0;
    auto* removedListener = new ax::CustomEventListener();
    expect(removedListener->init("cmc.test.removed", [&removedEventCalls](ax::CustomEvent*) { ++removedEventCalls; }),
           "Secondary CustomEventListener initializes");
    dispatcher.addEventListenerWithFixedPriority(removedListener, 1);
    removedListener->release();

    auto* remover = dispatcher.addCustomEventListener(
        "cmc.test.remover", [&dispatcher, removedListener](auto*) { dispatcher.removeEventListener(removedListener); });
    expect(remover != nullptr, "Cross-event remover listener initializes");
    dispatcher.dispatchCustomEvent("cmc.test.remover");
    dispatcher.dispatchCustomEvent("cmc.test.removed");
    expect(removedEventCalls == 0, "Removing another event listener during dispatch is safe");
    expect(!dispatcher.hasEventListener("cmc.test.removed"), "Detached cross-event listener is cleaned");

    int bulkCalls = 0;
    dispatcher.addCustomEventListener("cmc.test.bulk", [&dispatcher, &bulkCalls](auto*) {
        ++bulkCalls;
        dispatcher.removeCustomEventListeners("cmc.test.bulk");
    }, -1);
    dispatcher.addCustomEventListener("cmc.test.bulk", [&bulkCalls](auto*) { ++bulkCalls; }, 1);
    dispatcher.dispatchCustomEvent("cmc.test.bulk");
    expect(bulkCalls == 1, "Bulk removal during current event dispatch skips detached listeners");
    expect(!dispatcher.hasEventListener("cmc.test.bulk"), "Bulk-removed event listeners are cleaned");

    expect(dispatcher.addCustomEventListener("cmc.test.invalid", [](auto*) {}, 0) == nullptr,
           "Headless dispatcher rejects scene-graph priority zero");
}

void testTamperedCatalogIsRejected()
{
    std::ifstream input(CMC_TEST_TABLE_PATH);
    Json root;
    input >> root;
    root["characters"][0]["hp"] = root["characters"][0]["hp"].get<int>() + 1;

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("cmc_tampered_monsters_" + std::to_string(suffix) + ".json");
    {
        std::ofstream output(path);
        output << root.dump();
    }

    bool rejected = false;
    try
    {
        cmc::server::ServerMonsterCatalog catalog;
        catalog.load(path);
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    std::filesystem::remove(path);
    expect(rejected, "Server rejects a table whose canonical payload was modified");
}

void testQueuedRuntimeShutdown()
{
    boost::asio::io_context ioContext;
    {
        cmc::server::AxmolRuntime runtime(ioContext);
        runtime.start();
        runtime.stop();
        ioContext.run();
        expect(runtime.schedulerTicks() == 0, "Stop requested before run prevents a queued runtime start");
    }

    ioContext.restart();
    {
        cmc::server::AxmolRuntime runtime(ioContext);
        runtime.start();
    }
    ioContext.run();
}
}  // namespace

int main()
{
    testHttpCompression();
    testChatProtocol();
    testChatWebSocketIntegration();
    testAxmolHeadlessPrimitives();
    testTamperedCatalogIsRejected();
    testQueuedRuntimeShutdown();

    cmc::server::ServerMonsterCatalog catalog;
    catalog.load(CMC_TEST_TABLE_PATH);
    expect(catalog.size() == 168, "Server loads all canonical monster rows");
    expect(catalog.find(49) != nullptr, "Server resolves Drago1 by catalog unit id");

    boost::asio::io_context ioContext;
    cmc::server::AxmolRuntime runtime(ioContext);
    runtime.start();
    ioContext.run_for(std::chrono::milliseconds(20));
    ioContext.restart();
    expect(runtime.schedulerTicks() > 0, "Axmol runtime pumps scheduleUpdate through the Asio strand");

    const cmc::server::BattleApi api(catalog, runtime);
    const Json validRequest               = makeRequest(catalog);
    const cmc::server::ApiResponse first  = api.simulate(validRequest.dump());
    const cmc::server::ApiResponse second = api.simulate(validRequest.dump());
    expect(first.status == 200, "Canonical 10v10 request succeeds");
    expect(second.status == 200, "Repeated canonical request succeeds");

    const Json firstBody  = Json::parse(first.body);
    const Json secondBody = Json::parse(second.body);
    expect(firstBody.at("checksum") == secondBody.at("checksum"), "Same request produces the same checksum");
    expect(firstBody.at("checksum") == "ED6B118B", "Server adapter preserves the canonical 10v10 golden checksum");
    expect(firstBody.at("events").is_array() && !firstBody.at("events").empty(), "Response contains replay events");
    expect(firstBody.at("finalState").size() == 20, "Response contains all final unit states");

    Json mismatch           = validRequest;
    mismatch["contentHash"] = std::string(64, '0');
    expect(api.simulate(mismatch.dump()).status == 409, "Content hash mismatch is rejected");

    Json unknownCatalog                               = validRequest;
    unknownCatalog["playerUnits"][0]["catalogUnitId"] = 999999;
    expect(api.simulate(unknownCatalog.dump()).status == 400, "Unknown server catalog unit is rejected");

    Json clientStats                    = validRequest;
    clientStats["playerUnits"][0]["hp"] = 999999;
    expect(api.simulate(clientStats.dump()).status == 400, "Client-provided combat stats are rejected");

    Json duplicatePosition                   = validRequest;
    duplicatePosition["playerUnits"][1]["x"] = duplicatePosition["playerUnits"][0]["x"];
    duplicatePosition["playerUnits"][1]["z"] = duplicatePosition["playerUnits"][0]["z"];
    expect(api.simulate(duplicatePosition.dump()).status == 400, "Duplicate board positions are rejected");

    ioContext.poll();
    expect(runtime.completedBattles() == 2, "Battle completion uses Axmol CustomEvent dispatch");
    bool runtimeStopped = false;
    runtime.asyncStop([&runtimeStopped] { runtimeStopped = true; });
    ioContext.restart();
    ioContext.run();
    expect(runtimeStopped, "Axmol runtime stops on its strand");

    if (failures != 0)
        return 1;
    std::cout << "All server tests passed. checksum=" << firstBody.at("checksum").get<std::string>() << '\n';
    return 0;
}
