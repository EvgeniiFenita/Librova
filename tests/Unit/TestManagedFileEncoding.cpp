#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "ManagedFileEncoding/ManagedFileEncoding.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
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
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_CASE("Managed file encoding round-trips compressed files", "[managed-file-encoding]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-file-encoding-roundtrip");
    const auto sourcePath = sandbox.GetPath() / "source.fb2";
    const auto encodedPath = sandbox.GetPath() / "encoded.bin";
    const auto decodedPath = sandbox.GetPath() / "decoded.fb2";

    WriteTextFile(
        sourcePath,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><FictionBook><description><title-info>"
        "<book-title>Пикник на обочине</book-title></title-info></description><body>"
        "<section><p>Классика советской фантастики. Классика советской фантастики.</p></section>"
        "</body></FictionBook>");

    Librova::ManagedFileEncoding::EncodeFileToPath(
        sourcePath,
        Librova::Domain::EStorageEncoding::Compressed,
        encodedPath);
    Librova::ManagedFileEncoding::DecodeFileToPath(
        encodedPath,
        Librova::Domain::EStorageEncoding::Compressed,
        decodedPath);

    REQUIRE(std::filesystem::file_size(encodedPath) > 0);
    REQUIRE(ReadTextFile(decodedPath) == ReadTextFile(sourcePath));
}

TEST_CASE("Managed file encoding rejects invalid compressed content", "[managed-file-encoding]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-file-encoding-invalid");
    const auto sourcePath = sandbox.GetPath() / "broken.bin";
    const auto decodedPath = sandbox.GetPath() / "decoded.fb2";

    WriteTextFile(sourcePath, "not-a-gzip-stream");

    REQUIRE_THROWS_WITH(
        Librova::ManagedFileEncoding::DecodeFileToPath(
            sourcePath,
            Librova::Domain::EStorageEncoding::Compressed,
            decodedPath),
        Catch::Matchers::ContainsSubstring("decompression failed"));
    REQUIRE_FALSE(std::filesystem::exists(decodedPath));
}

TEST_CASE("Managed file encoding leaves same-path plain copies unchanged", "[managed-file-encoding]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-file-encoding-same-path");
    const auto sourcePath = sandbox.GetPath() / "book.epub";

    WriteTextFile(sourcePath, "plain-contents");

    Librova::ManagedFileEncoding::EncodeFileToPath(
        sourcePath,
        Librova::Domain::EStorageEncoding::Plain,
        sourcePath);

    REQUIRE(ReadTextFile(sourcePath) == "plain-contents");
}

TEST_CASE("Managed file encoding supports Unicode destination paths", "[managed-file-encoding]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-file-encoding-unicode");
    const auto sourcePath = sandbox.GetPath() / "source.fb2";
    const auto encodedPath = sandbox.GetPath() / u8"Книги" / u8"Страуд Д._Крадущаяся тень.fb2";
    const auto decodedPath = sandbox.GetPath() / u8"Выход" / u8"Страуд Д._Крадущаяся тень.fb2";

    WriteTextFile(sourcePath, "unicode-path-contents");
    std::filesystem::create_directories(encodedPath.parent_path());
    std::filesystem::create_directories(decodedPath.parent_path());

    Librova::ManagedFileEncoding::EncodeFileToPath(
        sourcePath,
        Librova::Domain::EStorageEncoding::Compressed,
        encodedPath);
    Librova::ManagedFileEncoding::DecodeFileToPath(
        encodedPath,
        Librova::Domain::EStorageEncoding::Compressed,
        decodedPath);

    REQUIRE(ReadTextFile(decodedPath) == "unicode-path-contents");
}
