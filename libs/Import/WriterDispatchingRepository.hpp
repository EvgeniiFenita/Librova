#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace Librova::Importing {

class CImportPerfTracker;

// Routes IBookRepository::Add() and ForceAdd() through a dedicated writer
// thread that batches calls into single-transaction AddBatch() invocations.
// All read operations (ReserveId, GetById, Remove, Compact) are forwarded
// directly to the underlying repository.
//
// Ownership model:
//   1. Construct before spawning workers (starts writer thread).
//   2. Workers call Add()/ForceAdd() — they block on future.get() until the
//      writer thread processes their entry.
//   3. After pool.wait(), call Drain() to stop the writer thread cleanly.
class CWriterDispatchingRepository final : public Librova::Domain::IBookRepository
{
public:
    static constexpr std::size_t kMaxBatchSize = 50;

    explicit CWriterDispatchingRepository(
        Librova::Domain::IBookRepository& realRepository,
        CImportPerfTracker* perfTracker = nullptr);
    ~CWriterDispatchingRepository() override;

    // Blocks until the queue is empty and stops the writer thread.
    // Must be called before destroying this object if the writer thread
    // was not already stopped.
    void Drain();

    // IBookRepository — read/management ops forwarded to real repository:
    Librova::Domain::SBookId ReserveId() override;
    [[nodiscard]] std::vector<Librova::Domain::SBookId> ReserveIds(std::size_t count) override;
    [[nodiscard]] std::optional<Librova::Domain::SBook>
        GetById(Librova::Domain::SBookId id) const override;
    void Remove(Librova::Domain::SBookId id) override;
    void Compact(const std::function<void()>& onProgressTick = nullptr) override;

    // Write ops go through the writer queue:
    Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override;
    Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override;

    // AddBatch inherits the default sequential implementation (calls Add/ForceAdd above).
    // The writer thread already batches these internally when the queue fills up.

    [[nodiscard]] bool HasFatalError();

private:
    struct SWriteRequest
    {
        Librova::Domain::SBook                              Book;
        bool                                                ForceAdd = false;
        std::promise<Librova::Domain::IBookRepository::SBatchBookResult> Promise;
    };

    [[nodiscard]] std::future<SBatchBookResult> Submit(
        const Librova::Domain::SBook& book, bool forceAdd);

    void NoteQueueDepth(std::size_t queueDepth) noexcept;
    void WriterLoop();

    Librova::Domain::IBookRepository& m_realRepository;
    CImportPerfTracker*               m_perfTracker = nullptr;

    std::queue<SWriteRequest> m_queue;
    std::mutex                m_queueMutex;
    std::condition_variable   m_cv;
    bool                      m_stopped = false;
    std::optional<std::string> m_fatalError;

    std::thread m_writerThread;
};

} // namespace Librova::Importing
