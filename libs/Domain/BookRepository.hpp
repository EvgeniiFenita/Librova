#pragma once

#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "Domain/Book.hpp"
#include "Domain/BookId.hpp"
#include "Domain/CandidateBook.hpp"
#include "Domain/DuplicateMatch.hpp"
#include "Domain/SearchQuery.hpp"

namespace Librova::Domain {

// Thrown by IBookRepository::Add when the incoming sha256_hex already exists
// in the catalog. The caller decides whether to reject or retry via ForceAdd.
class CDuplicateHashException final : public std::runtime_error
{
public:
    explicit CDuplicateHashException(SBookId existingId)
        : std::runtime_error("Hash duplicate detected at write time")
        , m_existingId(existingId)
    {
    }

    [[nodiscard]] SBookId ExistingBookId() const noexcept { return m_existingId; }

private:
    SBookId m_existingId;
};

class IBookRepository
{
public:
    virtual ~IBookRepository() = default;

    virtual SBookId ReserveId() = 0;

    // Reserves a contiguous range of ids for callers that need to plan work
    // off-thread before the final Add()/ForceAdd() call.
    virtual std::vector<SBookId> ReserveIds(std::size_t count);

    // Inserts the book. Throws CDuplicateHashException if sha256_hex already
    // exists in the catalog (non-empty hash only). Use when duplicates must
    // be rejected or detected late (e.g. concurrent import race).
    virtual SBookId Add(const SBook& book) = 0;

    // Inserts the book without checking for hash duplicates. Use when the
    // caller has already decided to allow the duplicate explicitly.
    virtual SBookId ForceAdd(const SBook& book) = 0;

    virtual std::optional<SBook> GetById(SBookId id) const = 0;
    virtual void Remove(SBookId id) = 0;
    virtual void Compact() {}

    // --- Batch write API ---

    enum class EBatchAddStatus { Imported, RejectedDuplicate, Failed };

    struct SBatchBookEntry
    {
        SBook Book;
        bool ForceAdd = false;
    };

    struct SBatchBookResult
    {
        EBatchAddStatus        Status = EBatchAddStatus::Failed;
        std::optional<SBookId> BookId;
        std::optional<SBookId> ConflictingBookId; // set when Status == RejectedDuplicate
        std::string            Error;
    };

    // Inserts multiple books in a single transaction.
    // Each entry is processed independently: a duplicate or failure on one
    // item does not roll back the others.
    // Default implementation falls back to sequential Add()/ForceAdd() calls —
    // override in CSqliteBookRepository for true single-transaction batching.
    virtual std::vector<SBatchBookResult> AddBatch(std::span<const SBatchBookEntry> entries);
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
    [[nodiscard]] virtual std::vector<std::string> ListAvailableTags(const SSearchQuery& query) const = 0;
    [[nodiscard]] virtual std::vector<std::string> ListAvailableGenres(const SSearchQuery& query) const = 0;
    virtual std::vector<SDuplicateMatch> FindDuplicates(const SCandidateBook& candidate) const = 0;
    [[nodiscard]] virtual SLibraryStatistics GetLibraryStatistics() const = 0;
};

} // namespace Librova::Domain
