#pragma once

#include "ApplicationJobs/ImportJobService.hpp"
#include "import_jobs.pb.h"

namespace Librova::ProtoServices {

class CLibraryJobServiceAdapter final
{
public:
    explicit CLibraryJobServiceAdapter(Librova::ApplicationJobs::CImportJobService& importJobService);

    [[nodiscard]] librova::v1::StartImportResponse StartImport(
        const librova::v1::StartImportRequest& request) const;

    [[nodiscard]] librova::v1::GetImportJobSnapshotResponse GetImportJobSnapshot(
        const librova::v1::GetImportJobSnapshotRequest& request) const;

    [[nodiscard]] librova::v1::GetImportJobResultResponse GetImportJobResult(
        const librova::v1::GetImportJobResultRequest& request) const;

    [[nodiscard]] librova::v1::WaitImportJobResponse WaitImportJob(
        const librova::v1::WaitImportJobRequest& request) const;

    [[nodiscard]] librova::v1::CancelImportJobResponse CancelImportJob(
        const librova::v1::CancelImportJobRequest& request) const;

    [[nodiscard]] librova::v1::RemoveImportJobResponse RemoveImportJob(
        const librova::v1::RemoveImportJobRequest& request) const;

private:
    Librova::ApplicationJobs::CImportJobService& m_importJobService;
};

} // namespace Librova::ProtoServices
