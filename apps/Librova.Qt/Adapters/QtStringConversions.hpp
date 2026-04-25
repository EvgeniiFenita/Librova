#pragma once

#include <filesystem>
#include <string>

#include <QString>

#include "Foundation/UnicodeConversion.hpp"

namespace LibrovaQt {

// Conversion helpers between Qt string types and the UTF-8 / std::filesystem
// types used throughout the backend. All inline — no .cpp required.

[[nodiscard]] inline QString qtFromUtf8(const std::string& s)
{
    return QString::fromUtf8(s.c_str(), static_cast<qsizetype>(s.size()));
}

[[nodiscard]] inline QString qtFromPath(const std::filesystem::path& p)
{
    const std::string utf8 = Librova::Unicode::PathToUtf8(p);
    return QString::fromUtf8(utf8.c_str(), static_cast<qsizetype>(utf8.size()));
}

[[nodiscard]] inline std::string qtToUtf8(const QString& q)
{
    const QByteArray ba = q.toUtf8();
    return std::string(ba.constData(), static_cast<std::size_t>(ba.size()));
}

[[nodiscard]] inline std::filesystem::path qtToPath(const QString& q)
{
    return Librova::Unicode::PathFromUtf8(qtToUtf8(q));
}

} // namespace LibrovaQt
