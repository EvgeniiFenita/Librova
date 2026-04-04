#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "ManagedFileEncoding/ManagedFileEncoding.hpp"
#include "ManagedStorage/ManagedFileStorage.hpp"

namespace {

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }

    ~CScopedDirectory()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

} // namespace

TEST_CASE("Managed file storage stages source and cover files before commit", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-stage");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.fb2";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");

    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {17},
        .Format = Librova::Domain::EBookFormat::Fb2,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    REQUIRE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE(prepared.StagedCoverPath.has_value());
    REQUIRE(std::filesystem::exists(*prepared.StagedCoverPath));
    REQUIRE(ReadTextFile(prepared.StagedBookPath) == "book-content");
    REQUIRE(ReadTextFile(*prepared.StagedCoverPath) == "cover-content");
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE(prepared.FinalCoverPath.has_value());
    REQUIRE(prepared.FinalBookPath == std::filesystem::path{sandbox.GetPath() / "Library/Books/0000000017/book.fb2"});
    REQUIRE(*prepared.FinalCoverPath == std::filesystem::path{sandbox.GetPath() / "Library/Covers/0000000017.jpg"});
}

TEST_CASE("Managed file storage commit finalizes staged files and removes temp staging", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-commit");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.png";

    WriteTextFile(sourceBookPath, "epub-content");
    WriteTextFile(sourceCoverPath, "png-cover");

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {21},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    storage.CommitImport(prepared);

    REQUIRE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE(prepared.FinalCoverPath.has_value());
    REQUIRE(std::filesystem::exists(*prepared.FinalCoverPath));
    REQUIRE(ReadTextFile(prepared.FinalBookPath) == "epub-content");
    REQUIRE(ReadTextFile(*prepared.FinalCoverPath) == "png-cover");
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath.parent_path()));
}

TEST_CASE("Managed file storage compresses FB2 files for managed storage when requested", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-compressed");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.fb2";

    WriteTextFile(
        sourceBookPath,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><FictionBook><body><section><p>"
        "compressed fb2 content compressed fb2 content compressed fb2 content"
        "</p></section></body></FictionBook>");

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {78},
        .Format = Librova::Domain::EBookFormat::Fb2,
        .StorageEncoding = Librova::Domain::EStorageEncoding::Compressed,
        .SourcePath = sourceBookPath
    });

    REQUIRE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE(std::filesystem::file_size(prepared.StagedBookPath) < std::filesystem::file_size(sourceBookPath));
    REQUIRE(ReadTextFile(prepared.StagedBookPath) != ReadTextFile(sourceBookPath));

    const auto decodedPath = sandbox.GetPath() / "decoded.fb2";
    Librova::ManagedFileEncoding::DecodeFileToPath(
        prepared.StagedBookPath,
        Librova::Domain::EStorageEncoding::Compressed,
        decodedPath);

    REQUIRE(ReadTextFile(decodedPath) == ReadTextFile(sourceBookPath));
}

TEST_CASE("Managed file storage rollback removes staged files and leaves final targets absent", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-rollback");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";

    WriteTextFile(sourceBookPath, "rollback-content");

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {34},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath
    });

    storage.RollbackImport(prepared);

    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath.parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath));
}

TEST_CASE("Managed file storage restores staging state when commit fails after moving the book", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-commit-failure");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");
    Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {55},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    REQUIRE(prepared.FinalCoverPath.has_value());
    std::filesystem::create_directories(*prepared.FinalCoverPath);

    REQUIRE_THROWS(storage.CommitImport(prepared));

    REQUIRE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE(prepared.StagedCoverPath.has_value());
    REQUIRE(std::filesystem::exists(*prepared.StagedCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE(std::filesystem::is_directory(*prepared.FinalCoverPath));
    REQUIRE(ReadTextFile(prepared.StagedBookPath) == "book-content");
    REQUIRE(ReadTextFile(*prepared.StagedCoverPath) == "cover-content");
}

TEST_CASE("Managed file storage cleans staging directory when preparation fails", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-prepare-failure");
    const std::filesystem::path missingSourceBookPath = sandbox.GetPath() / "input" / "missing.fb2";
    const auto expectedStagingDirectory = sandbox.GetPath() / "Library" / "Temp" / "0000000089";

    Librova::ManagedStorage::CManagedFileStorage storage(sandbox.GetPath() / "Library");

    REQUIRE_THROWS(storage.PrepareImport({
        .BookId = {89},
        .Format = Librova::Domain::EBookFormat::Fb2,
        .StorageEncoding = Librova::Domain::EStorageEncoding::Compressed,
        .SourcePath = missingSourceBookPath
    }));

    REQUIRE_FALSE(std::filesystem::exists(expectedStagingDirectory));
}
