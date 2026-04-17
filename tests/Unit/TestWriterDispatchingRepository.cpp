#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Importing/WriterDispatchingRepository.hpp"

namespace {

// Mock repository whose AddBatch() returns per-book results driven by configurable
// hash strings. All writes are serialised through the writer actor so AddBatch is
// always called from a single thread — no internal locking needed.
class CBatchConfigurableRepository final : public Librova::Domain::IBookRepository
{
public:
    Librova::Domain::SBookId ReserveId() override
    {
        return {++m_nextId};
    }

    Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        return ForceAdd(book);
    }

    Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook&) override
    {
        return {++m_nextId};
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook>
        GetById(const Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override {}

    std::vector<SBatchBookResult> AddBatch(std::span<const SBatchBookEntry> entries) override
    {
        ++BatchCallCount;

        if (ThrowOnBatch)
        {
            throw std::runtime_error(BatchErrorMessage);
        }

        std::vector<SBatchBookResult> results;
        results.reserve(entries.size());

        for (const auto& entry : entries)
        {
            const auto& hash = entry.Book.File.Sha256Hex;

            if (!hash.empty() && hash == DuplicateHash && !entry.ForceAdd)
            {
                results.push_back({
                    .Status            = EBatchAddStatus::RejectedDuplicate,
                    .ConflictingBookId = Librova::Domain::SBookId{99}
                });
            }
            else if (!hash.empty() && hash == FailHash)
            {
                results.push_back({
                    .Status = EBatchAddStatus::Failed,
                    .Error  = "Injected batch failure."
                });
            }
            else
            {
                results.push_back({
                    .Status = EBatchAddStatus::Imported,
                    .BookId = Librova::Domain::SBookId{++m_nextId}
                });
            }
        }

        if (WrongResultCount.has_value() && *WrongResultCount < results.size())
        {
            results.resize(*WrongResultCount);
        }

        return results;
    }

    std::atomic<int> BatchCallCount = 0;

    bool        ThrowOnBatch      = false;
    std::string BatchErrorMessage = "Batch threw.";
    std::string DuplicateHash;
    std::string FailHash;
    std::optional<std::size_t> WrongResultCount;

private:
    std::atomic<std::int64_t> m_nextId = 0;
};

Librova::Domain::SBook MakeBook(std::string sha256Hex = "")
{
    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8    = "Test Book";
    book.Metadata.AuthorsUtf8  = {"Test Author"};
    book.Metadata.Language     = "en";
    book.File.Format           = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath      = "Objects/5a/68/0000000001.book.epub";
    book.File.SizeBytes        = 1024;
    book.File.Sha256Hex        = std::move(sha256Hex);
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    return book;
}

} // namespace

TEST_CASE("Writer dispatching repository forwards Add and returns valid book id", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    const auto bookId = dispatcher.Add(MakeBook("add-hash"));
    dispatcher.Drain();

    REQUIRE(bookId.IsValid());
    REQUIRE(real.BatchCallCount >= 1);
}

TEST_CASE("Writer dispatching repository propagates CDuplicateHashException from Add", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.DuplicateHash = "dup-hash";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    REQUIRE_THROWS_AS(
        dispatcher.Add(MakeBook("dup-hash")),
        Librova::Domain::CDuplicateHashException);

    dispatcher.Drain();
}

TEST_CASE("Writer dispatching repository propagates failure message from Add", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.FailHash = "fail-hash";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    bool threw = false;
    std::string thrownMessage;
    try
    {
        dispatcher.Add(MakeBook("fail-hash"));
    }
    catch (const std::runtime_error& ex)
    {
        threw         = true;
        thrownMessage = ex.what();
    }

    dispatcher.Drain();

    REQUIRE(threw);
    REQUIRE(thrownMessage == "Injected batch failure.");
}

TEST_CASE("Writer dispatching repository ForceAdd succeeds even when hash matches configured duplicate", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.DuplicateHash = "force-hash";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    // ForceAdd bypasses duplicate check — mock returns Imported.
    const auto bookId = dispatcher.ForceAdd(MakeBook("force-hash"));
    dispatcher.Drain();

    REQUIRE(bookId.IsValid());
}

