#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cmc::game_core
{
inline constexpr std::int32_t FIXED_DELTA_MS        = 50;
inline constexpr std::int32_t TICKS_PER_SECOND      = 1000 / FIXED_DELTA_MS;
inline constexpr std::int32_t DEFAULT_MAX_TICKS     = TICKS_PER_SECOND * 90;
inline constexpr std::int32_t ENERGY_FULL           = 100;
inline constexpr std::int32_t DEFAULT_BOARD_WIDTH   = 10;
inline constexpr std::int32_t DEFAULT_BOARD_HEIGHT  = 5;
inline constexpr std::int32_t MAX_BOARD_WIDTH       = 32;
inline constexpr std::int32_t MAX_BOARD_HEIGHT      = 32;
inline constexpr std::int32_t MAX_BOARD_CELLS       = 256;
inline constexpr std::int32_t MAX_BATTLE_UNITS      = 64;
inline constexpr std::int32_t MAX_BATTLE_TICKS      = DEFAULT_MAX_TICKS;
inline constexpr std::int32_t MAX_UNIT_STAT         = 1'000'000;
inline constexpr std::int32_t MAX_EFFECT_VALUE      = 10'000;
inline constexpr std::int32_t MAX_SKILLS_PER_UNIT   = 16;
inline constexpr std::int32_t MAX_EFFECTS_PER_SKILL = 32;

enum class BattleValidationError : std::int32_t
{
    None = 0,
    EmptyTeams,
    InvalidTeam,
    InvalidBoard,
    BoardCapacityExceeded,
    InvalidUnitId,
    DuplicateUnitId,
    InvalidOptions,
    InvalidUnitStats,
    InvalidSkill,
};

enum class TeamId : std::int32_t
{
    Player = 0,
    Enemy  = 1,
};

enum class RoleType : std::int32_t
{
    Pawn   = 0,
    Knight = 1,
    Bishop = 2,
    Rook   = 3,
    Queen  = 4,
    King   = 5,
};

enum class ElementType : std::int32_t
{
    Fire  = 0,
    Water = 1,
    Grass = 2,
    Light = 3,
    Dark  = 4,
};

enum class DamageType : std::int32_t
{
    Physical = 0,
    Magical  = 1,
    Pure     = 2,
};

enum class StatScale : std::int32_t
{
    Fixed   = 0,
    Attack  = 1,
    Defense = 2,
    MaxHp   = 3,
};

enum class UnitState : std::int32_t
{
    Idle    = 0,
    Moving  = 1,
    Casting = 2,
    Dead    = 3,
};

enum class TriggerType : std::int32_t
{
    OnBattleStart = 0,
    OnEnergyFull  = 1,
    OnBasicAttack = 2,
    OnDeath       = 3,
};

enum class TargetRule : std::int32_t
{
    CurrentTarget = 0,
    Self          = 1,
    NearestEnemy  = 2,
    FarthestEnemy = 3,
    LowestHpEnemy = 4,
    LowestHpAlly  = 5,
    AllEnemies    = 6,
    AllAllies     = 7,
    RandomEnemy   = 8,
};

enum class EffectType : std::int32_t
{
    Damage       = 0,
    Heal         = 1,
    ApplyStatus  = 2,
    GainEnergy   = 3,
    JumpToTarget = 4,
    Knockback    = 5,
};

enum class StatusType : std::int32_t
{
    None         = 0,
    Strength     = 1,
    Armor        = 2,
    Haste        = 3,
    Focus        = 4,
    Regeneration = 5,
    Shield       = 6,
    Mark         = 20,
    Weakness     = 21,
    Crack        = 22,
    Slow         = 23,
    Curse        = 24,
    Blind        = 25,
    Burn         = 40,
    Poison       = 41,
    Frostbite    = 42,
    Stun         = 60,
    Silence      = 61,
    Taunt        = 62,
    Root         = 63,
};

enum class BattleEventType : std::int32_t
{
    BattleStart   = 0,
    UnitSpawn     = 1,
    UnitMove      = 2,
    UnitJump      = 3,
    BasicAttack   = 4,
    SkillCast     = 5,
    Damage        = 6,
    Heal          = 7,
    EnergyChanged = 8,
    StatusApplied = 9,
    StatusExpired = 10,
    UnitDeath     = 11,
    BattleEnd     = 12,
};

struct HexCoord final
{
    std::int32_t x = 0;
    std::int32_t z = 0;
};

struct BattleBoardConfig final
{
    std::int32_t width  = DEFAULT_BOARD_WIDTH;
    std::int32_t height = DEFAULT_BOARD_HEIGHT;
    std::vector<std::int32_t> blockedCells;
};

struct BattleOptions final
{
    std::int32_t fixedDeltaMs = FIXED_DELTA_MS;
    std::int32_t maxTicks     = DEFAULT_MAX_TICKS;
    bool stopOnFirstWinner    = true;
};

struct UnitStats final
{
    std::int32_t maxHp               = 100;
    std::int32_t attack              = 20;
    std::int32_t defense             = 10;
    std::int32_t attackRange         = 1;
    std::int32_t attackIntervalTicks = 20;
    std::int32_t moveIntervalTicks   = 10;
    std::int32_t energyOnAttack      = 12;
    std::int32_t energyOnHit         = 4;
    std::int32_t energyOnKill        = 25;
};

struct EffectDefinition final
{
    EffectType type            = EffectType::Damage;
    DamageType damageType      = DamageType::Physical;
    StatScale scale            = StatScale::Attack;
    std::int32_t value         = 100;
    TargetRule targetRule      = TargetRule::CurrentTarget;
    StatusType status          = StatusType::None;
    std::int32_t stacks        = 0;
    std::int32_t durationTicks = 0;
    std::int32_t radius        = 0;
};

struct SkillDefinition final
{
    std::string skillId;
    std::int32_t skillIndex = 0;
    std::string name;
    TriggerType trigger        = TriggerType::OnEnergyFull;
    std::int32_t energyCost    = ENERGY_FULL;
    std::int32_t cooldownTicks = 0;
    TargetRule targetRule      = TargetRule::CurrentTarget;
    std::vector<EffectDefinition> effects;
};

struct BattleUnitSetup final
{
    std::int32_t unitId = 0;
    std::string monsterId;
    std::string displayName;
    TeamId teamId               = TeamId::Player;
    ElementType element         = ElementType::Fire;
    RoleType role               = RoleType::Pawn;
    std::int32_t evolutionStage = 1;
    HexCoord position;
    UnitStats stats;
    std::vector<SkillDefinition> skills;
};

struct BattleTeamSetup final
{
    TeamId teamId = TeamId::Player;
    std::vector<BattleUnitSetup> units;
};

struct BattleSimulateRequest final
{
    std::string battleId;
    std::int32_t seed        = 1;
    std::string tableVersion = "dev";
    BattleBoardConfig board;
    BattleOptions options;
    std::vector<BattleTeamSetup> teams;
};

struct BattleLogEvent final
{
    std::int32_t tick       = 0;
    BattleEventType type    = BattleEventType::BattleStart;
    std::int32_t actorId    = 0;
    std::int32_t targetId   = 0;
    TeamId teamId           = TeamId::Player;
    std::int32_t fromCell   = -1;
    std::int32_t toCell     = -1;
    std::int32_t amount     = 0;
    std::int32_t value      = 0;
    StatusType status       = StatusType::None;
    std::int32_t stacks     = 0;
    std::int32_t skillIndex = 0;
};

struct BattleUnitFinalState final
{
    std::int32_t unitId    = 0;
    TeamId teamId          = TeamId::Player;
    std::int32_t cellIndex = -1;
    std::int32_t hp        = 0;
    std::int32_t energy    = 0;
    bool alive             = false;
};

struct BattleSimulateResult final
{
    std::string battleId;
    BattleValidationError validationError = BattleValidationError::None;
    TeamId winnerTeam                     = TeamId::Player;
    bool hasWinner                        = false;
    std::int32_t durationTicks            = 0;
    std::string checksum;
    std::vector<BattleLogEvent> events;
    std::vector<BattleUnitFinalState> finalState;

    [[nodiscard]] bool isValid() const noexcept { return validationError == BattleValidationError::None; }
};
}  // namespace cmc::game_core
