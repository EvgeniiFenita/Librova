#include "ProtoMapping/LibraryCatalogProtoMapper.hpp"

#include <chrono>
#include <stdexcept>

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ProtoMapping {
namespace {

std::int64_t ToUnixMilliseconds(const std::chrono::system_clock::time_point timePoint)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
}

} // namespace

Librova::Application::SBookListRequest CLibraryCatalogProtoMapper::FromProto(
    const librova::v1::BookListRequest& request)
{
    Librova::Application::SBookListRequest result{
        .TextUtf8 = request.text(),
        .Offset = static_cast<std::size_t>(request.offset()),
        .Limit = static_cast<std::size_t>(request.limit())
    };

    if (request.has_author())
    {
        result.AuthorUtf8 = request.author();
    }

    if (request.has_language())
    {
        result.Language = request.language();
    }

    if (request.has_series())
    {
        result.SeriesUtf8 = request.series();
    }

    result.TagsUtf8.assign(request.tags().begin(), request.tags().end());

    if (request.has_format())
    {
        result.Format = FromProto(request.format());
    }

    if (request.has_sort_by())
    {
        result.SortBy = FromProto(request.sort_by());
    }

    if (request.has_sort_direction())
    {
        result.SortDirection = FromProto(request.sort_direction());
    }

    return result;
}

librova::v1::BookListRequest CLibraryCatalogProtoMapper::ToProto(
    const Librova::Application::SBookListRequest& request)
{
    librova::v1::BookListRequest proto;
    proto.set_text(request.TextUtf8);
    proto.set_offset(request.Offset);
    proto.set_limit(request.Limit);

    if (request.AuthorUtf8.has_value())
    {
        proto.set_author(*request.AuthorUtf8);
    }

    if (request.Language.has_value())
    {
        proto.set_language(*request.Language);
    }

    if (request.SeriesUtf8.has_value())
    {
        proto.set_series(*request.SeriesUtf8);
    }

    for (const std::string& tag : request.TagsUtf8)
    {
        proto.add_tags(tag);
    }

    if (request.Format.has_value())
    {
        proto.set_format(ToProto(*request.Format));
    }

    if (request.SortBy.has_value())
    {
        proto.set_sort_by(ToProto(*request.SortBy));
    }

    if (request.SortDirection.has_value())
    {
        proto.set_sort_direction(ToProto(*request.SortDirection));
    }

    return proto;
}

librova::v1::BookListItem CLibraryCatalogProtoMapper::ToProto(
    const Librova::Application::SBookListItem& item)
{
    librova::v1::BookListItem proto;
    proto.set_book_id(item.Id.Value);
    proto.set_title(item.TitleUtf8);
    proto.set_language(item.Language);
    proto.set_format(ToProto(item.Format));
    proto.set_managed_path(PathToUtf8(item.ManagedPath));
    proto.set_size_bytes(item.SizeBytes);
    proto.set_added_at_unix_ms(ToUnixMilliseconds(item.AddedAtUtc));

    for (const std::string& author : item.AuthorsUtf8)
    {
        proto.add_authors(author);
    }

    for (const std::string& tag : item.TagsUtf8)
    {
        proto.add_tags(tag);
    }

    if (item.SeriesUtf8.has_value())
    {
        proto.set_series(*item.SeriesUtf8);
    }

    if (item.SeriesIndex.has_value())
    {
        proto.set_series_index(*item.SeriesIndex);
    }

    if (item.Year.has_value())
    {
        proto.set_year(*item.Year);
    }

    if (item.CoverPath.has_value())
    {
        proto.set_cover_path(PathToUtf8(*item.CoverPath));
    }

    return proto;
}

librova::v1::ListBooksResponse CLibraryCatalogProtoMapper::ToProtoResponse(
    const Librova::Application::SBookListResult& result)
{
    librova::v1::ListBooksResponse response;
    response.set_total_count(result.TotalCount);
    for (const std::string& language : result.AvailableLanguages)
    {
        response.add_available_languages(language);
    }

    for (const Librova::Application::SBookListItem& item : result.Items)
    {
        *response.add_items() = ToProto(item);
    }

    return response;
}

