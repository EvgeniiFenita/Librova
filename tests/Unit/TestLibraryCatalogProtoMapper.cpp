#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "Rpc/LibraryCatalogProtoMapper.hpp"

TEST_CASE("Library catalog proto mapper round-trips book list request filters", "[proto-mapping][catalog]")
{
    const Librova::Application::SBookListRequest request{
        .TextUtf8 = "zone",
        .AuthorUtf8 = std::string{"Arkady Strugatsky"},
        .Languages = {"en", "ru"},
        .GenresUtf8 = {"classic", "sci-fi"},
        .SeriesUtf8 = std::string{"Noon Universe"},
        .TagsUtf8 = {"classic", "sci-fi"},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SortBy = Librova::Domain::EBookSort::DateAdded,
        .SortDirection = Librova::Domain::ESortDirection::Descending,
        .Offset = 10,
        .Limit = 25
    };

    const auto proto = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProto(request);
    const auto restored = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(proto);

    REQUIRE(restored.TextUtf8 == request.TextUtf8);
    REQUIRE(restored.AuthorUtf8 == request.AuthorUtf8);
    REQUIRE(restored.Languages == request.Languages);
    REQUIRE(restored.GenresUtf8 == request.GenresUtf8);
    REQUIRE(restored.SeriesUtf8 == request.SeriesUtf8);
    REQUIRE(restored.TagsUtf8 == request.TagsUtf8);
    REQUIRE(restored.Format == request.Format);
    REQUIRE(restored.SortBy == request.SortBy);
    REQUIRE(restored.SortDirection == request.SortDirection);
    REQUIRE(restored.Offset == request.Offset);
    REQUIRE(restored.Limit == request.Limit);
}

TEST_CASE("Library catalog proto mapper round-trips sort direction ascending", "[proto-mapping][catalog]")
{
    const Librova::Application::SBookListRequest request{
        .SortBy = Librova::Domain::EBookSort::Title,
        .SortDirection = Librova::Domain::ESortDirection::Ascending,
        .Limit = 10
    };

    const auto proto = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProto(request);
    const auto restored = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(proto);

    REQUIRE(restored.SortDirection == Librova::Domain::ESortDirection::Ascending);
}

TEST_CASE("Library catalog proto mapper treats absent sort direction as ascending", "[proto-mapping][catalog]")
{
    librova::v1::BookListRequest proto;
    // sort_direction field is not set — simulates old client or explicit omission

    const auto restored = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(proto);

    // nullopt means "no preference"; SQL query defaults to ASC
    REQUIRE_FALSE(restored.SortDirection.has_value());
}

