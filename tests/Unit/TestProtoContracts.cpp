#include <catch2/catch_test_macros.hpp>
#include <google/protobuf/descriptor.h>

#include <string>

#include "import_jobs.pb.h"

TEST_CASE("Import job protobuf contract round-trips populated messages", "[proto]")
{
    librova::v1::ImportJobResult message;
    auto* snapshot = message.mutable_snapshot();
    snapshot->set_job_id(42);
    snapshot->set_status(librova::v1::IMPORT_JOB_STATUS_RUNNING);
    snapshot->set_percent(75);
    snapshot->set_message("Importing archive");
    snapshot->add_warnings("Nested ZIP skipped.");

    auto* summary = message.mutable_summary();
    summary->set_mode(librova::v1::IMPORT_MODE_ZIP_ARCHIVE);
    summary->set_total_entries(5);
    summary->set_imported_entries(3);
    summary->set_failed_entries(1);
    summary->set_skipped_entries(1);
    summary->add_warnings("One entry failed.");

    auto* error = message.mutable_error();
    error->set_code(librova::v1::ERROR_CODE_INTEGRITY_ISSUE);
    error->set_message("Import completed with warnings.");

    const std::string payload = message.SerializeAsString();

    librova::v1::ImportJobResult parsed;
    REQUIRE(parsed.ParseFromString(payload));
    REQUIRE(parsed.snapshot().job_id() == 42);
    REQUIRE(parsed.snapshot().status() == librova::v1::IMPORT_JOB_STATUS_RUNNING);
    REQUIRE(parsed.summary().mode() == librova::v1::IMPORT_MODE_ZIP_ARCHIVE);
    REQUIRE(parsed.summary().imported_entries() == 3);
    REQUIRE(parsed.summary().warnings_size() == 1);
    REQUIRE(parsed.error().code() == librova::v1::ERROR_CODE_INTEGRITY_ISSUE);
}

TEST_CASE("Import request protobuf preserves optional sha256 field presence", "[proto]")
{
    librova::v1::ImportRequest withHash;
    withHash.add_source_paths("C:/books/book.fb2");
    withHash.set_working_directory("C:/work");
    withHash.set_sha256_hex("abc123");

    librova::v1::ImportRequest withoutHash;
    withoutHash.add_source_paths("C:/books/book.fb2");
    withoutHash.set_working_directory("C:/work");

    REQUIRE(withHash.has_sha256_hex());
    REQUIRE_FALSE(withoutHash.has_sha256_hex());
}

TEST_CASE("Book list protobuf contract round-trips catalog items", "[proto]")
{
    librova::v1::ListBooksResponse response;
    response.set_total_count(77);
    response.add_available_languages()->set_value("en");
    response.add_available_languages()->set_value("ru");
    response.mutable_statistics()->set_book_count(77);
    response.mutable_statistics()->set_total_managed_book_size_bytes(4096);
    response.mutable_statistics()->set_total_library_size_bytes(8192);
    auto* item = response.add_items();
    item->set_book_id(101);
    item->set_title("Roadside Picnic");
    item->add_authors("Arkady Strugatsky");
    item->set_language("en");
    item->set_series("Noon Universe");
    item->set_series_index(1.0);
    item->set_year(1972);
    item->add_tags("classic");
    item->set_format(librova::v1::BOOK_FORMAT_EPUB);
    item->set_managed_file_name("book.epub");
    item->set_cover_resource_available(true);
    item->set_cover_file_extension("jpg");
    item->set_cover_relative_path("Objects/8a/5b/0000000101.cover.jpg");
    item->set_size_bytes(4096);
    item->set_added_at_unix_ms(1711807200000LL);

    const std::string payload = response.SerializeAsString();

    librova::v1::ListBooksResponse parsed;
    REQUIRE(parsed.ParseFromString(payload));
    REQUIRE(parsed.total_count() == 77);
    REQUIRE(parsed.available_languages_size() == 2);
    REQUIRE(parsed.available_languages(0).value() == "en");
    REQUIRE(parsed.available_languages(1).value() == "ru");
    REQUIRE(parsed.has_statistics());
    REQUIRE(parsed.statistics().book_count() == 77);
    REQUIRE(parsed.statistics().total_library_size_bytes() == 8192);
    REQUIRE(parsed.items_size() == 1);
    REQUIRE(parsed.items(0).book_id() == 101);
    REQUIRE(parsed.items(0).format() == librova::v1::BOOK_FORMAT_EPUB);
    REQUIRE(parsed.items(0).authors_size() == 1);
    REQUIRE(parsed.items(0).managed_file_name() == "book.epub");
    REQUIRE(parsed.items(0).cover_resource_available());
    REQUIRE(parsed.items(0).cover_file_extension() == "jpg");
    REQUIRE(parsed.items(0).cover_relative_path() == "Objects/8a/5b/0000000101.cover.jpg");
}

TEST_CASE("Import error code keeps removed duplicate-decision value reserved", "[proto]")
{
    const auto* enumDescriptor = librova::v1::ErrorCode_descriptor();
    REQUIRE(enumDescriptor != nullptr);
    REQUIRE(enumDescriptor->FindValueByNumber(4) == nullptr);

    bool hasReservedName = false;
    for (int index = 0; index < enumDescriptor->reserved_name_count(); ++index)
    {
        if (enumDescriptor->reserved_name(index) == "ERROR_CODE_DUPLICATE_DECISION_REQUIRED")
        {
            hasReservedName = true;
            break;
        }
    }

    REQUIRE(hasReservedName);
}

