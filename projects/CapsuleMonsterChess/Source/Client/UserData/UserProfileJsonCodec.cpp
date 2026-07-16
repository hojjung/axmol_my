#include "axmol/base/HashMap.h"

#include "Client/UserData/UserProfileJsonCodec.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <array>
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

bool readNonNegativeInt64(const rapidjson::Value& object, const char* name, std::int64_t& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsInt64() || member->value.GetInt64() < 0)
        return false;

    value = member->value.GetInt64();
    return true;
}

bool readBool(const rapidjson::Value& object, const char* name, bool& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsBool())
        return false;
    value = member->value.GetBool();
    return true;
}

void migrateSchema1(cmc::client::UserProfile& profile)
{
    profile.questProgress.clear();
    profile.battlePass = {};
}

template <std::size_t Size>
bool readIdArray(const rapidjson::Value& object, const char* name, std::array<int, Size>& values)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsArray() || member->value.Size() != Size)
        return false;

    std::size_t index = 0;
    for (const auto& value : member->value.GetArray())
    {
        if (!value.IsInt() || value.GetInt() < 0)
            return false;
        values[index++] = value.GetInt();
    }
    return true;
}

bool readPositiveIdVector(const rapidjson::Value& object, const char* name, std::vector<int>& values)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsArray())
        return false;

    values.clear();
    values.reserve(member->value.Size());
    for (const auto& value : member->value.GetArray())
    {
        if (!value.IsInt() || value.GetInt() <= 0)
            return false;
        values.emplace_back(value.GetInt());
    }
    return true;
}

template <std::size_t Size>
void writeIdArray(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                  const char* name,
                  const std::array<int, Size>& values)
{
    writer.Key(name);
    writer.StartArray();
    for (const int value : values)
        writer.Int(value);
    writer.EndArray();
}
}  // namespace

namespace cmc::client
{
bool UserProfileJsonCodec::decode(std::string_view json, UserProfile& profile, std::string& error)
{
    error.clear();
    if (json.empty())
    {
        error = "User profile JSON is empty";
        return false;
    }

    rapidjson::Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        error = "User profile JSON parse failed at byte " + std::to_string(document.GetErrorOffset()) + ": " +
                rapidjson::GetParseError_En(document.GetParseError());
        return false;
    }

    if (!document.IsObject())
    {
        error = "User profile root must be an object";
        return false;
    }

    const auto schema = document.FindMember("schemaVersion");
    if (schema == document.MemberEnd() || !schema->value.IsInt())
    {
        error = "Unsupported user profile schemaVersion";
        return false;
    }
    const int schemaVersion = schema->value.GetInt();
    if (schemaVersion != USER_PROFILE_LEGACY_SCHEMA_VERSION && schemaVersion != USER_PROFILE_SCHEMA_VERSION)
    {
        error = "Unsupported user profile schemaVersion";
        return false;
    }

    UserProfile decoded;
    const auto currencies = document.FindMember("currencies");
    if (currencies == document.MemberEnd() || !currencies->value.IsObject() ||
        !readNonNegativeInt64(currencies->value, "gold", decoded.currencies.gold) ||
        !readNonNegativeInt64(currencies->value, "gems", decoded.currencies.gems) ||
        !readNonNegativeInt64(currencies->value, "energy", decoded.currencies.energy))
    {
        error = "User profile currencies are invalid";
        return false;
    }

    const auto ownedUnits = document.FindMember("ownedUnits");
    if (ownedUnits == document.MemberEnd() || !ownedUnits->value.IsArray())
    {
        error = "User profile ownedUnits must be an array";
        return false;
    }

    decoded.ownedUnits.reserve(ownedUnits->value.Size());
    for (const auto& value : ownedUnits->value.GetArray())
    {
        OwnedUnitState unit;
        if (!value.IsObject() || !readInt(value, "unitId", unit.unitId) || !readInt(value, "level", unit.level) ||
            !readInt(value, "shards", unit.shards) || !readIdArray(value, "equipmentItemIds", unit.equipmentItemIds))
        {
            error = "User profile contains an invalid owned unit";
            return false;
        }
        decoded.ownedUnits.emplace_back(std::move(unit));
    }

    const auto deckPresets = document.FindMember("deckPresets");
    if (deckPresets == document.MemberEnd() || !deckPresets->value.IsArray())
    {
        error = "User profile deckPresets must be an array";
        return false;
    }

