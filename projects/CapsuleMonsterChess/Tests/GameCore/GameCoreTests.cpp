#include "cmc/game_core/BattleSimulator.h"
#include "cmc/game_core/DeterministicRandom.h"
#include "cmc/game_core/HexBoard.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace allocation_probe
{
std::size_t count = 0;
bool enabled      = false;

void* allocate(std::size_t size)
{
    if (enabled)
        ++count;
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    std::abort();
}
}  // namespace allocation_probe

void* operator new(std::size_t size)
{
    return allocation_probe::allocate(size);
}

void* operator new[](std::size_t size)
{
    return allocation_probe::allocate(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{
using namespace cmc::game_core;

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (condition)
        return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

void checkEqual(std::int32_t actual, std::int32_t expected, std::string_view message)
{
    if (actual == expected)
        return;
    ++failures;
    std::cerr << "FAILED: " << message << " expected=" << expected << " actual=" << actual << '\n';
}

void checkEqual(std::string_view actual, std::string_view expected, std::string_view message)
{
    if (actual == expected)
        return;
    ++failures;
    std::cerr << "FAILED: " << message << " expected=" << expected << " actual=" << actual << '\n';
}

void testHexBoard()
{
    HexBoard board(10, 5, {});
    std::vector<std::int32_t> neighbors;
    board.getNeighbors(0, neighbors);
    check(neighbors == std::vector<std::int32_t>{1, 10}, "corner neighbor order");
    board.getNeighbors(21, neighbors);
    check(neighbors == std::vector<std::int32_t>({22, 20, 31, 30, 11, 10}), "odd-row neighbor order");
    board.getNeighbors(18, neighbors);
    check(neighbors == std::vector<std::int32_t>({19, 17, 29, 28, 9, 8}), "even-row neighbor order");
    checkEqual(board.distance(0, 49), 11, "odd-r cube distance");

    std::int32_t next     = -1;
    std::int32_t distance = -1;
    check(board.tryFindNextStep(0, 24, 1, next, distance), "A* route exists");
    checkEqual(next, 1, "A* deterministic first step");
    checkEqual(distance, 5, "target distance output");

    HexBoard closed(10, 5, {1, 10});
    closed.occupy(11, 99);
    check(!closed.tryFindNextStep(0, 24, 1, next, distance), "blocked start has no route");
    checkEqual(next, -1, "failed route has no next step");

    HexBoard oversized(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max(), {});
    checkEqual(oversized.width(), DEFAULT_BOARD_WIDTH, "oversized direct board falls back safely");
    checkEqual(oversized.height(), DEFAULT_BOARD_HEIGHT, "oversized direct board height falls back safely");
    check(!oversized.tryFindNextStep(-1, 0, 1, next, distance), "invalid path input fails safely");
}

void testHexBoardPathWorkspace()
{
    const HexBoard board(10, 5, {});
    std::int32_t next     = -1;
    std::int32_t distance = -1;
    check(board.tryFindNextStep(0, 24, 1, next, distance), "path workspace warmup succeeds");

    allocation_probe::count   = 0;
    allocation_probe::enabled = true;
    bool stable               = true;
    for (std::int32_t i = 0; i < 2048; ++i)
    {
        const auto found = board.tryFindNextStep(0, 24, 1, next, distance);
        stable           = stable && found && next == 1 && distance == 5;
    }
    allocation_probe::enabled = false;

    check(stable, "reused path workspace preserves deterministic route");
    checkEqual(static_cast<std::int32_t>(allocation_probe::count), 0, "path search performs no heap allocations");
}

void testRandom()
{
    DeterministicRandom random(1);
    const std::vector<std::int32_t> expected{369, 4689, 5461, 9695, 9233};
    for (const auto value : expected)
        checkEqual(random.nextPercent(), value, "xorshift32 sequence");
    DeterministicRandom zeroSeed(0);
    checkEqual(zeroSeed.nextPercent(), 6063, "zero seed substitution");
}

void testUnitySmokeGolden()
{
    const auto result = BattleSimulator{}.simulate(createSmokeBattleRequest());
    check(result.hasWinner, "smoke battle has winner");
    check(result.winnerTeam == TeamId::Enemy, "smoke winner matches Unity");
    checkEqual(result.durationTicks, 198, "smoke duration matches Unity");
    checkEqual(static_cast<std::int32_t>(result.events.size()), 137, "smoke event count matches Unity");
    checkEqual(result.checksum, "D8DF9B75", "smoke checksum matches Unity");
    if (result.events.size() >= 5U)
    {
        checkEqual(result.events[0].toCell, 21, "spawn order is request order");
        check(result.events[4].type == BattleEventType::BattleStart, "BattleStart follows spawn events");
        check(result.events.back().type == BattleEventType::BattleEnd, "winner emits BattleEnd");
    }
    else
    {
        check(false, "smoke event sequence is large enough to inspect");
    }
}

void testRandomTargetDeterminism()
{
    auto request = createSmokeBattleRequest();
    request.seed = 1;
    auto& caster = request.teams[0].units[0];
    caster.stats.energyOnAttack = ENERGY_FULL;
    caster.skills[0].targetRule = TargetRule::RandomEnemy;

    const auto first  = BattleSimulator{}.simulate(request);
    const auto second = BattleSimulator{}.simulate(request);
    checkEqual(first.checksum, second.checksum, "random target sequence remains deterministic");

    const auto taunt = std::find_if(first.events.begin(), first.events.end(), [](const BattleLogEvent& event) {
        return event.type == BattleEventType::StatusApplied && event.actorId == 101 &&
               event.status == StatusType::Taunt;
    });
    check(taunt != first.events.end(), "random-target skill applies taunt");
    if (taunt != first.events.end())
        checkEqual(taunt->targetId, 202, "seeded random target keeps unit-id order selection");
}

void checkInvalid(const BattleSimulateRequest& request, BattleValidationError expected, std::string_view message)
{
    check(validateBattleRequest(request) == expected, message);
    const auto result = BattleSimulator{}.simulate(request);
    check(!result.isValid(), "invalid simulation reports validation failure");
    check(result.validationError == expected, "invalid simulation preserves validation reason");
    check(!result.hasWinner, "invalid simulation has no winner");
    check(result.events.empty(), "invalid simulation emits no replay events");
    check(result.finalState.empty(), "invalid simulation emits no final state");
    check(result.checksum.empty(), "invalid simulation cannot be accepted by checksum");
}

void testInvalidRequests()
{
    BattleSimulateRequest empty;
    empty.battleId = "empty";
    checkInvalid(empty, BattleValidationError::EmptyTeams, "empty team request is rejected");

    auto emptyRoster = createSmokeBattleRequest();
    emptyRoster.teams[1].units.clear();
    checkInvalid(emptyRoster, BattleValidationError::EmptyTeams, "empty team roster is rejected");

    auto duplicate                     = createSmokeBattleRequest();
    duplicate.teams[1].units[0].unitId = duplicate.teams[0].units[0].unitId;
    checkInvalid(duplicate, BattleValidationError::DuplicateUnitId, "duplicate runtime unit id is rejected");

    auto capacity         = createSmokeBattleRequest();
    capacity.board.width  = 1;
    capacity.board.height = 1;
    checkInvalid(capacity, BattleValidationError::BoardCapacityExceeded, "board capacity overflow is rejected");

    auto oversizedBoard        = createSmokeBattleRequest();
    oversizedBoard.board.width = std::numeric_limits<std::int32_t>::max();
    checkInvalid(oversizedBoard, BattleValidationError::InvalidBoard, "oversized board is rejected");

    auto excessiveTicks             = createSmokeBattleRequest();
    excessiveTicks.options.maxTicks = std::numeric_limits<std::int32_t>::max();
    checkInvalid(excessiveTicks, BattleValidationError::InvalidOptions, "excessive simulation ticks are rejected");

    auto excessiveStats                          = createSmokeBattleRequest();
    excessiveStats.teams[0].units[0].stats.maxHp = std::numeric_limits<std::int32_t>::max();
    checkInvalid(excessiveStats, BattleValidationError::InvalidUnitStats, "excessive unit stats are rejected");

    auto excessiveEffect                                         = createSmokeBattleRequest();
    excessiveEffect.teams[0].units[0].skills[0].effects[0].value = std::numeric_limits<std::int32_t>::max();
    checkInvalid(excessiveEffect, BattleValidationError::InvalidSkill, "excessive skill effect is rejected");
}

BattleSimulateRequest makeDragonBattle()
{
    BattleSimulateRequest request;
    request.battleId = "dragon-10v10";
    request.seed     = 20260715;
    BattleTeamSetup player;
    player.teamId = TeamId::Player;
    BattleTeamSetup enemy;
    enemy.teamId = TeamId::Enemy;
    for (std::int32_t i = 0; i < 10; ++i)
    {
        BattleUnitSetup unit;
        unit.unitId        = 101 + i;
        unit.monsterId     = "Unit01";
        unit.displayName   = "Dragon";
        unit.position      = {i / 5, i % 5};
        unit.stats.maxHp   = 180;
        unit.stats.attack  = 24;
        unit.stats.defense = 12;
        player.units.push_back(unit);

        unit.unitId   = 201 + i;
        unit.position = {8 + i / 5, i % 5};
        enemy.units.push_back(unit);
    }
    request.teams.push_back(std::move(player));
    request.teams.push_back(std::move(enemy));
    return request;
}

void testDragonDeterminism()
{
    const auto request = makeDragonBattle();
    const auto first   = BattleSimulator{}.simulate(request);
    const auto second  = BattleSimulator{}.simulate(request);
    check(first.hasWinner, "dragon battle has winner");
    check(first.winnerTeam == TeamId::Player, "dragon winner matches Unity");
    checkEqual(first.durationTicks, 450, "dragon duration matches Unity");
    checkEqual(static_cast<std::int32_t>(first.events.size()), 819, "dragon event count matches Unity");
    checkEqual(first.checksum, "5344CC81", "dragon checksum matches Unity");
    const auto playerAlive = std::count_if(first.finalState.begin(), first.finalState.end(), [](const auto& state) {
        return state.alive && state.teamId == TeamId::Player;
    });
    const auto enemyAlive  = std::count_if(first.finalState.begin(), first.finalState.end(), [](const auto& state) {
        return state.alive && state.teamId == TeamId::Enemy;
    });
    checkEqual(static_cast<std::int32_t>(playerAlive), 2, "dragon player survivors match Unity");
    checkEqual(static_cast<std::int32_t>(enemyAlive), 0, "dragon enemy survivors match Unity");
    checkEqual(first.durationTicks, second.durationTicks, "dragon duration deterministic");
    checkEqual(first.checksum, second.checksum, "dragon checksum deterministic");
    checkEqual(static_cast<std::int32_t>(first.finalState.size()), 20, "dragon battle preserves 20 final states");
    check(std::any_of(first.events.begin(), first.events.end(),
                      [](const BattleLogEvent& event) { return event.type == BattleEventType::UnitMove; }),
          "dragon battle moves on hex board");
    check(std::any_of(first.events.begin(), first.events.end(),
                      [](const BattleLogEvent& event) { return event.type == BattleEventType::UnitDeath; }),
          "dragon battle resolves deaths");
}
}  // namespace

int main()
{
    testHexBoard();
    testHexBoardPathWorkspace();
    testRandom();
    testUnitySmokeGolden();
    testRandomTargetDeterminism();
    testInvalidRequests();
    testDragonDeterminism();
    if (failures != 0)
        return EXIT_FAILURE;
    std::cout << "GameCore tests passed\n";
    return EXIT_SUCCESS;
}
