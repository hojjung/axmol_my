#pragma once

#include "Client/UserData/UserProfile.h"

#include <string>

namespace cmc::client
{
class IUserDataSource
{
public:
    virtual ~IUserDataSource() = default;

    virtual bool load(UserProfile& profile, std::string& error) const = 0;
    virtual bool save(const UserProfile& profile, std::string& error) = 0;
    [[nodiscard]] virtual bool isWritable() const noexcept            = 0;
};

class LocalJsonUserDataSource final : public IUserDataSource
{
public:
    explicit LocalJsonUserDataSource(std::string saveFileName       = "capsule_monster_chess_user.json",
                                     std::string defaultProfilePath = "Data/Local/default_user_profile.json");

    bool load(UserProfile& profile, std::string& error) const override;
    bool save(const UserProfile& profile, std::string& error) override;
    [[nodiscard]] bool isWritable() const noexcept override { return true; }
    [[nodiscard]] std::string writableFilePath() const;

private:
    std::string _saveFileName;
    std::string _defaultProfilePath;
};

class WebPayloadUserDataSource final : public IUserDataSource
{
public:
    explicit WebPayloadUserDataSource(std::string jsonPayload = {});

    void setPayload(std::string jsonPayload);
    bool load(UserProfile& profile, std::string& error) const override;
    bool save(const UserProfile& profile, std::string& error) override;
    [[nodiscard]] bool isWritable() const noexcept override { return false; }

private:
    std::string _jsonPayload;
};
}  // namespace cmc::client
