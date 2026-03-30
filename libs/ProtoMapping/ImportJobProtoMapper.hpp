#pragma once

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Domain/DomainError.hpp"
#include "import_jobs.pb.h"

namespace LibriFlow::ProtoMapping {

class CImportJobProtoMapper final
{
public:
    [[nodiscard]] static LibriFlow::Application::SImportRequest FromProto(
        const libriflow::v1::ImportRequest& request);

    [[nodiscard]] static libriflow::v1::ImportRequest ToProto(
        const LibriFlow::Application::SImportRequest& request);

    [[nodiscard]] static libriflow::v1::DomainError ToProto(
        const LibriFlow::Domain::SDomainError& error);

    [[nodiscard]] static libriflow::v1::ImportSummary ToProto(
        const LibriFlow::Application::SImportSummary& summary);

    [[nodiscard]] static libriflow::v1::ImportJobSnapshot ToProto(
        const LibriFlow::ApplicationJobs::SImportJobSnapshot& snapshot);

    [[nodiscard]] static libriflow::v1::ImportJobResult ToProto(
        const LibriFlow::ApplicationJobs::SImportJobResult& result);

    [[nodiscard]] static libriflow::v1::StartImportResponse ToProtoStartResponse(
        LibriFlow::ApplicationJobs::TImportJobId jobId);

    [[nodiscard]] static libriflow::v1::GetImportJobSnapshotResponse ToProtoSnapshotResponse(
        const LibriFlow::ApplicationJobs::SImportJobSnapshot* snapshot);

    [[nodiscard]] static libriflow::v1::GetImportJobResultResponse ToProtoResultResponse(
        const LibriFlow::ApplicationJobs::SImportJobResult* result);

private:
    [[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path PathFromUtf8(const std::string& value);
    [[nodiscard]] static libriflow::v1::ImportMode ToProto(LibriFlow::Application::EImportMode mode) noexcept;
    [[nodiscard]] static libriflow::v1::ImportJobStatus ToProto(
        LibriFlow::ApplicationJobs::EImportJobStatus status) noexcept;
    [[nodiscard]] static libriflow::v1::ErrorCode ToProto(
        LibriFlow::Domain::EDomainErrorCode code) noexcept;
};

} // namespace LibriFlow::ProtoMapping
