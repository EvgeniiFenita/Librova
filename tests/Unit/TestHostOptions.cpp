#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ConverterConfiguration/ConverterConfiguration.hpp"
#include "CoreHost/HostOptions.hpp"

TEST_CASE("Host options parse required pipe and library root", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova"
    });

    REQUIRE(options.PipePath == std::filesystem::path{R"(\\.\pipe\Librova.Test)"});
    REQUIRE(options.LibraryRoot == std::filesystem::path{"C:/Librova"});
    REQUIRE(options.MaxSessions == 0);
    REQUIRE(options.ConverterConfiguration.Mode
        == Librova::ConverterConfiguration::EConverterConfigurationMode::Disabled);
}

TEST_CASE("Host options parse built-in fb2cng configuration", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--fb2cng-exe", "C:/tools/fbc.exe",
        "--fb2cng-config", "C:/tools/fbc.yaml",
        "--serve-one"
    });

    REQUIRE(options.MaxSessions == 1);
    REQUIRE(options.ConverterConfiguration.Mode
        == Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng);
    REQUIRE(options.ConverterConfiguration.Fb2Cng.ExecutablePath == std::filesystem::path{"C:/tools/fbc.exe"});
    REQUIRE(options.ConverterConfiguration.Fb2Cng.ConfigPath == std::filesystem::path{"C:/tools/fbc.yaml"});
}

TEST_CASE("Host options parse custom converter configuration", "[core-host]")
{
    const auto options = Librova::CoreHost::CHostOptions::Parse({
        "--pipe", R"(\\.\pipe\Librova.Test)",
        "--library-root", "C:/Librova",
        "--converter-exe", "C:/tools/custom.exe",
        "--converter-arg", "--input",
        "--converter-arg", "{source}",
        "--converter-arg", "--output-dir",
        "--converter-arg", "{destination_dir}",
        "--converter-output", "directory",
        "--max-sessions", "3"
    });

    REQUIRE(options.MaxSessions == 3);
    REQUIRE(options.ConverterConfiguration.Mode
        == Librova::ConverterConfiguration::EConverterConfigurationMode::CustomCommand);
    REQUIRE(options.ConverterConfiguration.Custom.ExecutablePath == std::filesystem::path{"C:/tools/custom.exe"});
    REQUIRE(options.ConverterConfiguration.Custom.ArgumentTemplate.size() == 4);
    REQUIRE(
        options.ConverterConfiguration.Custom.OutputMode
        == Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
}

TEST_CASE("Host options reject missing required arguments", "[core-host]")
{
    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)"
        }),
        std::invalid_argument);
}

TEST_CASE("Host options reject mixed converter modes", "[core-host]")
{
    REQUIRE_THROWS_AS(
        Librova::CoreHost::CHostOptions::Parse({
            "--pipe", R"(\\.\pipe\Librova.Test)",
            "--library-root", "C:/Librova",
            "--fb2cng-exe", "C:/tools/fbc.exe",
            "--converter-exe", "C:/tools/custom.exe"
        }),
        std::invalid_argument);
}
