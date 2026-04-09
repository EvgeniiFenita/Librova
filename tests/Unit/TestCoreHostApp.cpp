#include <catch2/catch_test_macros.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

#include "ApplicationClient/ImportJobClient.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }

    ~CScopedDirectory()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::filesystem::path CreateFb2Fixture(const std::filesystem::path& outputPath)
{
    WriteTextFile(
        outputPath,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <book-title>Пикник на обочине</book-title>
      <author>
        <first-name>Аркадий</first-name>
        <last-name>Стругацкий</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
  </description>
  <body>
    <section><p>Body</p></section>
  </body>
</FictionBook>)");

    return outputPath;
}

std::filesystem::path GetHostAppPath()
{
    std::wstring modulePath(MAX_PATH, L'\0');
    const auto length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    modulePath.resize(length);

    const std::filesystem::path testExePath{modulePath};
    return testExePath.parent_path().parent_path().parent_path().parent_path()
        / "apps"
        / "Librova.Core.Host"
        / LIBROVA_TEST_CONFIGURATION
        / LIBROVA_CORE_HOST_APP_NAME;
}

std::filesystem::path BuildUniquePipePath()
{
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    return std::filesystem::path{LR"(\\.\pipe\Librova.CoreHost.Process.Test.)" + suffix};
}

std::filesystem::path BuildUniqueSandboxPath(const std::wstring_view prefix)
{
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    return std::filesystem::temp_directory_path() / std::filesystem::path{std::wstring(prefix) + L"." + suffix};
}

std::wstring Quote(const std::filesystem::path& value)
{
    return L"\"" + value.native() + L"\"";
}

std::filesystem::path GetCmdExePath()
{
    if (const char* comSpec = std::getenv("ComSpec"); comSpec != nullptr && comSpec[0] != '\0')
    {
        return std::filesystem::path{comSpec};
    }

    return std::filesystem::path{R"(C:\Windows\System32\cmd.exe)"};
}

std::wstring BuildUniqueShutdownEventName()
{
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    return L"Local\\Librova.CoreHost.Shutdown.Test." + suffix;
}

void WaitForPipeServerReady(const std::filesystem::path& pipePath, const PROCESS_INFORMATION& processInformation)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (WaitNamedPipeW(pipePath.native().c_str(), 100) != FALSE)
        {
            return;
        }

        const auto processState = WaitForSingleObject(processInformation.hProcess, 0);
        REQUIRE(processState != WAIT_OBJECT_0);
    }

    FAIL("Timed out waiting for core host named pipe server to become ready.");
}

class CScopedProcess final
{
public:
    explicit CScopedProcess(PROCESS_INFORMATION processInformation)
        : m_processInformation(processInformation)
    {
    }

    ~CScopedProcess()
    {
        if (m_processInformation.hProcess != nullptr)
        {
            EnsureStopped();
            CloseHandle(m_processInformation.hProcess);
        }

        if (m_processInformation.hThread != nullptr)
        {
            CloseHandle(m_processInformation.hThread);
        }
    }

    [[nodiscard]] PROCESS_INFORMATION Get() const noexcept
    {
        return m_processInformation;
    }

    void Stop(const UINT exitCode = 0)
    {
        if (m_processInformation.hProcess == nullptr)
        {
            return;
        }

        const auto waitResult = WaitForSingleObject(m_processInformation.hProcess, 0);
        if (waitResult == WAIT_TIMEOUT)
        {
            REQUIRE(TerminateProcess(m_processInformation.hProcess, exitCode));
            REQUIRE(WaitForSingleObject(m_processInformation.hProcess, 5000) == WAIT_OBJECT_0);
        }
    }

