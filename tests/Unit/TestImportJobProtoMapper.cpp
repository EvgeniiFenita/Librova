#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ProtoMapping/ImportJobProtoMapper.hpp"

TEST_CASE("Import job proto mapper round-trips import request paths and optional hash", "[proto-mapping]")
{
    const LibriFlow::Application::SImportRequest request{
        .SourcePath = std::filesystem::path{u8"C:/books/Тест.fb2"},
        .WorkingDirectory = std::filesystem::path{u8"C:/work/Каталог"},
        .Sha256Hex = std::string{"abc123"},
        .AllowProbableDuplicates = true
    };

    const auto proto = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProto(request);
    const auto restored = LibriFlow::ProtoMapping::CImportJobProtoMapper::FromProto(proto);

    REQUIRE(restored.SourcePath == request.SourcePath);
    REQUIRE(restored.WorkingDirectory == request.WorkingDirectory);
    REQUIRE(restored.Sha256Hex == request.Sha256Hex);
    REQUIRE(restored.AllowProbableDuplicates == request.AllowProbableDuplicates);
}

TEST_CASE("Import job proto mapper maps job result into transport DTO", "[proto-mapping]")
{
    const LibriFlow::ApplicationJobs::SImportJobResult result{
        .Snapshot = {
            .JobId = 17,
            .Status = LibriFlow::ApplicationJobs::EImportJobStatus::Failed,
            .Percent = 80,
            .Message = "Import failed",
            .Warnings = {"warning-1", "warning-2"}
        },
        .ImportResult = LibriFlow::Application::SImportResult{
            .Summary = {
                .Mode = LibriFlow::Application::EImportMode::ZipArchive,
                .TotalEntries = 4,
                .ImportedEntries = 2,
                .FailedEntries = 1,
                .SkippedEntries = 1,
                .Warnings = {"summary-warning"}
            }
        },
        .Error = LibriFlow::Domain::SDomainError{
            .Code = LibriFlow::Domain::EDomainErrorCode::IntegrityIssue,
            .Message = "Broken entry"
        }
    };

    const auto proto = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProto(result);

    REQUIRE(proto.snapshot().job_id() == 17);
    REQUIRE(proto.snapshot().status() == libriflow::v1::IMPORT_JOB_STATUS_FAILED);
    REQUIRE(proto.snapshot().warnings_size() == 2);
    REQUIRE(proto.summary().mode() == libriflow::v1::IMPORT_MODE_ZIP_ARCHIVE);
    REQUIRE(proto.summary().imported_entries() == 2);
    REQUIRE(proto.error().code() == libriflow::v1::ERROR_CODE_INTEGRITY_ISSUE);
    REQUIRE(proto.error().message() == "Broken entry");
}

TEST_CASE("Import job proto mapper populates optional snapshot and result responses", "[proto-mapping]")
{
    const LibriFlow::ApplicationJobs::SImportJobSnapshot snapshot{
        .JobId = 9,
        .Status = LibriFlow::ApplicationJobs::EImportJobStatus::Running,
        .Percent = 33,
        .Message = "Working"
    };

    const auto snapshotResponse = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(&snapshot);
    const auto emptySnapshotResponse = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(nullptr);

    REQUIRE(snapshotResponse.has_snapshot());
    REQUIRE(snapshotResponse.snapshot().job_id() == 9);
    REQUIRE_FALSE(emptySnapshotResponse.has_snapshot());

    const LibriFlow::ApplicationJobs::SImportJobResult result{
        .Snapshot = snapshot
    };

    const auto resultResponse = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(&result);
    const auto emptyResultResponse = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(nullptr);

    REQUIRE(resultResponse.has_result());
    REQUIRE(resultResponse.result().snapshot().job_id() == 9);
    REQUIRE_FALSE(emptyResultResponse.has_result());
}
