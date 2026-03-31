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

namespace Librova::ProtoMapping {

Librova::Application::SImportRequest CImportJobProtoMapper::FromProto(const librova::v1::ImportRequest& request)
{
    std::vector<std::filesystem::path> sourcePaths;
    sourcePaths.reserve(static_cast<std::size_t>(request.source_paths_size()));
    for (const auto& sourcePath : request.source_paths())
    {
        sourcePaths.push_back(PathFromUtf8(sourcePath));
    }

    return {
        .SourcePaths = std::move(sourcePaths),
        .WorkingDirectory = PathFromUtf8(request.working_directory()),
        .Sha256Hex = request.has_sha256_hex() ? std::optional<std::string>{request.sha256_hex()} : std::nullopt,
        .AllowProbableDuplicates = request.allow_probable_duplicates()
    };
}

Librova::Domain::SDomainError CImportJobProtoMapper::FromProto(const librova::v1::DomainError& error)
{
    return {
        .Code = FromProto(error.code()),
        .Message = error.message()
    };
}

Librova::Application::SImportSummary CImportJobProtoMapper::FromProto(const librova::v1::ImportSummary& summary)
{
    Librova::Application::SImportSummary result;
    result.Mode = FromProto(summary.mode());
    result.TotalEntries = summary.total_entries();
    result.ImportedEntries = summary.imported_entries();
    result.FailedEntries = summary.failed_entries();
    result.SkippedEntries = summary.skipped_entries();
    result.Warnings.assign(summary.warnings().begin(), summary.warnings().end());
    return result;
}

Librova::ApplicationJobs::SImportJobSnapshot CImportJobProtoMapper::FromProto(
    const librova::v1::ImportJobSnapshot& snapshot)
{
    Librova::ApplicationJobs::SImportJobSnapshot result;
    result.JobId = snapshot.job_id();
    result.Status = FromProto(snapshot.status());
    result.Percent = snapshot.percent();
    result.Message = snapshot.message();
    result.Warnings.assign(snapshot.warnings().begin(), snapshot.warnings().end());
    return result;
}

Librova::ApplicationJobs::SImportJobResult CImportJobProtoMapper::FromProto(
    const librova::v1::ImportJobResult& result)
{
    Librova::ApplicationJobs::SImportJobResult mapped;
    mapped.Snapshot = FromProto(result.snapshot());

    if (result.has_summary())
    {
        Librova::Application::SImportResult importResult;
        importResult.Summary = FromProto(result.summary());
        mapped.ImportResult = std::move(importResult);
    }

    if (result.has_error())
    {
        mapped.Error = FromProto(result.error());
    }

    return mapped;
}

librova::v1::ImportRequest CImportJobProtoMapper::ToProto(const Librova::Application::SImportRequest& request)
{
    librova::v1::ImportRequest message;
    for (const auto& sourcePath : request.SourcePaths)
    {
        message.add_source_paths(PathToUtf8(sourcePath));
    }
    message.set_working_directory(PathToUtf8(request.WorkingDirectory));

    if (request.Sha256Hex.has_value())
    {
        message.set_sha256_hex(*request.Sha256Hex);
    }

    message.set_allow_probable_duplicates(request.AllowProbableDuplicates);
    return message;
}

librova::v1::DomainError CImportJobProtoMapper::ToProto(const Librova::Domain::SDomainError& error)
{
    librova::v1::DomainError message;
    message.set_code(ToProto(error.Code));
    message.set_message(error.Message);
    return message;
}

librova::v1::ImportSummary CImportJobProtoMapper::ToProto(const Librova::Application::SImportSummary& summary)
{
    librova::v1::ImportSummary message;
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

librova::v1::ImportJobSnapshot CImportJobProtoMapper::ToProto(
    const Librova::ApplicationJobs::SImportJobSnapshot& snapshot)
{
    librova::v1::ImportJobSnapshot message;
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

librova::v1::ImportJobResult CImportJobProtoMapper::ToProto(
    const Librova::ApplicationJobs::SImportJobResult& result)
{
    librova::v1::ImportJobResult message;
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

librova::v1::StartImportResponse CImportJobProtoMapper::ToProtoStartResponse(
    const Librova::ApplicationJobs::TImportJobId jobId)
{
    librova::v1::StartImportResponse response;
    response.set_job_id(jobId);
    return response;
}

librova::v1::GetImportJobSnapshotResponse CImportJobProtoMapper::ToProtoSnapshotResponse(
    const Librova::ApplicationJobs::SImportJobSnapshot* snapshot)
{
    librova::v1::GetImportJobSnapshotResponse response;

    if (snapshot != nullptr)
    {
        *response.mutable_snapshot() = ToProto(*snapshot);
    }

    return response;
}

librova::v1::GetImportJobResultResponse CImportJobProtoMapper::ToProtoResultResponse(
    const Librova::ApplicationJobs::SImportJobResult* result)
{
    librova::v1::GetImportJobResultResponse response;

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

Librova::Application::EImportMode CImportJobProtoMapper::FromProto(const librova::v1::ImportMode mode) noexcept
{
    switch (mode)
    {
    case librova::v1::IMPORT_MODE_SINGLE_FILE:
        return Librova::Application::EImportMode::SingleFile;
    case librova::v1::IMPORT_MODE_ZIP_ARCHIVE:
        return Librova::Application::EImportMode::ZipArchive;
    case librova::v1::IMPORT_MODE_BATCH:
        return Librova::Application::EImportMode::Batch;
    case librova::v1::IMPORT_MODE_UNSPECIFIED:
        break;
    }

    return Librova::Application::EImportMode::SingleFile;
}

Librova::ApplicationJobs::EImportJobStatus CImportJobProtoMapper::FromProto(
    const librova::v1::ImportJobStatus status) noexcept
{
    switch (status)
    {
    case librova::v1::IMPORT_JOB_STATUS_PENDING:
        return Librova::ApplicationJobs::EImportJobStatus::Pending;
    case librova::v1::IMPORT_JOB_STATUS_RUNNING:
        return Librova::ApplicationJobs::EImportJobStatus::Running;
    case librova::v1::IMPORT_JOB_STATUS_COMPLETED:
        return Librova::ApplicationJobs::EImportJobStatus::Completed;
    case librova::v1::IMPORT_JOB_STATUS_FAILED:
        return Librova::ApplicationJobs::EImportJobStatus::Failed;
    case librova::v1::IMPORT_JOB_STATUS_CANCELLED:
        return Librova::ApplicationJobs::EImportJobStatus::Cancelled;
    case librova::v1::IMPORT_JOB_STATUS_UNSPECIFIED:
        break;
    }

    return Librova::ApplicationJobs::EImportJobStatus::Failed;
}

Librova::Domain::EDomainErrorCode CImportJobProtoMapper::FromProto(const librova::v1::ErrorCode code) noexcept
{
    switch (code)
    {
    case librova::v1::ERROR_CODE_VALIDATION:
        return Librova::Domain::EDomainErrorCode::Validation;
    case librova::v1::ERROR_CODE_UNSUPPORTED_FORMAT:
        return Librova::Domain::EDomainErrorCode::UnsupportedFormat;
    case librova::v1::ERROR_CODE_DUPLICATE_REJECTED:
        return Librova::Domain::EDomainErrorCode::DuplicateRejected;
    case librova::v1::ERROR_CODE_DUPLICATE_DECISION_REQUIRED:
        return Librova::Domain::EDomainErrorCode::DuplicateDecisionRequired;
    case librova::v1::ERROR_CODE_PARSER_FAILURE:
        return Librova::Domain::EDomainErrorCode::ParserFailure;
    case librova::v1::ERROR_CODE_CONVERTER_UNAVAILABLE:
        return Librova::Domain::EDomainErrorCode::ConverterUnavailable;
    case librova::v1::ERROR_CODE_CONVERTER_FAILED:
        return Librova::Domain::EDomainErrorCode::ConverterFailed;
    case librova::v1::ERROR_CODE_STORAGE_FAILURE:
        return Librova::Domain::EDomainErrorCode::StorageFailure;
    case librova::v1::ERROR_CODE_DATABASE_FAILURE:
        return Librova::Domain::EDomainErrorCode::DatabaseFailure;
    case librova::v1::ERROR_CODE_CANCELLATION:
        return Librova::Domain::EDomainErrorCode::Cancellation;
    case librova::v1::ERROR_CODE_INTEGRITY_ISSUE:
        return Librova::Domain::EDomainErrorCode::IntegrityIssue;
    case librova::v1::ERROR_CODE_UNSPECIFIED:
        break;
    }

    return Librova::Domain::EDomainErrorCode::IntegrityIssue;
}

librova::v1::ImportMode CImportJobProtoMapper::ToProto(const Librova::Application::EImportMode mode) noexcept
{
    switch (mode)
    {
    case Librova::Application::EImportMode::SingleFile:
        return librova::v1::IMPORT_MODE_SINGLE_FILE;
    case Librova::Application::EImportMode::ZipArchive:
        return librova::v1::IMPORT_MODE_ZIP_ARCHIVE;
    case Librova::Application::EImportMode::Batch:
        return librova::v1::IMPORT_MODE_BATCH;
    }

    return librova::v1::IMPORT_MODE_UNSPECIFIED;
}

librova::v1::ImportJobStatus CImportJobProtoMapper::ToProto(
    const Librova::ApplicationJobs::EImportJobStatus status) noexcept
{
    switch (status)
    {
    case Librova::ApplicationJobs::EImportJobStatus::Pending:
        return librova::v1::IMPORT_JOB_STATUS_PENDING;
    case Librova::ApplicationJobs::EImportJobStatus::Running:
        return librova::v1::IMPORT_JOB_STATUS_RUNNING;
    case Librova::ApplicationJobs::EImportJobStatus::Completed:
        return librova::v1::IMPORT_JOB_STATUS_COMPLETED;
    case Librova::ApplicationJobs::EImportJobStatus::Failed:
        return librova::v1::IMPORT_JOB_STATUS_FAILED;
    case Librova::ApplicationJobs::EImportJobStatus::Cancelled:
        return librova::v1::IMPORT_JOB_STATUS_CANCELLED;
    }

    return librova::v1::IMPORT_JOB_STATUS_UNSPECIFIED;
}

librova::v1::ErrorCode CImportJobProtoMapper::ToProto(const Librova::Domain::EDomainErrorCode code) noexcept
{
    switch (code)
    {
    case Librova::Domain::EDomainErrorCode::Validation:
        return librova::v1::ERROR_CODE_VALIDATION;
    case Librova::Domain::EDomainErrorCode::UnsupportedFormat:
        return librova::v1::ERROR_CODE_UNSUPPORTED_FORMAT;
    case Librova::Domain::EDomainErrorCode::DuplicateRejected:
        return librova::v1::ERROR_CODE_DUPLICATE_REJECTED;
    case Librova::Domain::EDomainErrorCode::DuplicateDecisionRequired:
        return librova::v1::ERROR_CODE_DUPLICATE_DECISION_REQUIRED;
    case Librova::Domain::EDomainErrorCode::ParserFailure:
        return librova::v1::ERROR_CODE_PARSER_FAILURE;
    case Librova::Domain::EDomainErrorCode::ConverterUnavailable:
        return librova::v1::ERROR_CODE_CONVERTER_UNAVAILABLE;
    case Librova::Domain::EDomainErrorCode::ConverterFailed:
        return librova::v1::ERROR_CODE_CONVERTER_FAILED;
    case Librova::Domain::EDomainErrorCode::StorageFailure:
        return librova::v1::ERROR_CODE_STORAGE_FAILURE;
    case Librova::Domain::EDomainErrorCode::DatabaseFailure:
        return librova::v1::ERROR_CODE_DATABASE_FAILURE;
    case Librova::Domain::EDomainErrorCode::Cancellation:
        return librova::v1::ERROR_CODE_CANCELLATION;
    case Librova::Domain::EDomainErrorCode::IntegrityIssue:
        return librova::v1::ERROR_CODE_INTEGRITY_ISSUE;
    }

    return librova::v1::ERROR_CODE_UNSPECIFIED;
}

} // namespace Librova::ProtoMapping
