#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Domain/BookId.hpp"

namespace Librova::Domain {

enum class EBookCollectionKind
{
    User,
    Preset
};

struct SBookCollection
{
    std::int64_t Id = 0;
    std::string NameUtf8;
    std::string IconKey;
    EBookCollectionKind Kind = EBookCollectionKind::User;
    bool IsDeletable = true;

    [[nodiscard]] bool operator==(const SBookCollection&) const noexcept = default;
};

class IBookCollectionQueryRepository
{
public:
    virtual ~IBookCollectionQueryRepository() = default;

    [[nodiscard]] virtual std::vector<SBookCollection> ListCollections() const = 0;
    [[nodiscard]] virtual std::vector<SBookCollection> ListCollectionsForBook(SBookId bookId) const = 0;
    [[nodiscard]] virtual std::unordered_map<std::int64_t, std::vector<SBookCollection>> ListCollectionsForBooks(
        const std::vector<SBookId>& bookIds) const = 0;
    [[nodiscard]] virtual std::optional<SBookCollection> GetCollectionById(std::int64_t collectionId) const = 0;
};

class IBookCollectionRepository
{
public:
    virtual ~IBookCollectionRepository() = default;

    [[nodiscard]] virtual SBookCollection CreateCollection(
        std::string nameUtf8,
        std::string iconKey,
        EBookCollectionKind kind = EBookCollectionKind::User,
        bool isDeletable = true) = 0;

    virtual bool DeleteCollection(std::int64_t collectionId) = 0;
    virtual bool AddBookToCollection(SBookId bookId, std::int64_t collectionId) = 0;
    virtual bool RemoveBookFromCollection(SBookId bookId, std::int64_t collectionId) = 0;
};

} // namespace Librova::Domain
