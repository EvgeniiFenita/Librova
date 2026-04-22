#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Librova::Unicode {

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view path);
[[nodiscard]] std::string CodePageToUtf8(std::string_view value, unsigned int codePage, std::string_view errorContext);
[[nodiscard]] bool IsValidUtf8(std::string_view value) noexcept;

#ifdef _WIN32
[[nodiscard]] std::string WideToUtf8(std::wstring_view value);
[[nodiscard]] std::wstring Utf8ToWide(std::string_view value);
[[nodiscard]] std::string Utf16LeToUtf8(const void* data, std::size_t byteCount);
[[nodiscard]] std::string Utf16BeToUtf8(const void* data, std::size_t byteCount);
#endif

} // namespace Librova::Unicode
