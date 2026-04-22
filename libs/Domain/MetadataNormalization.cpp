#include "Domain/MetadataNormalization.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ranges>
#include <vector>

namespace Librova::Domain {
namespace {

bool IsWhitespaceByte(const unsigned char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

[[nodiscard]] bool TryDecodeUtf8CodePoint(
    const std::string_view value,
    const std::size_t index,
    char32_t& codePoint,
    std::size_t& sequenceLength) noexcept
{
    const unsigned char firstByte = static_cast<unsigned char>(value[index]);

    if (firstByte < 0x80)
    {
        codePoint = firstByte;
        sequenceLength = 1;
        return true;
    }

    if ((firstByte & 0xE0) == 0xC0)
    {
        sequenceLength = 2;
        if (index + sequenceLength > value.size())
        {
            return false;
        }

        const unsigned char secondByte = static_cast<unsigned char>(value[index + 1]);
        if ((secondByte & 0xC0) != 0x80)
        {
            return false;
        }

        codePoint = (static_cast<char32_t>(firstByte & 0x1F) << 6)
            | static_cast<char32_t>(secondByte & 0x3F);
        return true;
    }

    if ((firstByte & 0xF0) == 0xE0)
    {
        sequenceLength = 3;
        if (index + sequenceLength > value.size())
        {
            return false;
        }

        const unsigned char secondByte = static_cast<unsigned char>(value[index + 1]);
        const unsigned char thirdByte = static_cast<unsigned char>(value[index + 2]);
        if ((secondByte & 0xC0) != 0x80 || (thirdByte & 0xC0) != 0x80)
        {
            return false;
        }

        codePoint = (static_cast<char32_t>(firstByte & 0x0F) << 12)
            | (static_cast<char32_t>(secondByte & 0x3F) << 6)
            | static_cast<char32_t>(thirdByte & 0x3F);
        return true;
    }

    if ((firstByte & 0xF8) == 0xF0)
    {
        sequenceLength = 4;
        if (index + sequenceLength > value.size())
        {
            return false;
        }

        const unsigned char secondByte = static_cast<unsigned char>(value[index + 1]);
        const unsigned char thirdByte = static_cast<unsigned char>(value[index + 2]);
        const unsigned char fourthByte = static_cast<unsigned char>(value[index + 3]);
        if ((secondByte & 0xC0) != 0x80 || (thirdByte & 0xC0) != 0x80 || (fourthByte & 0xC0) != 0x80)
        {
            return false;
        }

        codePoint = (static_cast<char32_t>(firstByte & 0x07) << 18)
            | (static_cast<char32_t>(secondByte & 0x3F) << 12)
            | (static_cast<char32_t>(thirdByte & 0x3F) << 6)
            | static_cast<char32_t>(fourthByte & 0x3F);
        return true;
    }

    return false;
}

void AppendUtf8CodePoint(const char32_t codePoint, std::string& result)
{
    if (codePoint <= 0x7F)
    {
        result.push_back(static_cast<char>(codePoint));
        return;
    }

    if (codePoint <= 0x7FF)
    {
        result.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }

    if (codePoint <= 0xFFFF)
    {
        result.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }

    result.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
    result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
}

[[nodiscard]] char32_t NormalizeLowercaseCodePoint(const char32_t codePoint) noexcept
{
    if (codePoint <= 0x7F)
    {
        return static_cast<char32_t>(std::tolower(static_cast<unsigned char>(codePoint)));
    }

    if (codePoint == U'\u0401' || codePoint == U'\u0451')
    {
        return U'\u0435';
    }

    if (codePoint >= U'\u0410' && codePoint <= U'\u042F')
    {
        return codePoint + 0x20;
    }

    if (codePoint >= U'\u0400' && codePoint <= U'\u040F')
    {
        switch (codePoint)
        {
        case U'\u0400':
            return U'\u0450';
        case U'\u0402':
            return U'\u0452';
        case U'\u0403':
            return U'\u0453';
        case U'\u0404':
            return U'\u0454';
        case U'\u0405':
            return U'\u0455';
        case U'\u0406':
            return U'\u0456';
        case U'\u0407':
            return U'\u0457';
        case U'\u0408':
            return U'\u0458';
        case U'\u0409':
            return U'\u0459';
        case U'\u040A':
            return U'\u045A';
        case U'\u040B':
            return U'\u045B';
        case U'\u040C':
            return U'\u045C';
        case U'\u040D':
            return U'\u045D';
        case U'\u040E':
            return U'\u045E';
        case U'\u040F':
            return U'\u045F';
        default:
            return codePoint;
        }
    }

    return codePoint;
}

void AppendLowercasedNormalizedUtf8(const std::string_view value, std::string& result)
{
    for (std::size_t index = 0; index < value.size();)
    {
        char32_t codePoint = 0;
        std::size_t sequenceLength = 0;

        if (!TryDecodeUtf8CodePoint(value, index, codePoint, sequenceLength))
        {
            result.push_back(value[index]);
            ++index;
            continue;
        }

        AppendUtf8CodePoint(NormalizeLowercaseCodePoint(codePoint), result);
        index += sequenceLength;
    }
}

} // namespace

std::string NormalizeText(const std::string_view value)
{
    std::string trimmed;
    trimmed.reserve(value.size());

    bool previousWasWhitespace = true;

    for (const unsigned char currentByte : value)
    {
        if (IsWhitespaceByte(currentByte))
        {
            if (!previousWasWhitespace)
            {
                trimmed.push_back(' ');
                previousWasWhitespace = true;
            }

            continue;
        }

        trimmed.push_back(static_cast<char>(currentByte));
        previousWasWhitespace = false;
    }

    if (!trimmed.empty() && trimmed.back() == ' ')
    {
        trimmed.pop_back();
    }

    std::string normalized;
    normalized.reserve(trimmed.size());
    AppendLowercasedNormalizedUtf8(trimmed, normalized);
    return normalized;
}

std::optional<std::string> NormalizeOptionalText(const std::optional<std::string>& value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const std::string normalized = NormalizeText(*value);

    if (normalized.empty())
    {
        return std::nullopt;
    }

    return normalized;
}

std::optional<std::string> NormalizeIsbn(const std::optional<std::string>& value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    std::string normalized;
    normalized.reserve(value->size());

    for (const unsigned char currentByte : *value)
    {
        if (std::isdigit(currentByte))
        {
            normalized.push_back(static_cast<char>(currentByte));
            continue;
        }

        if (currentByte == 'x' || currentByte == 'X')
        {
            normalized.push_back('X');
        }
    }

    if (normalized.size() == 10)
    {
        for (std::size_t index = 0; index < normalized.size(); ++index)
        {
            if (index == 9 && normalized[index] == 'X')
            {
                return normalized;
            }

            if (!std::isdigit(static_cast<unsigned char>(normalized[index])))
            {
                return std::nullopt;
            }
        }

        return normalized;
    }

    if (normalized.size() == 13)
    {
        if (std::ranges::all_of(normalized, [](const char valueToCheck) {
                return std::isdigit(static_cast<unsigned char>(valueToCheck)) != 0;
            }))
        {
            return normalized;
        }
    }

    return std::nullopt;
}

std::string BuildDuplicateKey(const SBookMetadata& metadata)
{
    std::vector<std::string> normalizedAuthors;
    normalizedAuthors.reserve(metadata.AuthorsUtf8.size());

    for (const std::string& author : metadata.AuthorsUtf8)
    {
        const std::string normalizedAuthor = NormalizeText(author);

        if (!normalizedAuthor.empty())
        {
            normalizedAuthors.push_back(normalizedAuthor);
        }
    }

    std::sort(normalizedAuthors.begin(), normalizedAuthors.end());

    std::string key = NormalizeText(metadata.TitleUtf8);
    key.append("||");

    for (std::size_t index = 0; index < normalizedAuthors.size(); ++index)
    {
        if (index > 0)
        {
            key.push_back('|');
        }

        key.append(normalizedAuthors[index]);
    }

    return key;
}

} // namespace Librova::Domain
