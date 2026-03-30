#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "PipeClient/NamedPipeClient.hpp"

namespace LibriFlow::ApplicationClient {

class CImportJobClient final
{
public:
    explicit CImportJobClient(std::filesystem::path pipePath);

    [[nodiscard]] LibriFlow::ApplicationJobs::TImportJobId Start(
        const LibriFlow::Application::SImportRequest& request,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] std::optional<LibriFlow::ApplicationJobs::SImportJobSnapshot> TryGetSnapshot(
        LibriFlow::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] std::optional<LibriFlow::ApplicationJobs::SImportJobResult> TryGetResult(
        LibriFlow::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] bool Cancel(
        LibriFlow::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] bool Wait(
        LibriFlow::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds waitTimeout) const;

    [[nodiscard]] bool Remove(
        LibriFlow::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

private:
    LibriFlow::PipeClient::CNamedPipeClient m_pipeClient;
};

} // namespace LibriFlow::ApplicationClient
