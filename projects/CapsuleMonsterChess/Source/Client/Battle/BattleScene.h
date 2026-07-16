#pragma once

#include "Client/Battle/BattlePresentationRequest.h"
#include "Client/Content/MonsterCatalog.h"
#include "axmol/axmol.h"
#include "cmc/game_core/BattleSimulator.h"

#include <cstddef>
#include <unordered_map>

namespace cmc::client
{
class BattleScene final : public ax::Scene
{
public:
    [[nodiscard]] static BattleScene* create(const BattlePresentationRequest& request);

    void onEnter() override;

private:
    struct UnitPresentation final
    {
        ax::Node* root                = nullptr;
        int cellIndex                 = -1;
        int hp                        = 0;
        int maxHp                     = 1;
        cmc::game_core::TeamId teamId = cmc::game_core::TeamId::Player;
        bool alive                    = true;
    };

    bool initWithRequest(const BattlePresentationRequest& request);
    void buildWorld();
    void buildInterface();
    void beginReplay();
    void update(float deltaSeconds) override;
    void advanceReplayTick();
    void dispatchReplayEvent(const cmc::game_core::BattleLogEvent& event, bool instant);
    void replayEvent(const cmc::game_core::BattleLogEvent& event, bool instant);
    void fastForwardReplay();
    void updateTeamHud();
    void showResult(bool victory);
    void returnToLobby();

    BattlePresentationRequest _request;
    MonsterCatalog _monsterCatalog;
    cmc::game_core::BattleSimulateRequest _simulationRequest;
    cmc::game_core::BattleSimulateResult _simulationResult;
    std::unordered_map<int, UnitPresentation> _units;
    ax::Node* _battleWorldRoot   = nullptr;
    ax::Node* _resultRoot        = nullptr;
    ax::Node* _playerCountLabel  = nullptr;
    ax::Node* _enemyCountLabel   = nullptr;
    ax::Node* _playerHealthFill  = nullptr;
    ax::Node* _enemyHealthFill   = nullptr;
    std::size_t _nextReplayEvent = 0;
    int _replayTick              = 0;
    float _replayAccumulator     = 0.0F;
    bool _replayStarted          = false;
    bool _battleEndQueued        = false;
    bool _transitionQueued       = false;
};
}  // namespace cmc::client
