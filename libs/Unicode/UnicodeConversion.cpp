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
