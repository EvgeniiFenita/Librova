#include "RecycleBin/WindowsRecycleBinService.hpp"

#include <stdexcept>
#include <string>

#include "Foundation/UnicodeConversion.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Librova::RecycleBin {
namespace {

[[nodiscard]] std::wstring BuildDoubleNullTerminatedList(const std::vector<std::filesystem::path>& paths)
{
    std::wstring buffer;

    for (const auto& path : paths)
    {
        if (path.empty())
        {
            continue;
        }

        const auto widePath = path.native();
        buffer.append(widePath);
        buffer.push_back(L'\0');
    }

    buffer.push_back(L'\0');
    return buffer;
}

} // namespace

void CWindowsRecycleBinService::MoveToRecycleBin(const std::vector<std::filesystem::path>& paths)
{
    if (paths.empty())
    {
        throw std::invalid_argument("Recycle Bin source path list must not be empty.");
    }

#ifndef _WIN32
    throw std::runtime_error("Windows Recycle Bin is not available on this platform.");
#else
    const auto pathBuffer = BuildDoubleNullTerminatedList(paths);

    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = pathBuffer.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    const int result = ::SHFileOperationW(&operation);
    if (result != 0)
    {
        throw std::runtime_error(
            "SHFileOperationW failed while moving files to the Windows Recycle Bin. FirstPath='"
            + Librova::Unicode::PathToUtf8(paths.front())
            + "'.");
    }

    if (operation.fAnyOperationsAborted)
    {
        throw std::runtime_error("Windows Recycle Bin operation was cancelled or aborted.");
    }
#endif
}

} // namespace Librova::RecycleBin
