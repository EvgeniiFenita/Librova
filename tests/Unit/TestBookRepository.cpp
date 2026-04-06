#include <catch2/catch_test_macros.hpp>

#include "Domain/BookRepository.hpp"

namespace {

class CInMemoryBookRepository final : public Librova::Domain::IBookRepository
{
public:
    Librova::Domain::SBookId ReserveId() override
    {
        return {1};
    }

    Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        m_book = book;
        m_book.Id = {1};
        return m_book.Id;
    }

    Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        return Add(book);
    }

    std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId id) const override
    {
        if (id.Value == m_book.Id.Value)
        {
            return m_book;
        }

        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId id) override
    {
        if (id.Value == m_book.Id.Value)
        {
            m_book = {};
        }
    }

private:
    Librova::Domain::SBook m_book;
};

class CInMemoryBookQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery& query) const override
    {
        if (query.HasText() || query.HasStructuredFilters())
        {
            return {m_book};
        }

        return {};
    }

    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery& query) const override
    {
        return static_cast<std::uint64_t>(Search(query).size());
    }

    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery&) const override
    {
        return {"en"};
    }

    std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook& candidate) const override
    {
        if (candidate.HasHash())
        {
            return {{
                .Severity = Librova::Domain::EDuplicateSeverity::Strict,
                .Reason = Librova::Domain::EDuplicateReason::SameHash,
                .ExistingBookId = {1}
            }};
        }

        return {};
    }

    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override
    {
        return {
            .BookCount = 1,
            .TotalManagedBookSizeBytes = 4096,
            .TotalLibrarySizeBytes = 4096
        };
    }

    Librova::Domain::SBook m_book{
        .Id = {1},
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .File = {.Format = Librova::Domain::EBookFormat::Epub}
    };
};

} // namespace

TEST_CASE("Book repository port supports aggregate roundtrip through fake implementation", "[domain][ports]")
{
    CInMemoryBookRepository repository;
    Librova::Domain::SBook book{
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .File = {.Format = Librova::Domain::EBookFormat::Epub}
    };

    const Librova::Domain::SBookId addedId = repository.Add(book);

    REQUIRE(addedId.IsValid());
    REQUIRE(repository.GetById(addedId).has_value());

    repository.Remove(addedId);

    REQUIRE_FALSE(repository.GetById(addedId).has_value());
}

TEST_CASE("Book query repository port supports search and duplicate lookup through fake implementation", "[domain][ports]")
{
    const CInMemoryBookQueryRepository repository;

    const Librova::Domain::SSearchQuery query{
        .TextUtf8 = "Roadside"
    };

    const Librova::Domain::SCandidateBook candidate{
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .Format = Librova::Domain::EBookFormat::Epub,
        .Sha256Hex = "deadbeef"
    };

    const auto books = repository.Search(query);
    const auto duplicates = repository.FindDuplicates(candidate);

    REQUIRE(books.size() == 1);
    REQUIRE(duplicates.size() == 1);
    REQUIRE(duplicates.front().IsStrictRejection());
}