TEST_CASE("Writer dispatching repository resolves all concurrent Add calls correctly", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    constexpr int kThreadCount = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successCount = 0;
    threads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i)
    {
        threads.emplace_back([&, i]
        {
            try
            {
                const auto id = dispatcher.Add(MakeBook("hash-" + std::to_string(i)));
                if (id.IsValid())
                {
                    ++successCount;
                }
            }
            catch (...) {}
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    dispatcher.Drain();

    REQUIRE(successCount == kThreadCount);
}

TEST_CASE("Writer dispatching repository resolves concurrent Add and ForceAdd calls for the same hash", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.DuplicateHash = "shared-hash";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    std::atomic<bool> addRejected = false;
    std::atomic<bool> forceAddSucceeded = false;

    std::thread addThread([&]
    {
        try
        {
            (void)dispatcher.Add(MakeBook("shared-hash"));
        }
        catch (const Librova::Domain::CDuplicateHashException&)
        {
            addRejected = true;
        }
    });
    std::thread forceAddThread([&]
    {
        try
        {
            const auto id = dispatcher.ForceAdd(MakeBook("shared-hash"));
            forceAddSucceeded = id.IsValid();
        }
        catch (...)
        {
        }
    });

    addThread.join();
    forceAddThread.join();
    dispatcher.Drain();

    REQUIRE(addRejected.load());
    REQUIRE(forceAddSucceeded.load());
}

// Both threads submit distinct books. ThrowOnBatch = true makes every batch flush throw,
// so both callers receive an exception regardless of whether they land in the same batch.
// We do not require same-batch placement — exception propagation to all concurrent callers
// is sufficient.
TEST_CASE("Writer dispatching repository propagates exception to every concurrent Add when repository always throws", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.ThrowOnBatch      = true;
    real.BatchErrorMessage = "Catastrophic failure";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    bool threw1 = false;
    bool threw2 = false;

    std::thread t1([&]
    {
        try { dispatcher.Add(MakeBook("a")); }
        catch (const std::exception&) { threw1 = true; }
    });
    std::thread t2([&]
    {
        try { dispatcher.Add(MakeBook("b")); }
        catch (const std::exception&) { threw2 = true; }
    });

    t1.join();
    t2.join();

    dispatcher.Drain();

    REQUIRE(threw1);
    REQUIRE(threw2);
}

TEST_CASE("Writer dispatching repository rejects new work after a fatal batch error", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.ThrowOnBatch = true;
    real.BatchErrorMessage = "Catastrophic failure";
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    REQUIRE_THROWS_WITH(
        dispatcher.Add(MakeBook("a")),
        Catch::Matchers::ContainsSubstring("Catastrophic failure"));

    bool threw = false;
    std::string message;
    try
    {
        (void)dispatcher.Add(MakeBook("b"));
    }
    catch (const std::runtime_error& ex)
    {
        threw = true;
        message = ex.what();
    }

    dispatcher.Drain();

    REQUIRE(threw);
    REQUIRE(message.find("fatal state") != std::string::npos);
}

TEST_CASE("Writer dispatching repository destructor drains cleanly without explicit Drain call", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;

    {
        Librova::Importing::CWriterDispatchingRepository dispatcher(real);
        dispatcher.Add(MakeBook("clean-up"));
        // Destructor must join the writer thread without blocking forever.
    }

    SUCCEED("Destructor completed without deadlock.");
}

TEST_CASE("Writer dispatching repository rejects new Add calls after Drain", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);
    dispatcher.Drain();

    bool threw = false;
    std::string thrownMessage;
    try
    {
        dispatcher.Add(MakeBook("late"));
    }
    catch (const std::runtime_error& ex)
    {
        threw         = true;
        thrownMessage = ex.what();
    }

    REQUIRE(threw);
    REQUIRE(thrownMessage == "WriterDispatchingRepository is stopped — cannot accept new writes");
}

TEST_CASE("Writer dispatching repository enters fatal state when AddBatch returns the wrong result count", "[importing][writer-actor]")
{
    CBatchConfigurableRepository real;
    real.WrongResultCount = 0;
    Librova::Importing::CWriterDispatchingRepository dispatcher(real);

    REQUIRE_THROWS_WITH(
        dispatcher.Add(MakeBook("short-batch")),
        Catch::Matchers::ContainsSubstring("returned 0 results"));
    REQUIRE(dispatcher.HasFatalError());

    dispatcher.Drain();
}

