#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "ProtoMapping/LibraryCatalogProtoMapper.hpp"

TEST_CASE("Library catalog proto mapper round-trips book list request filters", "[proto-mapping][catalog]")
{
    const Librova::Application::SBookListRequest request{
        .TextUtf8 = "zone",
        .AuthorUtf8 = std::string{"Arkady Strugatsky"},
        .Language = std::string{"en"},
        .SeriesUtf8 = std::string{"Noon Universe"},
        .TagsUtf8 = {"classic", "sci-fi"},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SortBy = Librova::Domain::EBookSort::DateAdded,
        .Offset = 10,
        .Limit = 25
    };

    const auto proto = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProto(request);
    const auto restored = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(proto);

    REQUIRE(restored.TextUtf8 == request.TextUtf8);
    REQUIRE(restored.AuthorUtf8 == request.AuthorUtf8);
    REQUIRE(restored.Language == request.Language);
    REQUIRE(restored.SeriesUtf8 == request.SeriesUtf8);
    REQUIRE(restored.TagsUtf8 == request.TagsUtf8);
    REQUIRE(restored.Format == request.Format);
    REQUIRE(restored.SortBy == request.SortBy);
    REQUIRE(restored.Offset == request.Offset);
    REQUIRE(restored.Limit == request.Limit);
}

TEST_CASE("Library catalog proto mapper builds list response with utf8 paths", "[proto-mapping][catalog]")
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
    item.ManagedPath = std::filesystem::path(u8"Books/0000000012/Пикник.epub");
    item.CoverPath = std::filesystem::path(u8"Covers/0000000012.jpg");
    item.SizeBytes = 4096;
    item.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const Librova::Application::SBookListResult result{
        .Items = {item},
        .TotalCount = 12,
        .AvailableLanguages = {"en", "ru"}
    };
    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(result);

    REQUIRE(response.items_size() == 1);
    REQUIRE(response.total_count() == 12);
    REQUIRE(response.available_languages_size() == 2);
    REQUIRE(response.available_languages(0) == "en");
    REQUIRE(response.available_languages(1) == "ru");
    REQUIRE(response.items(0).book_id() == 12);
    REQUIRE(response.items(0).title() == "Пикник на обочине");
    REQUIRE(response.items(0).managed_path() == "Books/0000000012/Пикник.epub");
    REQUIRE(response.items(0).cover_path() == "Covers/0000000012.jpg");
    REQUIRE(response.items(0).format() == librova::v1::BOOK_FORMAT_EPUB);
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
    details.ManagedPath = std::filesystem::path(u8"Books/0000000042/DefinitelyMaybe.fb2");
    details.CoverPath = std::filesystem::path(u8"Covers/0000000042.jpg");
    details.SizeBytes = 8192;
    details.Sha256Hex = "details-hash";
    details.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(&details);

    REQUIRE(response.has_details());
    REQUIRE(response.details().book_id() == 42);
    REQUIRE(response.details().publisher() == "Macmillan");
    REQUIRE(response.details().isbn() == "978-5-17-000000-1");
    REQUIRE(response.details().description() == "Aliens land only in one city.");
    REQUIRE(response.details().sha256_hex() == "details-hash");
    REQUIRE(response.details().managed_path() == "Books/0000000042/DefinitelyMaybe.fb2");
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

TEST_CASE("Library catalog proto mapper builds move-to-trash response", "[proto-mapping][catalog]")
{
    const Librova::Application::STrashedBookResult result{
        .BookId = Librova::Domain::SBookId{17},
        .Destination = Librova::Application::ETrashDestination::RecycleBin
    };

    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(&result);

    REQUIRE(response.has_trashed_book_id());
    REQUIRE(response.trashed_book_id() == 17);
    REQUIRE(response.destination() == librova::v1::DELETE_DESTINATION_RECYCLE_BIN);
}
