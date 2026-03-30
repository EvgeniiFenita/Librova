#include <catch2/catch_test_macros.hpp>

#include "ConverterConfiguration/ConverterConfiguration.hpp"

TEST_CASE("Disabled converter configuration builds no command profile", "[converter-config]")
{
    const Librova::ConverterConfiguration::SConverterConfiguration configuration;

    REQUIRE(configuration.IsValid());
    REQUIRE_FALSE(configuration.IsEnabled());
    REQUIRE_FALSE(Librova::ConverterConfiguration::TryBuildCommandProfile(configuration).has_value());
}

TEST_CASE("Built-in fb2cng configuration produces the default command profile", "[converter-config]")
{
    const Librova::ConverterConfiguration::SConverterConfiguration configuration{
        .Mode = Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng,
        .Fb2Cng = {
            .ExecutablePath = "C:/Tools/fbc.exe",
            .ConfigPath = "C:/Tools/fb2cng.yml"
        }
    };

    const auto profile = Librova::ConverterConfiguration::TryBuildCommandProfile(configuration);

    REQUIRE(configuration.IsValid());
    REQUIRE(profile.has_value());
    REQUIRE(profile->ExecutablePath == std::filesystem::path("C:/Tools/fbc.exe"));
    REQUIRE(profile->OutputMode == Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
    REQUIRE(profile->ArgumentTemplate == std::vector<std::string>({
        "-c",
        "C:/Tools/fb2cng.yml",
        "convert",
        "--to",
        "{output_format}",
        "--overwrite",
        "{source}",
        "{destination_dir}"
    }));
}

TEST_CASE("Custom converter configuration preserves explicit template and output mode", "[converter-config]")
{
    const Librova::ConverterConfiguration::SConverterConfiguration configuration{
        .Mode = Librova::ConverterConfiguration::EConverterConfigurationMode::CustomCommand,
        .Custom = {
            .ExecutablePath = "C:/Tools/custom-converter.exe",
            .ArgumentTemplate = {"run", "{source}", "{destination}"},
            .OutputMode = Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath
        }
    };

    const auto profile = Librova::ConverterConfiguration::TryBuildCommandProfile(configuration);

    REQUIRE(configuration.IsValid());
    REQUIRE(profile.has_value());
    REQUIRE(profile->ExecutablePath == std::filesystem::path("C:/Tools/custom-converter.exe"));
    REQUIRE(profile->ArgumentTemplate == std::vector<std::string>({
        "run",
        "{source}",
        "{destination}"
    }));
    REQUIRE(profile->OutputMode == Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath);
}

TEST_CASE("Invalid enabled converter configuration is rejected", "[converter-config]")
{
    const Librova::ConverterConfiguration::SConverterConfiguration configuration{
        .Mode = Librova::ConverterConfiguration::EConverterConfigurationMode::CustomCommand
    };

    REQUIRE_FALSE(configuration.IsValid());
    REQUIRE_FALSE(Librova::ConverterConfiguration::TryBuildCommandProfile(configuration).has_value());
}
