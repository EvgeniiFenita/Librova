#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ConverterConfiguration/ConverterConfiguration.hpp"
#include "CoreHost/HostOptions.hpp"
#include "Unicode/UnicodeConversion.hpp"

TEST_CASE("Host options parse required pipe and library root", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova"
    });

    REQUIRE(options.PipePath == std::filesystem::path{R"(\\.\pipe\Librova.Test)"});
    REQUIRE(options.LibraryRoot == std::filesystem::path{"C:/Librova"});
    REQUIRE(options.LibraryOpenMode == Librova::CoreHost::ELibraryOpenMode::OpenExisting);
    REQUIRE(options.MaxSessions == 0);
    REQUIRE(options.ConverterConfiguration.Mode
        == Librova::ConverterConfiguration::EConverterConfigurationMode::Disabled);
}

TEST_CASE("Host options parse explicit library creation mode", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova-New",
        "--library-mode", "create"
    });

    REQUIRE(options.LibraryOpenMode == Librova::CoreHost::ELibraryOpenMode::CreateNew);
}

TEST_CASE("Host options parse explicit host log file path", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--log-file", "C:/RuntimeLogs/host.log"
    });

    REQUIRE(options.LogFilePath.has_value());
    REQUIRE(*options.LogFilePath == std::filesystem::path{"C:/RuntimeLogs/host.log"});
}

TEST_CASE("Host options parse explicit converter working directory", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--converter-working-dir", "C:/Runtime/ConverterWorkspace"
    });

    REQUIRE(options.ConverterWorkingDirectory.has_value());
    REQUIRE(*options.ConverterWorkingDirectory == std::filesystem::path{"C:/Runtime/ConverterWorkspace"});
}

TEST_CASE("Host options parse explicit managed storage staging root", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--managed-storage-staging-root", "C:/Runtime/ManagedStorageStaging"
    });

    REQUIRE(options.ManagedStorageStagingRoot.has_value());
    REQUIRE(*options.ManagedStorageStagingRoot == std::filesystem::path{"C:/Runtime/ManagedStorageStaging"});
}

TEST_CASE("Host options parse UTF-8 encoded Unicode paths", "[core-host]")
{
    const auto utf8PipePathBytes = std::filesystem::path(u8R"(\\.\pipe\Librova.Тест)").generic_u8string();
    const auto utf8LibraryRootBytes = std::filesystem::path(u8"C:/Библиотека/Librova").generic_u8string();
    const auto utf8ExecutableBytes = std::filesystem::path(u8"C:/Конвертеры/fbc.exe").generic_u8string();
    const auto utf8ConfigBytes = std::filesystem::path(u8"C:/Конвертеры/fbc.yaml").generic_u8string();
    const auto utf8ConverterWorkingDirectoryBytes = std::filesystem::path(u8"C:/Рантайм/Конвертер").generic_u8string();
    const auto utf8ManagedStorageStagingRootBytes = std::filesystem::path(u8"C:/Рантайм/Стейджинг").generic_u8string();

    const std::string utf8PipePath(reinterpret_cast<const char*>(utf8PipePathBytes.data()), utf8PipePathBytes.size());
    const std::string utf8LibraryRoot(reinterpret_cast<const char*>(utf8LibraryRootBytes.data()), utf8LibraryRootBytes.size());
    const std::string utf8Executable(reinterpret_cast<const char*>(utf8ExecutableBytes.data()), utf8ExecutableBytes.size());
    const std::string utf8Config(reinterpret_cast<const char*>(utf8ConfigBytes.data()), utf8ConfigBytes.size());
    const std::string utf8ConverterWorkingDirectory(
        reinterpret_cast<const char*>(utf8ConverterWorkingDirectoryBytes.data()),
        utf8ConverterWorkingDirectoryBytes.size());
    const std::string utf8ManagedStorageStagingRoot(
        reinterpret_cast<const char*>(utf8ManagedStorageStagingRootBytes.data()),
        utf8ManagedStorageStagingRootBytes.size());

    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", utf8PipePath,
        "--library-root", utf8LibraryRoot,
        "--converter-working-dir", utf8ConverterWorkingDirectory,
        "--managed-storage-staging-root", utf8ManagedStorageStagingRoot,
        "--fb2cng-exe", utf8Executable,
        "--fb2cng-config", utf8Config
    });

    REQUIRE(Librova::Unicode::PathToUtf8(options.PipePath) == utf8PipePath);
    REQUIRE(Librova::Unicode::PathToUtf8(options.LibraryRoot) == utf8LibraryRoot);
    REQUIRE(options.ConverterWorkingDirectory.has_value());
    REQUIRE(Librova::Unicode::PathToUtf8(*options.ConverterWorkingDirectory) == utf8ConverterWorkingDirectory);
    REQUIRE(options.ManagedStorageStagingRoot.has_value());
    REQUIRE(Librova::Unicode::PathToUtf8(*options.ManagedStorageStagingRoot) == utf8ManagedStorageStagingRoot);
    REQUIRE(Librova::Unicode::PathToUtf8(options.ConverterConfiguration.Fb2Cng.ExecutablePath) == utf8Executable);
    REQUIRE(options.ConverterConfiguration.Fb2Cng.ConfigPath.has_value());
    REQUIRE(Librova::Unicode::PathToUtf8(*options.ConverterConfiguration.Fb2Cng.ConfigPath) == utf8Config);
}

TEST_CASE("Host options parse built-in fb2cng configuration", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--parent-pid", "4242",
        "--fb2cng-exe", "C:/tools/fbc.exe",
        "--fb2cng-config", "C:/tools/fbc.yaml",
        "--serve-one"
    });

    REQUIRE(options.MaxSessions == 1);
    REQUIRE(options.ParentProcessId == std::optional<std::uint32_t>{4242});
    REQUIRE(options.ConverterConfiguration.Mode
        == Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng);
    REQUIRE(options.ConverterConfiguration.Fb2Cng.ExecutablePath == std::filesystem::path{"C:/tools/fbc.exe"});
    REQUIRE(options.ConverterConfiguration.Fb2Cng.ConfigPath == std::filesystem::path{"C:/tools/fbc.yaml"});
}

TEST_CASE("Host options reject missing required arguments", "[core-host]")
{
    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)"
        }),
        std::invalid_argument);
}

TEST_CASE("Host options reject removed custom converter flags", "[core-host]")
{
    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)",
            "--library-root", "C:/Librova",
            "--converter-exe", "C:/tools/custom.exe"
        }),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)",
            "--library-root", "C:/Librova",
            "--converter-arg", "{source}"
        }),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)",
            "--library-root", "C:/Librova",
            "--converter-output", "directory"
        }),
        std::invalid_argument);
}

TEST_CASE("Host options allow help and version without required runtime arguments", "[core-host]")
{
    const auto helpOptions = Librova::CoreHost::CHostOptions::Parse({"--help"});
    REQUIRE(helpOptions.ShowHelp);
    REQUIRE_FALSE(helpOptions.ShowVersion);
    REQUIRE(helpOptions.PipePath.empty());
    REQUIRE(helpOptions.LibraryRoot.empty());

    const auto versionOptions = Librova::CoreHost::CHostOptions::Parse({"--version"});
    REQUIRE(versionOptions.ShowVersion);
    REQUIRE_FALSE(versionOptions.ShowHelp);
    REQUIRE(versionOptions.PipePath.empty());
    REQUIRE(versionOptions.LibraryRoot.empty());
}