    [[nodiscard]] bool WaitForExit(const std::chrono::milliseconds timeout) const
    {
        if (m_processInformation.hProcess == nullptr)
        {
            return true;
        }

        return WaitForSingleObject(m_processInformation.hProcess, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
    }

    [[nodiscard]] std::optional<DWORD> TryGetExitCode() const
    {
        if (m_processInformation.hProcess == nullptr)
        {
            return std::nullopt;
        }

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(m_processInformation.hProcess, &exitCode))
        {
            return std::nullopt;
        }

        return exitCode;
    }

private:
    void EnsureStopped()
    {
        if (m_processInformation.hProcess == nullptr)
        {
            return;
        }

        const auto waitResult = WaitForSingleObject(m_processInformation.hProcess, 5000);
        if (waitResult == WAIT_TIMEOUT)
        {
            TerminateProcess(m_processInformation.hProcess, 1);
            WaitForSingleObject(m_processInformation.hProcess, 5000);
        }
    }

    PROCESS_INFORMATION m_processInformation{};
};

PROCESS_INFORMATION StartProcess(const std::filesystem::path& executablePath, std::wstring commandLine, const DWORD creationFlags = 0)
{
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInformation{};

    REQUIRE(CreateProcessW(
        executablePath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        creationFlags,
        nullptr,
        nullptr,
        &startupInfo,
        &processInformation));

    return processInformation;
}

} // namespace

TEST_CASE("Core host executable serves import job requests over named pipes", "[core-host][process]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-core-host-process"));
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto workingDirectory = sandbox.GetPath() / "Work";
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "book.fb2");
    const auto pipePath = BuildUniquePipePath();
    const auto hostAppPath = GetHostAppPath();

    REQUIRE(std::filesystem::exists(hostAppPath));

    std::wstring commandLine = Quote(hostAppPath)
        + L" --pipe "
        + Quote(pipePath)
        + L" --library-root "
        + Quote(libraryRoot)
        + L" --library-mode create";

    PROCESS_INFORMATION processInformation = StartProcess(hostAppPath, std::move(commandLine));
    CScopedProcess process(processInformation);
    WaitForPipeServerReady(pipePath, processInformation);

    Librova::ApplicationClient::CImportJobClient client(pipePath);
    const auto jobId = client.Start({
        .SourcePaths = {sourcePath},
        .WorkingDirectory = workingDirectory
    }, std::chrono::seconds(5));

    REQUIRE(jobId != 0);

    std::optional<Librova::ApplicationJobs::SImportJobResult> result;
    for (int attempt = 0; attempt < 20 && !result.has_value(); ++attempt)
    {
        result = client.TryGetResult(jobId, std::chrono::seconds(5));
        if (!result.has_value())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == Librova::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);
    REQUIRE(client.Remove(jobId, std::chrono::seconds(5)));
    REQUIRE(std::filesystem::exists(libraryRoot / "Database" / "librova.db"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Books" / "0000000001" / "book.fb2.gz"));

    process.Stop();
}

TEST_CASE("Core host exits gracefully when the watched parent process terminates", "[core-host][process]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-core-host-parent-watchdog"));
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto pipePath = BuildUniquePipePath();
    const auto hostAppPath = GetHostAppPath();
    const auto cmdExePath = GetCmdExePath();

    REQUIRE(std::filesystem::exists(hostAppPath));
    REQUIRE(std::filesystem::exists(cmdExePath));

    std::wstring parentCommandLine = Quote(cmdExePath) + L" /C ping -n 2 127.0.0.1 >nul";

    PROCESS_INFORMATION parentProcessInformation = StartProcess(cmdExePath, std::move(parentCommandLine), CREATE_NO_WINDOW);
    CScopedProcess parentProcess(parentProcessInformation);

    std::wstring hostCommandLine = Quote(hostAppPath)
        + L" --pipe "
        + Quote(pipePath)
        + L" --library-root "
        + Quote(libraryRoot)
        + L" --library-mode create"
        + L" --parent-pid "
        + std::to_wstring(parentProcessInformation.dwProcessId);

    PROCESS_INFORMATION hostProcessInformation = StartProcess(hostAppPath, std::move(hostCommandLine));
    CScopedProcess hostProcess(hostProcessInformation);
    WaitForPipeServerReady(pipePath, hostProcessInformation);

    REQUIRE(parentProcess.WaitForExit(std::chrono::seconds(10)));
    REQUIRE(hostProcess.WaitForExit(std::chrono::seconds(10)));
    REQUIRE(hostProcess.TryGetExitCode() == std::optional<DWORD>{0});

    const auto logFilePath = libraryRoot / "Logs" / "host.log";
    REQUIRE(std::filesystem::exists(logFilePath));

    const std::string logText = ReadTextFile(logFilePath);
    REQUIRE(logText.find("Parent process") != std::string::npos);
    REQUIRE(logText.find("Librova.Core.Host stopped after serving 0 sessions.") != std::string::npos);
}

