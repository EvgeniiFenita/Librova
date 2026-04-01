#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ProtoMapping/ImportJobProtoMapper.hpp"

TEST_CASE("Import job proto mapper round-trips import request paths and optional hash", "[proto-mapping]")
{
    const Librova::Application::SImportRequest request{
        .SourcePaths = {std::filesystem::path{u8"C:/books/Тест.fb2"}},
        .WorkingDirectory = std::filesystem::path{u8"C:/work/Каталог"},
        .Sha256Hex = std::string{"abc123"},
        .AllowProbableDuplicates = true
    };

    const auto proto = Librova::ProtoMapping::CImportJobProtoMapper::ToProto(request);
    const auto restored = Librova::ProtoMapping::CImportJobProtoMapper::FromProto(proto);

    REQUIRE(restored.SourcePaths == request.SourcePaths);
    REQUIRE(restored.WorkingDirectory == request.WorkingDirectory);
    REQUIRE(restored.Sha256Hex == request.Sha256Hex);
    REQUIRE(restored.AllowProbableDuplicates == request.AllowProbableDuplicates);
}

TEST_CASE("Import job proto mapper maps job result into transport DTO", "[proto-mapping]")
{
    const Librova::ApplicationJobs::SImportJobResult result{
        .Snapshot = {
            .JobId = 17,
            .Status = Librova::ApplicationJobs::EImportJobStatus::Failed,
            .Percent = 80,
            .Message = "Import failed",
            .Warnings = {"warning-1", "warning-2"},
            .TotalEntries = 4,
            .ProcessedEntries = 3,
            .ImportedEntries = 1,
            .FailedEntries = 1,
            .SkippedEntries = 1
        },
        .ImportResult = Librova::Application::SImportResult{
            .Summary = {
                .Mode = Librova::Application::EImportMode::ZipArchive,
                .TotalEntries = 4,
                .ImportedEntries = 2,
                .FailedEntries = 1,
                .SkippedEntries = 1,
                .Warnings = {"summary-warning"}
            }
        },
        .Error = Librova::Domain::SDomainError{
            .Code = Librova::Domain::EDomainErrorCode::IntegrityIssue,
            .Message = "Broken entry"
        }
    };

    const auto proto = Librova::ProtoMapping::CImportJobProtoMapper::ToProto(result);

    REQUIRE(proto.snapshot().job_id() == 17);
    REQUIRE(proto.snapshot().status() == librova::v1::IMPORT_JOB_STATUS_FAILED);
    REQUIRE(proto.snapshot().warnings_size() == 2);
    REQUIRE(proto.snapshot().total_entries() == 4);
    REQUIRE(proto.snapshot().processed_entries() == 3);
    REQUIRE(proto.snapshot().imported_entries() == 1);
    REQUIRE(proto.snapshot().failed_entries() == 1);
    REQUIRE(proto.snapshot().skipped_entries() == 1);
    REQUIRE(proto.summary().mode() == librova::v1::IMPORT_MODE_ZIP_ARCHIVE);
    REQUIRE(proto.summary().imported_entries() == 2);
    REQUIRE(proto.error().code() == librova::v1::ERROR_CODE_INTEGRITY_ISSUE);
    REQUIRE(proto.error().message() == "Broken entry");
}

TEST_CASE("Import job proto mapper populates optional snapshot and result responses", "[proto-mapping]")
{
    const Librova::ApplicationJobs::SImportJobSnapshot snapshot{
        .JobId = 9,
        .Status = Librova::ApplicationJobs::EImportJobStatus::Running,
        .Percent = 33,
        .Message = "Working",
        .Warnings = {"entry warning"},
        .TotalEntries = 7,
        .ProcessedEntries = 2,
        .ImportedEntries = 1,
        .FailedEntries = 0,
        .SkippedEntries = 1
    };

    const auto snapshotResponse = Librova::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(&snapshot);
    const auto emptySnapshotResponse = Librova::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(nullptr);

    REQUIRE(snapshotResponse.has_snapshot());
    REQUIRE(snapshotResponse.snapshot().job_id() == 9);
    REQUIRE(snapshotResponse.snapshot().total_entries() == 7);
    REQUIRE(snapshotResponse.snapshot().processed_entries() == 2);
    REQUIRE(snapshotResponse.snapshot().imported_entries() == 1);
    REQUIRE(snapshotResponse.snapshot().skipped_entries() == 1);
    REQUIRE_FALSE(emptySnapshotResponse.has_snapshot());

    const Librova::ApplicationJobs::SImportJobResult result{
        .Snapshot = snapshot
    };

    const auto resultResponse = Librova::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(&result);
    const auto emptyResultResponse = Librova::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(nullptr);

    REQUIRE(resultResponse.has_result());
    REQUIRE(resultResponse.result().snapshot().job_id() == 9);
    REQUIRE_FALSE(emptyResultResponse.has_result());
}
