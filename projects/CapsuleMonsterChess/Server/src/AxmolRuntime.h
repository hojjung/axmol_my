#pragma once

#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace cmc::server
{
struct BattleCompletedEvent final
{
    std::int32_t durationTicks = 0;
    bool hasWinner             = false;
    std::int32_t winnerTeam    = 0;
};

class AxmolRuntimeState;

class AxmolRuntime final
{
public:
    explicit AxmolRuntime(boost::asio::io_context& ioContext);
    ~AxmolRuntime();

    AxmolRuntime(const AxmolRuntime&)            = delete;
    AxmolRuntime& operator=(const AxmolRuntime&) = delete;

    void start();
    void stop();
    void asyncStop(std::function<void()> completion);
    void publishBattleCompleted(BattleCompletedEvent event);

    [[nodiscard]] std::uint64_t completedBattles() const noexcept;
    [[nodiscard]] std::uint64_t schedulerTicks() const noexcept;

private:
    std::shared_ptr<AxmolRuntimeState> state_;
};
}  // namespace cmc::server
