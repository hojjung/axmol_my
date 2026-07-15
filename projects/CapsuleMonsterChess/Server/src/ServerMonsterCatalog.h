#pragma once

#include "cmc/game_core/BattleTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace cmc::server
{
struct ServerMonsterDefinition final
{
    std::int32_t catalogUnitId = 0;
    std::string nameKey;
    game_core::ElementType element = game_core::ElementType::Fire;
    game_core::RoleType role       = game_core::RoleType::Pawn;
    game_core::UnitStats stats;
};

class ServerMonsterCatalog final
{
public:
    void load(const std::filesystem::path& tablePath);

    [[nodiscard]] const ServerMonsterDefinition* find(std::int32_t catalogUnitId) const noexcept;
    [[nodiscard]] const std::string& tableVersion() const noexcept { return tableVersion_; }
    [[nodiscard]] const std::string& contentHash() const noexcept { return contentHash_; }
    [[nodiscard]] std::size_t size() const noexcept { return monsters_.size(); }

private:
    std::string tableVersion_;
    std::string contentHash_;
    std::unordered_map<std::int32_t, ServerMonsterDefinition> monsters_;
};
}  // namespace cmc::server
