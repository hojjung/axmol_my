#include "ServerMonsterCatalog.h"

#include "Util/json.hpp"
#include "mbedtls/sha256.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace cmc::server
{
namespace
{
using Json = nlohmann::json;

game_core::ElementType parseElement(std::string_view value)
{
    if (value == "Fire")
        return game_core::ElementType::Fire;
    if (value == "Water")
        return game_core::ElementType::Water;
    if (value == "Grass")
        return game_core::ElementType::Grass;
    if (value == "Light")
        return game_core::ElementType::Light;
    if (value == "Dark")
        return game_core::ElementType::Dark;
    throw std::runtime_error("Unknown monster element: " + std::string(value));
}

game_core::RoleType parseRole(std::string_view value)
{
    if (value == "Pawn")
        return game_core::RoleType::Pawn;
    if (value == "Knight")
        return game_core::RoleType::Knight;
    if (value == "Bishop")
        return game_core::RoleType::Bishop;
    if (value == "Rook")
        return game_core::RoleType::Rook;
    if (value == "Queen")
        return game_core::RoleType::Queen;
    if (value == "King")
        return game_core::RoleType::King;
    throw std::runtime_error("Unknown monster role: " + std::string(value));
}

bool isLowerHexHash(std::string_view value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

std::string sha256(std::string_view bytes)
{
    std::array<unsigned char, 32> digest{};
    const int result =
        mbedtls_sha256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), digest.data(), 0);
    if (result != 0)
        throw std::runtime_error("Mbed TLS failed to hash the monster table");

    constexpr char HEX[] = "0123456789abcdef";
    std::string encoded(digest.size() * 2, '0');
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        encoded[index * 2]     = HEX[digest[index] >> 4U];
        encoded[index * 2 + 1] = HEX[digest[index] & 0x0FU];
    }
    return encoded;
}
}  // namespace

void ServerMonsterCatalog::load(const std::filesystem::path& tablePath)
{
    std::ifstream stream(tablePath);
    if (!stream)
        throw std::runtime_error("Cannot open monster table: " + tablePath.string());

    Json root;
    stream >> root;
    if (!root.is_object() || root.at("schemaVersion").get<int>() != 1)
        throw std::runtime_error("Unsupported monster table schema");

    const std::string nextVersion = root.at("tableVersion").get<std::string>();
    const std::string nextHash    = root.at("contentHash").get<std::string>();
    if (nextVersion.empty() || !isLowerHexHash(nextHash))
        throw std::runtime_error("Monster table identity is invalid");

    const Json& characters = root.at("characters");
    if (!characters.is_array() || characters.empty())
        throw std::runtime_error("Monster table has no characters");

    const Json canonicalPayload{{"schemaVersion", root.at("schemaVersion")}, {"characters", characters}};
    if (sha256(canonicalPayload.dump()) != nextHash)
        throw std::runtime_error("Monster table contentHash does not match its canonical payload");

    std::unordered_map<std::int32_t, ServerMonsterDefinition> nextMonsters;
    nextMonsters.reserve(characters.size());
    for (const Json& value : characters)
    {
        ServerMonsterDefinition monster;
        monster.catalogUnitId             = value.at("unitId").get<std::int32_t>();
        monster.nameKey                   = value.at("nameKey").get<std::string>();
        monster.element                   = parseElement(value.at("element").get<std::string>());
        monster.role                      = parseRole(value.at("role").get<std::string>());
        monster.stats.maxHp               = value.at("hp").get<std::int32_t>();
        monster.stats.attack              = value.at("ad").get<std::int32_t>();
        monster.stats.defense             = value.at("adDefense").get<std::int32_t>();
        monster.stats.attackRange         = value.at("range").get<std::int32_t>();
        monster.stats.attackIntervalTicks = 16;
        monster.stats.moveIntervalTicks   = 5;
        monster.stats.energyOnAttack      = 12;
        monster.stats.energyOnHit         = 4;
        monster.stats.energyOnKill        = 25;

        if (monster.catalogUnitId <= 0 || monster.nameKey.empty() || monster.stats.maxHp <= 0 ||
            monster.stats.attack < 0 || monster.stats.defense < 0 || monster.stats.attackRange <= 0)
        {
            throw std::runtime_error("Monster table contains invalid unit stats");
        }
        if (!nextMonsters.emplace(monster.catalogUnitId, std::move(monster)).second)
            throw std::runtime_error("Monster table contains duplicate unitId");
    }

    tableVersion_ = nextVersion;
    contentHash_  = nextHash;
    monsters_     = std::move(nextMonsters);
}

const ServerMonsterDefinition* ServerMonsterCatalog::find(std::int32_t catalogUnitId) const noexcept
{
    const auto found = monsters_.find(catalogUnitId);
    return found == monsters_.end() ? nullptr : &found->second;
}
}  // namespace cmc::server