TEST_CASE("Core host exits gracefully when the shutdown event is signaled", "[core-host][process]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-core-host-shutdown-event"));
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto pipePath = BuildUniquePipePath();
    const auto hostAppPath = GetHostAppPath();
    const auto shutdownEventName = BuildUniqueShutdownEventName();

    REQUIRE(std::filesystem::exists(hostAppPath));

    HANDLE shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, shutdownEventName.c_str());
    REQUIRE(shutdownEvent != nullptr);

    std::wstring commandLine = Quote(hostAppPath)
        + L" --pipe "
        + Quote(pipePath)
        + L" --library-root "
        + Quote(libraryRoot)
        + L" --shutdown-event \""
        + shutdownEventName
        + L"\" --library-mode create";

    PROCESS_INFORMATION hostProcessInformation = StartProcess(hostAppPath, std::move(commandLine));
    CScopedProcess hostProcess(hostProcessInformation);
    WaitForPipeServerReady(pipePath, hostProcessInformation);

    REQUIRE(SetEvent(shutdownEvent) != FALSE);
    REQUIRE(hostProcess.WaitForExit(std::chrono::seconds(10)));
    REQUIRE(hostProcess.TryGetExitCode() == std::optional<DWORD>{0});

    CloseHandle(shutdownEvent);

    const auto logFilePath = libraryRoot / "Logs" / "host.log";
    REQUIRE(std::filesystem::exists(logFilePath));

    const std::string logText = ReadTextFile(logFilePath);
    REQUIRE(logText.find("Shutdown requested by UI event") != std::string::npos);
    REQUIRE(logText.find("Librova.Core.Host stopped after serving 0 sessions.") != std::string::npos);
}

TEST_CASE("Core host rejects opening a missing managed library root", "[core-host][process]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-core-host-open-missing"));
    const auto libraryRoot = sandbox.GetPath() / "MissingLibrary";
    const auto pipePath = BuildUniquePipePath();
    const auto hostAppPath = GetHostAppPath();

    REQUIRE(std::filesystem::exists(hostAppPath));

    std::wstring commandLine = Quote(hostAppPath)
        + L" --pipe "
        + Quote(pipePath)
        + L" --library-root "
        + Quote(libraryRoot)
        + L" --library-mode open";

    CScopedProcess process(StartProcess(hostAppPath, std::move(commandLine)));
    REQUIRE(process.WaitForExit(std::chrono::seconds(5)));
    REQUIRE(process.TryGetExitCode() == std::optional<DWORD>{1});
    REQUIRE_FALSE(std::filesystem::exists(libraryRoot));
}

TEST_CASE("Core host process supports help and version flags without runtime arguments", "[core-host][process]")
{
    const auto hostAppPath = GetHostAppPath();
    REQUIRE(std::filesystem::exists(hostAppPath));

    {
        std::wstring helpCommandLine = Quote(hostAppPath) + L" --help";
        CScopedProcess helpProcess(StartProcess(hostAppPath, std::move(helpCommandLine)));
        REQUIRE(helpProcess.WaitForExit(std::chrono::seconds(5)));
        REQUIRE(helpProcess.TryGetExitCode() == std::optional<DWORD>{0});
    }

    {
        std::wstring versionCommandLine = Quote(hostAppPath) + L" --version";
        CScopedProcess versionProcess(StartProcess(hostAppPath, std::move(versionCommandLine)));
        REQUIRE(versionProcess.WaitForExit(std::chrono::seconds(5)));
        REQUIRE(versionProcess.TryGetExitCode() == std::optional<DWORD>{0});
    }
}
