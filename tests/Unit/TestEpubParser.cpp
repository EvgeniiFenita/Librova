#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <zip.h>

#include "EpubParsing/EpubParser.hpp"

namespace {

constexpr std::string_view GValidEpubBase64 =
    "UEsDBBQAAAAIAJdWflxvYassFgAAABQAAAAIAAAAbWltZXR5cGVLLCjIyUxOLMnMz9NPLShN0q7KLAAAUEsDBBQAAAAIAJdWflwCqdJqrgAAAPsAAAAWAAAATUVUQS1JTkYvY29udGFpbmVyLnhtbF2OwQrCMBBE735F2KvU6k1C04KgVwX1A9Z0W4PJbmhSqX8v9iDi8cHMvKmaKXj1pCE5YQOb1RoUsZXWcW/gejkUW2jqRWWFMzqm4S87Bc/JwDiwFkwuacZASWerJRK3YsdAnPUc098RqBdKVYNI7pyn9KEfVt3ofREx3w0c97vTufwUifNKYgcqUOuwyK9IBjBG7yxmJ1wK3WIqItoH9rScgody1pQ/nmqemj/Ub1BLAwQUAAAACACXVn5cC8nkJuoBAABwAwAAEQAAAE9FQlBTL2NvbnRlbnQub3BmlZNNbtswEIX3OQXBbWHRTlI4MSRl1xO0B2DEkcxEohiKit2d7Rbttt12U/QG6k8Ax4V9huGNCtGynCCrbgSM5r2Pw3lgeDUvcnIPppKliugoGFICKimFVFlE3719M7igV/FJqHlyyzM4Kk9bZa3kXQ0DKUBZmUowEb0uy1spKJkXuaoiOrVWTxibzWaBFDoNSpOx0+FwzEqd0viEkLAAywW3fO+YiKQ36drk3iASBjkUoGzFRsGIeSMhoUgmVtocYvyOa9zgtv0S3GJDcIc/cec+4xq3+BCyXto7EwPclibGL26BG2zwD67xkeAPt3IL9wF/Y+M+4ab96e0H/UvAV9y5Ba7d8j/MOVdZzTOITe0Ffd0rjkslUvR7jWujJrK6VpPL8cXr0Xh4OTw7Oz/3jKPjSBFQJUZqK0sV4zf8i41buqXfVkPcEnf4Cx/cyi1xgzt8JO4jNrh1K69bed3aw5+C9vQ2OKJ4ARFNynswlCSlsqBsVw9kwTOgzKfMDjHHPnOuZAqV7UjSQuEv2QEomRpII2phblky5dqCGQXzqS1ySgoQkg/sew0R5VrnMuHtUMy3X82LfH/ic+xxnA7ti4r5TnCjs+dc32U3GrJ+/CcTh5WW6pBUe4iBlEjhuYcbdLZOGbLu/cT/AFBLAwQUAAAACACXVn5cJWuDMUMAAABKAAAAGQAAAE9FQlBTL3RleHQvY2hhcHRlcjEueGh0bWyzySjJzVGoyM3JK7ZVyigpKbDS1y8vL9crN9bLL0rXN7S0tNSvAKlRsrNJyk+ptLMpsAtJLS6x0S+ws9GHiOiD5O0AUEsDBBQAAAAIAJdWflxbcZxGBQAAAAMAAAAWAAAAT0VCUFMvaW1hZ2VzL2NvdmVyLmpwZ2NUdgUAUEsBAhQAFAAAAAgAl1Z+XG9hqywWAAAAFAAAAAgAAAAAAAAAAAAAAAAAAAAAAG1pbWV0eXBlUEsBAhQAFAAAAAgAl1Z+XAKp0mquAAAA+wAAABYAAAAAAAAAAAAAAAAAPAAAAE1FVEEtSU5GL2NvbnRhaW5lci54bWxQSwECFAAUAAAACACXVn5cC8nkJuoBAABwAwAAEQAAAAAAAAAAAAAAAAAeAQAAT0VCUFMvY29udGVudC5vcGZQSwECFAAUAAAACACXVn5cJWuDMUMAAABKAAAAGQAAAAAAAAAAAAAAAAA3AwAAT0VCUFMvdGV4dC9jaGFwdGVyMS54aHRtbFBLAQIUABQAAAAIAJdWflxbcZxGBQAAAAMAAAAWAAAAAAAAAAAAAAAAALEDAABPRUJQUy9pbWFnZXMvY292ZXIuanBnUEsFBgAAAAAFAAUARAEAAOoDAAAAAA==";

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

void AddZipEntry(zip_t* archive, const std::string& entryPath, const std::string& text)
{
    void* buffer = std::malloc(text.size());

    if (buffer == nullptr)
    {
        throw std::runtime_error("Failed to allocate zip text buffer.");
    }

    std::memcpy(buffer, text.data(), text.size());
    zip_source_t* source = zip_source_buffer(archive, buffer, text.size(), 1);

    if (source == nullptr)
    {
        std::free(buffer);
        throw std::runtime_error("Failed to allocate zip source.");
    }

    if (zip_file_add(archive, entryPath.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0)
    {
        zip_source_free(source);
        throw std::runtime_error("Failed to add zip entry.");
    }
}

void AddZipEntry(zip_t* archive, const std::string& entryPath, const std::vector<std::byte>& bytes)
{
    void* buffer = std::malloc(bytes.size());

    if (buffer == nullptr)
    {
        throw std::runtime_error("Failed to allocate zip binary buffer.");
    }

    std::memcpy(buffer, bytes.data(), bytes.size());
    zip_source_t* source = zip_source_buffer(archive, buffer, bytes.size(), 1);

    if (source == nullptr)
    {
        std::free(buffer);
        throw std::runtime_error("Failed to allocate zip source.");
    }

    if (zip_file_add(archive, entryPath.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0)
    {
        zip_source_free(source);
        throw std::runtime_error("Failed to add binary zip entry.");
    }
}

std::filesystem::path CreateSampleEpub(const std::filesystem::path& outputPath)
{
    const auto DecodeCharacter = [](const char value) -> std::uint8_t {
        if (value >= 'A' && value <= 'Z')
        {
            return static_cast<std::uint8_t>(value - 'A');
        }

        if (value >= 'a' && value <= 'z')
        {
            return static_cast<std::uint8_t>(value - 'a' + 26);
        }

        if (value >= '0' && value <= '9')
        {
            return static_cast<std::uint8_t>(value - '0' + 52);
        }

        if (value == '+')
        {
            return 62;
        }

        if (value == '/')
        {
            return 63;
        }

        throw std::runtime_error("Invalid base64 character in EPUB fixture.");
    };

    std::vector<std::uint8_t> bytes;
    bytes.reserve((GValidEpubBase64.size() * 3) / 4);

    for (std::size_t index = 0; index < GValidEpubBase64.size(); index += 4)
    {
        const char a = GValidEpubBase64[index];
        const char b = GValidEpubBase64[index + 1];
        const char c = GValidEpubBase64[index + 2];
        const char d = GValidEpubBase64[index + 3];

        const std::uint32_t block =
            (static_cast<std::uint32_t>(DecodeCharacter(a)) << 18) |
            (static_cast<std::uint32_t>(DecodeCharacter(b)) << 12) |
            ((c == '=' ? 0u : static_cast<std::uint32_t>(DecodeCharacter(c))) << 6) |
            (d == '=' ? 0u : static_cast<std::uint32_t>(DecodeCharacter(d)));

        bytes.push_back(static_cast<std::uint8_t>((block >> 16) & 0xFF));

        if (c != '=')
        {
            bytes.push_back(static_cast<std::uint8_t>((block >> 8) & 0xFF));
        }

        if (d != '=')
        {
            bytes.push_back(static_cast<std::uint8_t>(block & 0xFF));
        }
    }

    std::ofstream output(outputPath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return outputPath;
}

} // namespace

TEST_CASE("EPUB parser extracts metadata and cover from a valid EPUB package", "[epub-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-epub-parser");
    const std::filesystem::path epubPath = CreateSampleEpub(sandbox.GetPath() / "sample.epub");

    const Librova::EpubParsing::CEpubParser parser;
    const Librova::Domain::SParsedBook parsedBook = parser.Parse(epubPath);

    REQUIRE(parser.CanParse(Librova::Domain::EBookFormat::Epub));
    REQUIRE(parsedBook.SourceFormat == Librova::Domain::EBookFormat::Epub);
    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Пикник на обочине");
    REQUIRE(parsedBook.Metadata.AuthorsUtf8 == std::vector<std::string>({"Аркадий Стругацкий", "Борис Стругацкий"}));
    REQUIRE(parsedBook.Metadata.Language == "ru");
    REQUIRE(parsedBook.Metadata.Identifier == std::optional<std::string>{"urn:isbn:9785170903344"});
    REQUIRE(parsedBook.Metadata.DescriptionUtf8 == std::optional<std::string>{"Классика советской фантастики"});
    REQUIRE(parsedBook.CoverExtension == std::optional<std::string>{"jpg"});
    REQUIRE(parsedBook.CoverBytes == std::vector<std::byte>({std::byte{0x01}, std::byte{0x23}, std::byte{0x45}}));
}

TEST_CASE("EPUB parser rejects malformed package metadata", "[epub-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-epub-parser-invalid");
    const std::filesystem::path epubPath = sandbox.GetPath() / "invalid.epub";

    int errorCode = ZIP_ER_OK;
    zip_t* archive = zip_open(epubPath.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);
    REQUIRE(archive != nullptr);

    AddZipEntry(
        archive,
        "META-INF/container.xml",
        R"(<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>)");
    AddZipEntry(
        archive,
        "content.opf",
        R"(<?xml version="1.0" encoding="UTF-8"?>
<package version="2.0" xmlns="http://www.idpf.org/2007/opf">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:language>ru</dc:language>
  </metadata>
</package>)");

    REQUIRE(zip_close(archive) == 0);

    const Librova::EpubParsing::CEpubParser parser;
    REQUIRE_THROWS(parser.Parse(epubPath));
}

TEST_CASE("EPUB parser opens Unicode file paths on Windows", "[epub-parsing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-epub-parser-unicode");
    const std::filesystem::path epubPath = CreateSampleEpub(sandbox.GetPath() / std::filesystem::path{u8"фантастика.epub"});

    const Librova::EpubParsing::CEpubParser parser;
    const auto parsedBook = parser.Parse(epubPath);

    REQUIRE(parsedBook.Metadata.TitleUtf8 == "Пикник на обочине");
    REQUIRE(parsedBook.CoverBytes == std::vector<std::byte>({std::byte{0x01}, std::byte{0x23}, std::byte{0x45}}));
}