librova::v1::BookDetails CLibraryCatalogProtoMapper::ToProto(
    const Librova::Application::SBookDetails& details)
{
    librova::v1::BookDetails proto;
    proto.set_book_id(details.Id.Value);
    proto.set_title(details.TitleUtf8);
    proto.set_language(details.Language);
    proto.set_format(ToProto(details.Format));
    proto.set_managed_path(PathToUtf8(details.ManagedPath));
    proto.set_size_bytes(details.SizeBytes);
    proto.set_sha256_hex(details.Sha256Hex);
    proto.set_added_at_unix_ms(ToUnixMilliseconds(details.AddedAtUtc));

    for (const std::string& author : details.AuthorsUtf8)
    {
        proto.add_authors(author);
    }

    for (const std::string& tag : details.TagsUtf8)
    {
        proto.add_tags(tag);
    }

    if (details.SeriesUtf8.has_value())
    {
        proto.set_series(*details.SeriesUtf8);
    }

    if (details.SeriesIndex.has_value())
    {
        proto.set_series_index(*details.SeriesIndex);
    }

    if (details.PublisherUtf8.has_value())
    {
        proto.set_publisher(*details.PublisherUtf8);
    }

    if (details.Year.has_value())
    {
        proto.set_year(*details.Year);
    }

    if (details.Isbn.has_value())
    {
        proto.set_isbn(*details.Isbn);
    }

    if (details.DescriptionUtf8.has_value())
    {
        proto.set_description(*details.DescriptionUtf8);
    }

    if (details.Identifier.has_value())
    {
        proto.set_identifier(*details.Identifier);
    }

    if (details.CoverPath.has_value())
    {
        proto.set_cover_path(PathToUtf8(*details.CoverPath));
    }

    return proto;
}

librova::v1::GetBookDetailsResponse CLibraryCatalogProtoMapper::ToProtoResponse(
    const Librova::Application::SBookDetails* details)
{
    librova::v1::GetBookDetailsResponse response;
    if (details != nullptr)
    {
        *response.mutable_details() = ToProto(*details);
    }

    return response;
}

librova::v1::LibraryStatistics CLibraryCatalogProtoMapper::ToProto(
    const Librova::Application::SLibraryStatistics& statistics)
{
    librova::v1::LibraryStatistics proto;
    proto.set_book_count(statistics.BookCount);
    proto.set_total_managed_book_size_bytes(statistics.TotalManagedBookSizeBytes);
    proto.set_total_library_size_bytes(statistics.TotalLibrarySizeBytes);
    return proto;
}

librova::v1::GetLibraryStatisticsResponse CLibraryCatalogProtoMapper::ToProtoResponse(
    const Librova::Application::SLibraryStatistics& statistics)
{
    librova::v1::GetLibraryStatisticsResponse response;
    *response.mutable_statistics() = ToProto(statistics);
    return response;
}

Librova::Application::SExportBookRequest CLibraryCatalogProtoMapper::FromProto(
    const librova::v1::ExportBookRequest& request)
{
    Librova::Application::SExportBookRequest result{
        .BookId = Librova::Domain::SBookId{request.book_id()},
        .DestinationPath = PathFromUtf8(request.destination_path())
    };

    if (request.export_format() != librova::v1::BOOK_FORMAT_UNSPECIFIED)
    {
        result.ExportFormat = FromProto(request.export_format());
    }

    return result;
}

librova::v1::ExportBookResponse CLibraryCatalogProtoMapper::ToProtoResponse(
    const std::filesystem::path* exportedPath)
{
    librova::v1::ExportBookResponse response;
    if (exportedPath != nullptr)
    {
        response.set_exported_path(PathToUtf8(*exportedPath));
    }

    return response;
}

librova::v1::MoveBookToTrashResponse CLibraryCatalogProtoMapper::ToProtoResponse(
    const Librova::Application::STrashedBookResult* result)
{
    librova::v1::MoveBookToTrashResponse response;
    if (result != nullptr)
    {
        response.set_trashed_book_id(result->BookId.Value);
        response.set_destination(
            result->Destination == Librova::Application::ETrashDestination::RecycleBin
                ? librova::v1::DELETE_DESTINATION_RECYCLE_BIN
                : librova::v1::DELETE_DESTINATION_MANAGED_TRASH);
    }

    return response;
}

std::string CLibraryCatalogProtoMapper::PathToUtf8(const std::filesystem::path& path)
{
    return Librova::Unicode::PathToUtf8(path);
}

std::filesystem::path CLibraryCatalogProtoMapper::PathFromUtf8(const std::string& value)
{
    return Librova::Unicode::PathFromUtf8(value);
}

