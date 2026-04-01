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

TEST_CASE("FB2 parser decodes windows-1251 metadata as UTF-8", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-cp1251");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "cp1251.fb2";

    const std::string fb2Text =
        "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\r\n"
        "<FictionBook>\r\n"
        "  <description>\r\n"
        "    <title-info>\r\n"
        "      <book-title>\xC0\xED\xE3\xE5\xEB\xFB \xE8 \xE4\xE5\xEC\xEE\xED\xFB</book-title>\r\n"
        "      <author>\r\n"
        "        <first-name>\xC4\xFD\xED</first-name>\r\n"
        "        <last-name>\xC1\xF0\xE0\xF3\xED</last-name>\r\n"
        "      </author>\r\n"
        "      <lang>ru</lang>\r\n"
        "    </title-info>\r\n"
        "  </description>\r\n"
        "</FictionBook>";

    WriteTextFile(fb2Path, fb2Text);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Ангелы и демоны");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Дэн Браун"}));
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

TEST_CASE("FB2 parser decodes windows-1251 metadata when XML declaration uses single quotes", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-cp1251-single-quotes");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "cp1251-single-quotes.fb2";

    const std::string fb2Text =
        "<?xml version='1.0' encoding='windows-1251'?>\r\n"
        "<FictionBook>\r\n"
        "  <description>\r\n"
        "    <title-info>\r\n"
        "      <book-title>\xD2\xF0\xF3\xE4\xED\xEE \xE1\xFB\xF2\xFC \xE1\xEE\xE3\xEE\xEC</book-title>\r\n"
        "      <author>\r\n"
        "        <first-name>\xC0\xF0\xEA\xE0\xE4\xE8\xE9</first-name>\r\n"
        "        <last-name>\xD1\xF2\xF0\xF3\xE3\xE0\xF6\xEA\xE8\xE9</last-name>\r\n"
        "      </author>\r\n"
        "      <lang>ru</lang>\r\n"
        "    </title-info>\r\n"
        "  </description>\r\n"
        "</FictionBook>";

    WriteTextFile(fb2Path, fb2Text);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Трудно быть богом");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Аркадий Стругацкий"}));
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

TEST_CASE("FB2 parser decodes windows-1251 metadata when XML declaration uses uppercase encoding", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-cp1251-uppercase");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "cp1251-uppercase.fb2";

    const std::string fb2Text =
        "<?xml version=\"1.0\" encoding=\"WINDOWS-1251\"?>\r\n"
        "<FictionBook>\r\n"
        "  <description>\r\n"
        "    <title-info>\r\n"
        "      <book-title>\xCF\xE8\xEA\xED\xE8\xEA \xED\xE0 \xEE\xE1\xEE\xF7\xE8\xED\xE5</book-title>\r\n"
        "      <author>\r\n"
        "        <first-name>\xC0\xF0\xEA\xE0\xE4\xE8\xE9</first-name>\r\n"
        "        <last-name>\xD1\xF2\xF0\xF3\xE3\xE0\xF6\xEA\xE8\xE9</last-name>\r\n"
        "      </author>\r\n"
        "      <lang>ru</lang>\r\n"
        "    </title-info>\r\n"
        "  </description>\r\n"
        "</FictionBook>";

    WriteTextFile(fb2Path, fb2Text);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Пикник на обочине");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Аркадий Стругацкий"}));
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

TEST_CASE("FB2 parser accepts dot-separated sequence numbers independently from process locale", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-sequence-number");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "sequence-number.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Test</book-title>
      <author>
        <first-name>Arkady</first-name>
        <last-name>Strugatsky</last-name>
      </author>
      <lang>ru</lang>
      <sequence name="Cycle" number="1.5" />
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.SeriesUtf8 == std::optional<std::string>{"Cycle"});
    REQUIRE(parsedBook.Metadata.SeriesIndex == std::optional<double>{1.5});
}

TEST_CASE("FB2 parser rejects invalid publish year text", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-invalid-year");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "invalid-year.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Test</book-title>
      <author>
        <first-name>Arkady</first-name>
        <last-name>Strugatsky</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
    <publish-info>
      <year>1972abc</year>
    </publish-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;

    try
    {
        static_cast<void>(parser.Parse(fb2Path));
        FAIL("Expected parser to reject invalid FB2 publish year.");
    }
    catch (const std::exception& error)
    {
        REQUIRE(std::string{error.what()}.find("publish year") != std::string::npos);
    }
}