TEST_CASE("Library catalog proto mapper builds list response with safe storage metadata", "[proto-mapping][catalog]")
{
    Librova::Application::SBookListItem item;
    item.Id = Librova::Domain::SBookId{12};
    item.TitleUtf8 = "Пикник на обочине";
    item.AuthorsUtf8 = {"Аркадий Стругацкий", "Борис Стругацкий"};
    item.Language = "ru";
    item.SeriesUtf8 = std::string{"Миры"};
    item.SeriesIndex = 1.5;
    item.Year = 1972;
    item.TagsUtf8 = {"classic", "sci-fi"};
    item.Format = Librova::Domain::EBookFormat::Epub;
    item.ManagedPath = std::filesystem::path(u8"Objects/8a/5b/0000000012.book.Пикник.epub");
    item.CoverPath = std::filesystem::path(u8"Objects/8a/5b/0000000012.cover.jpg");
    item.SizeBytes = 4096;
    item.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    item.Collections = {{
        .Id = 3,
        .NameUtf8 = "Favorites",
        .IconKey = "star",
        .Kind = Librova::Domain::EBookCollectionKind::User,
        .IsDeletable = true
    }};

    const Librova::Application::SBookListResult result{
        .Items = {item},
        .TotalCount = 12,
        .AvailableLanguages = {{"en", 5}, {"ru", 7}},
        .AvailableGenres = {{"classic", 3}, {"sci-fi", 9}},
        .Statistics = {
            .BookCount = 12,
            .TotalManagedBookSizeBytes = 4096,
            .TotalLibrarySizeBytes = 16384
        }
    };
    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(result);

    REQUIRE(response.items_size() == 1);
    REQUIRE(response.total_count() == 12);
    REQUIRE(response.available_languages_size() == 2);
    REQUIRE(response.available_languages(0).value() == "en");
    REQUIRE(response.available_languages(0).count() == 5);
    REQUIRE(response.available_languages(1).value() == "ru");
    REQUIRE(response.available_languages(1).count() == 7);
    REQUIRE(response.available_genres_size() == 2);
    REQUIRE(response.available_genres(0).value() == "classic");
    REQUIRE(response.available_genres(0).count() == 3);
    REQUIRE(response.available_genres(1).value() == "sci-fi");
    REQUIRE(response.available_genres(1).count() == 9);
    REQUIRE(response.has_statistics());
    REQUIRE(response.statistics().book_count() == 12);
    REQUIRE(response.statistics().total_library_size_bytes() == 16384);
    REQUIRE(response.items(0).book_id() == 12);
    REQUIRE(response.items(0).title() == "Пикник на обочине");
    REQUIRE(response.items(0).managed_file_name() == "0000000012.book.Пикник.epub");
    REQUIRE(response.items(0).cover_resource_available());
    REQUIRE(response.items(0).cover_file_extension() == "jpg");
    REQUIRE(response.items(0).cover_relative_path() == "Objects/8a/5b/0000000012.cover.jpg");
    REQUIRE(response.items(0).format() == librova::v1::BOOK_FORMAT_EPUB);
    REQUIRE(response.items(0).memberships_size() == 1);
    REQUIRE(response.items(0).memberships(0).collection_id() == 3);
    REQUIRE(response.items(0).memberships(0).name() == "Favorites");
}

TEST_CASE("Library catalog proto mapper builds book details response", "[proto-mapping][catalog]")
{
    Librova::Application::SBookDetails details;
    details.Id = Librova::Domain::SBookId{42};
    details.TitleUtf8 = "Definitely Maybe";
    details.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky"};
    details.Language = "en";
    details.PublisherUtf8 = std::string{"Macmillan"};
    details.Year = 1978;
    details.Isbn = std::string{"978-5-17-000000-1"};
    details.TagsUtf8 = {"classic"};
    details.DescriptionUtf8 = std::string{"Aliens land only in one city."};
    details.Identifier = std::string{"details-id"};
    details.Format = Librova::Domain::EBookFormat::Fb2;
    details.ManagedPath = std::filesystem::path(u8"Objects/23/7d/0000000042.book.DefinitelyMaybe.fb2");
    details.CoverPath = std::filesystem::path(u8"Objects/23/7d/0000000042.cover.jpg");
    details.SizeBytes = 8192;
    details.Sha256Hex = "details-hash";
    details.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(&details);

    REQUIRE(response.has_details());
    REQUIRE(response.details().book_id() == 42);
    REQUIRE(response.details().publisher() == "Macmillan");
    REQUIRE(response.details().isbn() == "978-5-17-000000-1");
    REQUIRE(response.details().description() == "Aliens land only in one city.");
    REQUIRE(response.details().managed_file_name() == "0000000042.book.DefinitelyMaybe.fb2");
    REQUIRE(response.details().cover_resource_available());
    REQUIRE(response.details().content_hash_available());
    REQUIRE(response.details().cover_file_extension() == "jpg");
    REQUIRE(response.details().cover_relative_path() == "Objects/23/7d/0000000042.cover.jpg");
}

