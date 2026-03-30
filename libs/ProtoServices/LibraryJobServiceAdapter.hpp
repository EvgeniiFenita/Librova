#pragma once

#include "ApplicationJobs/ImportJobService.hpp"
#include "import_jobs.pb.h"

namespace LibriFlow::ProtoServices {

class CLibraryJobServiceAdapter final
{
public:
    explicit CLibraryJobServiceAdapter(LibriFlow::ApplicationJobs::CImportJobService& importJobService);

    [[nodiscard]] libriflow::v1::StartImportResponse StartImport(
        const libriflow::v1::StartImportRequest& request) const;

    [[nodiscard]] libriflow::v1::GetImportJobSnapshotResponse GetImportJobSnapshot(
        const libriflow::v1::GetImportJobSnapshotRequest& request) const;

    [[nodiscard]] libriflow::v1::GetImportJobResultResponse GetImportJobResult(
        const libriflow::v1::GetImportJobResultRequest& request) const;

    [[nodiscard]] libriflow::v1::WaitImportJobResponse WaitImportJob(
        const libriflow::v1::WaitImportJobRequest& request) const;

    [[nodiscard]] libriflow::v1::CancelImportJobResponse CancelImportJob(
        const libriflow::v1::CancelImportJobRequest& request) const;

    [[nodiscard]] libriflow::v1::RemoveImportJobResponse RemoveImportJob(
        const libriflow::v1::RemoveImportJobRequest& request) const;

private:
    LibriFlow::ApplicationJobs::CImportJobService& m_importJobService;
};

} // namespace LibriFlow::ProtoServices
