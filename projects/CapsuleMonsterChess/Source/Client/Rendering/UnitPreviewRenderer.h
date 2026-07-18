#pragma once

#include "axmol/axmol.h"

#include <string>

namespace cmc::client
{
struct MonsterCatalogEntry;

[[nodiscard]] ax::Node* createUnitPreview(const MonsterCatalogEntry& entry,
                                          const ax::Size& viewportSize,
                                          std::string& error);
}  // namespace cmc::client
