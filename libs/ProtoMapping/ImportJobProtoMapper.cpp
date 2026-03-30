#include "ProtoMapping/ImportJobProtoMapper.hpp"

#include <string>

namespace {

[[nodiscard]] std::string ToUtf8String(const std::u8string& value)
{
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

[[nodiscard]] std::u8string ToUtf8StringView(const std::string& value)
{
    return {reinterpret_cast<const char8_t*>(value.data()), reinterpret_cast<const char8_t*>(value.data() + value.size())};
}

} // namespace

namespace LibriFlow::ProtoMapping {

LibriFlow::Application::SImportRequest CImportJobProtoMapper::FromProto(const libriflow::v1::ImportRequest& request)
{
    return {
        .SourcePath = PathFromUtf8(request.source_path()),
        .WorkingDirectory = PathFromUtf8(request.working_directory()),
        .Sha256Hex = request.has_sha256_hex() ? std::optional<std::string>{request.sha256_hex()} : std::nullopt,
        .AllowProbableDuplicates = request.allow_probable_duplicates()
    };
}

libriflow::v1::ImportRequest CImportJobProtoMapper::ToProto(const LibriFlow::Application::SImportRequest& request)
{
    libriflow::v1::ImportRequest message;
    message.set_source_path(PathToUtf8(request.SourcePath));
    message.set_working_directory(PathToUtf8(request.WorkingDirectory));

    if (request.Sha256Hex.has_value())
    {
        message.set_sha256_hex(*request.Sha256Hex);
    }

    message.set_allow_probable_duplicates(request.AllowProbableDuplicates);
    return message;
}

libriflow::v1::DomainError CImportJobProtoMapper::ToProto(const LibriFlow::Domain::SDomainError& error)
{
    libriflow::v1::DomainError message;
    message.set_code(ToProto(error.Code));
    message.set_message(error.Message);
    return message;
}

libriflow::v1::ImportSummary CImportJobProtoMapper::ToProto(const LibriFlow::Application::SImportSummary& summary)
{
    libriflow::v1::ImportSummary message;
    message.set_mode(ToProto(summary.Mode));
    message.set_total_entries(summary.TotalEntries);
    message.set_imported_entries(summary.ImportedEntries);
    message.set_failed_entries(summary.FailedEntries);
    message.set_skipped_entries(summary.SkippedEntries);

    for (const auto& warning : summary.Warnings)
    {
        message.add_warnings(warning);
    }

    return message;
}

libriflow::v1::ImportJobSnapshot CImportJobProtoMapper::ToProto(
    const LibriFlow::ApplicationJobs::SImportJobSnapshot& snapshot)
{
    libriflow::v1::ImportJobSnapshot message;
    message.set_job_id(snapshot.JobId);
    message.set_status(ToProto(snapshot.Status));
    message.set_percent(snapshot.Percent);
    message.set_message(snapshot.Message);

    for (const auto& warning : snapshot.Warnings)
    {
        message.add_warnings(warning);
    }

    return message;
}

libriflow::v1::ImportJobResult CImportJobProtoMapper::ToProto(
    const LibriFlow::ApplicationJobs::SImportJobResult& result)
{
    libriflow::v1::ImportJobResult message;
    *message.mutable_snapshot() = ToProto(result.Snapshot);

    if (result.ImportResult.has_value())
    {
        *message.mutable_summary() = ToProto(result.ImportResult->Summary);
    }

    if (result.Error.has_value())
    {
        *message.mutable_error() = ToProto(*result.Error);
    }

    return message;
}

libriflow::v1::StartImportResponse CImportJobProtoMapper::ToProtoStartResponse(
    const LibriFlow::ApplicationJobs::TImportJobId jobId)
{
    libriflow::v1::StartImportResponse response;
    response.set_job_id(jobId);
    return response;
}

libriflow::v1::GetImportJobSnapshotResponse CImportJobProtoMapper::ToProtoSnapshotResponse(
    const LibriFlow::ApplicationJobs::SImportJobSnapshot* snapshot)
{
    libriflow::v1::GetImportJobSnapshotResponse response;

    if (snapshot != nullptr)
    {
        *response.mutable_snapshot() = ToProto(*snapshot);
    }

    return response;
}

libriflow::v1::GetImportJobResultResponse CImportJobProtoMapper::ToProtoResultResponse(
    const LibriFlow::ApplicationJobs::SImportJobResult* result)
{
    libriflow::v1::GetImportJobResultResponse response;

    if (result != nullptr)
    {
        *response.mutable_result() = ToProto(*result);
    }

    return response;
}

std::string CImportJobProtoMapper::PathToUtf8(const std::filesystem::path& path)
{
    return ToUtf8String(path.generic_u8string());
}

std::filesystem::path CImportJobProtoMapper::PathFromUtf8(const std::string& value)
{
    return std::filesystem::path{ToUtf8StringView(value)};
}

libriflow::v1::ImportMode CImportJobProtoMapper::ToProto(const LibriFlow::Application::EImportMode mode) noexcept
{
    switch (mode)
    {
    case LibriFlow::Application::EImportMode::SingleFile:
        return libriflow::v1::IMPORT_MODE_SINGLE_FILE;
    case LibriFlow::Application::EImportMode::ZipArchive:
        return libriflow::v1::IMPORT_MODE_ZIP_ARCHIVE;
    }

    return libriflow::v1::IMPORT_MODE_UNSPECIFIED;
}

libriflow::v1::ImportJobStatus CImportJobProtoMapper::ToProto(
    const LibriFlow::ApplicationJobs::EImportJobStatus status) noexcept
{
    switch (status)
    {
    case LibriFlow::ApplicationJobs::EImportJobStatus::Pending:
        return libriflow::v1::IMPORT_JOB_STATUS_PENDING;
    case LibriFlow::ApplicationJobs::EImportJobStatus::Running:
        return libriflow::v1::IMPORT_JOB_STATUS_RUNNING;
    case LibriFlow::ApplicationJobs::EImportJobStatus::Completed:
        return libriflow::v1::IMPORT_JOB_STATUS_COMPLETED;
    case LibriFlow::ApplicationJobs::EImportJobStatus::Failed:
        return libriflow::v1::IMPORT_JOB_STATUS_FAILED;
    case LibriFlow::ApplicationJobs::EImportJobStatus::Cancelled:
        return libriflow::v1::IMPORT_JOB_STATUS_CANCELLED;
    }

    return libriflow::v1::IMPORT_JOB_STATUS_UNSPECIFIED;
}

libriflow::v1::ErrorCode CImportJobProtoMapper::ToProto(const LibriFlow::Domain::EDomainErrorCode code) noexcept
{
    switch (code)
    {
    case LibriFlow::Domain::EDomainErrorCode::Validation:
        return libriflow::v1::ERROR_CODE_VALIDATION;
    case LibriFlow::Domain::EDomainErrorCode::UnsupportedFormat:
        return libriflow::v1::ERROR_CODE_UNSUPPORTED_FORMAT;
    case LibriFlow::Domain::EDomainErrorCode::DuplicateRejected:
        return libriflow::v1::ERROR_CODE_DUPLICATE_REJECTED;
    case LibriFlow::Domain::EDomainErrorCode::DuplicateDecisionRequired:
        return libriflow::v1::ERROR_CODE_DUPLICATE_DECISION_REQUIRED;
    case LibriFlow::Domain::EDomainErrorCode::ParserFailure:
        return libriflow::v1::ERROR_CODE_PARSER_FAILURE;
    case LibriFlow::Domain::EDomainErrorCode::ConverterUnavailable:
        return libriflow::v1::ERROR_CODE_CONVERTER_UNAVAILABLE;
    case LibriFlow::Domain::EDomainErrorCode::ConverterFailed:
        return libriflow::v1::ERROR_CODE_CONVERTER_FAILED;
    case LibriFlow::Domain::EDomainErrorCode::StorageFailure:
        return libriflow::v1::ERROR_CODE_STORAGE_FAILURE;
    case LibriFlow::Domain::EDomainErrorCode::DatabaseFailure:
        return libriflow::v1::ERROR_CODE_DATABASE_FAILURE;
    case LibriFlow::Domain::EDomainErrorCode::Cancellation:
        return libriflow::v1::ERROR_CODE_CANCELLATION;
    case LibriFlow::Domain::EDomainErrorCode::IntegrityIssue:
        return libriflow::v1::ERROR_CODE_INTEGRITY_ISSUE;
    }

    return libriflow::v1::ERROR_CODE_UNSPECIFIED;
}

} // namespace LibriFlow::ProtoMapping
