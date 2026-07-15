#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmc::client
{
inline constexpr int USER_PROFILE_LEGACY_SCHEMA_VERSION = 1;
inline constexpr int USER_PROFILE_SCHEMA_VERSION        = 2;
inline constexpr std::size_t USER_DECK_SLOT_COUNT       = 5;
inline constexpr std::size_t USER_DECK_PRESET_LIMIT     = 5;
inline constexpr std::size_t USER_EQUIPMENT_SLOT_COUNT  = 6;

struct UserCurrencies final
{
    std::int64_t gold   = 0;
    std::int64_t gems   = 0;
    std::int64_t energy = 0;
};

struct OwnedUnitState final
{
    int unitId = 0;
    int level  = 1;
    int shards = 0;
    std::array<int, USER_EQUIPMENT_SLOT_COUNT> equipmentItemIds{};
};

struct DeckPreset final
{
    std::array<int, USER_DECK_SLOT_COUNT> unitIds{};
};

struct QuestProgressState final
{
    int questId        = 0;
    int progress       = 0;
    bool rewardClaimed = false;
};

struct BattlePassProgressState final
{
    int seasonId         = 0;
    int points           = 0;
    bool premiumUnlocked = false;
    std::vector<int> claimedFreeTierIds;
    std::vector<int> claimedPremiumTierIds;
};

struct UserProfile final
{
    UserCurrencies currencies;
    std::vector<OwnedUnitState> ownedUnits;
    std::vector<DeckPreset> deckPresets;
    std::vector<QuestProgressState> questProgress;
    BattlePassProgressState battlePass;
    std::size_t selectedPresetIndex = 0;

    [[nodiscard]] const OwnedUnitState* findOwnedUnit(int unitId) const noexcept;
    [[nodiscard]] OwnedUnitState* findOwnedUnit(int unitId) noexcept;
    [[nodiscard]] const DeckPreset* selectedDeck() const noexcept;
    [[nodiscard]] DeckPreset* selectedDeck() noexcept;
    [[nodiscard]] const QuestProgressState* findQuestProgress(int questId) const noexcept;
    [[nodiscard]] QuestProgressState* findQuestProgress(int questId) noexcept;
};

inline const OwnedUnitState* UserProfile::findOwnedUnit(int unitId) const noexcept
{
    for (const auto& unit : ownedUnits)
    {
        if (unit.unitId == unitId)
            return &unit;
    }
    return nullptr;
}

inline OwnedUnitState* UserProfile::findOwnedUnit(int unitId) noexcept
{
    for (auto& unit : ownedUnits)
    {
        if (unit.unitId == unitId)
            return &unit;
    }
    return nullptr;
}

inline const DeckPreset* UserProfile::selectedDeck() const noexcept
{
    return selectedPresetIndex < deckPresets.size() ? &deckPresets[selectedPresetIndex] : nullptr;
}

inline DeckPreset* UserProfile::selectedDeck() noexcept
{
    return selectedPresetIndex < deckPresets.size() ? &deckPresets[selectedPresetIndex] : nullptr;
}

inline const QuestProgressState* UserProfile::findQuestProgress(int questId) const noexcept
{
    for (const auto& quest : questProgress)
    {
        if (quest.questId == questId)
            return &quest;
    }
    return nullptr;
}

inline QuestProgressState* UserProfile::findQuestProgress(int questId) noexcept
{
    for (auto& quest : questProgress)
    {
        if (quest.questId == questId)
            return &quest;
    }
    return nullptr;
}
}  // namespace cmc::client
