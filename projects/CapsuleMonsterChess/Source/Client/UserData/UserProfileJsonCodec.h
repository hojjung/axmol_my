#pragma once

#include "Client/UserData/UserProfile.h"

#include <string>
#include <string_view>

namespace cmc::client
{
class UserProfileJsonCodec final
{
public:
    static bool decode(std::string_view json, UserProfile& profile, std::string& error);
    static bool encode(const UserProfile& profile, std::string& json, std::string& error);
    static bool validate(const UserProfile& profile, std::string& error);
};
}  // namespace cmc::client
