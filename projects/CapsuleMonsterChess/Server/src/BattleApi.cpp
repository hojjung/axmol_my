#include "BattleApi.h"

#include "Util/json.hpp"
#include "cmc/game_core/BattleSimulator.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cmc::server
{
namespace
{
using Json = nlohmann::json;

constexpr std::int32_t BOARD_WIDTH       = 10;
constexpr std::int32_t BOARD_HEIGHT      = 5;
constexpr std::size_t MAX_UNITS_PER_TEAM = 10;

class ApiError final : public std::runtime_error
{
public:
    ApiError(unsigned int status, std::string code, std::string message)
        : std::runtime_error(std::move(message)), status(status), code(std::move(code))
    {}

    unsigned int status;
    std::string code;
};

struct InputUnit final
{
    std::int32_t runtimeUnitId = 0;
    std::int32_t catalogUnitId = 0;
    game_core::HexCoord position;
};

void requireObjectFields(const Json& value,
                         std::initializer_list<std::string_view> required,
                         std::initializer_list<std::string_view> allowed)
{
    if (!value.is_object())
        throw ApiError(400, "INVALID_JSON", "Expected a JSON object");

    for (std::string_view key : required)
    {
        if (!value.contains(key))
            throw ApiError(400, "MISSING_FIELD", "Missing required field: " + std::string(key));
    }

    for (const auto& entry : value.items())
    {
        bool accepted = false;
        for (std::string_view key : allowed)
        {
            if (entry.key() == key)
            {
                accepted = true;
                break;
            }
        }
        if (!accepted)
            throw ApiError(400, "UNKNOWN_FIELD", "Unknown field: " + entry.key());
    }
}

std::string requireIdentifier(const Json& root, std::string_view field)
{
    const std::string value = root.at(field).get<std::string>();
    if (value.empty() || value.size() > 128)
        throw ApiError(400, "INVALID_IDENTIFIER", "Invalid identifier: " + std::string(field));
    for (unsigned char character : value)
    {
        if (!(std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == ':'))
        {
            throw ApiError(400, "INVALID_IDENTIFIER", "Invalid identifier: " + std::string(field));
        }
    }
    return value;
}

std::vector<InputUnit> parseUnits(const Json& values, game_core::TeamId teamId)
{
    if (!values.is_array() || values.empty() || values.size() > MAX_UNITS_PER_TEAM)
        throw ApiError(400, "INVALID_TEAM_SIZE", "Each team must contain 1 to 10 units");

    std::vector<InputUnit> units;
    units.reserve(values.size());
    for (const Json& value : values)
    {
        requireObjectFields(value, {"runtimeUnitId", "catalogUnitId", "x", "z"},
                            {"runtimeUnitId", "catalogUnitId", "x", "z"});
        InputUnit unit;
        unit.runtimeUnitId = value.at("runtimeUnitId").get<std::int32_t>();
        unit.catalogUnitId = value.at("catalogUnitId").get<std::int32_t>();
        unit.position.x    = value.at("x").get<std::int32_t>();
        unit.position.z    = value.at("z").get<std::int32_t>();

        const bool insideBoard = unit.position.x >= 0 && unit.position.x < BOARD_WIDTH && unit.position.z >= 0 &&
                                 unit.position.z < BOARD_HEIGHT;
        const bool insideTeamHalf = teamId == game_core::TeamId::Player ? unit.position.x < BOARD_WIDTH / 2
                                                                        : unit.position.x >= BOARD_WIDTH / 2;
        if (unit.runtimeUnitId <= 0 || unit.catalogUnitId <= 0 || !insideBoard || !insideTeamHalf)
            throw ApiError(400, "INVALID_UNIT", "Unit identifier or position is invalid");
        units.emplace_back(unit);
    }
    return units;
}

game_core::BattleTeamSetup makeTeam(const std::vector<InputUnit>& inputs,
                                    game_core::TeamId teamId,
                                    const ServerMonsterCatalog& catalog,
                                    std::unordered_set<std::int32_t>& runtimeIds,
                                    std::unordered_set<std::int32_t>& occupiedCells)
{
    game_core::BattleTeamSetup team;
    team.teamId = teamId;
    team.units.reserve(inputs.size());
    for (const InputUnit& input : inputs)
    {
        if (!runtimeIds.emplace(input.runtimeUnitId).second)
            throw ApiError(400, "DUPLICATE_RUNTIME_UNIT", "runtimeUnitId must be unique across the battle");

        const std::int32_t cellIndex = input.position.z * BOARD_WIDTH + input.position.x;
        if (!occupiedCells.emplace(cellIndex).second)
            throw ApiError(400, "DUPLICATE_POSITION", "Two units cannot occupy the same cell");

        const ServerMonsterDefinition* monster = catalog.find(input.catalogUnitId);
        if (monster == nullptr)
            throw ApiError(400, "UNKNOWN_CATALOG_UNIT", "catalogUnitId does not exist in the server table");

        game_core::BattleUnitSetup unit;
        unit.unitId         = input.runtimeUnitId;
        unit.monsterId      = monster->nameKey;
        unit.displayName    = monster->nameKey;
        unit.teamId         = teamId;
        unit.element        = monster->element;
        unit.role           = monster->role;
        unit.evolutionStage = 1;
        unit.position       = input.position;
        unit.stats          = monster->stats;
        team.units.emplace_back(std::move(unit));
    }
    return team;
}

Json eventToJson(const game_core::BattleLogEvent& event)
{
    return Json{{"tick", event.tick},
                {"type", static_cast<std::int32_t>(event.type)},
                {"actorId", event.actorId},
                {"targetId", event.targetId},
                {"teamId", static_cast<std::int32_t>(event.teamId)},
                {"fromCell", event.fromCell},
                {"toCell", event.toCell},
                {"amount", event.amount},
                {"value", event.value},
                {"status", static_cast<std::int32_t>(event.status)},
                {"stacks", event.stacks},
                {"skillIndex", event.skillIndex}};
}

Json finalStateToJson(const game_core::BattleUnitFinalState& state)
{
    return Json{{"unitId", state.unitId},       {"teamId", static_cast<std::int32_t>(state.teamId)},
                {"cellIndex", state.cellIndex}, {"hp", state.hp},
                {"energy", state.energy},       {"alive", state.alive}};
}

ApiResponse jsonResponse(unsigned int status, const Json& body)
{
    return ApiResponse{status, body.dump()};
}

ApiResponse errorResponse(unsigned int status, std::string_view code, std::string_view message)
{
    return jsonResponse(status, Json{{"error", {{"code", code}, {"message", message}}}});
}
}  // namespace

ApiResponse BattleApi::health() const
{
    return jsonResponse(200, Json{{"status", "ok"},
                                  {"tableVersion", catalog_.tableVersion()},
                                  {"contentHash", catalog_.contentHash()},
                                  {"catalogUnits", catalog_.size()},
                                  {"completedBattles", runtime_.completedBattles()},
                                  {"schedulerTicks", runtime_.schedulerTicks()}});
}

ApiResponse BattleApi::simulate(std::string_view requestBody) const
{
    try
    {
        const Json root = Json::parse(requestBody);
        requireObjectFields(root,
                            {"schemaVersion", "requestId", "battleId", "uid", "seed", "tableVersion", "contentHash",
                             "playerUnits", "enemyUnits"},
                            {"schemaVersion", "requestId", "battleId", "uid", "seed", "tableVersion", "contentHash",
                             "playerUnits", "enemyUnits"});

        if (root.at("schemaVersion").get<std::int32_t>() != 1)
            throw ApiError(400, "UNSUPPORTED_SCHEMA", "Only battle request schemaVersion 1 is supported");

        const std::string requestId = requireIdentifier(root, "requestId");
        const std::string battleId  = requireIdentifier(root, "battleId");
        static_cast<void>(requireIdentifier(root, "uid"));

        const std::string requestTableVersion = root.at("tableVersion").get<std::string>();
        const std::string requestContentHash  = root.at("contentHash").get<std::string>();
        if (requestTableVersion != catalog_.tableVersion() || requestContentHash != catalog_.contentHash())
            throw ApiError(409, "CONTENT_VERSION_MISMATCH", "Client and battle server content versions differ");

        const auto playerInputs = parseUnits(root.at("playerUnits"), game_core::TeamId::Player);
        const auto enemyInputs  = parseUnits(root.at("enemyUnits"), game_core::TeamId::Enemy);

        game_core::BattleSimulateRequest request;
        request.battleId                  = battleId;
        request.seed                      = root.at("seed").get<std::int32_t>();
        request.tableVersion              = catalog_.tableVersion();
        request.board.width               = BOARD_WIDTH;
        request.board.height              = BOARD_HEIGHT;
        request.options.fixedDeltaMs      = game_core::FIXED_DELTA_MS;
        request.options.maxTicks          = game_core::DEFAULT_MAX_TICKS;
        request.options.stopOnFirstWinner = true;

        std::unordered_set<std::int32_t> runtimeIds;
        std::unordered_set<std::int32_t> occupiedCells;
        runtimeIds.reserve(playerInputs.size() + enemyInputs.size());
        occupiedCells.reserve(playerInputs.size() + enemyInputs.size());
        request.teams.emplace_back(
            makeTeam(playerInputs, game_core::TeamId::Player, catalog_, runtimeIds, occupiedCells));
        request.teams.emplace_back(
            makeTeam(enemyInputs, game_core::TeamId::Enemy, catalog_, runtimeIds, occupiedCells));

        const game_core::BattleSimulator simulator;
        const game_core::BattleSimulateResult result = simulator.simulate(request);
        if (!result.isValid())
            throw ApiError(400, "INVALID_BATTLE", "GameCore rejected the canonical battle request");

        Json events = Json::array();
        for (const game_core::BattleLogEvent& event : result.events)
            events.emplace_back(eventToJson(event));
        Json finalState = Json::array();
        for (const game_core::BattleUnitFinalState& state : result.finalState)
            finalState.emplace_back(finalStateToJson(state));

        runtime_.publishBattleCompleted(
            BattleCompletedEvent{result.durationTicks, result.hasWinner, static_cast<std::int32_t>(result.winnerTeam)});

        return jsonResponse(200, Json{{"schemaVersion", 1},
                                      {"requestId", requestId},
                                      {"battleId", result.battleId},
                                      {"tableVersion", catalog_.tableVersion()},
                                      {"contentHash", catalog_.contentHash()},
                                      {"hasWinner", result.hasWinner},
                                      {"winnerTeam", static_cast<std::int32_t>(result.winnerTeam)},
                                      {"durationTicks", result.durationTicks},
                                      {"checksum", result.checksum},
                                      {"events", std::move(events)},
                                      {"finalState", std::move(finalState)}});
    }
    catch (const ApiError& error)
    {
        return errorResponse(error.status, error.code, error.what());
    }
    catch (const Json::exception& error)
    {
        static_cast<void>(error);
        return errorResponse(400, "INVALID_JSON", "Battle request JSON is malformed or has invalid field types");
    }
    catch (const std::exception& error)
    {
        static_cast<void>(error);
        return errorResponse(500, "INTERNAL_ERROR", "Battle simulation failed");
    }
}
}  // namespace cmc::server