    decoded.deckPresets.reserve(deckPresets->value.Size());
    for (const auto& value : deckPresets->value.GetArray())
    {
        DeckPreset preset;
        if (!value.IsObject() || !readIdArray(value, "unitIds", preset.unitIds))
        {
            error = "User profile contains an invalid deck preset";
            return false;
        }
        decoded.deckPresets.emplace_back(std::move(preset));
    }

    int selectedPresetIndex = 0;
    if (!readInt(document, "selectedPresetIndex", selectedPresetIndex) || selectedPresetIndex < 0)
    {
        error = "User profile selectedPresetIndex is invalid";
        return false;
    }
    decoded.selectedPresetIndex = static_cast<std::size_t>(selectedPresetIndex);

    if (schemaVersion == USER_PROFILE_LEGACY_SCHEMA_VERSION)
    {
        migrateSchema1(decoded);
    }
    else
    {
        const auto questProgress = document.FindMember("questProgress");
        const auto battlePass    = document.FindMember("battlePass");
        if (questProgress == document.MemberEnd() || !questProgress->value.IsArray() ||
            battlePass == document.MemberEnd() || !battlePass->value.IsObject())
        {
            error = "User profile progression data is invalid";
            return false;
        }

        decoded.questProgress.reserve(questProgress->value.Size());
        for (const auto& value : questProgress->value.GetArray())
        {
            QuestProgressState quest;
            if (!value.IsObject() || !readInt(value, "questId", quest.questId) ||
                !readInt(value, "progress", quest.progress) || !readBool(value, "rewardClaimed", quest.rewardClaimed))
            {
                error = "User profile contains invalid quest progress";
                return false;
            }
            decoded.questProgress.emplace_back(quest);
        }

        if (!readInt(battlePass->value, "seasonId", decoded.battlePass.seasonId) ||
            !readInt(battlePass->value, "points", decoded.battlePass.points) ||
            !readBool(battlePass->value, "premiumUnlocked", decoded.battlePass.premiumUnlocked) ||
            !readPositiveIdVector(battlePass->value, "claimedFreeTierIds", decoded.battlePass.claimedFreeTierIds) ||
            !readPositiveIdVector(battlePass->value, "claimedPremiumTierIds", decoded.battlePass.claimedPremiumTierIds))
        {
            error = "User profile battlePass progress is invalid";
            return false;
        }
    }

    if (!validate(decoded, error))
        return false;

    profile = std::move(decoded);
    return true;
}

