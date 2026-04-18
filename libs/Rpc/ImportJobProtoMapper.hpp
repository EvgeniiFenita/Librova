#pragma once

#include "App/LibraryImportFacade.hpp"
#include "App/ImportJobService.hpp"
#include "Domain/DomainError.hpp"
#include "import_jobs.pb.h"

namespace Librova::ProtoMapping {

class CImportJobProtoMapper final
{
public:
    [[nodiscard]] static Librova::Application::SImportRequest FromProto(
        const librova::v1::ImportRequest& request);

    [[nodiscard]] static Librova::Domain::SDomainError FromProto(
        const librova::v1::DomainError& error);

    [[nodiscard]] static Librova::Application::SImportSummary FromProto(
        const librova::v1::ImportSummary& summary);

    [[nodiscard]] static Librova::ApplicationJobs::SImportJobSnapshot FromProto(
        const librova::v1::ImportJobSnapshot& snapshot);

    [[nodiscard]] static Librova::ApplicationJobs::SImportJobResult FromProto(
        const librova::v1::ImportJobResult& result);

    [[nodiscard]] static librova::v1::ImportRequest ToProto(
        const Librova::Application::SImportRequest& request);

    [[nodiscard]] static librova::v1::DomainError ToProto(
        const Librova::Domain::SDomainError& error);

    [[nodiscard]] static librova::v1::ImportSummary ToProto(
        const Librova::Application::SImportSummary& summary);

    [[nodiscard]] static librova::v1::ImportJobSnapshot ToProto(
        const Librova::ApplicationJobs::SImportJobSnapshot& snapshot);

    [[nodiscard]] static librova::v1::ImportJobResult ToProto(
        const Librova::ApplicationJobs::SImportJobResult& result);

    [[nodiscard]] static librova::v1::StartImportResponse ToProtoStartResponse(
        Librova::ApplicationJobs::TImportJobId jobId);

    [[nodiscard]] static librova::v1::GetImportJobSnapshotResponse ToProtoSnapshotResponse(
        const Librova::ApplicationJobs::SImportJobSnapshot* snapshot);

    [[nodiscard]] static librova::v1::GetImportJobResultResponse ToProtoResultResponse(
        const Librova::ApplicationJobs::SImportJobResult* result);

private:
    [[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path PathFromUtf8(const std::string& value);
    [[nodiscard]] static Librova::Application::EImportMode FromProto(librova::v1::ImportMode mode) noexcept;
    [[nodiscard]] static Librova::ApplicationJobs::EImportJobStatus FromProto(
        librova::v1::ImportJobStatus status) noexcept;
    [[nodiscard]] static Librova::Domain::EDomainErrorCode FromProto(
        librova::v1::ErrorCode code) noexcept;
    [[nodiscard]] static librova::v1::ImportMode ToProto(Librova::Application::EImportMode mode) noexcept;
    [[nodiscard]] static librova::v1::ImportJobStatus ToProto(
        Librova::ApplicationJobs::EImportJobStatus status) noexcept;
    [[nodiscard]] static librova::v1::ErrorCode ToProto(
        Librova::Domain::EDomainErrorCode code) noexcept;
};

} // namespace Librova::ProtoMapping
