#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Domain/BookFormat.hpp"

namespace Librova::Domain {

enum class ESortDirection
{
    Ascending,
    Descending
};

enum class EBookSort
{
    Title,
    Author,
    DateAdded
};

struct SSearchQuery
{
    std::string TextUtf8;
    std::optional<std::string> AuthorUtf8;
    std::optional<std::string> Language;
    std::optional<std::string> SeriesUtf8;
    std::vector<std::string> TagsUtf8;
    std::optional<EBookFormat> Format;
    std::optional<EBookSort> SortBy;
    std::optional<ESortDirection> SortDirection;
    std::size_t Offset = 0;
    std::size_t Limit = 50;

    [[nodiscard]] bool HasText() const noexcept
    {
        return !TextUtf8.empty();
    }

    [[nodiscard]] bool HasStructuredFilters() const noexcept
    {
        return AuthorUtf8.has_value()
            || Language.has_value()
            || SeriesUtf8.has_value()
            || !TagsUtf8.empty()
            || Format.has_value();
    }
};

} // namespace Librova::Domain
