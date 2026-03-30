#include "Jobs/ImportJobRunner.hpp"

#include <stdexcept>

namespace LibriFlow::Jobs {

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
    m_snapshot.Status = EJobStatus::Running;
    m_snapshot.Percent = percent;
    m_snapshot.Message.assign(message);
    PublishSnapshot();
}

bool CImportJobRunner::CJobProgressSink::IsCancellationRequested() const
{
    return m_stopToken.stop_requested();
}

const SJobProgressSnapshot& CImportJobRunner::CJobProgressSink::GetSnapshot() const noexcept
{
    return m_snapshot;
}

void CImportJobRunner::CJobProgressSink::SetWarnings(std::vector<std::string> warnings)
{
    m_snapshot.Warnings = std::move(warnings);
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Complete(std::string_view message)
{
    m_snapshot.Status = EJobStatus::Completed;
    m_snapshot.Percent = 100;
    m_snapshot.Message.assign(message);
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Cancel(std::string_view message)
{
    m_snapshot.Status = EJobStatus::Cancelled;
    m_snapshot.Message.assign(message);
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::Fail(std::string_view message)
{
    m_snapshot.Status = EJobStatus::Failed;
    m_snapshot.Message.assign(message);
    PublishSnapshot();
}

void CImportJobRunner::CJobProgressSink::PublishSnapshot() const
{
    if (m_progressCallback)
    {
        m_progressCallback(m_snapshot);
    }
}

CImportJobRunner::CImportJobRunner(const LibriFlow::Application::CLibraryImportFacade& importFacade)
    : m_importFacade(importFacade)
{
}

std::optional<LibriFlow::Domain::SDomainError> CImportJobRunner::TryMapError(
    const LibriFlow::Application::SImportResult& importResult)
{
    if (importResult.SingleFileResult.has_value())
    {
        const auto& single = *importResult.SingleFileResult;

        switch (single.Status)
        {
        case LibriFlow::Importing::ESingleFileImportStatus::RejectedDuplicate:
            return LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::DuplicateRejected,
                .Message = "Import rejected because a strict duplicate already exists."
            };
        case LibriFlow::Importing::ESingleFileImportStatus::DecisionRequired:
            return LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::DuplicateDecisionRequired,
                .Message = "Import requires user confirmation for a probable duplicate."
            };
        case LibriFlow::Importing::ESingleFileImportStatus::Cancelled:
            return LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::Cancellation,
                .Message = "Import was cancelled."
            };
        case LibriFlow::Importing::ESingleFileImportStatus::UnsupportedFormat:
            return LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::UnsupportedFormat,
                .Message = "Unsupported input format."
            };
        case LibriFlow::Importing::ESingleFileImportStatus::Failed:
            return LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::IntegrityIssue,
                .Message = single.Error.empty() ? "Import failed." : single.Error
            };
        case LibriFlow::Importing::ESingleFileImportStatus::Imported:
            return std::nullopt;
        }
    }

    if (importResult.Summary.ImportedEntries == 0 && importResult.Summary.FailedEntries > 0)
    {
        return LibriFlow::Domain::SDomainError{
            .Code = LibriFlow::Domain::EDomainErrorCode::IntegrityIssue,
            .Message = "Import completed without successful entries."
        };
    }

    return std::nullopt;
}

SImportJobResult CImportJobRunner::Run(
    const LibriFlow::Application::SImportRequest& request,
    const std::stop_token stopToken) const
{
    return Run(request, stopToken, {});
}

SImportJobResult CImportJobRunner::Run(
    const LibriFlow::Application::SImportRequest& request,
    const std::stop_token stopToken,
    TProgressCallback progressCallback) const
{
    CJobProgressSink progressSink(stopToken, std::move(progressCallback));

    try
    {
        const LibriFlow::Application::SImportResult importResult =
            m_importFacade.Run(request, progressSink, stopToken);

        progressSink.SetWarnings(importResult.Summary.Warnings);

        if (const auto mappedError = TryMapError(importResult); mappedError.has_value())
        {
            if (mappedError->Code == LibriFlow::Domain::EDomainErrorCode::Cancellation)
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
            .Error = LibriFlow::Domain::SDomainError{
                .Code = LibriFlow::Domain::EDomainErrorCode::IntegrityIssue,
                .Message = error.what()
            }
        };
    }
}

} // namespace LibriFlow::Jobs