Librova::Domain::EBookFormat CLibraryCatalogProtoMapper::FromProto(const librova::v1::BookFormat format)
{
    switch (format)
    {
    case librova::v1::BOOK_FORMAT_FB2:
        return Librova::Domain::EBookFormat::Fb2;
    case librova::v1::BOOK_FORMAT_EPUB:
        return Librova::Domain::EBookFormat::Epub;
    case librova::v1::BOOK_FORMAT_ZIP:
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Error(
                "catalog proto mapper received unsupported book format BOOK_FORMAT_ZIP ({}); "
                "rejecting to prevent contract drift",
                static_cast<int>(format));
        }
        throw std::invalid_argument("BOOK_FORMAT_ZIP is not a valid catalog book format");
    case librova::v1::BOOK_FORMAT_UNSPECIFIED:
    default:
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Error(
                "catalog proto mapper received unexpected book format value {}; "
                "rejecting to prevent contract drift",
                static_cast<int>(format));
        }
        throw std::invalid_argument(
            "unexpected book format value in catalog proto mapper: " + std::to_string(static_cast<int>(format)));
    }
}

Librova::Domain::EBookSort CLibraryCatalogProtoMapper::FromProto(const librova::v1::BookSort sort) noexcept
{
    switch (sort)
    {
    case librova::v1::BOOK_SORT_AUTHOR:
        return Librova::Domain::EBookSort::Author;
    case librova::v1::BOOK_SORT_DATE_ADDED:
        return Librova::Domain::EBookSort::DateAdded;
    case librova::v1::BOOK_SORT_SERIES:
        return Librova::Domain::EBookSort::Series;
    case librova::v1::BOOK_SORT_YEAR:
        return Librova::Domain::EBookSort::Year;
    case librova::v1::BOOK_SORT_FILE_SIZE:
        return Librova::Domain::EBookSort::FileSize;
    case librova::v1::BOOK_SORT_TITLE:
    case librova::v1::BOOK_SORT_UNSPECIFIED:
    default:
        return Librova::Domain::EBookSort::Title;
    }
}

librova::v1::BookFormat CLibraryCatalogProtoMapper::ToProto(const Librova::Domain::EBookFormat format) noexcept
{
    switch (format)
    {
    case Librova::Domain::EBookFormat::Fb2:
        return librova::v1::BOOK_FORMAT_FB2;
    case Librova::Domain::EBookFormat::Epub:
    default:
        return librova::v1::BOOK_FORMAT_EPUB;
    }
}

librova::v1::BookSort CLibraryCatalogProtoMapper::ToProto(const Librova::Domain::EBookSort sort) noexcept
{
    switch (sort)
    {
    case Librova::Domain::EBookSort::Author:
        return librova::v1::BOOK_SORT_AUTHOR;
    case Librova::Domain::EBookSort::DateAdded:
        return librova::v1::BOOK_SORT_DATE_ADDED;
    case Librova::Domain::EBookSort::Series:
        return librova::v1::BOOK_SORT_SERIES;
    case Librova::Domain::EBookSort::Year:
        return librova::v1::BOOK_SORT_YEAR;
    case Librova::Domain::EBookSort::FileSize:
        return librova::v1::BOOK_SORT_FILE_SIZE;
    case Librova::Domain::EBookSort::Title:
    default:
        return librova::v1::BOOK_SORT_TITLE;
    }
}

Librova::Domain::ESortDirection CLibraryCatalogProtoMapper::FromProto(
    const librova::v1::BookSortDirection dir) noexcept
{
    switch (dir)
    {
    case librova::v1::BOOK_SORT_DIRECTION_DESC:
        return Librova::Domain::ESortDirection::Descending;
    case librova::v1::BOOK_SORT_DIRECTION_ASC:
    case librova::v1::BOOK_SORT_DIRECTION_UNSPECIFIED:
    default:
        return Librova::Domain::ESortDirection::Ascending;
    }
}

librova::v1::BookSortDirection CLibraryCatalogProtoMapper::ToProto(
    const Librova::Domain::ESortDirection dir) noexcept
{
    switch (dir)
    {
    case Librova::Domain::ESortDirection::Descending:
        return librova::v1::BOOK_SORT_DIRECTION_DESC;
    case Librova::Domain::ESortDirection::Ascending:
    default:
        return librova::v1::BOOK_SORT_DIRECTION_ASC;
    }
}

} // namespace Librova::ProtoMapping
