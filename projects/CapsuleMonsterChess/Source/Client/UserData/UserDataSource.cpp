#include "Client/UserData/UserDataSource.h"

#include "Client/UserData/UserProfileJsonCodec.h"
#include "axmol/platform/FileUtils.h"

#if defined(__EMSCRIPTEN__)
#    include <emscripten/em_asm.h>
#endif

#include <string_view>
#include <utility>

namespace
{
bool decodeFile(std::string_view path, cmc::client::UserProfile& profile, std::string& error)
{
    const auto* fileUtils  = ax::FileUtils::getInstance();
    const std::string json = fileUtils->getStringFromFile(path);
    if (json.empty())
    {
        error = "User profile file is empty: " + std::string(path);
        return false;
    }
    return cmc::client::UserProfileJsonCodec::decode(json, profile, error);
}

#if defined(__EMSCRIPTEN__)
void syncWebFileSystem()
{
    MAIN_THREAD_EM_ASM({
        Module['cmcUserDataSyncPending'] = true;
        if (Module['cmcUserDataSyncActive'])
            return;

        function flushUserData()
        {
            Module['cmcUserDataSyncActive']  = true;
            Module['cmcUserDataSyncPending'] = false;
            FS.syncfs(false, function(error) {
                Module['cmcUserDataSyncActive'] = false;
                if (error)
                    console.error('Failed to persist Capsule Monster Chess user data', error);
                if (Module['cmcUserDataSyncPending'])
                    flushUserData();
            });
        }
        flushUserData();
    });
}
#endif
}  // namespace

namespace cmc::client
{
LocalJsonUserDataSource::LocalJsonUserDataSource(std::string saveFileName, std::string defaultProfilePath)
    : _saveFileName(std::move(saveFileName)), _defaultProfilePath(std::move(defaultProfilePath))
{}

bool LocalJsonUserDataSource::load(UserProfile& profile, std::string& error) const
{
    error.clear();
    if (_saveFileName.empty() || _defaultProfilePath.empty())
    {
        error = "Local user profile paths cannot be empty";
        return false;
    }

    const auto* fileUtils      = ax::FileUtils::getInstance();
    const std::string savePath = writableFilePath();
    if (fileUtils->isFileExist(savePath))
        return decodeFile(savePath, profile, error);

    if (!fileUtils->isFileExist(_defaultProfilePath))
    {
        error = "Default user profile not found: " + _defaultProfilePath;
        return false;
    }
    return decodeFile(_defaultProfilePath, profile, error);
}

bool LocalJsonUserDataSource::save(const UserProfile& profile, std::string& error)
{
    if (_saveFileName.empty())
    {
        error = "Local user profile save file name cannot be empty";
        return false;
    }

    std::string json;
    if (!UserProfileJsonCodec::encode(profile, json, error))
        return false;

    const std::string savePath = writableFilePath();
    if (!ax::FileUtils::getInstance()->writeStringToFile(json, savePath))
    {
        error = "Failed to write user profile: " + savePath;
        return false;
    }

#if defined(__EMSCRIPTEN__)
    syncWebFileSystem();
#endif
    error.clear();
    return true;
}

std::string LocalJsonUserDataSource::writableFilePath() const
{
    return ax::FileUtils::getInstance()->getWritablePath() + _saveFileName;
}

WebPayloadUserDataSource::WebPayloadUserDataSource(std::string jsonPayload) : _jsonPayload(std::move(jsonPayload)) {}

void WebPayloadUserDataSource::setPayload(std::string jsonPayload)
{
    _jsonPayload = std::move(jsonPayload);
}

bool WebPayloadUserDataSource::load(UserProfile& profile, std::string& error) const
{
    if (_jsonPayload.empty())
    {
        error = "Web user profile payload is empty";
        return false;
    }
    return UserProfileJsonCodec::decode(_jsonPayload, profile, error);
}

bool WebPayloadUserDataSource::save(const UserProfile&, std::string& error)
{
    error = "Web user profile payload is read-only";
    return false;
}
}  // namespace cmc::client
