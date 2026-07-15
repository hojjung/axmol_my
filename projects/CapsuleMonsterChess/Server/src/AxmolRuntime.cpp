#include "AxmolRuntime.h"

#include "axmol/base/CustomEvent.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/Scheduler.h"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace cmc::server
{
class AxmolRuntimeState final : public std::enable_shared_from_this<AxmolRuntimeState>
{
public:
    explicit AxmolRuntimeState(boost::asio::io_context& ioContext)
        : strand_(boost::asio::make_strand(ioContext)), pumpTimer_(ioContext)
    {
        eventDispatcher_.setEnabled(true);
    }

    void initialize()
    {
        battleCompletedListener_ = new ax::CustomEventListener();
        if (!battleCompletedListener_->init(BATTLE_COMPLETED_EVENT, [weak = weak_from_this()](ax::CustomEvent* event) {
            const auto* completed = static_cast<const BattleCompletedEvent*>(event->getUserData());
            if (completed)
            {
                if (const auto self = weak.lock())
                    self->completedBattles_.fetch_add(1, std::memory_order_relaxed);
            }
        }))
        {
            delete battleCompletedListener_;
            battleCompletedListener_ = nullptr;
            throw std::runtime_error("Failed to initialize Axmol battle completion listener");
        }

        eventDispatcher_.addEventListenerWithFixedPriority(battleCompletedListener_, 1);
        battleCompletedListener_->release();
    }

    void start()
    {
        if (stopRequested_.load(std::memory_order_acquire))
            return;

        boost::asio::post(strand_, [self = shared_from_this()] {
            if (self->running_ || self->stopRequested_.load(std::memory_order_acquire))
                return;
            self->running_  = true;
            self->lastPump_ = std::chrono::steady_clock::now();
            self->scheduler_.scheduleUpdate(self.get(), 0, false);
            self->armPump();
        });
    }

    void stop(std::function<void()> completion)
    {
        stopRequested_.store(true, std::memory_order_release);
        boost::asio::post(strand_, [self = shared_from_this(), completion = std::move(completion)]() mutable {
            if (self->running_)
            {
                self->running_ = false;
                self->pumpTimer_.cancel();
                self->scheduler_.unscheduleUpdate(self.get());
            }
            if (completion)
                completion();
        });
    }

    void publishBattleCompleted(BattleCompletedEvent event)
    {
        if (stopRequested_.load(std::memory_order_acquire))
            return;

        boost::asio::post(strand_, [self = shared_from_this(), event]() mutable {
            if (self->running_ && !self->stopRequested_.load(std::memory_order_acquire))
                self->eventDispatcher_.dispatchCustomEvent(BATTLE_COMPLETED_EVENT, &event);
        });
    }

    void update(float deltaSeconds)
    {
        static_cast<void>(deltaSeconds);
        schedulerTicks_.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t completedBattles() const noexcept
    {
        return completedBattles_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t schedulerTicks() const noexcept
    {
        return schedulerTicks_.load(std::memory_order_relaxed);
    }

private:
    void armPump()
    {
        pumpTimer_.expires_after(std::chrono::milliseconds(16));
        auto onPump = [self = shared_from_this()](const boost::system::error_code& error) {
            if (error || !self->running_ || self->stopRequested_.load(std::memory_order_acquire))
                return;

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds =
                std::clamp(std::chrono::duration<float>(now - self->lastPump_).count(), 0.0F, 0.25F);
            self->lastPump_ = now;
            self->scheduler_.update(deltaSeconds);
            self->armPump();
        };
        pumpTimer_.async_wait(boost::asio::bind_executor(strand_, std::move(onPump)));
    }

    static constexpr std::string_view BATTLE_COMPLETED_EVENT = "cmc.server.battle.completed";

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer pumpTimer_;
    ax::Scheduler scheduler_;
    ax::EventDispatcher eventDispatcher_;
    ax::CustomEventListener* battleCompletedListener_ = nullptr;
    std::chrono::steady_clock::time_point lastPump_;
    std::atomic<std::uint64_t> completedBattles_{0};
    std::atomic<std::uint64_t> schedulerTicks_{0};
    std::atomic<bool> stopRequested_{false};
    bool running_ = false;
};

AxmolRuntime::AxmolRuntime(boost::asio::io_context& ioContext) : state_(std::make_shared<AxmolRuntimeState>(ioContext))
{
    state_->initialize();
}

AxmolRuntime::~AxmolRuntime()
{
    stop();
}

void AxmolRuntime::start()
{
    state_->start();
}

void AxmolRuntime::stop()
{
    state_->stop({});
}

void AxmolRuntime::asyncStop(std::function<void()> completion)
{
    state_->stop(std::move(completion));
}

void AxmolRuntime::publishBattleCompleted(BattleCompletedEvent event)
{
    state_->publishBattleCompleted(event);
}

std::uint64_t AxmolRuntime::completedBattles() const noexcept
{
    return state_->completedBattles();
}

std::uint64_t AxmolRuntime::schedulerTicks() const noexcept
{
    return state_->schedulerTicks();
}
}  // namespace cmc::server