bool UserProfileJsonCodec::encode(const UserProfile& profile, std::string& json, std::string& error)
{
    json.clear();
    if (!validate(profile, error))
        return false;

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 2);

    writer.StartObject();
    writer.Key("schemaVersion");
    writer.Int(USER_PROFILE_SCHEMA_VERSION);

    writer.Key("currencies");
    writer.StartObject();
    writer.Key("gold");
    writer.Int64(profile.currencies.gold);
    writer.Key("gems");
    writer.Int64(profile.currencies.gems);
    writer.Key("energy");
    writer.Int64(profile.currencies.energy);
    writer.EndObject();

    writer.Key("ownedUnits");
    writer.StartArray();
    for (const auto& unit : profile.ownedUnits)
    {
        writer.StartObject();
        writer.Key("unitId");
        writer.Int(unit.unitId);
        writer.Key("level");
        writer.Int(unit.level);
        writer.Key("shards");
        writer.Int(unit.shards);
        writeIdArray(writer, "equipmentItemIds", unit.equipmentItemIds);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("deckPresets");
    writer.StartArray();
    for (const auto& preset : profile.deckPresets)
    {
        writer.StartObject();
        writeIdArray(writer, "unitIds", preset.unitIds);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("questProgress");
    writer.StartArray();
    for (const auto& quest : profile.questProgress)
    {
        writer.StartObject();
        writer.Key("questId");
        writer.Int(quest.questId);
        writer.Key("progress");
        writer.Int(quest.progress);
        writer.Key("rewardClaimed");
        writer.Bool(quest.rewardClaimed);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("battlePass");
    writer.StartObject();
    writer.Key("seasonId");
    writer.Int(profile.battlePass.seasonId);
    writer.Key("points");
    writer.Int(profile.battlePass.points);
    writer.Key("premiumUnlocked");
    writer.Bool(profile.battlePass.premiumUnlocked);
    writer.Key("claimedFreeTierIds");
    writer.StartArray();
    for (const int tier : profile.battlePass.claimedFreeTierIds)
        writer.Int(tier);
    writer.EndArray();
    writer.Key("claimedPremiumTierIds");
    writer.StartArray();
    for (const int tier : profile.battlePass.claimedPremiumTierIds)
        writer.Int(tier);
    writer.EndArray();
    writer.EndObject();

    writer.Key("selectedPresetIndex");
    writer.Uint(static_cast<unsigned int>(profile.selectedPresetIndex));
    writer.EndObject();

    json.assign(buffer.GetString(), buffer.GetSize());
    error.clear();
    return true;
}

bool UserProfileJsonCodec::validate(const UserProfile& profile, std::string& error)
{
    error.clear();
    if (profile.currencies.gold < 0 || profile.currencies.gems < 0 || profile.currencies.energy < 0)
    {
        error = "User profile currencies cannot be negative";
        return false;
    }

    ax::HashSet<int> ownedUnitIds;
    ownedUnitIds.reserve(profile.ownedUnits.size());
    for (const auto& unit : profile.ownedUnits)
    {
        if (unit.unitId <= 0 || unit.level <= 0 || unit.shards < 0)
        {
            error = "Owned unit id, level, or shards are invalid";
            return false;
        }
        if (!ownedUnitIds.emplace(unit.unitId).second)
        {
            error = "User profile contains a duplicate owned unit";
            return false;
        }
        for (const int itemId : unit.equipmentItemIds)
        {
            if (itemId < 0)
            {
                error = "Owned unit equipment item ids cannot be negative";
                return false;
            }
        }
    }

    if (profile.deckPresets.empty() || profile.deckPresets.size() > USER_DECK_PRESET_LIMIT)
    {
        error = "User profile must contain between one and five deck presets";
        return false;
    }
    if (profile.selectedPresetIndex >= profile.deckPresets.size())
    {
        error = "User profile selectedPresetIndex is out of range";
        return false;
    }

    for (const auto& preset : profile.deckPresets)
    {
        ax::HashSet<int> deckUnitIds;
        for (const int unitId : preset.unitIds)
        {
            if (unitId < 0)
            {
                error = "Deck unit ids cannot be negative";
                return false;
            }
            if (unitId == 0)
                continue;
            if (ownedUnitIds.find(unitId) == ownedUnitIds.end())
            {
                error = "Deck contains a unit that is not owned";
                return false;
            }
            if (!deckUnitIds.emplace(unitId).second)
            {
                error = "Deck contains the same unit more than once";
                return false;
            }
        }
    }

    ax::HashSet<int> questIds;
    questIds.reserve(profile.questProgress.size());
    for (const auto& quest : profile.questProgress)
    {
        if (quest.questId <= 0 || quest.progress < 0 || !questIds.emplace(quest.questId).second)
        {
            error = "User profile quest progress is invalid";
            return false;
        }
    }

    if (profile.battlePass.seasonId < 0 || profile.battlePass.points < 0)
    {
        error = "User profile battlePass progress is invalid";
        return false;
    }

    if (profile.battlePass.seasonId == 0 &&
        (profile.battlePass.points != 0 || profile.battlePass.premiumUnlocked ||
         !profile.battlePass.claimedFreeTierIds.empty() || !profile.battlePass.claimedPremiumTierIds.empty()))
    {
        error = "User profile inactive battlePass contains active progress";
        return false;
    }

    if (!profile.battlePass.premiumUnlocked && !profile.battlePass.claimedPremiumTierIds.empty())
    {
        error = "User profile contains premium claims without premium access";
        return false;
    }

    const auto validateClaimedTiers = [&error](const std::vector<int>& tiers) {
        ax::HashSet<int> uniqueTiers;
        for (const int tier : tiers)
        {
            if (tier <= 0 || !uniqueTiers.emplace(tier).second)
            {
                error = "User profile battlePass claimed tiers are invalid";
                return false;
            }
        }
        return true;
    };
    if (!validateClaimedTiers(profile.battlePass.claimedFreeTierIds) ||
        !validateClaimedTiers(profile.battlePass.claimedPremiumTierIds))
        return false;

    return true;
}
}  // namespace cmc::client
