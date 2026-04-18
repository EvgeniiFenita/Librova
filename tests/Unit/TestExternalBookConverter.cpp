#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <stdexcept>

#include "ConverterCommand/ConverterCommandBuilder.hpp"
#include "ConverterRuntime/ExternalBookConverter.hpp"

namespace {

constexpr auto GPwshPath = "C:/Program Files/PowerShell/7/pwsh.exe";

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

class CTestProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(const int percent, std::string_view message) override
    {
        LastPercent = percent;
        LastMessage.assign(message);
    }

    bool IsCancellationRequested() const override
    {
        return CancellationRequested;
    }

    int LastPercent = 0;
    std::string LastMessage;
    bool CancellationRequested = false;
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

template <typename TPredicate>
void WaitForCondition(
    TPredicate&& predicate,
    const std::chrono::steady_clock::duration timeout,
    const char* failureMessage)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    FAIL(failureMessage);
}

Librova::ConverterCommand::SConverterCommandProfile CreatePwshProfile(
    const std::filesystem::path& scriptPath,
    const Librova::ConverterCommand::EConverterOutputMode outputMode,
    const std::vector<std::string>& trailingArguments)
{
    Librova::ConverterCommand::SConverterCommandProfile profile{
        .ExecutablePath = GPwshPath,
        .ArgumentTemplate = {"-File", scriptPath.generic_string()},
        .OutputMode = outputMode
    };

    profile.ArgumentTemplate.insert(profile.ArgumentTemplate.end(), trailingArguments.begin(), trailingArguments.end());
    return profile;
}

} // namespace

TEST_CASE("External book converter supports exact destination path converters", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-exact");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "copy-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destination = $args[1]\n"
        "New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($destination)) | Out-Null\n"
        "Copy-Item -LiteralPath $source -Destination $destination -Force\n");
    WriteTextFile(sourcePath, "converted-content");

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"})
    });
    CTestProgressSink progressSink;

    const Librova::Domain::SConversionResult result = converter.Convert({
        .SourcePath = sourcePath,
        .DestinationPath = destinationPath,
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.Status == Librova::Domain::EConversionStatus::Succeeded);
    REQUIRE(result.OutputPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE(ReadTextFile(destinationPath) == "converted-content");
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("External book converter relocates single-file output directory results", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-directory");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "dir-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "managed.epub";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destinationDir = $args[1]\n"
        "New-Item -ItemType Directory -Force $destinationDir | Out-Null\n"
        "$actualPath = Join-Path $destinationDir 'generated-by-converter.epub'\n"
        "Copy-Item -LiteralPath $source -Destination $actualPath -Force\n");
    WriteTextFile(sourcePath, "directory-output");

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory,
            {"{source}", "{destination_dir}"})
    });
    CTestProgressSink progressSink;

    const Librova::Domain::SConversionResult result = converter.Convert({
        .SourcePath = sourcePath,
        .DestinationPath = destinationPath,
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.Status == Librova::Domain::EConversionStatus::Succeeded);
    REQUIRE(result.OutputPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE_FALSE(std::filesystem::exists(destinationPath.parent_path() / "generated-by-converter.epub"));
    REQUIRE(ReadTextFile(destinationPath) == "directory-output");
}

TEST_CASE("External book converter reports cancellation and stops the process", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-cancel");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "slow-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";
    const std::filesystem::path startedMarkerPath = sandbox.GetPath() / "converter-started.txt";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destination = $args[1]\n"
        "Set-Content -LiteralPath '"
            + startedMarkerPath.generic_string()
            + "' -Value 'started'\n"
        "Start-Sleep -Seconds 5\n"
        "New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($destination)) | Out-Null\n"
        "Copy-Item -LiteralPath $source -Destination $destination -Force\n");
    WriteTextFile(sourcePath, "cancel-me");

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"}),
        .PollInterval = std::chrono::milliseconds{50}
    });
    CTestProgressSink progressSink;
    std::stop_source stopSource;
    const auto startTime = std::chrono::steady_clock::now();

    Librova::Domain::SConversionResult result;
    std::jthread worker([&] {
        result = converter.Convert({
            .SourcePath = sourcePath,
            .DestinationPath = destinationPath,
            .SourceFormat = Librova::Domain::EBookFormat::Fb2,
            .DestinationFormat = Librova::Domain::EBookFormat::Epub
        }, progressSink, stopSource.get_token());
    });

    WaitForCondition(
        [&startedMarkerPath] { return std::filesystem::exists(startedMarkerPath); },
        std::chrono::seconds{2},
        "Timed out waiting for converter process startup.");
    stopSource.request_stop();
    worker.join();
    const auto elapsed = std::chrono::steady_clock::now() - startTime;

    REQUIRE_FALSE(result.IsSuccess());
    REQUIRE(result.IsCancelled());
    REQUIRE(result.Status == Librova::Domain::EConversionStatus::Cancelled);
    REQUIRE(result.Warnings == std::vector<std::string>({"Conversion cancelled."}));
    REQUIRE_FALSE(std::filesystem::exists(destinationPath));
    REQUIRE(elapsed < std::chrono::seconds{3});
}

