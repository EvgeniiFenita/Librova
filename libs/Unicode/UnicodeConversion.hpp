#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Librova::Unicode {

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view path);
[[nodiscard]] std::string CodePageToUtf8(std::string_view value, unsigned int codePage, std::string_view errorContext);

#ifdef _WIN32
[[nodiscard]] std::string WideToUtf8(std::wstring_view value);
[[nodiscard]] std::wstring Utf8ToWide(std::string_view value);
#endif

} // namespace Librova::Unicode
