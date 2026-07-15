#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cmc::client
{
struct MonsterCatalogEntry final
{
    int unitId    = 0;
    int hp        = 0;
    int ad        = 0;
    int ap        = 0;
    int adDefense = 0;
    int apDefense = 0;
    int range     = 0;
    std::string nameKey;
    std::string modelId;
    std::string icon;
    std::string element;
    std::string role;
    std::string evolutionKey;
    std::vector<std::string> skills;
};

class MonsterCatalog final
{
public:
    bool load(std::string_view resourcePath, std::string& error);

    [[nodiscard]] const std::vector<MonsterCatalogEntry>& entries() const noexcept { return _entries; }
    [[nodiscard]] const MonsterCatalogEntry* findById(int unitId) const noexcept;
    [[nodiscard]] size_t iconCount() const noexcept { return _iconCount; }

private:
    std::vector<MonsterCatalogEntry> _entries;
    size_t _iconCount = 0;
};
}  // namespace cmc::client
