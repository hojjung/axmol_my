#include "axmol/base/HashMap.h"

#include "Client/Content/ProgressionCatalog.h"

#include "axmol/platform/FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <utility>

namespace
{
bool readInt(const rapidjson::Value& object, const char* name, int& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsInt())
        return false;
    value = member->value.GetInt();
    return true;
}

bool readString(const rapidjson::Value& object, const char* name, std::string& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsString())
        return false;
    value.assign(member->value.GetString(), member->value.GetStringLength());
    return true;
}

bool isQuestCategory(std::string_view value)
{
    return value == "DAILY" || value == "CAREER" || value == "SEASON";
}

bool isRewardType(std::string_view value)
{
    return value == "GOLD" || value == "GEMS" || value == "ENERGY";
}
}  // namespace

namespace cmc::client
{
bool ProgressionCatalog::load(std::string_view resourcePath, std::string& error)
{
    _quests.clear();
    _battlePass = {};
    error.clear();

    const auto* fileUtils = ax::FileUtils::getInstance();
    if (!fileUtils->isFileExist(resourcePath))
    {
        error = "Progression table not found: " + std::string(resourcePath);
        return false;
    }

    const std::string json = fileUtils->getStringFromFile(resourcePath);
    if (json.empty())
    {
        error = "Progression table is empty: " + std::string(resourcePath);
        return false;
    }

    rapidjson::Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        error = "Progression table JSON parse failed at byte " + std::to_string(document.GetErrorOffset()) + ": " +
                rapidjson::GetParseError_En(document.GetParseError());
        return false;
    }

    if (!document.IsObject())
    {
        error = "Progression table root must be an object";
        return false;
    }

    const auto schema = document.FindMember("schemaVersion");
    const auto quests = document.FindMember("quests");
    const auto pass   = document.FindMember("battlePass");
    if (schema == document.MemberEnd() || !schema->value.IsInt() || schema->value.GetInt() != 1 ||
        quests == document.MemberEnd() || !quests->value.IsArray() || pass == document.MemberEnd() ||
        !pass->value.IsObject())
    {
        error = "Progression table root is invalid";
        return false;
    }

    ax::HashSet<int> questIds;
    ax::HashMap<int, std::string> questCategories;
    questIds.reserve(quests->value.Size());
    questCategories.reserve(quests->value.Size());
    _quests.reserve(quests->value.Size());
    for (const auto& value : quests->value.GetArray())
    {
        QuestDefinition quest;
        if (!value.IsObject() || !readInt(value, "questId", quest.questId) || quest.questId <= 0 ||
            !readInt(value, "prerequisiteQuestId", quest.prerequisiteQuestId) || quest.prerequisiteQuestId < 0 ||
            !readString(value, "category", quest.category) || !isQuestCategory(quest.category) ||
            !readString(value, "title", quest.title) || quest.title.empty() ||
            !readString(value, "description", quest.description) || quest.description.empty() ||
            !readInt(value, "target", quest.target) || quest.target <= 0 ||
            !readString(value, "rewardType", quest.rewardType) || !isRewardType(quest.rewardType) ||
            !readInt(value, "rewardAmount", quest.rewardAmount) || quest.rewardAmount <= 0 ||
            !questIds.emplace(quest.questId).second)
        {
            error = "Progression table contains an invalid quest";
            _quests.clear();
            return false;
        }

        if (quest.prerequisiteQuestId > 0)
        {
            const auto prerequisite = questCategories.find(quest.prerequisiteQuestId);
            if (prerequisite == questCategories.end() || prerequisite->second != quest.category)
            {
                error = "Progression table quest prerequisite must reference an earlier quest in the same category";
                _quests.clear();
                return false;
            }
        }
        questCategories.emplace(quest.questId, quest.category);
        _quests.emplace_back(std::move(quest));
    }

    if (_quests.empty() || !readInt(pass->value, "seasonId", _battlePass.seasonId) || _battlePass.seasonId <= 0 ||
        !readInt(pass->value, "pointsPerTier", _battlePass.pointsPerTier) || _battlePass.pointsPerTier <= 0 ||
        !readString(pass->value, "name", _battlePass.name) || _battlePass.name.empty() ||
        !readString(pass->value, "subtitle", _battlePass.subtitle) || _battlePass.subtitle.empty() ||
        !readString(pass->value, "seasonEndsLabel", _battlePass.seasonEndsLabel) || _battlePass.seasonEndsLabel.empty())
    {
        error = "Progression table battlePass is invalid";
        _quests.clear();
        _battlePass = {};
        return false;
    }

    const auto tiers = pass->value.FindMember("tiers");
    if (tiers == pass->value.MemberEnd() || !tiers->value.IsArray() || tiers->value.Empty())
    {
        error = "Progression table battlePass tiers are invalid";
        _quests.clear();
        _battlePass = {};
        return false;
    }

    ax::HashSet<int> tierIds;
    tierIds.reserve(tiers->value.Size());
    _battlePass.tiers.reserve(tiers->value.Size());
    int expectedTier = 1;
    for (const auto& value : tiers->value.GetArray())
    {
        BattlePassTierDefinition tier;
        if (!value.IsObject() || !readInt(value, "tier", tier.tier) || tier.tier != expectedTier ||
            !readString(value, "freeReward", tier.freeReward) || tier.freeReward.empty() ||
            !readString(value, "premiumReward", tier.premiumReward) || tier.premiumReward.empty() ||
            !tierIds.emplace(tier.tier).second)
        {
            error = "Progression table contains an invalid battlePass tier";
            _quests.clear();
            _battlePass = {};
            return false;
        }
        _battlePass.tiers.emplace_back(std::move(tier));
        ++expectedTier;
    }
    return true;
}

const QuestDefinition* ProgressionCatalog::findQuest(int questId) const noexcept
{
    for (const auto& quest : _quests)
    {
        if (quest.questId == questId)
            return &quest;
    }
    return nullptr;
}
}  // namespace cmc::client
