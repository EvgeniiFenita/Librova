#pragma once

#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryTrashFacade.hpp"
#include "import_jobs.pb.h"

namespace Librova::ProtoMapping {

class CLibraryCatalogProtoMapper final
{
public:
    [[nodiscard]] static Librova::Application::SBookListRequest FromProto(
        const librova::v1::BookListRequest& request);

    [[nodiscard]] static librova::v1::BookListRequest ToProto(
        const Librova::Application::SBookListRequest& request);

    [[nodiscard]] static librova::v1::BookListItem ToProto(
        const Librova::Application::SBookListItem& item);

    [[nodiscard]] static librova::v1::ListBooksResponse ToProtoResponse(
        const Librova::Application::SBookListResult& result);

    [[nodiscard]] static librova::v1::BookDetails ToProto(
        const Librova::Application::SBookDetails& details);

    [[nodiscard]] static librova::v1::GetBookDetailsResponse ToProtoResponse(
        const Librova::Application::SBookDetails* details);

    [[nodiscard]] static librova::v1::LibraryStatistics ToProto(
        const Librova::Application::SLibraryStatistics& statistics);

    [[nodiscard]] static librova::v1::GetLibraryStatisticsResponse ToProtoResponse(
        const Librova::Application::SLibraryStatistics& statistics);

    [[nodiscard]] static Librova::Application::SExportBookRequest FromProto(
        const librova::v1::ExportBookRequest& request);

    [[nodiscard]] static librova::v1::ExportBookResponse ToProtoResponse(
        const std::filesystem::path* exportedPath);

    [[nodiscard]] static librova::v1::MoveBookToTrashResponse ToProtoResponse(
        const Librova::Application::STrashedBookResult* result);

private:
    [[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path PathFromUtf8(const std::string& value);
    [[nodiscard]] static Librova::Domain::EBookFormat FromProto(librova::v1::BookFormat format);
    [[nodiscard]] static Librova::Domain::EBookSort FromProto(librova::v1::BookSort sort) noexcept;
    [[nodiscard]] static librova::v1::BookFormat ToProto(Librova::Domain::EBookFormat format) noexcept;
    [[nodiscard]] static librova::v1::BookSort ToProto(Librova::Domain::EBookSort sort) noexcept;
};

} // namespace Librova::ProtoMapping
