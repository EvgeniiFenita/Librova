#include <catch2/catch_test_macros.hpp>

#include "ConverterCommand/ConverterCommandBuilder.hpp"

TEST_CASE("Converter command builder creates default fb2cng command profile", "[converter-command]")
{
    const auto profile = Librova::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile("tools/fbc.exe");

    REQUIRE(profile.IsValid());
    REQUIRE(profile.ExecutablePath == std::filesystem::path{"tools/fbc.exe"});
    REQUIRE(profile.OutputMode == Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
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
    const auto profile = Librova::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile(
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
    const Librova::ConverterCommand::SConverterCommandProfile profile{
        .ExecutablePath = "tools/custom-converter.exe",
        .ArgumentTemplate = {"--input", "{source}", "--output", "{destination}", "--format", "{output_format}"},
        .OutputMode = Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath
    };

    const Librova::Domain::SConversionRequest request{
        .SourcePath = "Import/source.fb2",
        .DestinationPath = "Temp/book.epub",
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    };

    const auto command = Librova::ConverterCommand::CConverterCommandBuilder::Build(profile, request);

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
    const auto profile = Librova::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile("tools/fbc.exe");
    const Librova::Domain::SConversionRequest request{
        .SourcePath = "Import/book.fb2",
        .DestinationPath = "Temp/converted/book.epub",
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    };

    const auto command = Librova::ConverterCommand::CConverterCommandBuilder::Build(profile, request);

    REQUIRE(command.OutputMode == Librova::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory);
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

TEST_CASE("Converter command builder preserves literal placeholder text inside replacement paths", "[converter-command]")
{
    const Librova::ConverterCommand::SConverterCommandProfile profile{
        .ExecutablePath = "tools/custom-converter.exe",
        .ArgumentTemplate = {"--input", "{source}", "--output", "{destination}", "--dir", "{destination_dir}"},
        .OutputMode = Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath
    };

    const Librova::Domain::SConversionRequest request{
        .SourcePath = std::filesystem::path{u8"Import/{output_format}/source.fb2"},
        .DestinationPath = std::filesystem::path{u8"Temp/{output_format}/book.epub"},
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    };

    const auto command = Librova::ConverterCommand::CConverterCommandBuilder::Build(profile, request);

    REQUIRE(command.Arguments == std::vector<std::string>({
        "--input",
        "Import/{output_format}/source.fb2",
        "--output",
        "Temp/{output_format}/book.epub",
        "--dir",
        "Temp/{output_format}"
    }));
}
