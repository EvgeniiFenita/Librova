#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "Fb2Parsing/Fb2Parser.hpp"

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

TEST_CASE("FB2 parser extracts metadata and embedded cover", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "sample.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <book-title>Пикник на обочине</book-title>
      <author>
        <first-name>Аркадий</first-name>
        <last-name>Стругацкий</last-name>
      </author>
      <author>
        <first-name>Борис</first-name>
        <last-name>Стругацкий</last-name>
      </author>
      <lang>ru</lang>
      <sequence name="Миры" number="1.5" />
      <annotation>
        <p>Классика советской фантастики</p>
      </annotation>
      <coverpage>
        <image l:href="#cover-image"/>
      </coverpage>
    </title-info>
    <publish-info>
      <publisher>АСТ</publisher>
      <year>1972</year>
      <isbn>978-5-17-090334-4</isbn>
    </publish-info>
    <document-info>
      <id>roadside-picnic-fb2</id>
    </document-info>
  </description>
  <body>
    <section><p>Test body</p></section>
  </body>
  <binary id="cover-image" content-type="image/jpeg">ASNF</binary>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parser.CanParse(Librova::Domain::EBookFormat::Fb2));
    REQUIRE(parsedBook.SourceFormat == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Пикник на обочине");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Аркадий Стругацкий", "Борис Стругацкий"}));
    REQUIRE(parsedBook.Metadata.Language == "ru");
    REQUIRE(parsedBook.Metadata.SeriesUtf8 == std::optional<std::string>{"Миры"});
    REQUIRE(parsedBook.Metadata.SeriesIndex == std::optional<double>{1.5});
    REQUIRE(parsedBook.Metadata.DescriptionUtf8 == std::optional<std::string>{"Классика советской фантастики"});
    REQUIRE(parsedBook.Metadata.PublisherUtf8 == std::optional<std::string>{"АСТ"});
    REQUIRE(parsedBook.Metadata.Year == std::optional<int>{1972});
    REQUIRE(parsedBook.Metadata.Isbn == std::optional<std::string>{"978-5-17-090334-4"});
    REQUIRE(parsedBook.Metadata.Identifier == std::optional<std::string>{"roadside-picnic-fb2"});
    REQUIRE(parsedBook.CoverExtension == std::optional<std::string>{"jpg"});
    REQUIRE(parsedBook.CoverBytes == std::vector<std::byte>({std::byte{0x01}, std::byte{0x23}, std::byte{0x45}}));
}

TEST_CASE("FB2 parser rejects malformed metadata", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-invalid");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "invalid.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <lang>ru</lang>
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    REQUIRE_THROWS(parser.Parse(fb2Path));
}
