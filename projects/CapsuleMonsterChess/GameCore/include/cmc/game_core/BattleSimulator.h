#pragma once

#include "cmc/game_core/BattleTypes.h"

#include <string>

namespace cmc::game_core
{
class BattleSimulator final
{
public:
    [[nodiscard]] BattleSimulateResult simulate(const BattleSimulateRequest& request) const;
};

[[nodiscard]] BattleValidationError validateBattleRequest(const BattleSimulateRequest& request) noexcept;
[[nodiscard]] std::string computeBattleChecksum(const BattleSimulateResult& result);
[[nodiscard]] BattleSimulateRequest createSmokeBattleRequest();
}  // namespace cmc::game_core
