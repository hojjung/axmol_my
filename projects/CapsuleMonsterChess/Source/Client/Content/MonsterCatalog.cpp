#include "axmol/base/HashMap.h"

#include "Client/Content/MonsterCatalog.h"

#include "axmol/platform/FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

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

bool readInt(const rapidjson::Value& object, const char* name, int& value)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsInt())
        return false;

    value = member->value.GetInt();
    return true;
}

bool readStringArray(const rapidjson::Value& object, const char* name, std::vector<std::string>& values)
{
    const auto member = object.FindMember(name);
    if (member == object.MemberEnd() || !member->value.IsArray())
        return false;

    values.clear();
    values.reserve(member->value.Size());
    for (const auto& value : member->value.GetArray())
    {
        if (!value.IsString())
            return false;
        values.emplace_back(value.GetString(), value.GetStringLength());
    }
    return true;
}
}  // namespace

namespace cmc::client
{
bool MonsterCatalog::load(std::string_view resourcePath, std::string& error)
{
    _entries.clear();
    _iconCount = 0;
    error.clear();

    const auto* fileUtils = ax::FileUtils::getInstance();
    if (!fileUtils->isFileExist(resourcePath))
    {
        error = "Monster table not found: " + std::string(resourcePath);
        return false;
    }

    const std::string json = fileUtils->getStringFromFile(resourcePath);
    if (json.empty())
    {
        error = "Monster table is empty: " + std::string(resourcePath);
        return false;
    }

    rapidjson::Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        error = "Monster table JSON parse failed at byte " + std::to_string(document.GetErrorOffset()) + ": " +
                rapidjson::GetParseError_En(document.GetParseError());
        return false;
    }

    if (!document.IsObject())
    {
        error = "Monster table root must be an object";
        return false;
    }

    const auto schema = document.FindMember("schemaVersion");
    if (schema == document.MemberEnd() || !schema->value.IsInt() || schema->value.GetInt() != 1)
    {
        error = "Unsupported monster table schemaVersion";
        return false;
    }

    const auto characters = document.FindMember("characters");
    if (characters == document.MemberEnd() || !characters->value.IsArray())
    {
        error = "Monster table characters must be an array";
        return false;
    }

    ax::HashSet<int> unitIds;
    ax::HashSet<std::string> nameKeys;
    unitIds.reserve(characters->value.Size());
    nameKeys.reserve(characters->value.Size());
    _entries.reserve(characters->value.Size());

    for (const auto& value : characters->value.GetArray())
    {
        MonsterCatalogEntry entry;
        if (!value.IsObject() || !readInt(value, "unitId", entry.unitId) || entry.unitId <= 0 ||
            !readInt(value, "hp", entry.hp) || entry.hp <= 0 || !readInt(value, "ad", entry.ad) || entry.ad < 0 ||
            !readInt(value, "ap", entry.ap) || entry.ap < 0 || !readInt(value, "adDefense", entry.adDefense) ||
            entry.adDefense < 0 || !readInt(value, "apDefense", entry.apDefense) || entry.apDefense < 0 ||
            !readInt(value, "range", entry.range) || entry.range <= 0 || !readString(value, "nameKey", entry.nameKey) ||
            entry.nameKey.empty() || !readString(value, "modelId", entry.modelId) ||
            !readString(value, "icon", entry.icon) || !readString(value, "element", entry.element) ||
            !readString(value, "role", entry.role) || !readString(value, "evolutionKey", entry.evolutionKey) ||
            !readStringArray(value, "skills", entry.skills))
        {
            error = "Monster table contains an invalid character entry";
            _entries.clear();
            return false;
        }

        if (!unitIds.emplace(entry.unitId).second || !nameKeys.emplace(entry.nameKey).second)
        {
            error = "Monster table contains a duplicate unitId or nameKey";
            _entries.clear();
            return false;
        }

        if (!entry.icon.empty() && fileUtils->isFileExist(entry.icon))
            ++_iconCount;
        _entries.emplace_back(std::move(entry));
    }

    if (_entries.empty())
    {
        error = "Monster table contains no characters";
        return false;
    }

    return true;
}

const MonsterCatalogEntry* MonsterCatalog::findById(int unitId) const noexcept
{
    for (const auto& entry : _entries)
    {
        if (entry.unitId == unitId)
            return &entry;
    }
    return nullptr;
}
}  // namespace cmc::client
