#include "axmol/base/HashMap.h"

#include "Client/Content/StageCatalog.h"

#include "axmol/platform/FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <utility>

namespace
{
bool readString(const rapidjson::Value& object, const char* name, std::string& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsString())
        return false;

    value.assign(member->value.GetString(), member->value.GetStringLength());
    return true;
}

bool readPositiveInt(const rapidjson::Value& object, const char* name, int& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsInt() || member->value.GetInt() <= 0)
        return false;

    value = member->value.GetInt();
    return true;
}
}  // namespace

namespace cmc::client
{
bool StageCatalog::load(std::string_view resourcePath, std::string& error)
{
    _entries.clear();
    _chapterName.clear();
    error.clear();

    const auto* fileUtils = ax::FileUtils::getInstance();
    if (!fileUtils->isFileExist(resourcePath))
    {
        error = "Stage table not found: " + std::string(resourcePath);
        return false;
    }

    const std::string json = fileUtils->getStringFromFile(resourcePath);
    if (json.empty())
    {
        error = "Stage table is empty: " + std::string(resourcePath);
        return false;
    }

    rapidjson::Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        error = "Stage table JSON parse failed at byte " + std::to_string(document.GetErrorOffset()) + ": " +
                rapidjson::GetParseError_En(document.GetParseError());
        return false;
    }

    if (!document.IsObject())
    {
        error = "Stage table root must be an object";
        return false;
    }

    const auto schema = document.FindMember("schemaVersion");
    if (schema == document.MemberEnd() || !schema->value.IsInt() || schema->value.GetInt() != 1)
    {
        error = "Unsupported stage table schemaVersion";
        return false;
    }

    if (!readString(document, "chapterName", _chapterName) || _chapterName.empty())
    {
        error = "Stage table chapterName is invalid";
        return false;
    }

    const auto stages = document.FindMember("stages");
    if (stages == document.MemberEnd() || !stages->value.IsArray())
    {
        error = "Stage table stages must be an array";
        return false;
    }

    ax::HashSet<int> stageIds;
    ax::HashSet<int> stageNumbers;
    stageIds.reserve(stages->value.Size());
    stageNumbers.reserve(stages->value.Size());
    _entries.reserve(stages->value.Size());

    for (const auto& value : stages->value.GetArray())
    {
        StageCatalogEntry entry;
        if (!value.IsObject() || !readPositiveInt(value, "stageId", entry.stageId) ||
            !readPositiveInt(value, "chapterId", entry.chapterId) ||
            !readPositiveInt(value, "stageNumber", entry.stageNumber) ||
            !readPositiveInt(value, "enemyPower", entry.enemyPower) ||
            !readPositiveInt(value, "energyCost", entry.energyCost) ||
            !readPositiveInt(value, "rewardGold", entry.rewardGold) || !readString(value, "name", entry.name) ||
            entry.name.empty())
        {
            error = "Stage table contains an invalid stage entry";
            _entries.clear();
            return false;
        }

        if (!stageIds.emplace(entry.stageId).second || !stageNumbers.emplace(entry.stageNumber).second)
        {
            error = "Stage table contains a duplicate stageId or stageNumber";
            _entries.clear();
            return false;
        }

        _entries.emplace_back(std::move(entry));
    }

    if (_entries.empty())
    {
        error = "Stage table contains no stages";
        return false;
    }

    return true;
}

const StageCatalogEntry* StageCatalog::findById(int stageId) const noexcept
{
    const auto found = std::find_if(_entries.begin(), _entries.end(),
                                    [stageId](const StageCatalogEntry& entry) { return entry.stageId == stageId; });
    return found != _entries.end() ? &*found : nullptr;
}
}  // namespace cmc::client
