#include "App/ImportJobManager.hpp"

#include <ranges>
#include <utility>
#include <vector>

namespace Librova::Jobs {

CImportJobManager::CImportJobManager(const CImportJobRunner& jobRunner)
    : m_jobRunner(jobRunner)
{
}

CImportJobManager::~CImportJobManager()
{
    std::vector<std::shared_ptr<SJobRecord>> records;

    {
        const std::scoped_lock lock(m_jobsMutex);
        records.reserve(m_jobs.size());

        for (const auto& record : m_jobs | std::views::values)
        {
            records.push_back(record);
        }
    }

    for (const auto& record : records)
    {
        if (record->Worker.joinable())
        {
            record->Worker.request_stop();
            record->Worker.join();
        }
    }
}

SImportJobHandle CImportJobManager::Start(const Librova::Application::SImportRequest& request)
{
    auto record = std::make_shared<SJobRecord>();
    record->Snapshot = {
        .Status = EJobStatus::Pending,
        .Message = "Queued"
    };

    TImportJobId jobId = 0;

    {
        const std::scoped_lock lock(m_jobsMutex);
        jobId = m_nextJobId++;
        m_jobs.emplace(jobId, record);
    }

    const auto* jobRunner = &m_jobRunner;
    auto effectiveRequest = request;
    effectiveRequest.JobId = jobId;

    record->Worker = std::jthread([jobRunner, record, effectiveRequest = std::move(effectiveRequest)](const std::stop_token stopToken) {
        const auto publishSnapshot = [record](const SJobProgressSnapshot& snapshot) {
            {
                const std::scoped_lock lock(record->Mutex);
                record->Snapshot = snapshot;
            }

            record->Condition.notify_one();
        };

        publishSnapshot({
            .Status = EJobStatus::Running,
            .Message = "Starting import job"
        });

        SImportJobResult result = jobRunner->Run(effectiveRequest, stopToken, publishSnapshot);

        {
            const std::scoped_lock lock(record->Mutex);
            record->Snapshot = result.Snapshot;
            record->Result = std::move(result);
        }

        record->Condition.notify_one();
    });

    return {.Id = jobId};
}

std::optional<SJobProgressSnapshot> CImportJobManager::TryGetSnapshot(const TImportJobId jobId) const
{
    const auto record = TryGetRecord(jobId);

    if (!record)
    {
        return std::nullopt;
    }

    const std::scoped_lock lock(record->Mutex);
    return record->Snapshot;
}

std::optional<SImportJobResult> CImportJobManager::TryGetResult(const TImportJobId jobId) const
{
    const auto record = TryGetRecord(jobId);

    if (!record)
    {
        return std::nullopt;
    }

    const std::scoped_lock lock(record->Mutex);
    return record->Result;
}

bool CImportJobManager::Cancel(const TImportJobId jobId)
{
    const auto record = TryGetRecord(jobId);

    if (!record || !record->Worker.joinable())
    {
        return false;
    }

    {
        const std::scoped_lock lock(record->Mutex);

        if (record->Result.has_value())
        {
            return false;
        }
    }

    record->Worker.request_stop();
    return true;
}

bool CImportJobManager::Wait(const TImportJobId jobId, const std::chrono::milliseconds timeout) const
{
    const auto record = TryGetRecord(jobId);

    if (!record)
    {
        return false;
    }

    std::unique_lock lock(record->Mutex);
    return record->Condition.wait_for(lock, timeout, [&record] {
        return record->Result.has_value();
    });
}

bool CImportJobManager::Remove(const TImportJobId jobId)
{
    std::shared_ptr<SJobRecord> record;

    {
        const std::scoped_lock lock(m_jobsMutex);
        const auto iterator = m_jobs.find(jobId);

        if (iterator == m_jobs.end())
        {
            return false;
        }

        record = iterator->second;

        {
            const std::scoped_lock recordLock(record->Mutex);

            if (!record->Result.has_value())
            {
                return false;
            }
        }

        m_jobs.erase(iterator);
    }

    return true;
}

std::shared_ptr<CImportJobManager::SJobRecord> CImportJobManager::TryGetRecord(const TImportJobId jobId) const
{
    const std::scoped_lock lock(m_jobsMutex);
    const auto iterator = m_jobs.find(jobId);
    return iterator == m_jobs.end() ? nullptr : iterator->second;
}

} // namespace Librova::Jobs
