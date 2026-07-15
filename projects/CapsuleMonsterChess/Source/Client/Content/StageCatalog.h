#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cmc::client
{
struct StageCatalogEntry final
{
    int stageId     = 0;
    int chapterId   = 0;
    int stageNumber = 0;
    int enemyPower  = 0;
    int energyCost  = 0;
    int rewardGold  = 0;
    std::string name;
};

class StageCatalog final
{
public:
    bool load(std::string_view resourcePath, std::string& error);

    [[nodiscard]] const std::vector<StageCatalogEntry>& entries() const noexcept { return _entries; }
    [[nodiscard]] const std::string& chapterName() const noexcept { return _chapterName; }
    [[nodiscard]] const StageCatalogEntry* findById(int stageId) const noexcept;

private:
    std::vector<StageCatalogEntry> _entries;
    std::string _chapterName;
};
}  // namespace cmc::client
