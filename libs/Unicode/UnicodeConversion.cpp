#include "Unicode/UnicodeConversion.hpp"

#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Librova::Unicode {

std::string PathToUtf8(const std::filesystem::path& path)
{
    const auto utf8Path = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
}

std::filesystem::path PathFromUtf8(const std::string_view path)
{
#ifdef _WIN32
    return std::filesystem::path{Utf8ToWide(path)};
#else
    const auto utf8Path = std::u8string{
        reinterpret_cast<const char8_t*>(path.data()),
        reinterpret_cast<const char8_t*>(path.data()) + path.size()
    };
    return std::filesystem::path{utf8Path};
#endif
}

std::string CodePageToUtf8(const std::string_view value, const unsigned int codePage, const std::string_view errorContext)
{
    if (value.empty())
    {
        return {};
    }

#ifdef _WIN32
    const int wideLength = ::MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (wideLength <= 0)
    {
        throw std::runtime_error(std::string{errorContext});
    }

    std::wstring wideValue(static_cast<std::size_t>(wideLength), L'\0');
    if (::MultiByteToWideChar(
            codePage,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            wideValue.data(),
            wideLength)
        <= 0)
    {
        throw std::runtime_error(std::string{errorContext});
    }

    return WideToUtf8(wideValue);
#else
    (void)codePage;
    throw std::runtime_error(std::string{errorContext});
#endif
}

bool IsValidUtf8(const std::string_view value) noexcept
{
    // RFC 3629 state machine — validates full Unicode range while rejecting
    // overlong encodings and surrogate pairs (D800–DFFF).
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    const std::size_t len = value.size();
    std::size_t i = 0;

    while (i < len)
    {
        const unsigned char b0 = bytes[i];

        if (b0 <= 0x7F)
        {
            ++i;
        }
        else if (b0 >= 0xC2 && b0 <= 0xDF && i + 1 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0xBF)
        {
            i += 2;
        }
        else if (b0 == 0xE0 && i + 2 < len
            && bytes[i + 1] >= 0xA0 && bytes[i + 1] <= 0xBF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF)
        {
            i += 3;
        }
        else if (b0 >= 0xE1 && b0 <= 0xEC && i + 2 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0xBF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF)
        {
            i += 3;
        }
        else if (b0 == 0xED && i + 2 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0x9F  // excludes surrogates D800–DFFF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF)
        {
            i += 3;
        }
        else if ((b0 == 0xEE || b0 == 0xEF) && i + 2 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0xBF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF)
        {
            i += 3;
        }
        else if (b0 == 0xF0 && i + 3 < len
            && bytes[i + 1] >= 0x90 && bytes[i + 1] <= 0xBF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF
            && bytes[i + 3] >= 0x80 && bytes[i + 3] <= 0xBF)
        {
            i += 4;
        }
        else if (b0 >= 0xF1 && b0 <= 0xF3 && i + 3 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0xBF
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF
            && bytes[i + 3] >= 0x80 && bytes[i + 3] <= 0xBF)
        {
            i += 4;
        }
        else if (b0 == 0xF4 && i + 3 < len
            && bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0x8F
            && bytes[i + 2] >= 0x80 && bytes[i + 2] <= 0xBF
            && bytes[i + 3] >= 0x80 && bytes[i + 3] <= 0xBF)
        {
            i += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}

#ifdef _WIN32
std::string WideToUtf8(const std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int length = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (length <= 0)
    {
        throw std::runtime_error("Failed to convert UTF-16 text to UTF-8.");
    }

    std::string utf8Value(static_cast<std::size_t>(length), '\0');
    if (::WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
        utf8Value.data(),
        length,
        nullptr,
        nullptr)
        <= 0)
    {
        throw std::runtime_error("Failed to convert UTF-16 text to UTF-8.");
    }

    return utf8Value;
}

std::wstring Utf8ToWide(const std::string_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int length = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (length <= 0)
    {
        throw std::runtime_error("Failed to convert UTF-8 text to UTF-16.");
    }

    std::wstring wideValue(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        wideValue.data(),
        length)
        <= 0)
    {
        throw std::runtime_error("Failed to convert UTF-8 text to UTF-16.");
    }

    return wideValue;
}
#endif

} // namespace Librova::Unicode
