#pragma once

#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::Jobs {

enum class EJobStatus
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct SJobProgressSnapshot
{
    EJobStatus Status = EJobStatus::Pending;
    int Percent = 0;
    std::string Message;
    std::vector<std::string> Warnings;

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
    std::optional<LibriFlow::Application::SImportResult> ImportResult;
    std::optional<LibriFlow::Domain::SDomainError> Error;
};

class CImportJobRunner final
{
public:
    using TProgressCallback = std::function<void(const SJobProgressSnapshot&)>;

    explicit CImportJobRunner(const LibriFlow::Application::CLibraryImportFacade& importFacade);

    [[nodiscard]] SImportJobResult Run(
        const LibriFlow::Application::SImportRequest& request,
        std::stop_token stopToken) const;

    [[nodiscard]] SImportJobResult Run(
        const LibriFlow::Application::SImportRequest& request,
        std::stop_token stopToken,
        TProgressCallback progressCallback) const;

private:
    class CJobProgressSink final : public LibriFlow::Domain::IProgressSink
    {
    public:
        explicit CJobProgressSink(std::stop_token stopToken, TProgressCallback progressCallback);

        void ReportValue(int percent, std::string_view message) override;
        bool IsCancellationRequested() const override;

        [[nodiscard]] const SJobProgressSnapshot& GetSnapshot() const noexcept;
        void SetWarnings(std::vector<std::string> warnings);
        void Complete(std::string_view message);
        void Cancel(std::string_view message);
        void Fail(std::string_view message);

    private:
        void PublishSnapshot() const;

        std::stop_token m_stopToken;
        TProgressCallback m_progressCallback;
        SJobProgressSnapshot m_snapshot{
            .Status = EJobStatus::Running
        };
    };

    [[nodiscard]] static std::optional<LibriFlow::Domain::SDomainError> TryMapError(
        const LibriFlow::Application::SImportResult& importResult);

    const LibriFlow::Application::CLibraryImportFacade& m_importFacade;
};

} // namespace LibriFlow::Jobs
