#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cmc::client
{
struct QuestDefinition final
{
    int questId             = 0;
    int prerequisiteQuestId = 0;
    int target              = 0;
    int rewardAmount        = 0;
    std::string category;
    std::string title;
    std::string description;
    std::string rewardType;
};

struct BattlePassTierDefinition final
{
    int tier = 0;
    std::string freeReward;
    std::string premiumReward;
};

struct BattlePassDefinition final
{
    int seasonId      = 0;
    int pointsPerTier = 0;
    std::string name;
    std::string subtitle;
    std::string seasonEndsLabel;
    std::vector<BattlePassTierDefinition> tiers;
};

class ProgressionCatalog final
{
public:
    bool load(std::string_view resourcePath, std::string& error);

    [[nodiscard]] const std::vector<QuestDefinition>& quests() const noexcept { return _quests; }
    [[nodiscard]] const QuestDefinition* findQuest(int questId) const noexcept;
    [[nodiscard]] const BattlePassDefinition& battlePass() const noexcept { return _battlePass; }

private:
    std::vector<QuestDefinition> _quests;
    BattlePassDefinition _battlePass;
};
}  // namespace cmc::client
