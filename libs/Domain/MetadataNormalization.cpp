#include "Domain/MetadataNormalization.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace LibriFlow::Domain {
namespace {

bool IsWhitespaceByte(const unsigned char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

void AppendLowercasedNormalizedUtf8(const std::string_view value, std::string& result)
{
    for (std::size_t index = 0; index < value.size();)
    {
        const unsigned char firstByte = static_cast<unsigned char>(value[index]);

        if (firstByte < 0x80)
        {
            result.push_back(static_cast<char>(std::tolower(firstByte)));
            ++index;
            continue;
        }

        if (index + 1 < value.size())
        {
            const unsigned char secondByte = static_cast<unsigned char>(value[index + 1]);

            if (firstByte == 0xD0)
            {
                if (secondByte == 0x81)
                {
                    result.push_back(static_cast<char>(0xD0));
                    result.push_back(static_cast<char>(0xB5));
                    index += 2;
                    continue;
                }

                if (secondByte >= 0x90 && secondByte <= 0x9F)
                {
                    result.push_back(static_cast<char>(0xD0));
                    result.push_back(static_cast<char>(secondByte + 0x20));
                    index += 2;
                    continue;
                }

                if (secondByte >= 0xA0 && secondByte <= 0xAF)
                {
                    result.push_back(static_cast<char>(0xD1));
                    result.push_back(static_cast<char>(secondByte - 0x20));
                    index += 2;
                    continue;
                }
            }

            if (firstByte == 0xD1 && secondByte == 0x91)
            {
                result.push_back(static_cast<char>(0xD0));
                result.push_back(static_cast<char>(0xB5));
                index += 2;
                continue;
            }

            result.push_back(static_cast<char>(firstByte));
            result.push_back(static_cast<char>(secondByte));
            index += 2;
            continue;
        }

        result.push_back(static_cast<char>(firstByte));
        ++index;
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
        if (std::all_of(normalized.begin(), normalized.end(), [](const char valueToCheck) {
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

} // namespace LibriFlow::Domain
