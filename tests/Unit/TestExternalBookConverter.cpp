#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

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

class CTestProgressSink final : public LibriFlow::Domain::IProgressSink
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

LibriFlow::ConverterCommand::SConverterCommandProfile CreatePwshProfile(
    const std::filesystem::path& scriptPath,
    const LibriFlow::ConverterCommand::EConverterOutputMode outputMode,
    const std::vector<std::string>& trailingArguments)
{
    LibriFlow::ConverterCommand::SConverterCommandProfile profile{
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
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-external-converter-exact");
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

    const LibriFlow::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            LibriFlow::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"})
    });
    CTestProgressSink progressSink;

    const LibriFlow::Domain::SConversionResult result = converter.Convert({
        .SourcePath = sourcePath,
        .DestinationPath = destinationPath,
        .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
        .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
    }, progressSink, {});

    REQUIRE(result.Succeeded);
    REQUIRE(result.OutputPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE(ReadTextFile(destinationPath) == "converted-content");
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("External book converter relocates single-file output directory results", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-external-converter-directory");
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

    const LibriFlow::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            LibriFlow::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory,
            {"{source}", "{destination_dir}"})
    });
    CTestProgressSink progressSink;

    const LibriFlow::Domain::SConversionResult result = converter.Convert({
        .SourcePath = sourcePath,
        .DestinationPath = destinationPath,
        .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
        .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
    }, progressSink, {});

    REQUIRE(result.Succeeded);
    REQUIRE(result.OutputPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE_FALSE(std::filesystem::exists(destinationPath.parent_path() / "generated-by-converter.epub"));
    REQUIRE(ReadTextFile(destinationPath) == "directory-output");
}

TEST_CASE("External book converter reports cancellation and stops the process", "[converter-runtime]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-external-converter-cancel");
    const std::filesystem::path scriptPath = sandbox.GetPath() / "slow-converter.ps1";
    const std::filesystem::path sourcePath = sandbox.GetPath() / "source.fb2";
    const std::filesystem::path destinationPath = sandbox.GetPath() / "output" / "book.epub";

    WriteTextFile(
        scriptPath,
        "$source = $args[0]\n"
        "$destination = $args[1]\n"
        "Start-Sleep -Seconds 5\n"
        "New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($destination)) | Out-Null\n"
        "Copy-Item -LiteralPath $source -Destination $destination -Force\n");
    WriteTextFile(sourcePath, "cancel-me");

    const LibriFlow::ConverterRuntime::CExternalBookConverter converter({
        .CommandProfile = CreatePwshProfile(
            scriptPath,
            LibriFlow::ConverterCommand::EConverterOutputMode::ExactDestinationPath,
            {"{source}", "{destination}"}),
        .PollInterval = std::chrono::milliseconds{50}
    });
    CTestProgressSink progressSink;
    std::stop_source stopSource;

    LibriFlow::Domain::SConversionResult result;
    std::jthread worker([&] {
        result = converter.Convert({
            .SourcePath = sourcePath,
            .DestinationPath = destinationPath,
            .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
            .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
        }, progressSink, stopSource.get_token());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    stopSource.request_stop();
    worker.join();

    REQUIRE_FALSE(result.Succeeded);
    REQUIRE(result.Warnings == std::vector<std::string>({"Conversion cancelled."}));
    REQUIRE_FALSE(std::filesystem::exists(destinationPath));
}
