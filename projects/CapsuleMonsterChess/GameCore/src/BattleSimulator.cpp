#include "cmc/game_core/BattleSimulator.h"

#include "cmc/game_core/DeterministicRandom.h"
#include "cmc/game_core/HexBoard.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace cmc::game_core
{
namespace
{
[[nodiscard]] bool isHardCrowdControl(StatusType status) noexcept
{
    return status == StatusType::Stun || status == StatusType::Silence || status == StatusType::Taunt ||
           status == StatusType::Root;
}

[[nodiscard]] bool isDamageOverTime(StatusType status) noexcept
{
    return status == StatusType::Burn || status == StatusType::Poison || status == StatusType::Frostbite;
}

template <class Enum>
[[nodiscard]] bool enumInRange(Enum value, Enum first, Enum last) noexcept
{
    const auto raw = static_cast<std::int32_t>(value);
    return raw >= static_cast<std::int32_t>(first) && raw <= static_cast<std::int32_t>(last);
}

[[nodiscard]] bool isKnownStatus(StatusType status) noexcept
{
    switch (status)
    {
    case StatusType::None:
    case StatusType::Strength:
    case StatusType::Armor:
    case StatusType::Haste:
    case StatusType::Focus:
    case StatusType::Regeneration:
    case StatusType::Shield:
    case StatusType::Mark:
    case StatusType::Weakness:
    case StatusType::Crack:
    case StatusType::Slow:
    case StatusType::Curse:
    case StatusType::Blind:
    case StatusType::Burn:
    case StatusType::Poison:
    case StatusType::Frostbite:
    case StatusType::Stun:
    case StatusType::Silence:
    case StatusType::Taunt:
    case StatusType::Root:
        return true;
    }
    return false;
}

[[nodiscard]] bool validStats(const UnitStats& stats) noexcept
{
    return stats.maxHp > 0 && stats.maxHp <= MAX_UNIT_STAT && stats.attack >= 0 && stats.attack <= MAX_UNIT_STAT &&
           stats.defense >= 0 && stats.defense <= MAX_UNIT_STAT && stats.attackRange > 0 &&
           stats.attackRange <= MAX_BOARD_CELLS && stats.attackIntervalTicks > 0 &&
           stats.attackIntervalTicks <= MAX_BATTLE_TICKS && stats.moveIntervalTicks > 0 &&
           stats.moveIntervalTicks <= MAX_BATTLE_TICKS && stats.energyOnAttack >= 0 &&
           stats.energyOnAttack <= ENERGY_FULL && stats.energyOnHit >= 0 && stats.energyOnHit <= ENERGY_FULL &&
           stats.energyOnKill >= 0 && stats.energyOnKill <= ENERGY_FULL;
}

[[nodiscard]] bool validEffect(const EffectDefinition& effect) noexcept
{
    if (!enumInRange(effect.type, EffectType::Damage, EffectType::Knockback) ||
        !enumInRange(effect.damageType, DamageType::Physical, DamageType::Pure) ||
        !enumInRange(effect.scale, StatScale::Fixed, StatScale::MaxHp) ||
        !enumInRange(effect.targetRule, TargetRule::CurrentTarget, TargetRule::RandomEnemy) ||
        !isKnownStatus(effect.status))
        return false;
    if (effect.type == EffectType::ApplyStatus && effect.status == StatusType::None)
        return false;
    return effect.value >= -MAX_EFFECT_VALUE && effect.value <= MAX_EFFECT_VALUE && effect.stacks >= 0 &&
           effect.stacks <= 99 && effect.durationTicks >= 0 && effect.durationTicks <= MAX_BATTLE_TICKS &&
           effect.radius >= 0 && effect.radius <= MAX_BOARD_CELLS;
}

[[nodiscard]] bool validSkill(const SkillDefinition& skill) noexcept
{
    if (!enumInRange(skill.trigger, TriggerType::OnBattleStart, TriggerType::OnDeath) ||
        !enumInRange(skill.targetRule, TargetRule::CurrentTarget, TargetRule::RandomEnemy) || skill.energyCost < 0 ||
        skill.energyCost > ENERGY_FULL || skill.cooldownTicks < 0 || skill.cooldownTicks > MAX_BATTLE_TICKS ||
        skill.effects.size() > static_cast<std::size_t>(MAX_EFFECTS_PER_SKILL))
        return false;
    return std::all_of(skill.effects.begin(), skill.effects.end(), validEffect);
}

[[nodiscard]] BattleValidationError validateRequest(const BattleSimulateRequest& request) noexcept
{
    if (request.teams.empty())
        return BattleValidationError::EmptyTeams;

    const auto width  = request.board.width;
    const auto height = request.board.height;
    const auto cells  = static_cast<std::int64_t>(width) * static_cast<std::int64_t>(height);
    if (width <= 0 || height <= 0 || width > MAX_BOARD_WIDTH || height > MAX_BOARD_HEIGHT || cells <= 0 ||
        cells > MAX_BOARD_CELLS || request.board.blockedCells.size() > static_cast<std::size_t>(MAX_BOARD_CELLS))
        return BattleValidationError::InvalidBoard;
    if (request.options.fixedDeltaMs != FIXED_DELTA_MS || request.options.maxTicks > MAX_BATTLE_TICKS)
        return BattleValidationError::InvalidOptions;
    if (std::any_of(request.teams.begin(), request.teams.end(),
                    [](const BattleTeamSetup& team) { return team.units.empty(); }))
        return BattleValidationError::EmptyTeams;
    if (request.teams.size() != 2U)
        return BattleValidationError::InvalidTeam;

    bool hasPlayer        = false;
    bool hasEnemy         = false;
    std::size_t unitCount = 0;
    std::array<std::int32_t, static_cast<std::size_t>(MAX_BATTLE_UNITS)> unitIds{};
    std::size_t unitIdCount = 0;
    for (std::size_t teamIndex = 0; teamIndex < request.teams.size(); ++teamIndex)
    {
        const auto& team = request.teams[teamIndex];
        if (team.teamId == TeamId::Player)
        {
            if (hasPlayer)
                return BattleValidationError::InvalidTeam;
            hasPlayer = true;
        }
        else if (team.teamId == TeamId::Enemy)
        {
            if (hasEnemy)
                return BattleValidationError::InvalidTeam;
            hasEnemy = true;
        }
        else
        {
            return BattleValidationError::InvalidTeam;
        }
        if (team.units.size() > static_cast<std::size_t>(MAX_BATTLE_UNITS) - unitCount)
            return BattleValidationError::BoardCapacityExceeded;
        unitCount += team.units.size();

        for (std::size_t unitIndex = 0; unitIndex < team.units.size(); ++unitIndex)
        {
            const auto& setup      = team.units[unitIndex];
            const auto effectiveId = setup.unitId == 0 ? 1000 + static_cast<std::int32_t>(teamIndex) * 100 +
                                                             static_cast<std::int32_t>(unitIndex)
                                                       : setup.unitId;
            if (effectiveId <= 0)
                return BattleValidationError::InvalidUnitId;
            if (std::find(unitIds.begin(), unitIds.begin() + static_cast<std::ptrdiff_t>(unitIdCount), effectiveId) !=
                unitIds.begin() + static_cast<std::ptrdiff_t>(unitIdCount))
                return BattleValidationError::DuplicateUnitId;
            unitIds[unitIdCount++] = effectiveId;

            if (!enumInRange(setup.element, ElementType::Fire, ElementType::Dark) ||
                !enumInRange(setup.role, RoleType::Pawn, RoleType::King) || !validStats(setup.stats))
                return BattleValidationError::InvalidUnitStats;
            if (setup.skills.size() > static_cast<std::size_t>(MAX_SKILLS_PER_UNIT) ||
                !std::all_of(setup.skills.begin(), setup.skills.end(), validSkill))
                return BattleValidationError::InvalidSkill;
        }
    }
    if (!hasPlayer || !hasEnemy)
        return BattleValidationError::InvalidTeam;

    std::array<std::uint8_t, static_cast<std::size_t>(MAX_BOARD_CELLS)> blocked{};
    std::size_t blockedCount = 0;
    for (const auto cell : request.board.blockedCells)
    {
        if (cell < 0 || cell >= cells || blocked[static_cast<std::size_t>(cell)] != 0U)
            continue;
        blocked[static_cast<std::size_t>(cell)] = 1U;
        ++blockedCount;
    }
    if (unitCount > static_cast<std::size_t>(cells) - blockedCount)
        return BattleValidationError::BoardCapacityExceeded;
    return BattleValidationError::None;
}

struct StatusInstance final
{
    StatusType type             = StatusType::None;
    std::int32_t stacks         = 0;
    std::int32_t remainingTicks = 0;
    std::int32_t sourceUnitId   = 0;
    std::int32_t appliedTick    = 0;
};

struct SimUnit final
{
    BattleUnitSetup setup;
    std::int32_t hp             = 0;
    std::int32_t energy         = 0;
    std::int32_t cellIndex      = -1;
    std::int32_t nextActionTick = 0;
    UnitState state             = UnitState::Idle;
    bool alive                  = true;
    std::vector<StatusInstance> statuses;

    SimUnit(BattleUnitSetup value, std::int32_t cell) : setup(std::move(value)), hp(setup.stats.maxHp), cellIndex(cell)
    {}

    [[nodiscard]] std::int32_t statusStacks(StatusType type) const noexcept
    {
        std::int32_t result = 0;
        for (const auto& status : statuses)
        {
            if (status.type == type)
                result += status.stacks;
        }
        return result;
    }

    [[nodiscard]] bool hasStatus(StatusType type) const noexcept { return statusStacks(type) > 0; }

    [[nodiscard]] std::int32_t statusSource(StatusType type) const noexcept
    {
        for (auto it = statuses.rbegin(); it != statuses.rend(); ++it)
        {
            if (it->type == type)
                return it->sourceUnitId;
        }
        return 0;
    }

    void addStatus(StatusType type,
                   std::int32_t stacksToAdd,
                   std::int32_t durationTicks,
                   std::int32_t sourceUnitId,
                   std::int32_t appliedTick)
    {
        for (auto& status : statuses)
        {
            if (status.type != type)
                continue;
            status.stacks += stacksToAdd;
            if (!isHardCrowdControl(type))
                status.stacks = std::min(status.stacks, 99);
            status.remainingTicks = std::max(status.remainingTicks, durationTicks);
            status.sourceUnitId   = sourceUnitId;
            status.appliedTick    = appliedTick;
            return;
        }

        auto clampedStacks = stacksToAdd;
        if (!isHardCrowdControl(type))
            clampedStacks = std::min(clampedStacks, 99);
        statuses.push_back({type, clampedStacks, durationTicks, sourceUnitId, appliedTick});
    }

    void removeStatusStacks(StatusType type, std::int32_t stacksToRemove)
    {
        auto remaining = stacksToRemove;
        for (std::size_t i = statuses.size(); i > 0U && remaining > 0; --i)
        {
            auto& status = statuses[i - 1U];
            if (status.type != type)
                continue;
            const auto removed = std::min(status.stacks, remaining);
            status.stacks -= removed;
            remaining -= removed;
            if (status.stacks <= 0)
                statuses.erase(statuses.begin() + static_cast<std::ptrdiff_t>(i - 1U));
        }
    }
};

enum class CommandType : std::uint8_t
{
    Damage,
    Status,
};

struct ScheduledCommand final
{
    std::int32_t tick          = 0;
    std::uint64_t order        = 0;
    CommandType type           = CommandType::Damage;
    std::int32_t sourceId      = 0;
    std::int32_t targetId      = 0;
    std::int32_t amount        = 0;
    DamageType damageType      = DamageType::Physical;
    bool basicAttack           = false;
    StatusType status          = StatusType::None;
    std::int32_t stacks        = 0;
    std::int32_t durationTicks = 0;
};

class BattleRuntime final
{
public:
    explicit BattleRuntime(BattleSimulateRequest request)
        : request_(std::move(request)), board_(request_.board), random_(request_.seed)
    {
        result_.battleId = request_.battleId;
        spawnUnits();
    }

    [[nodiscard]] BattleSimulateResult run()
    {
        addEvent(BattleEventType::BattleStart, 0, 0, TeamId::Player, -1, -1, 0, 0, StatusType::None, 0, 0);
        const auto maxTicks = request_.options.maxTicks <= 0 ? DEFAULT_MAX_TICKS : request_.options.maxTicks;
        for (currentTick_ = 0; currentTick_ <= maxTicks; ++currentTick_)
        {
            executeDueCommands();
            tickStatuses();
            for (auto& unit : units_)
                tickUnit(unit);
            executeDueCommands();

            TeamId winner = TeamId::Player;
            if (tryGetWinner(winner))
            {
                result_.hasWinner     = true;
                result_.winnerTeam    = winner;
                result_.durationTicks = currentTick_;
                addEvent(BattleEventType::BattleEnd, 0, 0, winner, -1, -1, 0, 0, StatusType::None, 0, 0);
                finalizeResult();
                return std::move(result_);
            }
        }

        result_.hasWinner     = false;
        result_.durationTicks = maxTicks;
        finalizeResult();
        return std::move(result_);
    }

private:
    [[nodiscard]] SimUnit* unit(std::int32_t unitId) noexcept
    {
        const auto it =
            std::lower_bound(units_.begin(), units_.end(), unitId,
                             [](const SimUnit& candidate, std::int32_t id) { return candidate.setup.unitId < id; });
        return it != units_.end() && it->setup.unitId == unitId ? &*it : nullptr;
    }

    void scheduleDamage(std::int32_t sourceId,
                        std::int32_t targetId,
                        std::int32_t amount,
                        DamageType damageType,
                        bool basicAttack)
    {
        ScheduledCommand command;
        command.tick        = currentTick_;
        command.order       = nextCommandOrder_++;
        command.type        = CommandType::Damage;
        command.sourceId    = sourceId;
        command.targetId    = targetId;
        command.amount      = amount;
        command.damageType  = damageType;
        command.basicAttack = basicAttack;
        commands_.push_back(command);
    }

    void scheduleStatus(std::int32_t sourceId,
                        std::int32_t targetId,
                        StatusType status,
                        std::int32_t stacks,
                        std::int32_t durationTicks)
    {
        ScheduledCommand command;
        command.tick          = currentTick_;
        command.order         = nextCommandOrder_++;
        command.type          = CommandType::Status;
        command.sourceId      = sourceId;
        command.targetId      = targetId;
        command.status        = status;
        command.stacks        = stacks;
        command.durationTicks = durationTicks;
        commands_.push_back(command);
    }

    void executeDueCommands()
    {
        while (true)
        {
            auto best = commands_.end();
            for (auto it = commands_.begin(); it != commands_.end(); ++it)
            {
                if (it->tick > currentTick_)
                    continue;
                if (best == commands_.end() || it->tick < best->tick ||
                    (it->tick == best->tick && it->order < best->order))
                    best = it;
            }
            if (best == commands_.end())
                return;

            const auto command = *best;
            commands_.erase(best);
            if (command.type == CommandType::Damage)
                applyDamage(command.sourceId, command.targetId, command.amount, command.damageType,
                            command.basicAttack);
            else
                applyStatus(command.sourceId, command.targetId, command.status, command.stacks, command.durationTicks);
        }
    }

    void applyDamage(std::int32_t sourceId,
                     std::int32_t targetId,
                     std::int32_t baseAmount,
                     DamageType damageType,
                     bool basicAttack)
    {
        (void)basicAttack;
        auto* source = unit(sourceId);
        auto* target = unit(targetId);
        if (!source || !target || !source->alive || !target->alive)
            return;

        auto amount       = calculateDamage(*source, *target, baseAmount, damageType);
        const auto shield = target->statusStacks(StatusType::Shield);
        if (shield > 0 && amount > 0)
        {
            const auto absorbed = std::min(shield, amount);
            target->removeStatusStacks(StatusType::Shield, absorbed);
            amount -= absorbed;
        }
        if (amount <= 0)
            amount = 1;

        target->hp -= amount;
        addEvent(BattleEventType::Damage, sourceId, targetId, source->setup.teamId, target->cellIndex,
                 target->cellIndex, amount, target->hp, StatusType::None, 0, 0);
        gainEnergy(*source, source->setup.stats.energyOnAttack);
        gainEnergy(*target, target->setup.stats.energyOnHit);
        if (target->hp <= 0)
            killUnit(*source, *target);
    }

    void applyStatus(std::int32_t sourceId,
                     std::int32_t targetId,
                     StatusType status,
                     std::int32_t stacks,
                     std::int32_t durationTicks)
    {
        auto* target = unit(targetId);
        if (!target || !target->alive || status == StatusType::None)
            return;
        const auto safeStacks   = stacks <= 0 ? 1 : stacks;
        const auto safeDuration = durationTicks <= 0 ? TICKS_PER_SECOND * 5 : durationTicks;
        target->addStatus(status, safeStacks, safeDuration, sourceId, currentTick_);
        addEvent(BattleEventType::StatusApplied, sourceId, targetId, target->setup.teamId, target->cellIndex,
                 target->cellIndex, 0, safeDuration, status, safeStacks, 0);
    }

    void tickUnit(SimUnit& actor)
    {
        if (!actor.alive || currentTick_ < actor.nextActionTick)
            return;
        if (actor.hasStatus(StatusType::Stun))
        {
            actor.nextActionTick = currentTick_ + 1;
            return;
        }

        auto* target = selectTarget(actor, TargetRule::NearestEnemy, nullptr);
        if (!target)
        {
            actor.nextActionTick = currentTick_ + 1;
            return;
        }

        const auto* skill = readySkill(actor);
        if (skill)
        {
            castSkill(actor, *target, *skill);
            actor.nextActionTick = currentTick_ + applySpeed(actor, 10);
            return;
        }

        if (board_.distance(actor.cellIndex, target->cellIndex) <= actor.setup.stats.attackRange)
        {
            addEvent(BattleEventType::BasicAttack, actor.setup.unitId, target->setup.unitId, actor.setup.teamId,
                     actor.cellIndex, target->cellIndex, 0, 0, StatusType::None, 0, 0);
            scheduleDamage(actor.setup.unitId, target->setup.unitId, actor.setup.stats.attack, DamageType::Physical,
                           true);
            executeDueCommands();
            actor.nextActionTick = currentTick_ + applySpeed(actor, actor.setup.stats.attackIntervalTicks);
            return;
        }

        if (actor.hasStatus(StatusType::Root))
        {
            actor.nextActionTick = currentTick_ + 1;
            return;
        }

        std::int32_t nextStep       = -1;
        std::int32_t targetDistance = 0;
        if (board_.tryFindNextStep(actor.cellIndex, target->cellIndex, actor.setup.stats.attackRange, nextStep,
                                   targetDistance) &&
            nextStep >= 0)
        {
            const auto from = actor.cellIndex;
            if (board_.moveOccupant(from, nextStep, actor.setup.unitId))
            {
                actor.cellIndex = nextStep;
                addEvent(BattleEventType::UnitMove, actor.setup.unitId, target->setup.unitId, actor.setup.teamId, from,
                         nextStep, 0, 0, StatusType::None, 0, 0);
            }
        }
        actor.nextActionTick = currentTick_ + applySpeed(actor, actor.setup.stats.moveIntervalTicks);
    }

    void castSkill(SimUnit& caster, SimUnit& currentTarget, const SkillDefinition& skill)
    {
        caster.energy -= skill.energyCost <= 0 ? ENERGY_FULL : skill.energyCost;
        caster.energy = std::max(caster.energy, 0);
        addEvent(BattleEventType::SkillCast, caster.setup.unitId, currentTarget.setup.unitId, caster.setup.teamId,
                 caster.cellIndex, currentTarget.cellIndex, 0, caster.energy, StatusType::None, 0, skill.skillIndex);

        for (const auto& effect : skill.effects)
        {
            const auto rule = effect.targetRule == TargetRule::CurrentTarget ? skill.targetRule : effect.targetRule;
            collectTargets(caster, currentTarget, rule, effect.radius);
            const auto targets = targetBuffer_;
            for (const auto targetId : targets)
            {
                auto* target = unit(targetId);
                if (target && target->alive)
                    applyEffect(caster, *target, effect);
            }
        }
    }

    void applyEffect(SimUnit& caster, SimUnit& target, const EffectDefinition& effect)
    {
        switch (effect.type)
        {
        case EffectType::Damage:
            scheduleDamage(caster.setup.unitId, target.setup.unitId, scaleValue(caster, target, effect),
                           effect.damageType, false);
            executeDueCommands();
            break;
        case EffectType::Heal:
            heal(caster, target, scaleValue(caster, target, effect));
            break;
        case EffectType::ApplyStatus:
            scheduleStatus(caster.setup.unitId, target.setup.unitId, effect.status, effect.stacks,
                           effect.durationTicks);
            executeDueCommands();
            break;
        case EffectType::GainEnergy:
            gainEnergy(target, effect.value);
            break;
        case EffectType::JumpToTarget:
            jumpNear(caster, target);
            break;
        case EffectType::Knockback:
            knockback(caster, target);
            break;
        }
    }

    void tickStatuses()
    {
        for (auto& current : units_)
        {
            if (!current.alive)
                continue;
            for (std::size_t i = current.statuses.size(); i > 0U; --i)
            {
                auto status = current.statuses[i - 1U];
                if (isDamageOverTime(status.type) && currentTick_ > status.appliedTick &&
                    (currentTick_ - status.appliedTick) % TICKS_PER_SECOND == 0)
                {
                    const auto sourceId = status.sourceUnitId <= 0 ? current.setup.unitId : status.sourceUnitId;
                    scheduleDamage(sourceId, current.setup.unitId, dotDamage(current, status), DamageType::Magical,
                                   false);
                }

                --status.remainingTicks;
                if (status.remainingTicks <= 0)
                {
                    current.statuses.erase(current.statuses.begin() + static_cast<std::ptrdiff_t>(i - 1U));
                    addEvent(BattleEventType::StatusExpired, status.sourceUnitId, current.setup.unitId,
                             current.setup.teamId, current.cellIndex, current.cellIndex, 0, 0, status.type, 0, 0);
                }
                else
                {
                    current.statuses[i - 1U] = status;
                }
            }
        }
    }

    [[nodiscard]] std::int32_t calculateDamage(const SimUnit& source,
                                               const SimUnit& target,
                                               std::int32_t baseAmount,
                                               DamageType damageType) const
    {
        auto amount = static_cast<double>(baseAmount);
        amount *= elementMultiplier(source.setup.element, target.setup.element);
        amount *= 1.0 + static_cast<double>(source.statusStacks(StatusType::Strength)) / 100.0;
        amount *= 1.0 + static_cast<double>(target.statusStacks(StatusType::Mark)) / 100.0;
        amount *= 1.0 - static_cast<double>(target.statusStacks(StatusType::Weakness)) / 200.0;
        if (damageType != DamageType::Pure)
        {
            auto armorRate = 1.0 + static_cast<double>(target.statusStacks(StatusType::Armor)) / 100.0 -
                             static_cast<double>(target.statusStacks(StatusType::Crack)) / 100.0;
            armorRate          = std::max(armorRate, 0.0);
            const auto defense = static_cast<double>(target.setup.stats.defense) * armorRate;
            amount *= 100.0 / (100.0 + defense);
        }
        const auto rounded = amount >= 0.0 ? std::floor(amount + 0.5) : std::ceil(amount - 0.5);
        return std::max(static_cast<std::int32_t>(rounded), 1);
    }

    [[nodiscard]] static std::int32_t dotDamage(const SimUnit& target, const StatusInstance& status) noexcept
    {
        if (status.type == StatusType::Poison)
            return std::max(1, target.setup.stats.maxHp * status.stacks / 200);
        if (status.type == StatusType::Frostbite)
            return std::max(1, status.stacks);
        return std::max(1, status.stacks * 2);
    }

    [[nodiscard]] static double elementMultiplier(ElementType attacker, ElementType defender) noexcept
    {
        if ((attacker == ElementType::Fire && defender == ElementType::Grass) ||
            (attacker == ElementType::Grass && defender == ElementType::Water) ||
            (attacker == ElementType::Water && defender == ElementType::Fire))
            return 1.15;
        if ((attacker == ElementType::Grass && defender == ElementType::Fire) ||
            (attacker == ElementType::Water && defender == ElementType::Grass) ||
            (attacker == ElementType::Fire && defender == ElementType::Water))
            return 0.85;
        if ((attacker == ElementType::Light && defender == ElementType::Dark) ||
            (attacker == ElementType::Dark && defender == ElementType::Light))
            return 1.15;
        return 1.0;
    }

    [[nodiscard]] static std::int32_t scaleValue(const SimUnit& caster,
                                                 const SimUnit& target,
                                                 const EffectDefinition& effect) noexcept
    {
        std::int32_t basis = caster.setup.stats.attack;
        if (effect.scale == StatScale::Fixed)
            basis = 100;
        else if (effect.scale == StatScale::Defense)
            basis = caster.setup.stats.defense;
        else if (effect.scale == StatScale::MaxHp)
            basis = target.setup.stats.maxHp;
        const auto scaled = static_cast<std::int64_t>(basis) * static_cast<std::int64_t>(effect.value) / 100;
        return static_cast<std::int32_t>(std::clamp<std::int64_t>(scaled, 1, std::numeric_limits<std::int32_t>::max()));
    }

    void heal(const SimUnit& source, SimUnit& target, std::int32_t amount)
    {
        if (!target.alive)
            return;
        const auto before = target.hp;
        target.hp         = std::min(target.hp + amount, target.setup.stats.maxHp);
        const auto healed = target.hp - before;
        if (healed > 0)
            addEvent(BattleEventType::Heal, source.setup.unitId, target.setup.unitId, source.setup.teamId,
                     target.cellIndex, target.cellIndex, healed, target.hp, StatusType::None, 0, 0);
    }

    void gainEnergy(SimUnit& target, std::int32_t amount)
    {
        if (!target.alive || amount == 0)
            return;
        auto modified    = amount;
        const auto curse = target.statusStacks(StatusType::Curse);
        if (curse > 0 && modified > 0)
            modified = std::max(0, modified * std::max(0, 100 - curse) / 100);
        target.energy = std::clamp(target.energy + modified, 0, ENERGY_FULL);
        addEvent(BattleEventType::EnergyChanged, target.setup.unitId, target.setup.unitId, target.setup.teamId,
                 target.cellIndex, target.cellIndex, modified, target.energy, StatusType::None, 0, 0);
    }

    void jumpNear(SimUnit& caster, const SimUnit& target)
    {
        if (caster.hasStatus(StatusType::Root))
            return;
        const auto destination = board_.getBestEmptyNeighborNear(target.cellIndex, caster.cellIndex);
        if (destination < 0)
            return;
        const auto from = caster.cellIndex;
        if (board_.moveOccupant(from, destination, caster.setup.unitId))
        {
            caster.cellIndex = destination;
            addEvent(BattleEventType::UnitJump, caster.setup.unitId, target.setup.unitId, caster.setup.teamId, from,
                     destination, 0, 0, StatusType::None, 0, 0);
        }
    }

    void knockback(const SimUnit& caster, SimUnit& target)
    {
        const auto destination = board_.getKnockbackCell(caster.cellIndex, target.cellIndex);
        if (destination < 0)
            return;
        const auto from = target.cellIndex;
        if (board_.moveOccupant(from, destination, target.setup.unitId))
        {
            target.cellIndex = destination;
            addEvent(BattleEventType::UnitMove, target.setup.unitId, caster.setup.unitId, target.setup.teamId, from,
                     destination, 0, 0, StatusType::None, 0, 0);
        }
    }

    [[nodiscard]] static const SkillDefinition* readySkill(const SimUnit& actor) noexcept
    {
        if (actor.energy < ENERGY_FULL || actor.hasStatus(StatusType::Silence))
            return nullptr;
        for (const auto& skill : actor.setup.skills)
        {
            if (skill.trigger == TriggerType::OnEnergyFull)
                return &skill;
        }
        return nullptr;
    }

    [[nodiscard]] SimUnit* selectTarget(SimUnit& actor, TargetRule rule, SimUnit* currentTarget)
    {
        if (rule == TargetRule::Self)
            return &actor;
        const auto tauntSource = actor.statusSource(StatusType::Taunt);
        if (tauntSource > 0)
        {
            auto* taunter = unit(tauntSource);
            if (taunter && taunter->alive && taunter->setup.teamId != actor.setup.teamId)
                return taunter;
        }
        if (rule == TargetRule::CurrentTarget && currentTarget && currentTarget->alive)
            return currentTarget;

        SimUnit* best  = nullptr;
        auto bestScore = rule == TargetRule::FarthestEnemy ? std::numeric_limits<std::int32_t>::min()
                                                           : std::numeric_limits<std::int32_t>::max();
        for (auto& candidate : units_)
        {
            if (!candidate.alive || candidate.setup.teamId == actor.setup.teamId)
                continue;
            const auto score  = rule == TargetRule::LowestHpEnemy
                                    ? candidate.hp
                                    : board_.distance(actor.cellIndex, candidate.cellIndex);
            const bool better = rule == TargetRule::FarthestEnemy ? score > bestScore : score < bestScore;
            if (better || (score == bestScore && best && candidate.setup.unitId < best->setup.unitId))
            {
                best      = &candidate;
                bestScore = score;
            }
        }
        return best;
    }

    void collectTargets(SimUnit& caster, SimUnit& currentTarget, TargetRule rule, std::int32_t radius)
    {
        targetBuffer_.clear();
        if (rule == TargetRule::AllEnemies || rule == TargetRule::AllAllies)
        {
            for (const auto& candidate : units_)
            {
                if (!candidate.alive)
                    continue;
                const bool ally = candidate.setup.teamId == caster.setup.teamId;
                if ((rule == TargetRule::AllEnemies && !ally) || (rule == TargetRule::AllAllies && ally))
                    targetBuffer_.push_back(candidate.setup.unitId);
            }
            return;
        }

        SimUnit* primary = nullptr;
        if (rule == TargetRule::LowestHpAlly)
            primary = selectLowestHpAlly(caster);
        else if (rule == TargetRule::RandomEnemy)
            primary = selectRandomEnemy(caster);
        else
            primary = selectTarget(caster, rule, &currentTarget);
        if (!primary)
            return;

        targetBuffer_.push_back(primary->setup.unitId);
        if (radius <= 0)
            return;
        for (const auto& candidate : units_)
        {
            if (!candidate.alive || candidate.setup.unitId == primary->setup.unitId ||
                candidate.setup.teamId != primary->setup.teamId)
                continue;
            if (board_.distance(primary->cellIndex, candidate.cellIndex) <= radius)
                targetBuffer_.push_back(candidate.setup.unitId);
        }
    }

    [[nodiscard]] SimUnit* selectLowestHpAlly(SimUnit& actor) noexcept
    {
        SimUnit* best = nullptr;
        auto bestHp   = std::numeric_limits<std::int32_t>::max();
        for (auto& candidate : units_)
        {
            if (!candidate.alive || candidate.setup.teamId != actor.setup.teamId)
                continue;
            if (candidate.hp < bestHp)
            {
                best   = &candidate;
                bestHp = candidate.hp;
            }
        }
        return best;
    }

    [[nodiscard]] SimUnit* selectRandomEnemy(const SimUnit& actor)
    {
        std::vector<SimUnit*> enemies;
        enemies.reserve(units_.size());
        for (auto& candidate : units_)
        {
            if (candidate.alive && candidate.setup.teamId != actor.setup.teamId)
                enemies.push_back(&candidate);
        }
        if (enemies.empty())
            return nullptr;
        return enemies[static_cast<std::size_t>(random_.nextInt(0, static_cast<std::int32_t>(enemies.size())))];
    }

    [[nodiscard]] static std::int32_t applySpeed(const SimUnit& actor, std::int32_t baseInterval) noexcept
    {
        const auto speedPercent =
            std::clamp(100 + actor.statusStacks(StatusType::Haste) - actor.statusStacks(StatusType::Slow), 20, 300);
        const auto interval = baseInterval * 100 / speedPercent;
        return std::max(interval, 1);
    }

    void killUnit(SimUnit& source, SimUnit& target)
    {
        if (!target.alive)
            return;
        target.alive = false;
        target.hp    = 0;
        target.state = UnitState::Dead;
        board_.clearOccupant(target.cellIndex, target.setup.unitId);
        addEvent(BattleEventType::UnitDeath, source.setup.unitId, target.setup.unitId, target.setup.teamId,
                 target.cellIndex, target.cellIndex, 0, 0, StatusType::None, 0, 0);
        gainEnergy(source, source.setup.stats.energyOnKill);
    }

    [[nodiscard]] bool tryGetWinner(TeamId& winner) const noexcept
    {
        bool playerAlive = false;
        bool enemyAlive  = false;
        for (const auto& current : units_)
        {
            if (!current.alive)
                continue;
            if (current.setup.teamId == TeamId::Player)
                playerAlive = true;
            else if (current.setup.teamId == TeamId::Enemy)
                enemyAlive = true;
        }
        if (playerAlive && !enemyAlive)
        {
            winner = TeamId::Player;
            return true;
        }
        if (enemyAlive && !playerAlive)
        {
            winner = TeamId::Enemy;
            return true;
        }
        winner = TeamId::Player;
        return false;
    }

    void spawnUnits()
    {
        for (std::size_t teamIndex = 0; teamIndex < request_.teams.size(); ++teamIndex)
        {
            const auto& team = request_.teams[teamIndex];
            for (std::size_t unitIndex = 0; unitIndex < team.units.size(); ++unitIndex)
            {
                auto setup   = team.units[unitIndex];
                setup.teamId = team.teamId;
                if (setup.unitId == 0)
                    setup.unitId =
                        1000 + static_cast<std::int32_t>(teamIndex) * 100 + static_cast<std::int32_t>(unitIndex);
                auto cell = board_.toIndex(setup.position);
                if (!board_.canStand(cell))
                    cell = fallbackCell(team.teamId);
                if (cell < 0)
                    continue;
                board_.occupy(cell, setup.unitId);
                addEvent(BattleEventType::UnitSpawn, setup.unitId, setup.unitId, setup.teamId, -1, cell,
                         setup.stats.maxHp, setup.evolutionStage, StatusType::None, 0, 0);
                units_.emplace_back(std::move(setup), cell);
            }
        }
        std::sort(units_.begin(), units_.end(),
                  [](const SimUnit& left, const SimUnit& right) { return left.setup.unitId < right.setup.unitId; });
    }

    [[nodiscard]] std::int32_t fallbackCell(TeamId team) const noexcept
    {
        if (team == TeamId::Player)
        {
            for (std::int32_t cell = 0; cell < board_.cellCount(); ++cell)
            {
                if (board_.canStand(cell))
                    return cell;
            }
        }
        else
        {
            for (auto cell = board_.cellCount() - 1; cell >= 0; --cell)
            {
                if (board_.canStand(cell))
                    return cell;
            }
        }
        return -1;
    }

    void addEvent(BattleEventType type,
                  std::int32_t actorId,
                  std::int32_t targetId,
                  TeamId teamId,
                  std::int32_t fromCell,
                  std::int32_t toCell,
                  std::int32_t amount,
                  std::int32_t value,
                  StatusType status,
                  std::int32_t stacks,
                  std::int32_t skillIndex)
    {
        result_.events.push_back({currentTick_, type, actorId, targetId, teamId, fromCell, toCell, amount, value,
                                  status, stacks, skillIndex});
    }

    void finalizeResult()
    {
        result_.finalState.reserve(units_.size());
        for (const auto& current : units_)
        {
            result_.finalState.push_back({current.setup.unitId, current.setup.teamId, current.cellIndex, current.hp,
                                          current.energy, current.alive});
        }
        result_.checksum = computeBattleChecksum(result_);
    }

    BattleSimulateRequest request_;
    BattleSimulateResult result_;
    HexBoard board_;
    DeterministicRandom random_;
    std::vector<SimUnit> units_;
    std::vector<ScheduledCommand> commands_;
    std::vector<std::int32_t> targetBuffer_;
    std::uint64_t nextCommandOrder_ = 0;
    std::int32_t currentTick_       = 0;
};

[[nodiscard]] StatusType elementStatus(ElementType element) noexcept
{
    if (element == ElementType::Fire)
        return StatusType::Burn;
    if (element == ElementType::Water)
        return StatusType::Frostbite;
    if (element == ElementType::Grass)
        return StatusType::Poison;
    if (element == ElementType::Dark)
        return StatusType::Mark;
    return StatusType::Weakness;
}

[[nodiscard]] SkillDefinition createDefaultSkill(RoleType role, ElementType element)
{
    SkillDefinition skill;
    skill.skillId = std::to_string(static_cast<std::int32_t>(role)) + "_" +
                    std::to_string(static_cast<std::int32_t>(element)) + "_001";
    skill.skillIndex = static_cast<std::int32_t>(role) * 10 + static_cast<std::int32_t>(element);
    skill.targetRule = role == RoleType::Queen ? TargetRule::AllEnemies : TargetRule::CurrentTarget;
    if (role == RoleType::Knight)
    {
        skill.name = "Leap Assassination";
        skill.effects.push_back({EffectType::JumpToTarget});
        skill.effects.push_back(
            {EffectType::Damage, DamageType::Physical, StatScale::Attack, 160, TargetRule::CurrentTarget});
        skill.effects.push_back({EffectType::ApplyStatus, DamageType::Physical, StatScale::Attack, 100,
                                 TargetRule::CurrentTarget, StatusType::Mark, 20, TICKS_PER_SECOND * 6});
    }
    else if (role == RoleType::Queen)
    {
        skill.name = "Crown Barrage";
        skill.effects.push_back(
            {EffectType::Damage, DamageType::Magical, StatScale::Attack, 75, TargetRule::AllEnemies});
        skill.effects.push_back({EffectType::ApplyStatus, DamageType::Physical, StatScale::Attack, 100,
                                 TargetRule::AllEnemies, elementStatus(element), 10, TICKS_PER_SECOND * 5});
    }
    else if (role == RoleType::Pawn)
    {
        skill.name = "Shield Challenge";
        skill.effects.push_back({EffectType::ApplyStatus, DamageType::Physical, StatScale::Attack, 100,
                                 TargetRule::Self, StatusType::Armor, 20, TICKS_PER_SECOND * 8});
        skill.effects.push_back({EffectType::ApplyStatus, DamageType::Physical, StatScale::Attack, 100,
                                 TargetRule::CurrentTarget, StatusType::Taunt, 1, TICKS_PER_SECOND * 2});
    }
    else
    {
        skill.name = "Role Strike";
        skill.effects.push_back(
            {EffectType::Damage, DamageType::Physical, StatScale::Attack, 140, TargetRule::CurrentTarget});
        skill.effects.push_back({EffectType::ApplyStatus, DamageType::Physical, StatScale::Attack, 100,
                                 TargetRule::CurrentTarget, elementStatus(element), 10, TICKS_PER_SECOND * 5});
    }
    return skill;
}

[[nodiscard]] BattleUnitSetup createSmokeUnit(std::int32_t id,
                                              TeamId team,
                                              std::string name,
                                              ElementType element,
                                              RoleType role,
                                              std::int32_t stage,
                                              std::int32_t x,
                                              std::int32_t z)
{
    BattleUnitSetup unit;
    unit.unitId                    = id;
    unit.monsterId                 = name;
    unit.displayName               = std::move(name);
    unit.teamId                    = team;
    unit.element                   = element;
    unit.role                      = role;
    unit.evolutionStage            = stage;
    unit.position                  = {x, z};
    unit.stats.maxHp               = role == RoleType::Pawn ? 180 : 110;
    unit.stats.attack              = role == RoleType::Queen ? 32 : role == RoleType::Knight ? 26 : 18;
    unit.stats.defense             = role == RoleType::Pawn ? 24 : 10;
    unit.stats.attackRange         = role == RoleType::Queen ? 3 : 1;
    unit.stats.attackIntervalTicks = role == RoleType::Queen ? 24 : 20;
    unit.skills.push_back(createDefaultSkill(role, element));
    return unit;
}
}  // namespace

BattleValidationError validateBattleRequest(const BattleSimulateRequest& request) noexcept
{
    return validateRequest(request);
}

BattleSimulateResult BattleSimulator::simulate(const BattleSimulateRequest& request) const
{
    const auto validationError = validateRequest(request);
    if (validationError != BattleValidationError::None)
    {
        BattleSimulateResult result;
        result.battleId        = request.battleId;
        result.validationError = validationError;
        return result;
    }
    return BattleRuntime(request).run();
}

std::string computeBattleChecksum(const BattleSimulateResult& result)
{
    std::uint32_t hash = 2166136261U;
    const auto mix     = [&hash](std::int32_t value) {
        hash ^= static_cast<std::uint32_t>(value);
        hash *= 16777619U;
    };
    mix(result.hasWinner ? 1 : 0);
    mix(static_cast<std::int32_t>(result.winnerTeam));
    mix(result.durationTicks);
    for (const auto& event : result.events)
    {
        mix(event.tick);
        mix(static_cast<std::int32_t>(event.type));
        mix(event.actorId);
        mix(event.targetId);
        mix(static_cast<std::int32_t>(event.teamId));
        mix(event.fromCell);
        mix(event.toCell);
        mix(event.amount);
        mix(event.value);
        mix(static_cast<std::int32_t>(event.status));
        mix(event.stacks);
        mix(event.skillIndex);
    }
    for (const auto& state : result.finalState)
    {
        mix(state.unitId);
        mix(static_cast<std::int32_t>(state.teamId));
        mix(state.cellIndex);
        mix(state.hp);
        mix(state.energy);
        mix(state.alive ? 1 : 0);
    }

    constexpr std::array<char, 16> HEX = {'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string checksum(8U, '0');
    for (std::size_t i = 0; i < checksum.size(); ++i)
        checksum[i] = HEX[(hash >> ((7U - i) * 4U)) & 0xFU];
    return checksum;
}

BattleSimulateRequest createSmokeBattleRequest()
{
    BattleSimulateRequest request;
    request.battleId     = "dev-smoke";
    request.seed         = 424242;
    request.tableVersion = "dev";

    BattleTeamSetup player;
    player.teamId = TeamId::Player;
    player.units.push_back(
        createSmokeUnit(101, TeamId::Player, "Fire Pawn", ElementType::Fire, RoleType::Pawn, 1, 1, 2));
    player.units.push_back(
        createSmokeUnit(102, TeamId::Player, "Water Knight", ElementType::Water, RoleType::Knight, 2, 1, 1));

    BattleTeamSetup enemy;
    enemy.teamId = TeamId::Enemy;
    enemy.units.push_back(
        createSmokeUnit(201, TeamId::Enemy, "Grass Pawn", ElementType::Grass, RoleType::Pawn, 1, 8, 2));
    enemy.units.push_back(
        createSmokeUnit(202, TeamId::Enemy, "Dark Queen", ElementType::Dark, RoleType::Queen, 2, 8, 1));
    request.teams.push_back(std::move(player));
    request.teams.push_back(std::move(enemy));
    return request;
}
}  // namespace cmc::game_core
