#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "Domain/ServiceContracts.hpp"

namespace Librova::Jobs {

enum class EJobStatus
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    // Post-cancel phases — not terminal; job is still executing cleanup.
    Cancelling,
    RollingBack,
    Compacting
};

struct SJobProgressSnapshot
{
    EJobStatus Status = EJobStatus::Pending;
    int Percent = 0;
    std::string Message;
    std::vector<std::string> Warnings;
    std::size_t TotalEntries = 0;
    std::size_t ProcessedEntries = 0;
    std::size_t ImportedEntries = 0;
    std::size_t FailedEntries = 0;
    std::size_t SkippedEntries = 0;

    [[nodiscard]] bool IsTerminal() const noexcept
    {
        return Status == EJobStatus::Completed
            || Status == EJobStatus::Failed
            || Status == EJobStatus::Cancelled;
    }
};

struct SImportJobResult
{
    SJobProgressSnapshot Snapshot;
    std::optional<Librova::Application::SImportResult> ImportResult;
    std::optional<Librova::Domain::SDomainError> Error;
};

class CImportJobRunner final
{
public:
    using TProgressCallback = std::function<void(const SJobProgressSnapshot&)>;

    explicit CImportJobRunner(const Librova::Application::CLibraryImportFacade& importFacade);

    [[nodiscard]] SImportJobResult Run(
        const Librova::Application::SImportRequest& request,
        std::stop_token stopToken) const;

    [[nodiscard]] SImportJobResult Run(
        const Librova::Application::SImportRequest& request,
        std::stop_token stopToken,
        TProgressCallback progressCallback) const;

private:
    [[nodiscard]] static bool HasNoSuccessfulImports(const Librova::Application::SImportResult& importResult) noexcept;

    class CJobProgressSink final : public Librova::Domain::IProgressSink, public Librova::Domain::IStructuredImportProgressSink
    {
    public:
        explicit CJobProgressSink(std::stop_token stopToken, TProgressCallback progressCallback);

        void ReportValue(int percent, std::string_view message) override;
        void ReportStructuredProgress(
            std::size_t totalEntries,
            std::size_t processedEntries,
            std::size_t importedEntries,
            std::size_t failedEntries,
            std::size_t skippedEntries,
            int percent,
            std::string_view message) override;
        bool IsCancellationRequested() const override;
        void BeginRollback(std::size_t totalToRollback) noexcept override;
        void ReportRollbackProgress(std::size_t rolledBack, std::size_t total) noexcept override;
        void BeginCompacting() noexcept override;

        [[nodiscard]] SJobProgressSnapshot GetSnapshot() const;
        void SetWarnings(std::vector<std::string> warnings);
        void Complete(std::string_view message);
        void Cancel(std::string_view message);
        void Fail(std::string_view message);

    private:
        void PublishSnapshot() const;

        std::stop_token m_stopToken;
        TProgressCallback m_progressCallback;
        mutable std::mutex m_snapshotMutex;
        SJobProgressSnapshot m_snapshot{
            .Status = EJobStatus::Running
        };
    };

    [[nodiscard]] static std::optional<Librova::Domain::SDomainError> TryMapError(
        const Librova::Application::SImportResult& importResult);

    const Librova::Application::CLibraryImportFacade& m_importFacade;
};

} // namespace Librova::Jobs
