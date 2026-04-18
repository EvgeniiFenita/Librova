#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "Storage/ManagedFileEncoding.hpp"
#include "Storage/ManagedFileStorage.hpp"

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

std::filesystem::path BuildManagedStorageStagingRoot(const CScopedDirectory& sandbox)
{
    return sandbox.GetPath() / "Runtime" / "ManagedStorageStaging";
}

} // namespace

TEST_CASE("Managed file storage stages source and cover files before commit", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-stage");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.fb2";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));

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
    REQUIRE(prepared.FinalBookPath == std::filesystem::path{sandbox.GetPath() / "Library/Objects/d1/56/0000000017.book.fb2"});
    REQUIRE(*prepared.FinalCoverPath == std::filesystem::path{sandbox.GetPath() / "Library/Objects/d1/56/0000000017.cover.jpg"});
    REQUIRE_FALSE(prepared.RelativeBookPath.is_absolute());
    REQUIRE(prepared.RelativeCoverPath.has_value());
    REQUIRE_FALSE(prepared.RelativeCoverPath->is_absolute());
    REQUIRE(prepared.RelativeBookPath == std::filesystem::path{"Objects/d1/56/0000000017.book.fb2"});
    REQUIRE(*prepared.RelativeCoverPath == std::filesystem::path{"Objects/d1/56/0000000017.cover.jpg"});
}

TEST_CASE("Managed file storage commit finalizes staged files and removes temp staging", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-commit");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.png";

    WriteTextFile(sourceBookPath, "epub-content");
    WriteTextFile(sourceCoverPath, "png-cover");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
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

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {78},
        .Format = Librova::Domain::EBookFormat::Fb2,
        .StorageEncoding = Librova::Domain::EStorageEncoding::Compressed,
        .SourcePath = sourceBookPath
    });

    REQUIRE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE(std::filesystem::file_size(prepared.StagedBookPath) < std::filesystem::file_size(sourceBookPath));
    REQUIRE(ReadTextFile(prepared.StagedBookPath) != ReadTextFile(sourceBookPath));
    REQUIRE(prepared.FinalBookPath == std::filesystem::path{sandbox.GetPath() / "Library/Objects/ea/d4/0000000078.book.fb2.gz"});
    REQUIRE(prepared.RelativeBookPath == std::filesystem::path{"Objects/ea/d4/0000000078.book.fb2.gz"});

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

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {34},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath
    });

    storage.RollbackImport(prepared);

    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath.parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath.parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath.parent_path().parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.GetPath() / "Library" / "Objects"));
    REQUIRE(std::filesystem::is_directory(BuildManagedStorageStagingRoot(sandbox)));
}

TEST_CASE("Managed file storage rollback preserves top-level Objects directory when it becomes empty", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-preserve-covers");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {35},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    REQUIRE(prepared.FinalCoverPath.has_value());

    storage.RollbackImport(prepared);

    REQUIRE_FALSE(std::filesystem::exists(*prepared.FinalCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedCoverPath->parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalCoverPath->parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalCoverPath->parent_path().parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.GetPath() / "Library" / "Objects"));
    REQUIRE(std::filesystem::is_directory(BuildManagedStorageStagingRoot(sandbox)));
}

TEST_CASE("Managed file storage rollback removes shard residue when only Thumbs.db remains", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-thumbs");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";

    WriteTextFile(sourceBookPath, "rollback-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {36},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath
    });

    WriteTextFile(prepared.FinalBookPath.parent_path() / "Thumbs.db", "cache");

    storage.RollbackImport(prepared);

    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath.parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath.parent_path().parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.GetPath() / "Library" / "Objects"));
}

