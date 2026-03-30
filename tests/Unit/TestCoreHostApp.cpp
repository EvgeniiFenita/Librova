#include <catch2/catch_test_macros.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
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
        / "LibriFlow.Core.Host"
        / "Debug"
        / LIBRIFLOW_CORE_HOST_APP_NAME;
}

std::filesystem::path BuildUniquePipePath()
{
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    return std::filesystem::path{LR"(\\.\pipe\LibriFlow.CoreHost.Process.Test.)" + suffix};
}

std::wstring Quote(const std::filesystem::path& value)
{
    return L"\"" + value.native() + L"\"";
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

} // namespace

TEST_CASE("Core host executable serves import job requests over named pipes", "[core-host][process]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-core-host-process");
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
        + Quote(libraryRoot);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInformation{};

    REQUIRE(CreateProcessW(
        hostAppPath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInformation));

    CScopedProcess process(processInformation);
    WaitForPipeServerReady(pipePath, processInformation);

    LibriFlow::ApplicationClient::CImportJobClient client(pipePath);
    const auto jobId = client.Start({
        .SourcePath = sourcePath,
        .WorkingDirectory = workingDirectory
    }, std::chrono::seconds(5));

    REQUIRE(jobId != 0);

    std::optional<LibriFlow::ApplicationJobs::SImportJobResult> result;
    for (int attempt = 0; attempt < 20 && !result.has_value(); ++attempt)
    {
        result = client.TryGetResult(jobId, std::chrono::seconds(5));
        if (!result.has_value())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == LibriFlow::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);
    REQUIRE(client.Remove(jobId, std::chrono::seconds(5)));
    REQUIRE(std::filesystem::exists(libraryRoot / "Database" / "libriflow.db"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Books" / "0000000001" / "book.fb2"));

    process.Stop();
}
