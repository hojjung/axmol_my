#pragma once

#include <cstdint>

namespace cmc::game_core
{
class DeterministicRandom final
{
public:
    explicit DeterministicRandom(std::int32_t seed) noexcept
        : state_(seed == 0 ? 0x6D2B79F5U : static_cast<std::uint32_t>(seed))
    {}

    [[nodiscard]] std::int32_t nextInt(std::int32_t minInclusive, std::int32_t maxExclusive) noexcept
    {
        if (maxExclusive <= minInclusive)
            return minInclusive;
        const auto range = static_cast<std::uint32_t>(static_cast<std::int64_t>(maxExclusive) - minInclusive);
        const auto value = static_cast<std::int64_t>(minInclusive) + static_cast<std::int64_t>(nextUInt() % range);
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t nextPercent() noexcept { return nextInt(0, 10000); }

private:
    [[nodiscard]] std::uint32_t nextUInt() noexcept
    {
        auto value = state_;
        value ^= value << 13U;
        value ^= value >> 17U;
        value ^= value << 5U;
        state_ = value;
        return value;
    }

    std::uint32_t state_;
};
}  // namespace cmc::game_core
