#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "ParserRegistry/BookParserRegistry.hpp"

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
    std::ofstream output(path, std::ios::binary);
    output << text;
}

} // namespace

TEST_CASE("Parser registry detects supported book formats from extension", "[parser-registry]")
{
    REQUIRE(
        LibriFlow::ParserRegistry::CBookParserRegistry::TryDetectFormat("book.epub") ==
        std::optional<LibriFlow::Domain::EBookFormat>{LibriFlow::Domain::EBookFormat::Epub});
    REQUIRE(
        LibriFlow::ParserRegistry::CBookParserRegistry::TryDetectFormat("BOOK.FB2") ==
        std::optional<LibriFlow::Domain::EBookFormat>{LibriFlow::Domain::EBookFormat::Fb2});
    REQUIRE_FALSE(LibriFlow::ParserRegistry::CBookParserRegistry::TryDetectFormat("book.zip").has_value());
}

TEST_CASE("Parser registry routes FB2 parsing through the matching parser", "[parser-registry]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-parser-registry");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "sample.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Roadside Picnic</book-title>
      <author>
        <first-name>Arkady</first-name>
        <last-name>Strugatsky</last-name>
      </author>
      <lang>en</lang>
    </title-info>
  </description>
</FictionBook>)");

    const LibriFlow::ParserRegistry::CBookParserRegistry registry;
    const LibriFlow::Domain::SParsedBook parsedBook = registry.Parse(fb2Path);

    REQUIRE(registry.CanParse(LibriFlow::Domain::EBookFormat::Fb2));
    REQUIRE(parsedBook.SourceFormat == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Roadside Picnic");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Arkady Strugatsky"}));
}

TEST_CASE("Parser registry rejects unsupported extensions", "[parser-registry]")
{
    const LibriFlow::ParserRegistry::CBookParserRegistry registry;
    REQUIRE_THROWS(registry.Parse("book.zip"));
}
