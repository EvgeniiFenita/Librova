#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "PipeClient/NamedPipeClient.hpp"

namespace Librova::ApplicationClient {

class CImportJobClient final
{
public:
    explicit CImportJobClient(std::filesystem::path pipePath);

    [[nodiscard]] Librova::ApplicationJobs::TImportJobId Start(
        const Librova::Application::SImportRequest& request,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] std::optional<Librova::ApplicationJobs::SImportJobSnapshot> TryGetSnapshot(
        Librova::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] std::optional<Librova::ApplicationJobs::SImportJobResult> TryGetResult(
        Librova::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] bool Cancel(
        Librova::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

    [[nodiscard]] bool Wait(
        Librova::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds waitTimeout) const;

    [[nodiscard]] bool Remove(
        Librova::ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) const;

private:
    Librova::PipeClient::CNamedPipeClient m_pipeClient;
};

} // namespace Librova::ApplicationClient
