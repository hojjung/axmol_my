#pragma once

#include "Client/Battle/BattleExecutionMode.h"
#include "Client/UserData/UserProfile.h"

#include <array>

namespace cmc::client
{
struct BattlePresentationRequest final
{
    int stageId     = 0;
    int stageNumber = 0;
    int enemyPower  = 0;
    int rewardGold  = 0;
    std::array<int, USER_DECK_SLOT_COUNT> playerUnitIds{};
    cmc::BattleExecutionMode executionMode = cmc::BattleExecutionMode::Local;
};
}  // namespace cmc::client
