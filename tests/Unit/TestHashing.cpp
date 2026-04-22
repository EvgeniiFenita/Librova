#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "Foundation/Sha256.hpp"

namespace {

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

class CScopedTempFile final
{
public:
    explicit CScopedTempFile(const std::filesystem::path& path)
        : m_path(path)
    {
    }

    ~CScopedTempFile()
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    CScopedTempFile(const CScopedTempFile&) = delete;
    CScopedTempFile& operator=(const CScopedTempFile&) = delete;

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

} // namespace

TEST_CASE("SHA-256 of known content produces correct hex digest", "[hashing]")
{
    const auto path = MakeUniqueTestPath(L"librova-hash-test-hello.bin");
    CScopedTempFile tempFile(path);
    WriteTextFile(path, "hello");

    // SHA-256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
    const std::string expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    REQUIRE(Librova::Hashing::ComputeFileSha256Hex(path) == expected);
}

TEST_CASE("SHA-256 of empty file produces correct hex digest", "[hashing]")
{
    const auto path = MakeUniqueTestPath(L"librova-hash-test-empty.bin");
    CScopedTempFile tempFile(path);
    WriteTextFile(path, "");

    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    REQUIRE(Librova::Hashing::ComputeFileSha256Hex(path) == expected);
}

TEST_CASE("SHA-256 hex digest is 64 lowercase hex characters", "[hashing]")
{
    const auto path = MakeUniqueTestPath(L"librova-hash-test-len.bin");
    CScopedTempFile tempFile(path);
    WriteTextFile(path, "test content for length check");

    const std::string result = Librova::Hashing::ComputeFileSha256Hex(path);
    REQUIRE(result.size() == 64);
    for (const char ch : result)
    {
        REQUIRE(((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')));
    }
}

TEST_CASE("SHA-256 throws for non-existent file", "[hashing]")
{
    const auto path = MakeUniqueTestPath(L"librova-hash-test-missing-xyz.bin");
    REQUIRE_THROWS_AS(Librova::Hashing::ComputeFileSha256Hex(path), std::runtime_error);
}

