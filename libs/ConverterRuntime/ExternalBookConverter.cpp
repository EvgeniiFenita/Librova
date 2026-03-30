#include "ConverterRuntime/ExternalBookConverter.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Librova::ConverterRuntime {
namespace {

std::wstring PathToWide(const std::filesystem::path& path)
{
    return path.native();
}

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

    if (length <= 0)
    {
        throw std::runtime_error("Failed to convert UTF-8 argument to UTF-16.");
    }

    std::wstring wideValue(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wideValue.data(), length);
    return wideValue;
}

std::wstring QuoteWindowsArgument(const std::wstring& argument)
{
    if (argument.empty())
    {
        return L"\"\"";
    }

    if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        return argument;
    }

    std::wstring quoted{L"\""};
    unsigned backslashCount = 0;

    for (const wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++backslashCount;
            continue;
        }

        if (character == L'"')
        {
            quoted.append(backslashCount * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashCount = 0;
            continue;
        }

        if (backslashCount > 0)
        {
            quoted.append(backslashCount, L'\\');
            backslashCount = 0;
        }

        quoted.push_back(character);
    }

    if (backslashCount > 0)
    {
        quoted.append(backslashCount * 2, L'\\');
    }

    quoted.push_back(L'"');
    return quoted;
}

std::wstring BuildCommandLine(const Librova::ConverterCommand::SResolvedConverterCommand& command)
{
    std::wstring commandLine = QuoteWindowsArgument(PathToWide(command.ExecutablePath));

    for (const std::string& argument : command.Arguments)
    {
        commandLine.push_back(L' ');
        commandLine.append(QuoteWindowsArgument(Utf8ToWide(argument)));
    }

    return commandLine;
}

class CProcessHandle final
{
public:
    explicit CProcessHandle(HANDLE handle = nullptr)
        : m_handle(handle)
    {
    }

    ~CProcessHandle()
    {
        if (m_handle != nullptr)
        {
            CloseHandle(m_handle);
        }
    }

    CProcessHandle(const CProcessHandle&) = delete;
    CProcessHandle& operator=(const CProcessHandle&) = delete;

    CProcessHandle(CProcessHandle&& other) noexcept
        : m_handle(std::exchange(other.m_handle, nullptr))
    {
    }

    CProcessHandle& operator=(CProcessHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (m_handle != nullptr)
            {
                CloseHandle(m_handle);
            }

            m_handle = std::exchange(other.m_handle, nullptr);
        }

        return *this;
    }

    [[nodiscard]] HANDLE Get() const noexcept
    {
        return m_handle;
    }

private:
    HANDLE m_handle = nullptr;
};

class CProcessInfo final
{
public:
    explicit CProcessInfo(PROCESS_INFORMATION info)
        : m_process(info.hProcess)
        , m_thread(info.hThread)
    {
    }

    [[nodiscard]] HANDLE GetProcessHandle() const noexcept
    {
        return m_process.Get();
    }

private:
    CProcessHandle m_process;
    CProcessHandle m_thread;
};

std::unordered_set<std::filesystem::path> SnapshotDirectoryFiles(const std::filesystem::path& directoryPath)
{
    std::unordered_set<std::filesystem::path> snapshot;

    if (!std::filesystem::exists(directoryPath))
    {
        return snapshot;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directoryPath))
    {
        if (entry.is_regular_file())
        {
            snapshot.insert(entry.path().lexically_normal());
        }
    }

    return snapshot;
}

std::optional<std::filesystem::path> ResolveSingleProducedFile(
    const std::filesystem::path& directoryPath,
    const std::unordered_set<std::filesystem::path>& beforeSnapshot,
    const std::filesystem::path& expectedOutputPath)
{
    std::vector<std::filesystem::path> newFiles;

    if (std::filesystem::exists(directoryPath))
    {
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path currentPath = entry.path().lexically_normal();

            if (!beforeSnapshot.contains(currentPath))
            {
                newFiles.push_back(currentPath);
            }
        }
    }

    if (newFiles.size() == 1)
    {
        return newFiles.front();
    }

    for (const std::filesystem::path& filePath : newFiles)
    {
        if (filePath.extension() == expectedOutputPath.extension())
        {
            return filePath;
        }
    }

    const std::filesystem::path exactPath = expectedOutputPath.lexically_normal();

    if (std::filesystem::exists(exactPath))
    {
        return exactPath;
    }

    return std::nullopt;
}

void MoveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
{
    std::error_code errorCode;
    std::filesystem::rename(sourcePath, destinationPath, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to move converter output from "} + sourcePath.string() + " to " + destinationPath.string());
    }
}

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(std::string{"Failed to create converter directory: "} + path.string());
    }
}

