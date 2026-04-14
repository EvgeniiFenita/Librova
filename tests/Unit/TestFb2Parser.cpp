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
      <genre>science_fiction</genre>
      <genre> adventure </genre>
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
    REQUIRE(parsedBook.Metadata.TagsUtf8.empty());
    REQUIRE(parsedBook.Metadata.GenresUtf8 == std::vector<std::string>({"Science Fiction", "Adventure"}));
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

TEST_CASE("FB2 parser includes XML preview when the file is empty", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-empty");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "empty.fb2";
    WriteTextFile(fb2Path, "");

    const Librova::Fb2Parsing::CFb2Parser parser;

    try
    {
        static_cast<void>(parser.Parse(fb2Path));
        FAIL("Expected parser to reject empty FB2 input.");
    }
    catch (const std::exception& error)
    {
        const std::string message = error.what();
        REQUIRE(message.find("size_bytes=0") != std::string::npos);
        REQUIRE(message.find("xml_preview=\"<empty>\"") != std::string::npos);
    }
}

TEST_CASE("FB2 parser uses 'Anonimous' fallback when all title-info author nodes are empty", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-empty-author");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "empty-author.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Magazine Issue</book-title>
      <author>
        <first-name></first-name>
        <last-name></last-name>
      </author>
      <lang>ru</lang>
    </title-info>
    <document-info>
      <author>
        <nickname>Scanner Team</nickname>
      </author>
    </document-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);

    REQUIRE(result.Metadata.AuthorsUtf8 == std::vector<std::string>{"Аноним"});
    REQUIRE(result.Metadata.TitleUtf8 == "Magazine Issue");
}

TEST_CASE("FB2 parser uses 'Anonimous' fallback when title-info has no author node at all", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-no-author-node");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "no-author-node.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <genre>religion</genre>
      <book-title>Anonymous Religious Text</book-title>
      <lang>ru</lang>
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);

    REQUIRE(result.Metadata.AuthorsUtf8 == std::vector<std::string>{"Аноним"});
    REQUIRE(result.Metadata.TitleUtf8 == "Anonymous Religious Text");
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

