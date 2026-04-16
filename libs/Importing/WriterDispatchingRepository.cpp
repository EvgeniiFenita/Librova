#include "Importing/WriterDispatchingRepository.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include "Importing/ImportPerfTracker.hpp"
#include "Logging/Logging.hpp"

namespace Librova::Importing {

CWriterDispatchingRepository::CWriterDispatchingRepository(
    Librova::Domain::IBookRepository& realRepository,
    CImportPerfTracker* const perfTracker)
    : m_realRepository(realRepository)
    , m_perfTracker(perfTracker)
    , m_writerThread([this] { WriterLoop(); })
{
    NoteQueueDepth(0);
}

CWriterDispatchingRepository::~CWriterDispatchingRepository()
{
    Drain();
}

void CWriterDispatchingRepository::Drain()
{
    bool shouldNotify = false;
    {
        const std::scoped_lock lock(m_queueMutex);
        if (!m_stopped)
        {
            m_stopped = true;
            shouldNotify = true;
        }
    }
    if (shouldNotify)
    {
        m_cv.notify_one();
    }

    if (m_writerThread.joinable())
    {
        m_writerThread.join();
    }

    NoteQueueDepth(0);
}

Librova::Domain::SBookId CWriterDispatchingRepository::ReserveId()
{
    return m_realRepository.ReserveId();
}

std::vector<Librova::Domain::SBookId> CWriterDispatchingRepository::ReserveIds(const std::size_t count)
{
    return m_realRepository.ReserveIds(count);
}

std::optional<Librova::Domain::SBook>
CWriterDispatchingRepository::GetById(const Librova::Domain::SBookId id) const
{
    return m_realRepository.GetById(id);
}

void CWriterDispatchingRepository::Remove(const Librova::Domain::SBookId id)
{
    m_realRepository.Remove(id);
}

void CWriterDispatchingRepository::Compact(const std::function<void()>& onProgressTick)
{
    m_realRepository.Compact(onProgressTick);
}

Librova::Domain::SBookId CWriterDispatchingRepository::Add(const Librova::Domain::SBook& book)
{
    auto future = Submit(book, false);
    const auto result = future.get();

    switch (result.Status)
    {
    case EBatchAddStatus::Imported:
        return *result.BookId;
    case EBatchAddStatus::RejectedDuplicate:
        throw Librova::Domain::CDuplicateHashException{
            result.ConflictingBookId.value_or(Librova::Domain::SBookId{})};
    case EBatchAddStatus::Failed:
        throw std::runtime_error(result.Error.empty() ? "Batch writer failed to add book" : result.Error);
    }

    throw std::runtime_error("Unexpected batch result status");
}

Librova::Domain::SBookId CWriterDispatchingRepository::ForceAdd(const Librova::Domain::SBook& book)
{
    auto future = Submit(book, true);
    const auto result = future.get();

    if (result.Status == EBatchAddStatus::Imported)
    {
        return *result.BookId;
    }

    throw std::runtime_error(result.Error.empty() ? "Batch writer failed to force-add book" : result.Error);
}

std::future<Librova::Domain::IBookRepository::SBatchBookResult>
CWriterDispatchingRepository::Submit(const Librova::Domain::SBook& book, const bool forceAdd)
{
    std::promise<SBatchBookResult> promise;
    auto future = promise.get_future();

    {
        const std::scoped_lock lock(m_queueMutex);
        if (m_fatalError.has_value())
        {
            throw std::runtime_error(
                "WriterDispatchingRepository is in fatal state: " + *m_fatalError);
        }
        if (m_stopped)
        {
            throw std::runtime_error("WriterDispatchingRepository is stopped — cannot accept new writes");
        }
        m_queue.push({book, forceAdd, std::move(promise)});
        NoteQueueDepth(m_queue.size());
    }

    m_cv.notify_one();
    return future;
}

void CWriterDispatchingRepository::NoteQueueDepth(const std::size_t queueDepth) noexcept
{
    if (m_perfTracker == nullptr)
    {
        return;
    }

    m_perfTracker->SetWriterQueueDepth(static_cast<std::uint32_t>(std::min<std::size_t>(
        queueDepth,
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))));
}

bool CWriterDispatchingRepository::HasFatalError()
{
    const std::scoped_lock lock(m_queueMutex);
    return m_fatalError.has_value();
}

void CWriterDispatchingRepository::WriterLoop()
{
    auto logger = Librova::Logging::CLogging::IsInitialized()
        ? Librova::Logging::CLogging::GetLogger()
        : nullptr;

    while (true)
    {
        std::vector<SWriteRequest> batch;
        batch.reserve(kMaxBatchSize);

        {
            std::unique_lock lock(m_queueMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(10), [this]
            {
                return !m_queue.empty() || m_stopped;
            });

            while (!m_queue.empty() && batch.size() < kMaxBatchSize)
            {
                batch.push_back(std::move(m_queue.front()));
                m_queue.pop();
            }
            NoteQueueDepth(m_queue.size());

            if (batch.empty() && m_stopped)
            {
                return;
            }
        }

        if (batch.empty())
        {
            continue;
        }

        std::vector<SBatchBookEntry> entries;
        entries.reserve(batch.size());
        for (auto& req : batch)
        {
            entries.push_back({std::move(req.Book), req.ForceAdd});
        }

        std::vector<SBatchBookResult> results;
        try
        {
            results = m_realRepository.AddBatch(entries);

            if (results.size() != batch.size())
            {
                throw std::runtime_error(std::format(
                    "AddBatch returned {} results for {} queued writes.",
                    results.size(),
                    batch.size()));
            }
        }
        catch (const std::exception& ex)
        {
            if (logger)
            {
                logger->error("[writer-actor] AddBatch threw unexpectedly: {}", ex.what());
            }

            SBatchBookResult failResult{
                .Status = EBatchAddStatus::Failed,
                .Error  = ex.what()
            };
            for (auto& req : batch)
            {
                req.Promise.set_value(failResult);
            }

            std::queue<SWriteRequest> pendingRequests;
            {
                const std::scoped_lock lock(m_queueMutex);
                m_fatalError = ex.what();
                m_stopped = true;
                pendingRequests.swap(m_queue);
                NoteQueueDepth(0);
            }

            while (!pendingRequests.empty())
            {
                pendingRequests.front().Promise.set_value(failResult);
                pendingRequests.pop();
            }

            return;
        }

        for (std::size_t i = 0; i < batch.size(); ++i)
        {
            batch[i].Promise.set_value(std::move(results[i]));
        }
    }
}

} // namespace Librova::Importing
