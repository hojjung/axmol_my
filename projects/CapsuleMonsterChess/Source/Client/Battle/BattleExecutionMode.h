#pragma once

#include <cstdint>

namespace cmc
{
enum class BattleExecutionMode : std::uint8_t
{
    Local,
    AuthoritativeRemote,
};
}  // namespace cmc
