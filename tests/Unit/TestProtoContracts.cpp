#include <catch2/catch_test_macros.hpp>

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
    withHash.set_source_path("C:/books/book.fb2");
    withHash.set_working_directory("C:/work");
    withHash.set_sha256_hex("abc123");

    librova::v1::ImportRequest withoutHash;
    withoutHash.set_source_path("C:/books/book.fb2");
    withoutHash.set_working_directory("C:/work");

    REQUIRE(withHash.has_sha256_hex());
    REQUIRE_FALSE(withoutHash.has_sha256_hex());
}
