#pragma once

#include <string>
#include <vector>

namespace axasset
{

struct ProcessResult
{
    int exitCode{};
    std::string error;
};

// Executes arguments directly. No command shell is involved, so spaces and
// shell metacharacters in asset paths remain literal argument data.
[[nodiscard]] ProcessResult runProcess(const std::vector<std::string>& arguments);

}  // namespace axasset
