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

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ConverterRuntime {
namespace {

constexpr auto GShutdownWaitTimeout = std::chrono::milliseconds{2000};

std::wstring PathToWide(const std::filesystem::path& path)
{
    return path.native();
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
        commandLine.append(QuoteWindowsArgument(Librova::Unicode::Utf8ToWide(argument)));
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

class CJobHandle final
{
public:
    explicit CJobHandle(HANDLE handle = nullptr)
        : m_handle(handle)
    {
    }

    ~CJobHandle()
    {
        if (m_handle != nullptr)
        {
            CloseHandle(m_handle);
        }
    }

    CJobHandle(const CJobHandle&) = delete;
    CJobHandle& operator=(const CJobHandle&) = delete;

    CJobHandle(CJobHandle&& other) noexcept
        : m_handle(std::exchange(other.m_handle, nullptr))
    {
    }

    CJobHandle& operator=(CJobHandle&& other) noexcept
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

CJobHandle CreateKillOnCloseJob()
{
    HANDLE rawJobHandle = CreateJobObjectW(nullptr, nullptr);
    if (rawJobHandle == nullptr)
    {
        throw std::runtime_error("Failed to create converter job object.");
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInformation{};
    limitInformation.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(
            rawJobHandle,
            JobObjectExtendedLimitInformation,
            &limitInformation,
            sizeof(limitInformation)))
    {
        CloseHandle(rawJobHandle);
        throw std::runtime_error("Failed to configure converter job object.");
    }

    return CJobHandle(rawJobHandle);
}

void AssignProcessToKillOnCloseJob(const HANDLE jobHandle, const HANDLE processHandle)
{
    if (!AssignProcessToJobObject(jobHandle, processHandle))
    {
        throw std::runtime_error("Failed to assign converter process to job object.");
    }
}

void LogShutdownWarning(
    const std::string_view stage,
    const std::string_view detail,
    const DWORD code) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    Librova::Logging::Warn(
        "External converter shutdown issue. Stage={} Detail={} Win32Code={}",
        stage,
        detail,
        code);
}

[[nodiscard]] bool WaitForProcessExit(const HANDLE processHandle, const std::string_view stage) noexcept
{
    const DWORD waitResult = WaitForSingleObject(
        processHandle,
        static_cast<DWORD>(GShutdownWaitTimeout.count()));

    if (waitResult == WAIT_OBJECT_0)
    {
        return true;
    }

    if (waitResult == WAIT_TIMEOUT)
    {
        LogShutdownWarning(stage, "timed out while waiting for process exit", WAIT_TIMEOUT);
        return false;
    }

    LogShutdownWarning(stage, "failed while waiting for process exit", waitResult);
    return false;
}

void TerminateProcessNoThrow(
    const HANDLE processHandle,
    const HANDLE jobHandle,
    const std::string_view stage) noexcept
{
    if (processHandle == nullptr)
    {
        return;
    }

    if (WaitForSingleObject(processHandle, 0) == WAIT_OBJECT_0)
    {
        return;
    }

    if (!TerminateProcess(processHandle, 1))
    {
        LogShutdownWarning(stage, "TerminateProcess failed", GetLastError());
    }

    if (WaitForProcessExit(processHandle, stage))
    {
        return;
    }

    if (jobHandle == nullptr)
    {
        return;
    }

    if (!TerminateJobObject(jobHandle, 1))
    {
        LogShutdownWarning(stage, "TerminateJobObject failed", GetLastError());
        return;
    }

    const bool exitedAfterJobTermination = WaitForProcessExit(processHandle, stage);
    static_cast<void>(exitedAfterJobTermination);
}

struct SWaitOutcome
{
    DWORD ExitCode = 0;
    bool WasCancelled = false;
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
            std::string{"Failed to move converter output from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(destinationPath));
    }
}

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to create converter directory: "} + Librova::Unicode::PathToUtf8(path));
    }
}

void CleanupProducedFiles(
    const std::filesystem::path& directoryPath,
    const std::unordered_set<std::filesystem::path>& beforeSnapshot,
    const std::filesystem::path& expectedOutputPath) noexcept
{
    std::error_code errorCode;
    std::filesystem::remove(expectedOutputPath, errorCode);

    if (!std::filesystem::exists(directoryPath))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directoryPath, errorCode))
    {
        if (errorCode)
        {
            return;
        }

        if (!entry.is_regular_file(errorCode) || errorCode)
        {
            errorCode.clear();
            continue;
        }

        const std::filesystem::path currentPath = entry.path().lexically_normal();
        if (beforeSnapshot.contains(currentPath))
        {
            continue;
        }

        std::filesystem::remove(currentPath, errorCode);
        errorCode.clear();
    }
}

SWaitOutcome WaitForProcessWithCancellation(
    const HANDLE jobHandle,
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

            return {
                .ExitCode = exitCode
            };
        }

        if (waitResult != WAIT_TIMEOUT)
        {
            throw std::runtime_error("Failed while waiting for converter process.");
        }

        if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            TerminateProcessNoThrow(processHandle, jobHandle, "cancellation");
            return {
                .ExitCode = 1,
                .WasCancelled = true
            };
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

    const std::filesystem::path processWorkingDirectory =
        m_settings.WorkingDirectory.empty()
            ? command.ExpectedOutputDirectory
            : m_settings.WorkingDirectory;
    EnsureDirectory(processWorkingDirectory);

    std::wstring commandLine = BuildCommandLine(command);
    std::wstring workingDirectory = PathToWide(processWorkingDirectory);

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
            CREATE_NO_WINDOW | CREATE_SUSPENDED,
            nullptr,
            workingDirectory.c_str(),
            &startupInfo,
            &processInformation))
    {
        throw std::runtime_error("Failed to launch external converter process.");
    }

    DWORD exitCode = 0;
    bool wasCancelled = false;

    {
        const CJobHandle jobHandle = CreateKillOnCloseJob();
        CProcessInfo process(processInformation);

        try
        {
            AssignProcessToKillOnCloseJob(jobHandle.Get(), process.GetProcessHandle());
        }
        catch (...)
        {
            TerminateProcessNoThrow(process.GetProcessHandle(), jobHandle.Get(), "job-assignment-failure");
            throw;
        }

        if (ResumeThread(processInformation.hThread) == static_cast<DWORD>(-1))
        {
            TerminateProcessNoThrow(process.GetProcessHandle(), jobHandle.Get(), "resume-failure");
            throw std::runtime_error("Failed to resume external converter process.");
        }

        const SWaitOutcome waitOutcome = WaitForProcessWithCancellation(
            jobHandle.Get(),
            process.GetProcessHandle(),
            progressSink,
            stopToken,
            m_settings.PollInterval);
        exitCode = waitOutcome.ExitCode;
        wasCancelled = waitOutcome.WasCancelled;
    }

    if (wasCancelled)
    {
        CleanupProducedFiles(command.ExpectedOutputDirectory, outputSnapshot, command.ExpectedOutputPath);
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
