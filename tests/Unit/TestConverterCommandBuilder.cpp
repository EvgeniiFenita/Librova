#include <catch2/catch_test_macros.hpp>

#include "ConverterCommand/ConverterCommandBuilder.hpp"

TEST_CASE("Converter command builder creates default fb2cng command profile", "[converter-command]")
{
    const auto profile = LibriFlow::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile("tools/fbc.exe");

    REQUIRE(profile.IsValid());
    REQUIRE(profile.ExecutablePath == std::filesystem::path{"tools/fbc.exe"});
    REQUIRE(profile.OutputMode == LibriFlow::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
    REQUIRE(profile.ArgumentTemplate == std::vector<std::string>({
        "convert",
        "--to",
        "{output_format}",
        "--overwrite",
        "{source}",
        "{destination_dir}"
    }));
}

TEST_CASE("Converter command builder includes optional fb2cng config file when requested", "[converter-command]")
{
    const auto profile = LibriFlow::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile(
        "tools/fbc.exe",
        std::filesystem::path{"config/fb2cng.yaml"});

    REQUIRE(profile.ArgumentTemplate == std::vector<std::string>({
        "-c",
        "config/fb2cng.yaml",
        "convert",
        "--to",
        "{output_format}",
        "--overwrite",
        "{source}",
        "{destination_dir}"
    }));
}

TEST_CASE("Converter command builder resolves placeholders for generic templates", "[converter-command]")
{
    const LibriFlow::ConverterCommand::SConverterCommandProfile profile{
        .ExecutablePath = "tools/custom-converter.exe",
        .ArgumentTemplate = {"--input", "{source}", "--output", "{destination}", "--format", "{output_format}"},
        .OutputMode = LibriFlow::ConverterCommand::EConverterOutputMode::ExactDestinationPath
    };

    const LibriFlow::Domain::SConversionRequest request{
        .SourcePath = "Import/source.fb2",
        .DestinationPath = "Temp/book.epub",
        .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
        .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
    };

    const auto command = LibriFlow::ConverterCommand::CConverterCommandBuilder::Build(profile, request);

    REQUIRE(command.IsValid());
    REQUIRE(command.ExecutablePath == std::filesystem::path{"tools/custom-converter.exe"});
    REQUIRE(command.Arguments == std::vector<std::string>({
        "--input",
        "Import/source.fb2",
        "--output",
        "Temp/book.epub",
        "--format",
        "epub2"
    }));
    REQUIRE(command.ExpectedOutputPath == std::filesystem::path{"Temp/book.epub"});
    REQUIRE(command.ExpectedOutputDirectory == std::filesystem::path{"Temp"});
}

TEST_CASE("Converter command builder resolves fb2cng output directory contract", "[converter-command]")
{
    const auto profile = LibriFlow::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile("tools/fbc.exe");
    const LibriFlow::Domain::SConversionRequest request{
        .SourcePath = "Import/book.fb2",
        .DestinationPath = "Temp/converted/book.epub",
        .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
        .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
    };

    const auto command = LibriFlow::ConverterCommand::CConverterCommandBuilder::Build(profile, request);

    REQUIRE(command.OutputMode == LibriFlow::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
    REQUIRE(command.Arguments == std::vector<std::string>({
        "convert",
        "--to",
        "epub2",
        "--overwrite",
        "Import/book.fb2",
        "Temp/converted"
    }));
    REQUIRE(command.ExpectedOutputPath == std::filesystem::path{"Temp/converted/book.epub"});
}