DWORD WaitForProcessWithCancellation(
    const HANDLE processHandle,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken,
    const std::chrono::milliseconds pollInterval)
{
    while (true)
    {
        const DWORD waitResult = WaitForSingleObject(processHandle, static_cast<DWORD>(pollInterval.count()));

        if (waitResult == WAIT_OBJECT_0)
        {
            DWORD exitCode = 0;

            if (!GetExitCodeProcess(processHandle, &exitCode))
            {
                throw std::runtime_error("Failed to read converter process exit code.");
            }

            return exitCode;
        }

        if (waitResult != WAIT_TIMEOUT)
        {
            throw std::runtime_error("Failed while waiting for converter process.");
        }

        if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            TerminateProcess(processHandle, 1);
            WaitForSingleObject(processHandle, INFINITE);
            return 1;
        }
    }
}

Librova::Domain::SConversionResult BuildCancelledResult()
{
    return {
        .Status = Librova::Domain::EConversionStatus::Cancelled,
        .Warnings = {"Conversion cancelled."}
    };
}

Librova::Domain::SConversionResult BuildFailedResult(const std::string& warning)
{
    return {
        .Status = Librova::Domain::EConversionStatus::Failed,
        .Warnings = {warning}
    };
}

} // namespace

CExternalBookConverter::CExternalBookConverter(SExternalConverterSettings settings)
    : m_settings(std::move(settings))
{
    if (!m_settings.IsValid())
    {
        throw std::invalid_argument("External converter settings must contain a valid command profile and poll interval.");
    }
}

bool CExternalBookConverter::CanConvert(
    const Librova::Domain::EBookFormat sourceFormat,
    const Librova::Domain::EBookFormat destinationFormat) const
{
    return sourceFormat == Librova::Domain::EBookFormat::Fb2
        && destinationFormat == Librova::Domain::EBookFormat::Epub;
}

Librova::Domain::SConversionResult CExternalBookConverter::Convert(
    const Librova::Domain::SConversionRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!CanConvert(request.SourceFormat, request.DestinationFormat))
    {
        return BuildFailedResult("Requested conversion direction is not supported.");
    }

    const Librova::ConverterCommand::SResolvedConverterCommand command =
        Librova::ConverterCommand::CConverterCommandBuilder::Build(m_settings.CommandProfile, request);

    EnsureDirectory(command.ExpectedOutputDirectory);
    std::filesystem::remove(command.ExpectedOutputPath);
    const std::unordered_set<std::filesystem::path> outputSnapshot = SnapshotDirectoryFiles(command.ExpectedOutputDirectory);

    std::wstring commandLine = BuildCommandLine(command);
    std::wstring workingDirectory = PathToWide(command.ExpectedOutputDirectory);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInformation{};

    progressSink.ReportValue(0, "Starting converter process");

    if (!CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.c_str(),
            &startupInfo,
            &processInformation))
    {
        throw std::runtime_error("Failed to launch external converter process.");
    }

    CProcessInfo process(processInformation);
    const DWORD exitCode = WaitForProcessWithCancellation(process.GetProcessHandle(), progressSink, stopToken, m_settings.PollInterval);

    if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
    {
        return BuildCancelledResult();
    }

    if (exitCode != 0)
    {
        return BuildFailedResult("Converter process exited with a non-zero code.");
    }

    std::filesystem::path resolvedOutputPath = command.ExpectedOutputPath;

    if (command.OutputMode == Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory)
    {
        const std::optional<std::filesystem::path> producedFile =
            ResolveSingleProducedFile(command.ExpectedOutputDirectory, outputSnapshot, command.ExpectedOutputPath);

        if (!producedFile.has_value())
        {
            return BuildFailedResult("Converter process completed, but no output file was discovered.");
        }

        if (producedFile->lexically_normal() != command.ExpectedOutputPath.lexically_normal())
        {
            std::filesystem::remove(command.ExpectedOutputPath);
            MoveFile(*producedFile, command.ExpectedOutputPath);
        }

        resolvedOutputPath = command.ExpectedOutputPath;
    }
    else if (!std::filesystem::exists(command.ExpectedOutputPath))
    {
        return BuildFailedResult("Converter process completed, but expected output file is missing.");
    }

    progressSink.ReportValue(100, "Converter process completed");

    return {
        .Status = Librova::Domain::EConversionStatus::Succeeded,
        .OutputPath = resolvedOutputPath
    };
}

} // namespace Librova::ConverterRuntime
