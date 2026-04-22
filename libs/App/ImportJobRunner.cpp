#include "App/ImportJobRunner.hpp"

#include <algorithm>
#include <stdexcept>

namespace Librova::Jobs {

CImportJobRunner::CJobProgressSink::CJobProgressSink(
    const std::stop_token stopToken,
    TProgressCallback progressCallback)
    : m_stopToken(stopToken)
    , m_progressCallback(std::move(progressCallback))
{
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::ReportValue(const int percent, std::string_view message)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Running;
        m_snapshot.Percent = percent;
        m_snapshot.Message.assign(message);
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::ReportStructuredProgress(
    const std::size_t totalEntries,
    const std::size_t processedEntries,
    const std::size_t importedEntries,
    const std::size_t failedEntries,
    const std::size_t skippedEntries,
    const int percent,
    std::string_view message)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Running;
        m_snapshot.TotalEntries = totalEntries;
        m_snapshot.ProcessedEntries = processedEntries;
        m_snapshot.ImportedEntries = importedEntries;
        m_snapshot.FailedEntries = failedEntries;
        m_snapshot.SkippedEntries = skippedEntries;
        m_snapshot.Percent = percent;
        m_snapshot.Message.assign(message);
    }
    PublishSnapshot();
}

bool CImportJobRunner::CJobProgressSink::IsCancellationRequested() const
{
    return m_stopToken.stop_requested();
}

SJobProgressSnapshot CImportJobRunner::CJobProgressSink::GetSnapshot() const
{
    const std::scoped_lock lock(m_snapshotMutex);
    return m_snapshot;
}

void CImportJobRunner::CJobProgressSink::SetWarnings(std::vector<std::string> warnings)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Warnings = std::move(warnings);
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Complete(std::string_view message)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Completed;
        m_snapshot.Percent = 100;
        m_snapshot.Message.assign(message);
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Cancel(std::string_view message)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Cancelled;
        m_snapshot.Message.assign(message);
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Fail(std::string_view message)
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Failed;
        m_snapshot.Message.assign(message);
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::BeginRollback(const std::size_t totalToRollback) noexcept
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Cancelling;
        m_snapshot.TotalEntries = totalToRollback;
        m_snapshot.ProcessedEntries = 0;
        m_snapshot.Message = "Cancelling: preparing rollback of "
            + std::to_string(totalToRollback) + " book(s)";
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::ReportRollbackProgress(
    const std::size_t rolledBack,
    const std::size_t total) noexcept
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::RollingBack;
        m_snapshot.TotalEntries = total;
        m_snapshot.ProcessedEntries = rolledBack;
        m_snapshot.Percent = total > 0
            ? static_cast<int>((rolledBack * 100) / total)
            : 0;
        m_snapshot.Message = "Rolling back: "
            + std::to_string(rolledBack) + " / " + std::to_string(total) + " book(s)";
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::BeginCompacting() noexcept
{
    {
        const std::scoped_lock lock(m_snapshotMutex);
        m_snapshot.Status = EJobStatus::Compacting;
        m_snapshot.Message = "Compacting database...";
    }
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::PublishSnapshot() const
{
    SJobProgressSnapshot snapshotCopy;
    {
        const std::scoped_lock lock(m_snapshotMutex);
        snapshotCopy = m_snapshot;
    }

    if (m_progressCallback)
    {
        try
        {
            m_progressCallback(snapshotCopy);
        }
        catch (...)
        {
        }
    }
}

CImportJobRunner::CImportJobRunner(const Librova::Application::CLibraryImportFacade& importFacade)
    : m_importFacade(importFacade)
{
}

std::optional<Librova::Domain::SDomainError> CImportJobRunner::TryMapError(
    const Librova::Application::SImportResult& importResult)
{
    const auto cancellationMessage = importResult.HasRollbackCleanupResidue
        ? "Import was cancelled. Some managed files or directories could not be removed during rollback."
        : "Import was cancelled.";

    if (importResult.WasCancelled)
    {
        return Librova::Domain::SDomainError{
            .Code = Librova::Domain::EDomainErrorCode::Cancellation,
            .Message = cancellationMessage
        };
    }

    if (importResult.ZipResult.has_value())
    {
        const auto wasCancelled = std::ranges::any_of(importResult.ZipResult->Entries, [](const auto& entry) {
            return entry.Status == Librova::ZipImporting::EZipEntryImportStatus::Cancelled
                || (entry.SingleFileResult.has_value()
                    && entry.SingleFileResult->Status == Librova::Importing::ESingleFileImportStatus::Cancelled);
        });

        if (wasCancelled)
        {
            return Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::Cancellation,
                .Message = cancellationMessage
            };
        }
    }

    if (importResult.SingleFileResult.has_value())
    {
        const auto& single = *importResult.SingleFileResult;

        switch (single.Status)
        {
        case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
            return Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::DuplicateRejected,
                .Message = "Import rejected because a duplicate already exists."
            };
        case Librova::Importing::ESingleFileImportStatus::Cancelled:
            return Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::Cancellation,
                .Message = cancellationMessage
            };
        case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
            return Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::UnsupportedFormat,
                .Message = "Unsupported input format."
            };
        case Librova::Importing::ESingleFileImportStatus::Failed:
            return Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::IntegrityIssue,
                .Message = single.Error.empty() ? "Import failed." : single.Error
            };
        case Librova::Importing::ESingleFileImportStatus::Imported:
            return std::nullopt;
        }
    }

    if (importResult.Summary.ImportedEntries == 0 && importResult.Summary.FailedEntries > 0)
    {
        return Librova::Domain::SDomainError{
            .Code = Librova::Domain::EDomainErrorCode::IntegrityIssue,
            .Message = "Import completed without successful entries."
        };
    }

    return std::nullopt;
}

