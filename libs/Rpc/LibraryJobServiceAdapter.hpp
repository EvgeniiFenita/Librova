#pragma once

#include "App/LibraryImportFacade.hpp"
#include "App/LibraryCatalogFacade.hpp"
#include "App/LibraryExportFacade.hpp"
#include "App/LibraryTrashFacade.hpp"
#include "App/ImportJobService.hpp"
#include "import_jobs.pb.h"

namespace Librova::ProtoServices {

class CLibraryJobServiceAdapter final
{
public:
    CLibraryJobServiceAdapter(
        Librova::ApplicationJobs::CImportJobService& importJobService,
        const Librova::Application::CLibraryImportFacade& libraryImportFacade,
        const Librova::Application::CLibraryCatalogFacade& libraryCatalogFacade,
        const Librova::Application::CLibraryExportFacade& libraryExportFacade,
        const Librova::Application::CLibraryTrashFacade& libraryTrashFacade);

    [[nodiscard]] librova::v1::StartImportResponse StartImport(
        const librova::v1::StartImportRequest& request) const;

    [[nodiscard]] librova::v1::ListBooksResponse ListBooks(
        const librova::v1::ListBooksRequest& request) const;

    [[nodiscard]] librova::v1::GetBookDetailsResponse GetBookDetails(
        const librova::v1::GetBookDetailsRequest& request) const;

    [[nodiscard]] librova::v1::ExportBookResponse ExportBook(
        const librova::v1::ExportBookRequest& request) const;

    [[nodiscard]] librova::v1::MoveBookToTrashResponse MoveBookToTrash(
        const librova::v1::MoveBookToTrashRequest& request) const;

    [[nodiscard]] librova::v1::ListCollectionsResponse ListCollections(
        const librova::v1::ListCollectionsRequest& request) const;

    [[nodiscard]] librova::v1::CreateCollectionResponse CreateCollection(
        const librova::v1::CreateCollectionRequest& request) const;

    [[nodiscard]] librova::v1::DeleteCollectionResponse DeleteCollection(
        const librova::v1::DeleteCollectionRequest& request) const;

    [[nodiscard]] librova::v1::AddBookToCollectionResponse AddBookToCollection(
        const librova::v1::AddBookToCollectionRequest& request) const;

    [[nodiscard]] librova::v1::RemoveBookFromCollectionResponse RemoveBookFromCollection(
        const librova::v1::RemoveBookFromCollectionRequest& request) const;

    [[nodiscard]] librova::v1::GetImportJobSnapshotResponse GetImportJobSnapshot(
        const librova::v1::GetImportJobSnapshotRequest& request) const;

    [[nodiscard]] librova::v1::GetImportJobResultResponse GetImportJobResult(
        const librova::v1::GetImportJobResultRequest& request) const;

    [[nodiscard]] librova::v1::WaitImportJobResponse WaitImportJob(
        const librova::v1::WaitImportJobRequest& request) const;

    [[nodiscard]] librova::v1::ValidateImportSourcesResponse ValidateImportSources(
        const librova::v1::ValidateImportSourcesRequest& request) const;

    [[nodiscard]] librova::v1::CancelImportJobResponse CancelImportJob(
        const librova::v1::CancelImportJobRequest& request) const;

    [[nodiscard]] librova::v1::RemoveImportJobResponse RemoveImportJob(
        const librova::v1::RemoveImportJobRequest& request) const;

private:
    Librova::ApplicationJobs::CImportJobService& m_importJobService;
    const Librova::Application::CLibraryImportFacade& m_libraryImportFacade;
    const Librova::Application::CLibraryCatalogFacade& m_libraryCatalogFacade;
    const Librova::Application::CLibraryExportFacade& m_libraryExportFacade;
    const Librova::Application::CLibraryTrashFacade& m_libraryTrashFacade;
};

} // namespace Librova::ProtoServices