TEST_CASE("FB2 parser strips UTF-8 BOM and parses correctly", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-bom");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "bom.fb2";

    // UTF-8 BOM (EF BB BF) prepended to a valid UTF-8 FB2 file.
    // Causes "Error parsing document declaration" in pugixml without BOM stripping.
    std::string fb2Text;
    fb2Text += '\xEF';
    fb2Text += '\xBB';
    fb2Text += '\xBF';
    fb2Text +=
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<FictionBook>\n"
        "  <description>\n"
        "    <title-info>\n"
        "      <book-title>\xD0\x9C\xD0\xB0\xD1\x81\xD1\x82\xD0\xB5\xD1\x80 \xD0\xB8 \xD0\x9C\xD0\xB0\xD1\x80\xD0\xB3\xD0\xB0\xD1\x80\xD0\xB8\xD1\x82\xD0\xB0</book-title>\n"
        "      <author>\n"
        "        <first-name>\xD0\x9C\xD0\xB8\xD1\x85\xD0\xB0\xD0\xB8\xD0\xBB</first-name>\n"
        "        <last-name>\xD0\x91\xD1\x83\xD0\xBB\xD0\xB3\xD0\xB0\xD0\xBA\xD0\xBE\xD0\xB2</last-name>\n"
        "      </author>\n"
        "      <lang>ru</lang>\n"
        "    </title-info>\n"
        "  </description>\n"
        "</FictionBook>";

    WriteTextFile(fb2Path, fb2Text);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Мастер и Маргарита");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>{"Михаил Булгаков"});
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

#ifdef _WIN32
TEST_CASE("FB2 parser falls back to CP1251 for misdeclared UTF-8 files", "[fb2-parsing]")
{
    // Simulates a real-world file with no encoding declaration (or silently
    // misdeclared as UTF-8) but containing raw CP1251 bytes.
    // Affects ~100–180 books per lib.rus.ec 6000-book batch based on import log analysis.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-cp1251-fallback");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "misdeclared.fb2";

    const std::string fb2Text =
        "<?xml version=\"1.0\"?>\r\n"
        "<FictionBook>\r\n"
        "  <description>\r\n"
        "    <title-info>\r\n"
        "      <book-title>\xC2\xEE\xE9\xED\xE0 \xE8 \xEC\xE8\xF0</book-title>\r\n"
        "      <author>\r\n"
        "        <first-name>\xcb\xe5\xe2</first-name>\r\n"
        "        <last-name>\xd2\xee\xeb\xf1\xf2\xee\xe9</last-name>\r\n"
        "      </author>\r\n"
        "      <lang>ru</lang>\r\n"
        "    </title-info>\r\n"
        "  </description>\r\n"
        "</FictionBook>";

    WriteTextFile(fb2Path, fb2Text);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Война и мир");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>{"Лев Толстой"});
    REQUIRE(parsedBook.Metadata.Language == "ru");
}
#endif

TEST_CASE("FB2 parser recovers metadata from file with malformed body XML", "[fb2-parsing]")
{
    // Simulates lib.rus.ec files where body text contains unescaped '<' (e.g. "<...>" ellipsis,
    // "<:>" separators). The <description> comes before <body> so parse_recover salvages metadata.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-recover");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "malformed-body.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Роза мира</book-title>
      <author>
        <first-name>Даниил</first-name>
        <last-name>Андреев</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
  </description>
  <body>
    <section>
      <p>Текст <...> с многоточием и <:> разделителем.</p>
    </section>
  </body>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Роза мира");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>{"Даниил Андреев"});
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

TEST_CASE("FB2 parser skips non-integer publish year with warning and still imports book", "[fb2-parsing]")
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
      <year>March 2004</year>
    </publish-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);
    REQUIRE(result.Metadata.TitleUtf8 == "Test");
    REQUIRE_FALSE(result.Metadata.Year.has_value());
}

TEST_CASE("FB2 parser skips non-numeric sequence number with warning and still imports book", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-invalid-seq");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "invalid-seq.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Test Series</book-title>
      <author>
        <first-name>Author</first-name>
        <last-name>Name</last-name>
      </author>
      <lang>ru</lang>
      <sequence name="TestSeries" number="abc" />
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);
    REQUIRE(result.Metadata.TitleUtf8 == "Test Series");
    REQUIRE(result.Metadata.SeriesUtf8.has_value());
    REQUIRE(*result.Metadata.SeriesUtf8 == "TestSeries");
    REQUIRE_FALSE(result.Metadata.SeriesIndex.has_value());
}

TEST_CASE("FB2 parser accepts missing lang node and imports book with empty language", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-no-lang");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "no-lang.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>No Language Book</book-title>
      <author>
        <first-name>Author</first-name>
        <last-name>Name</last-name>
      </author>
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);
    REQUIRE(result.Metadata.TitleUtf8 == "No Language Book");
    REQUIRE(result.Metadata.Language.empty());
}

TEST_CASE("FB2 parser splits comma-concatenated genre node value into multiple genres", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-comma-genres");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "comma-genres.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Comma Genre Test</book-title>
      <genre>sf_fantasy_city,sf_horror</genre>
      <author>
        <first-name>Test</first-name>
        <last-name>Author</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);
    REQUIRE(result.Metadata.TitleUtf8 == "Comma Genre Test");
    REQUIRE(result.Metadata.GenresUtf8 == std::vector<std::string>{"Urban Fantasy", "Horror & Mystic"});
}