TEST_CASE("Library catalog proto mapper carries structured not-found errors", "[proto-mapping][catalog]")
{
    const Librova::Domain::SDomainError error{
        .Code = Librova::Domain::EDomainErrorCode::NotFound,
        .Message = "Book 42 was not found."
    };

    const auto detailsResponse = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        static_cast<const Librova::Application::SBookDetails*>(nullptr),
        &error);
    REQUIRE(detailsResponse.has_error());
    REQUIRE(detailsResponse.error().code() == librova::v1::ERROR_CODE_NOT_FOUND);
    REQUIRE(detailsResponse.error().message() == "Book 42 was not found.");

    const auto exportResponse = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        static_cast<const std::filesystem::path*>(nullptr),
        &error);
    REQUIRE(exportResponse.has_error());
    REQUIRE(exportResponse.error().code() == librova::v1::ERROR_CODE_NOT_FOUND);

    const auto trashResponse = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        static_cast<const Librova::Application::STrashedBookResult*>(nullptr),
        &error);
    REQUIRE(trashResponse.has_error());
    REQUIRE(trashResponse.error().code() == librova::v1::ERROR_CODE_NOT_FOUND);
}

TEST_CASE("Library catalog proto mapper builds export response", "[proto-mapping][catalog]")
{
    const auto exportedPath = std::filesystem::path(u8"C:/Exports/DefinitelyMaybe.fb2");
    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(&exportedPath);

    REQUIRE(response.has_exported_path());
    REQUIRE(response.exported_path() == "C:/Exports/DefinitelyMaybe.fb2");
}

TEST_CASE("Library catalog proto mapper restores export request format and destination", "[proto-mapping][catalog]")
{
    librova::v1::ExportBookRequest request;
    request.set_book_id(44);
    request.set_destination_path("C:/Exports/DefinitelyMaybe.epub");
    request.set_export_format(librova::v1::BOOK_FORMAT_EPUB);

    const auto restored = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request);

    REQUIRE(restored.BookId.Value == 44);
    REQUIRE(restored.DestinationPath == std::filesystem::path(u8"C:/Exports/DefinitelyMaybe.epub"));
    REQUIRE(restored.ExportFormat == Librova::Domain::EBookFormat::Epub);
}

TEST_CASE("Library catalog proto mapper rejects BOOK_FORMAT_ZIP in book list request", "[proto-mapping][catalog]")
{
    librova::v1::BookListRequest request;
    request.set_format(librova::v1::BOOK_FORMAT_ZIP);

    REQUIRE_THROWS_AS(
        Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request),
        std::invalid_argument);
}

TEST_CASE("Library catalog proto mapper rejects BOOK_FORMAT_ZIP in export request", "[proto-mapping][catalog]")
{
    librova::v1::ExportBookRequest request;
    request.set_book_id(1);
    request.set_destination_path("C:/Exports/book.epub");
    request.set_export_format(librova::v1::BOOK_FORMAT_ZIP);

    REQUIRE_THROWS_AS(
        Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request),
        std::invalid_argument);
}

TEST_CASE("Library catalog proto mapper rejects unknown numeric book format value", "[proto-mapping][catalog]")
{
    librova::v1::BookListRequest request;
    request.set_format(static_cast<librova::v1::BookFormat>(99));

    REQUIRE_THROWS_AS(
        Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request),
        std::invalid_argument);
}

TEST_CASE("Library catalog proto mapper builds move-to-trash response", "[proto-mapping][catalog]")
{
    const Librova::Application::STrashedBookResult result{
        .BookId = Librova::Domain::SBookId{17},
        .Destination = Librova::Application::ETrashDestination::RecycleBin,
        .HasOrphanedFiles = true
    };

    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(&result);

    REQUIRE(response.has_trashed_book_id());
    REQUIRE(response.trashed_book_id() == 17);
    REQUIRE(response.destination() == librova::v1::DELETE_DESTINATION_RECYCLE_BIN);
    REQUIRE(response.has_orphaned_files());
}