bool CImportJobRunner::HasNoSuccessfulImports(const Librova::Application::SImportResult& importResult) noexcept
{
    return importResult.Summary.ImportedEntries == 0
        && importResult.Summary.FailedEntries == 0
        && importResult.NoSuccessfulImportReason != Librova::Application::ENoSuccessfulImportReason::None;
}

SImportJobResult CImportJobRunner::Run(
    const Librova::Application::SImportRequest& request,
    const std::stop_token stopToken) const
{
    return Run(request, stopToken, {});
}

SImportJobResult CImportJobRunner::Run(
    const Librova::Application::SImportRequest& request,
    const std::stop_token stopToken,
    TProgressCallback progressCallback) const
{
    CJobProgressSink progressSink(stopToken, std::move(progressCallback));

    try
    {
        const Librova::Application::SImportResult importResult =
            m_importFacade.Run(request, progressSink, stopToken);

        progressSink.SetWarnings(importResult.Summary.Warnings);

        if (const auto mappedError = TryMapError(importResult); mappedError.has_value())
        {
            if (mappedError->Code == Librova::Domain::EDomainErrorCode::Cancellation)
            {
                progressSink.Cancel(mappedError->Message);
            }
            else
            {
                progressSink.Fail(mappedError->Message);
            }

            return {
                .Snapshot = progressSink.GetSnapshot(),
                .ImportResult = importResult,
                .Error = mappedError
            };
        }

        if (HasNoSuccessfulImports(importResult))
        {
            Librova::Domain::SDomainError error;
            switch (importResult.NoSuccessfulImportReason)
            {
            case Librova::Application::ENoSuccessfulImportReason::DuplicateRejected:
                error = {
                    .Code = Librova::Domain::EDomainErrorCode::DuplicateRejected,
                    .Message = "Import rejected because duplicates already exist for all selected sources."
                };
                break;
            case Librova::Application::ENoSuccessfulImportReason::UnsupportedFormat:
            case Librova::Application::ENoSuccessfulImportReason::None:
                error = {
                    .Code = Librova::Domain::EDomainErrorCode::UnsupportedFormat,
                    .Message = "Import completed without importing any supported books."
                };
                break;
            }

            progressSink.Fail(error.Message);
            return {
                .Snapshot = progressSink.GetSnapshot(),
                .ImportResult = importResult,
                .Error = error
            };
        }

        if (importResult.IsPartialSuccess())
        {
            progressSink.Complete("Import completed with partial success.");
        }
        else
        {
            progressSink.Complete("Import completed successfully.");
        }

        return {
            .Snapshot = progressSink.GetSnapshot(),
            .ImportResult = importResult
        };
    }
    catch (const std::exception& error)
    {
        progressSink.Fail(error.what());
        return {
            .Snapshot = progressSink.GetSnapshot(),
            .Error = Librova::Domain::SDomainError{
                .Code = Librova::Domain::EDomainErrorCode::IntegrityIssue,
                .Message = error.what()
            }
        };
    }
}

} // namespace Librova::Jobs
