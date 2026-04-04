#pragma once

#include <optional>
#include <vector>

#include "Domain/Book.hpp"
#include "Domain/CandidateBook.hpp"
#include "Domain/DuplicateMatch.hpp"
#include "Domain/SearchQuery.hpp"

namespace Librova::Domain {

class IBookRepository
{
public:
    virtual ~IBookRepository() = default;

    virtual SBookId ReserveId() = 0;
    virtual SBookId Add(const SBook& book) = 0;
    virtual std::optional<SBook> GetById(SBookId id) const = 0;
    virtual void Remove(SBookId id) = 0;
};

class IBookQueryRepository
{
public:
    virtual ~IBookQueryRepository() = default;

    struct SLibraryStatistics
    {
        std::uint64_t BookCount = 0;
        std::uint64_t TotalManagedBookSizeBytes = 0;
        std::uint64_t TotalLibrarySizeBytes = 0;
    };

    virtual std::vector<SBook> Search(const SSearchQuery& query) const = 0;
    [[nodiscard]] virtual std::uint64_t CountSearchResults(const SSearchQuery& query) const = 0;
    [[nodiscard]] virtual std::vector<std::string> ListAvailableLanguages(const SSearchQuery& query) const = 0;
    virtual std::vector<SDuplicateMatch> FindDuplicates(const SCandidateBook& candidate) const = 0;
    [[nodiscard]] virtual SLibraryStatistics GetLibraryStatistics() const = 0;
};

} // namespace Librova::Domain
