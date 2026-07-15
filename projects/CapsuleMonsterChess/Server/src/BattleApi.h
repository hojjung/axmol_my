#pragma once

#include "AxmolRuntime.h"
#include "ServerMonsterCatalog.h"

#include <string>
#include <string_view>

namespace cmc::server
{
struct ApiResponse final
{
    unsigned int status = 500;
    std::string body;
};

class BattleApi final
{
public:
    BattleApi(const ServerMonsterCatalog& catalog, AxmolRuntime& runtime) : catalog_(catalog), runtime_(runtime) {}

    [[nodiscard]] ApiResponse health() const;
    [[nodiscard]] ApiResponse simulate(std::string_view requestBody) const;

private:
    const ServerMonsterCatalog& catalog_;
    AxmolRuntime& runtime_;
};
}  // namespace cmc::server
