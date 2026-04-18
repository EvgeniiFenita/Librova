#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "Foundation/UnicodeConversion.hpp"

TEST_CASE("Unicode conversion converts Cyrillic paths to UTF-8", "[unicode]")
{
    const std::filesystem::path path = std::filesystem::path(u8"Objects/Автор/Книга.fb2");
    const auto expectedUtf8 = path.generic_u8string();
    const std::string expected(
        reinterpret_cast<const char*>(expectedUtf8.data()),
        expectedUtf8.size());

    REQUIRE(Librova::Unicode::PathToUtf8(path) == expected);
}

TEST_CASE("Unicode conversion restores Cyrillic paths from UTF-8", "[unicode]")
{
    const auto utf8Bytes = std::filesystem::path(u8"Objects/Автор/Книга.fb2").generic_u8string();
    const std::string utf8Path(
        reinterpret_cast<const char*>(utf8Bytes.data()),
        utf8Bytes.size());

    REQUIRE(Librova::Unicode::PathFromUtf8(utf8Path) == std::filesystem::path(u8"Objects/Автор/Книга.fb2"));
}

#ifdef _WIN32
TEST_CASE("Unicode conversion converts wide text to UTF-8 and back", "[unicode]")
{
    const std::wstring wide = L"Книга Unicode";

    const std::string utf8 = Librova::Unicode::WideToUtf8(wide);
    REQUIRE(Librova::Unicode::Utf8ToWide(utf8) == wide);
}

TEST_CASE("Unicode conversion decodes windows-1251 text into UTF-8", "[unicode]")
{
    const std::string cp1251Text("\xCA\xED\xE8\xE3\xE0");
    const auto expectedUtf8Bytes = std::filesystem::path(u8"Книга").generic_u8string();
    const std::string expectedUtf8(
        reinterpret_cast<const char*>(expectedUtf8Bytes.data()),
        expectedUtf8Bytes.size());

    REQUIRE(Librova::Unicode::CodePageToUtf8(
        cp1251Text,
        1251,
        "cp1251 decode failed") == expectedUtf8);
}

TEST_CASE("Unicode conversion restores Windows absolute UTF-8 paths", "[unicode]")
{
    const auto utf8Bytes = std::filesystem::path(u8"C:/Книги/Новая библиотека").generic_u8string();
    const std::string utf8Path(
        reinterpret_cast<const char*>(utf8Bytes.data()),
        utf8Bytes.size());

    const std::filesystem::path restored = Librova::Unicode::PathFromUtf8(utf8Path);

    REQUIRE(restored == std::filesystem::path(u8"C:/Книги/Новая библиотека"));
    REQUIRE(Librova::Unicode::PathToUtf8(restored) == utf8Path);
}

TEST_CASE("Unicode conversion restores Windows named pipe UTF-8 paths", "[unicode]")
{
    const auto utf8Bytes = std::filesystem::path(u8R"(\\.\pipe\Librova.Тест)").generic_u8string();
    const std::string utf8Path(
        reinterpret_cast<const char*>(utf8Bytes.data()),
        utf8Bytes.size());

    const std::filesystem::path restored = Librova::Unicode::PathFromUtf8(utf8Path);

    REQUIRE(Librova::Unicode::PathToUtf8(restored) == utf8Path);
}
#endif

TEST_CASE("IsValidUtf8 accepts valid ASCII", "[unicode]")
{
    REQUIRE(Librova::Unicode::IsValidUtf8("Hello, world!"));
}

TEST_CASE("IsValidUtf8 accepts valid Cyrillic UTF-8", "[unicode]")
{
    // "Привет" encoded as UTF-8
    const std::string cyrillic = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    REQUIRE(Librova::Unicode::IsValidUtf8(cyrillic));
}

TEST_CASE("IsValidUtf8 accepts empty string", "[unicode]")
{
    REQUIRE(Librova::Unicode::IsValidUtf8(""));
}

TEST_CASE("IsValidUtf8 rejects raw CP1251 Cyrillic bytes", "[unicode]")
{
    // "Привет" in CP1251 — not valid UTF-8
    const std::string cp1251 = "\xCF\xF0\xE8\xE2\xE5\xF2";
    REQUIRE_FALSE(Librova::Unicode::IsValidUtf8(cp1251));
}

TEST_CASE("IsValidUtf8 rejects truncated multi-byte sequence", "[unicode]")
{
    // First byte of a 2-byte sequence with no continuation
    const std::string truncated = "\xC3";
    REQUIRE_FALSE(Librova::Unicode::IsValidUtf8(truncated));
}


