#include "Process.h"

#include <cerrno>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#    include <Windows.h>
#    include <process.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>

extern char** environ;
#endif

namespace axasset
{
namespace
{

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};

    const int required =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0)
        return {};

    std::wstring result(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                            required) != required)
        return {};

    return result;
}
#endif

}  // namespace

ProcessResult runProcess(const std::vector<std::string>& arguments)
{
    if (arguments.empty() || arguments.front().empty())
        return {127, "process executable is empty"};

#if defined(_WIN32)
    std::vector<std::wstring> wideArguments;
    wideArguments.reserve(arguments.size());
    for (const auto& argument : arguments)
    {
        auto wide = utf8ToWide(argument);
        if (!argument.empty() && wide.empty())
            return {127, "process argument is not valid UTF-8"};
        wideArguments.emplace_back(std::move(wide));
    }

    std::vector<const wchar_t*> argv;
    argv.reserve(wideArguments.size() + 1);
    for (const auto& argument : wideArguments)
        argv.push_back(argument.c_str());
    argv.push_back(nullptr);

    errno               = 0;
    const intptr_t code = _wspawnvp(_P_WAIT, wideArguments.front().c_str(), argv.data());
    if (code == -1)
        return {127, std::string("unable to launch process: ") + std::strerror(errno)};
    return {static_cast<int>(code), {}};
#else
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);

    pid_t pid{};
    const int spawnError = posix_spawnp(&pid, argv.front(), nullptr, nullptr, argv.data(), environ);
    if (spawnError != 0)
        return {127, std::string("unable to launch process: ") + std::strerror(spawnError)};

    int status{};
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
            return {127, std::string("unable to wait for process: ") + std::strerror(errno)};
    }

    if (WIFEXITED(status))
        return {WEXITSTATUS(status), {}};
    if (WIFSIGNALED(status))
        return {128 + WTERMSIG(status), "process terminated by a signal"};
    return {127, "process ended without an exit status"};
#endif
}

}  // namespace axasset
