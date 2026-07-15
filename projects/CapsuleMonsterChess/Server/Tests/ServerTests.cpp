#include "AxmolRuntime.h"
#include "BattleApi.h"
#include "ServerMonsterCatalog.h"

#include "Util/json.hpp"
#include "axmol/base/CustomEvent.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/Scheduler.h"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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