TEST_CASE("External book converter removes partial output files after cancellation", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-cancel-cleanup");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "partial-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destination = $args[1]\n"
        "New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($destination)) | Out-Null\n"
        "Set-Content -LiteralPath $destination -Value 'partial-output'\n"
        "Start-Sleep -Seconds 5\n");
    WriteTextFile(sourcePath, "cancel-me");

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"}),
        .PollInterval = std::chrono::milliseconds{50}
    });
    CTestProgressSink progressSink;
    std::stop_source stopSource;

    Librova::Domain::SConversionResult result;
    std::jthread worker([&] {
        result = converter.Convert({
            .SourcePath = sourcePath,
            .DestinationPath = destinationPath,
            .SourceFormat = Librova::Domain::EBookFormat::Fb2,
            .DestinationFormat = Librova::Domain::EBookFormat::Epub
        }, progressSink, stopSource.get_token());
    });

    WaitForCondition(
        [&destinationPath] { return std::filesystem::exists(destinationPath); },
        std::chrono::seconds{2},
        "Timed out waiting for partial converter output.");
    stopSource.request_stop();
    worker.join();

    REQUIRE_FALSE(result.IsSuccess());
    REQUIRE(result.IsCancelled());
    REQUIRE_FALSE(std::filesystem::exists(destinationPath));
}

TEST_CASE("External book converter can run with an explicit working directory", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-working-directory");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "cwd-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";
    const std::filesystem::path workingDirectory = sandbox.GetPath() / "logs";
    const std::filesystem::path markerPath = workingDirectory / "cwd-marker.txt";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destination = $args[1]\n"
        "New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($destination)) | Out-Null\n"
        "Set-Content -LiteralPath '"
            + markerPath.generic_string()
            + "' -Value (Get-Location).Path\n"
        "Copy-Item -LiteralPath $source -Destination $destination -Force\n");
    WriteTextFile(sourcePath, "converted-content");

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"}),
        .WorkingDirectory = workingDirectory
    });
    CTestProgressSink progressSink;

    const Librova::Domain::SConversionResult result = converter.Convert({
        .SourcePath = sourcePath,
        .DestinationPath = destinationPath,
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE(std::filesystem::exists(markerPath));
    REQUIRE(ReadTextFile(markerPath).find(workingDirectory.string()) != std::string::npos);
}

TEST_CASE("External book converter launch failures include executable and working-directory diagnostics", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-external-converter-launch-failure");
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";
    const std::filesystem::path missingExecutable = sandbox.GetPath() / "missing-converter.exe";
    const std::filesystem::path workingDirectory = sandbox.GetPath() / "converter-work";
    WriteTextFile(sourcePath, "content");

    Librova::ConverterCommand::SConverterCommandProfile profile{
        .ExecutablePath = missingExecutable,
        .ArgumentTemplate = {"{source}", "{destination}"},
        .OutputMode = Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath
    };

    const Librova::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = profile,
        .WorkingDirectory = workingDirectory
    });
    CTestProgressSink progressSink;

    try
    {
        static_cast<void>(converter.Convert({
            .SourcePath = sourcePath,
            .DestinationPath = destinationPath,
            .SourceFormat = Librova::Domain::EBookFormat::Fb2,
            .DestinationFormat = Librova::Domain::EBookFormat::Epub
        }, progressSink, {}));
        FAIL("Expected converter launch to fail");
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        REQUIRE(message.find("missing-converter.exe") != std::string::npos);
        REQUIRE(message.find("converter-work") != std::string::npos);
        REQUIRE(message.find("win32_error=") != std::string::npos);
    }
}