TEST_CASE("Managed file storage restores staging state when commit fails after moving the book", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-commit-failure");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
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

TEST_CASE("Managed file storage rollback removes final paths left by a partial commit", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-partial-rollback");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "book-content");
    WriteTextFile(sourceCoverPath, "cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {56},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    REQUIRE(prepared.StagedCoverPath.has_value());
    REQUIRE(prepared.FinalCoverPath.has_value());

    std::filesystem::create_directories(prepared.FinalBookPath.parent_path());
    std::filesystem::create_directories(prepared.FinalCoverPath->parent_path());
    std::filesystem::rename(prepared.StagedBookPath, prepared.FinalBookPath);
    std::filesystem::rename(*prepared.StagedCoverPath, *prepared.FinalCoverPath);

    storage.RollbackImport(prepared);

    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath));
    REQUIRE_FALSE(std::filesystem::exists(*prepared.StagedCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE_FALSE(std::filesystem::exists(*prepared.FinalCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath.parent_path()));
    REQUIRE_FALSE(std::filesystem::exists(prepared.FinalBookPath.parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.GetPath() / "Library" / "Objects"));
}

TEST_CASE("Managed file storage PrepareImport produces forward-slash relative paths with Cyrillic library root", "[managed-storage]")
{
    // Regression guard for C23. std::filesystem::relative() on Windows returns
    // paths with native backslash separators; the stored relative path must still
    // use forward slashes (via generic_string()) so that ManagedPath is portable
    // across comparisons and SQLite storage even when the library root contains
    // non-ASCII (Cyrillic) directory names.
    const auto cyrillicRoot = std::filesystem::temp_directory_path() / u8"librova-\u0411\u0438\u0431\u043b\u0438\u043e\u0442\u0435\u043a\u0430-test";
    CScopedDirectory sandbox(cyrillicRoot);
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";

    WriteTextFile(sourceBookPath, "epub-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath(),
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {42},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath
    });

    REQUIRE_FALSE(prepared.RelativeBookPath.is_absolute());
    // generic_string() must not contain backslashes regardless of platform or root encoding
    REQUIRE(prepared.RelativeBookPath.generic_string().find('\\') == std::string::npos);
    REQUIRE(prepared.RelativeBookPath == std::filesystem::path{"Objects/23/7d/0000000042.book.epub"});
}

TEST_CASE("Managed file storage cleans staging directory when preparation fails", "[managed-storage]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-prepare-failure");
    const std::filesystem::path missingSourceBookPath = sandbox.GetPath() / "input" / "missing.fb2";
    const auto expectedStagingDirectory = BuildManagedStorageStagingRoot(sandbox) / "0000000089";

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));

    REQUIRE_THROWS(storage.PrepareImport({
        .BookId = {89},
        .Format = Librova::Domain::EBookFormat::Fb2,
        .StorageEncoding = Librova::Domain::EStorageEncoding::Compressed,
        .SourcePath = missingSourceBookPath
    }));

    REQUIRE_FALSE(std::filesystem::exists(expectedStagingDirectory));
}

TEST_CASE("Managed file storage CommitImport overwrites stale cover at destination", "[managed-storage]")
{
    // Regression: destination collisions — a stale cover left by a prior failed import
    // at the final cover path must be overwritten, not cause CommitImport to abort.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-cover-overwrite");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.jpg";

    WriteTextFile(sourceBookPath, "epub-content");
    WriteTextFile(sourceCoverPath, "new-cover-content");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {91},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    REQUIRE(prepared.FinalCoverPath.has_value());

    // Plant a stale file at the final cover destination to simulate a prior partial commit.
    std::filesystem::create_directories(prepared.FinalCoverPath->parent_path());
    WriteTextFile(*prepared.FinalCoverPath, "stale-cover-content");

    // CommitImport must succeed and replace the stale cover.
    REQUIRE_NOTHROW(storage.CommitImport(prepared));

    REQUIRE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE(std::filesystem::exists(*prepared.FinalCoverPath));
    REQUIRE(ReadTextFile(*prepared.FinalCoverPath) == "new-cover-content");
}

TEST_CASE("Managed file storage CommitImport succeeds via copy+delete when staging and library share a root", "[managed-storage]")
{
    // Regression guard for #149: MoveFile must not abort a book commit when the
    // atomic rename fails. Here staging and library are under the same sandbox root
    // (same device, rename always works) — this verifies that the refactored MoveFile
    // still produces the correct final layout whether it took the rename or copy+delete path.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-storage-movefile");
    const std::filesystem::path sourceBookPath = sandbox.GetPath() / "input" / "book.epub";
    const std::filesystem::path sourceCoverPath = sandbox.GetPath() / "input" / "cover.png";

    WriteTextFile(sourceBookPath, "movefile-book");
    WriteTextFile(sourceCoverPath, "movefile-cover");

    Librova::ManagedStorage::CManagedFileStorage storage(
        sandbox.GetPath() / "Library",
        BuildManagedStorageStagingRoot(sandbox));
    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {93},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = sourceBookPath,
        .CoverSourcePath = sourceCoverPath
    });

    storage.CommitImport(prepared);

    REQUIRE(std::filesystem::exists(prepared.FinalBookPath));
    REQUIRE(prepared.FinalCoverPath.has_value());
    REQUIRE(std::filesystem::exists(*prepared.FinalCoverPath));
    REQUIRE(ReadTextFile(prepared.FinalBookPath) == "movefile-book");
    REQUIRE(ReadTextFile(*prepared.FinalCoverPath) == "movefile-cover");
    REQUIRE_FALSE(std::filesystem::exists(prepared.StagedBookPath));
}
