#include <catch2/catch_test_macros.hpp>

#include "Domain/BookRepository.hpp"

namespace {

class CInMemoryBookRepository final : public LibriFlow::Domain::IBookRepository
{
public:
    LibriFlow::Domain::SBookId Add(const LibriFlow::Domain::SBook& book) override
    {
        m_book = book;
        m_book.Id = {1};
        return m_book.Id;
    }

    std::optional<LibriFlow::Domain::SBook> GetById(const LibriFlow::Domain::SBookId id) const override
    {
        if (id.Value == m_book.Id.Value)
        {
            return m_book;
        }

        return std::nullopt;
    }

    void Remove(const LibriFlow::Domain::SBookId id) override
    {
        if (id.Value == m_book.Id.Value)
        {
            m_book = {};
        }
    }

private:
    LibriFlow::Domain::SBook m_book;
};

class CInMemoryBookQueryRepository final : public LibriFlow::Domain::IBookQueryRepository
{
public:
    std::vector<LibriFlow::Domain::SBook> Search(const LibriFlow::Domain::SSearchQuery& query) const override
    {
        if (query.HasText() || query.HasStructuredFilters())
        {
            return {m_book};
        }

        return {};
    }

    std::vector<LibriFlow::Domain::SDuplicateMatch> FindDuplicates(const LibriFlow::Domain::SCandidateBook& candidate) const override
    {
        if (candidate.HasHash())
        {
            return {{
                .Severity = LibriFlow::Domain::EDuplicateSeverity::Strict,
                .Reason = LibriFlow::Domain::EDuplicateReason::SameHash,
                .ExistingBookId = {1}
            }};
        }

        return {};
    }

    LibriFlow::Domain::SBook m_book{
        .Id = {1},
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .File = {.Format = LibriFlow::Domain::EBookFormat::Epub}
    };
};

} // namespace

TEST_CASE("Book repository port supports aggregate roundtrip through fake implementation", "[domain][ports]")
{
    CInMemoryBookRepository repository;
    LibriFlow::Domain::SBook book{
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .File = {.Format = LibriFlow::Domain::EBookFormat::Epub}
    };

    const LibriFlow::Domain::SBookId addedId = repository.Add(book);

    REQUIRE(addedId.IsValid());
    REQUIRE(repository.GetById(addedId).has_value());

    repository.Remove(addedId);

    REQUIRE_FALSE(repository.GetById(addedId).has_value());
}

TEST_CASE("Book query repository port supports search and duplicate lookup through fake implementation", "[domain][ports]")
{
    const CInMemoryBookQueryRepository repository;

    const LibriFlow::Domain::SSearchQuery query{
        .TextUtf8 = "Roadside"
    };

    const LibriFlow::Domain::SCandidateBook candidate{
        .Metadata = {.TitleUtf8 = "Roadside Picnic", .AuthorsUtf8 = {"Arkady Strugatsky"}},
        .Format = LibriFlow::Domain::EBookFormat::Epub,
        .Sha256Hex = "deadbeef"
    };

    const auto books = repository.Search(query);
    const auto duplicates = repository.FindDuplicates(candidate);

    REQUIRE(books.size() == 1);
    REQUIRE(duplicates.size() == 1);
    REQUIRE(duplicates.front().IsStrictRejection());
}
