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

    const Librova::Application::SBookListResult result{{item}};
    const auto response = Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(result);

    REQUIRE(response.items_size() == 1);
    REQUIRE(response.items(0).book_id() == 12);
    REQUIRE(response.items(0).title() == "Пикник на обочине");
    REQUIRE(response.items(0).managed_path() == "Books/0000000012/Пикник.epub");
    REQUIRE(response.items(0).cover_path() == "Covers/0000000012.jpg");
    REQUIRE(response.items(0).format() == librova::v1::BOOK_FORMAT_EPUB);
}