TEST_CASE("FB2 parser preserves unknown token as-is when splitting comma-concatenated genre value", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-comma-unknown-genre");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "comma-unknown-genre.fb2";

    WriteTextFile(
        fb2Path,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook>
  <description>
    <title-info>
      <book-title>Mixed Genre Test</book-title>
      <genre>sf_horror,unknown_community_code</genre>
      <author>
        <first-name>Test</first-name>
        <last-name>Author</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
  </description>
</FictionBook>)");

    const Librova::Fb2Parsing::CFb2Parser parser;
    const auto result = parser.Parse(fb2Path);
    REQUIRE(result.Metadata.TitleUtf8 == "Mixed Genre Test");
    // Known code resolves to display name; unknown code is stored as-is (raw).
    REQUIRE(result.Metadata.GenresUtf8 == std::vector<std::string>{"Horror & Mystic", "unknown_community_code"});
}

#ifdef _WIN32
TEST_CASE("FB2 parser handles UTF-16 LE encoded file", "[fb2-parsing]")
{
    // Simulates lib.rus.ec files that begin with 0xFF 0xFE BOM and contain raw UTF-16 LE bytes.
    // pugixml receives raw UTF-16 and fails with "Could not determine tag type" without this fix.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-utf16le");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "utf16le.fb2";

    const std::wstring wideContent =
        L"<?xml version=\"1.0\" encoding=\"utf-16\"?>\n"
        L"<FictionBook>\n"
        L"  <description>\n"
        L"    <title-info>\n"
        L"      <book-title>\u041F\u0438\u043A\u043D\u0438\u043A \u043D\u0430 \u043E\u0431\u043E\u0447\u0438\u043D\u0435</book-title>\n"
        L"      <author>\n"
        L"        <first-name>\u0410\u0440\u043A\u0430\u0434\u0438\u0439</first-name>\n"
        L"        <last-name>\u0421\u0442\u0440\u0443\u0433\u0430\u0446\u043A\u0438\u0439</last-name>\n"
        L"      </author>\n"
        L"      <lang>ru</lang>\n"
        L"    </title-info>\n"
        L"  </description>\n"
        L"</FictionBook>";

    std::string bytes;
    bytes += '\xFF';
    bytes += '\xFE';
    const auto* wideBytes = reinterpret_cast<const char*>(wideContent.data());
    bytes.append(wideBytes, wideContent.size() * sizeof(wchar_t));

    WriteTextFile(fb2Path, bytes);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Пикник на обочине");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>{"Аркадий Стругацкий"});
    REQUIRE(parsedBook.Metadata.Language == "ru");
}

TEST_CASE("FB2 parser handles UTF-16 BE encoded file", "[fb2-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-fb2-parser-utf16be");
    const std::filesystem::path fb2Path = sandbox.GetPath() / "utf16be.fb2";

    const std::wstring wideContent =
        L"<?xml version=\"1.0\" encoding=\"utf-16be\"?>\n"
        L"<FictionBook>\n"
        L"  <description>\n"
        L"    <title-info>\n"
        L"      <book-title>\u0410\u043D\u0434\u0440\u043E\u043C\u0435\u0434\u0430</book-title>\n"
        L"      <author>\n"
        L"        <first-name>\u0418\u0432\u0430\u043D</first-name>\n"
        L"        <last-name>\u0415\u0444\u0440\u0435\u043C\u043E\u0432</last-name>\n"
        L"      </author>\n"
        L"      <lang>ru</lang>\n"
        L"    </title-info>\n"
        L"  </description>\n"
        L"</FictionBook>";

    std::string bytes;
    bytes += '\xFE';
    bytes += '\xFF';
    for (const wchar_t wc : wideContent)
    {
        bytes += static_cast<char>((static_cast<unsigned>(wc) >> 8u) & 0xFFu);
        bytes += static_cast<char>(static_cast<unsigned>(wc) & 0xFFu);
    }

    WriteTextFile(fb2Path, bytes);

    const Librova::Fb2Parsing::CFb2Parser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(fb2Path);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Андромеда");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>{"Иван Ефремов"});
    REQUIRE(parsedBook.Metadata.Language == "ru");
}
#endif
