#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "ParserRegistry/BookParserRegistry.hpp"

namespace {

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

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::filesystem::path CreateFb2Fixture(const std::filesystem::path& outputPath)
{
    WriteTextFile(
        outputPath,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <book-title>Пикник на обочине</book-title>
      <author>
        <first-name>Аркадий</first-name>
        <last-name>Стругацкий</last-name>
      </author>
      <lang>ru</lang>
      <annotation>
        <p>Классика советской фантастики</p>
      </annotation>
      <coverpage>
        <image l:href="#cover-image"/>
      </coverpage>
    </title-info>
    <publish-info>
      <isbn>978-5-17-090334-4</isbn>
    </publish-info>
  </description>
  <body>
    <section><p>Body</p></section>
  </body>
  <binary id="cover-image" content-type="image/jpeg">ASNF</binary>
</FictionBook>)");

    return outputPath;
}

class CTestProgressSink final : public LibriFlow::Domain::IProgressSink
{
public:
    void ReportValue(const int percent, std::string_view message) override
    {
        LastPercent = percent;
        LastMessage.assign(message);
    }

    bool IsCancellationRequested() const override
    {
        return CancellationRequested;
    }

    int LastPercent = 0;
    std::string LastMessage;
    bool CancellationRequested = false;
};

class CStubBookRepository final : public LibriFlow::Domain::IBookRepository
{
public:
    [[nodiscard]] LibriFlow::Domain::SBookId ReserveId() override
    {
        ReservedId = LibriFlow::Domain::SBookId{NextId++};
        return *ReservedId;
    }

    [[nodiscard]] LibriFlow::Domain::SBookId Add(const LibriFlow::Domain::SBook& book) override
    {
        if (ThrowOnAdd)
        {
            throw std::runtime_error("Repository add failed.");
        }

        AddedBook = book;
        return book.Id;
    }

    [[nodiscard]] std::optional<LibriFlow::Domain::SBook> GetById(LibriFlow::Domain::SBookId) const override
    {
        return AddedBook;
    }

    void Remove(const LibriFlow::Domain::SBookId id) override
    {
        RemovedIds.push_back(id);
    }

    std::int64_t NextId = 1;
    std::optional<LibriFlow::Domain::SBookId> ReservedId;
    std::optional<LibriFlow::Domain::SBook> AddedBook;
    std::vector<LibriFlow::Domain::SBookId> RemovedIds;
    bool ThrowOnAdd = false;
};

class CStubQueryRepository final : public LibriFlow::Domain::IBookQueryRepository
{
public:
    [[nodiscard]] std::vector<LibriFlow::Domain::SBook> Search(const LibriFlow::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<LibriFlow::Domain::SDuplicateMatch> FindDuplicates(const LibriFlow::Domain::SCandidateBook&) const override
    {
        return Duplicates;
    }

    std::vector<LibriFlow::Domain::SDuplicateMatch> Duplicates;
};

class CStubManagedStorage final : public LibriFlow::Domain::IManagedStorage
{
public:
    explicit CStubManagedStorage(std::filesystem::path root)
        : Root(std::move(root))
    {
    }

    [[nodiscard]] LibriFlow::Domain::SPreparedStorage PrepareImport(const LibriFlow::Domain::SStoragePlan& plan) override
    {
        LastPlan = plan;
        const auto layout = LibriFlow::StoragePlanning::CManagedLibraryLayout::Build(Root);
        std::filesystem::create_directories(layout.TempDirectory);
        std::filesystem::create_directories(layout.BooksDirectory);
        std::filesystem::create_directories(layout.CoversDirectory);

        const std::filesystem::path stagingDirectory =
            LibriFlow::StoragePlanning::CManagedLibraryLayout::GetStagingDirectory(Root, plan.BookId);
        std::filesystem::remove_all(stagingDirectory);
        std::filesystem::create_directories(stagingDirectory);

        const std::filesystem::path stagedBookPath = stagingDirectory / plan.SourcePath.filename();
        std::filesystem::copy_file(plan.SourcePath, stagedBookPath, std::filesystem::copy_options::overwrite_existing);

        const std::filesystem::path finalBookPath =
            LibriFlow::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(Root, plan.BookId, plan.Format);

        std::optional<std::filesystem::path> stagedCoverPath;
        std::optional<std::filesystem::path> finalCoverPath;

        if (plan.CoverSourcePath.has_value())
        {
            stagedCoverPath = stagingDirectory / plan.CoverSourcePath->filename();
            std::filesystem::copy_file(
                *plan.CoverSourcePath,
                *stagedCoverPath,
                std::filesystem::copy_options::overwrite_existing);
            finalCoverPath = Root / "Covers" / std::to_string(plan.BookId.Value) / "cover.jpg";
        }

        return {
            .StagedBookPath = stagedBookPath,
            .StagedCoverPath = stagedCoverPath,
            .FinalBookPath = finalBookPath,
            .FinalCoverPath = finalCoverPath
        };
    }

    void CommitImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) override
    {
        if (ThrowOnCommit)
        {
            throw std::runtime_error("Storage commit failed.");
        }

        LastPreparedStorage = preparedStorage;
        std::filesystem::create_directories(preparedStorage.FinalBookPath.parent_path());
        std::filesystem::copy_file(
            preparedStorage.StagedBookPath,
            preparedStorage.FinalBookPath,
            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(preparedStorage.StagedBookPath);

        if (preparedStorage.StagedCoverPath.has_value() && preparedStorage.FinalCoverPath.has_value())
        {
            std::filesystem::create_directories(preparedStorage.FinalCoverPath->parent_path());
            std::filesystem::copy_file(
                *preparedStorage.StagedCoverPath,
                *preparedStorage.FinalCoverPath,
                std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove(*preparedStorage.StagedCoverPath);
        }

        CommitCalled = true;
    }

    void RollbackImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) noexcept override
    {
        LastPreparedStorage = preparedStorage;
        std::error_code errorCode;
        std::filesystem::remove(preparedStorage.StagedBookPath, errorCode);

        if (preparedStorage.StagedCoverPath.has_value())
        {
            std::filesystem::remove(*preparedStorage.StagedCoverPath, errorCode);
        }

        RollbackCalled = true;
    }

    std::filesystem::path Root;
    std::optional<LibriFlow::Domain::SStoragePlan> LastPlan;
    std::optional<LibriFlow::Domain::SPreparedStorage> LastPreparedStorage;
    bool CommitCalled = false;
    bool RollbackCalled = false;
    bool ThrowOnCommit = false;
};

class CStubBookConverter final : public LibriFlow::Domain::IBookConverter
{
public:
    [[nodiscard]] bool CanConvert(
        const LibriFlow::Domain::EBookFormat sourceFormat,
        const LibriFlow::Domain::EBookFormat destinationFormat) const override
    {
        return Enabled
            && sourceFormat == LibriFlow::Domain::EBookFormat::Fb2
            && destinationFormat == LibriFlow::Domain::EBookFormat::Epub;
    }

    [[nodiscard]] LibriFlow::Domain::SConversionResult Convert(
        const LibriFlow::Domain::SConversionRequest& request,
        LibriFlow::Domain::IProgressSink&,
        std::stop_token) const override
    {
        LastRequest = request;
        return Result;
    }

    bool Enabled = false;
    mutable std::optional<LibriFlow::Domain::SConversionRequest> LastRequest;
    LibriFlow::Domain::SConversionResult Result;
};

} // namespace

TEST_CASE("Single file import imports FB2 with converter fallback to source file", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-fallback");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = std::string{"fb2-hash"}
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::Imported);
    REQUIRE(result.StoredFormat == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(result.Warnings == std::vector<std::string>({
        "FB2 converter unavailable. Original FB2 will be stored."
    }));
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->Id.Value == 1);
    REQUIRE(bookRepository.AddedBook->File.Format == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(bookRepository.AddedBook->File.Sha256Hex == "fb2-hash");
    REQUIRE(managedStorage.LastPlan.has_value());
    REQUIRE(managedStorage.LastPlan->Format == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(managedStorage.LastPlan->CoverSourcePath.has_value());
    REQUIRE(managedStorage.CommitCalled);
    REQUIRE_FALSE(managedStorage.RollbackCalled);
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("Single file import rejects strict duplicates before reserving storage", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-strict-duplicate");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = LibriFlow::Domain::EDuplicateSeverity::Strict,
        .Reason = LibriFlow::Domain::EDuplicateReason::SameIsbn,
        .ExistingBookId = {42}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import returns decision required for probable duplicates without approval", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-probable-duplicate");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = LibriFlow::Domain::EDuplicateSeverity::Probable,
        .Reason = LibriFlow::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {24}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::DecisionRequired);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import stores converted EPUB when conversion succeeds", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-conversion");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");
    const auto convertedPath = sandbox.GetPath() / "work" / "converted.epub";
    WriteTextFile(convertedPath, "converted-epub");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = LibriFlow::Domain::EConversionStatus::Succeeded,
        .OutputPath = convertedPath
    };
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.StoredFormat == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE(converter.LastRequest.has_value());
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->File.Format == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE(managedStorage.LastPlan.has_value());
    REQUIRE(managedStorage.LastPlan->Format == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE_FALSE(std::filesystem::exists(convertedPath));
}

TEST_CASE("Single file import can continue after probable duplicate when explicitly allowed", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-probable-force");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = LibriFlow::Domain::EDuplicateSeverity::Probable,
        .Reason = LibriFlow::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {99}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE(result.Warnings == std::vector<std::string>({
        "Import continued with explicit probable duplicate override.",
        "FB2 converter unavailable. Original FB2 will be stored."
    }));
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import removes temporary converter output on cancellation", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-cancel-cleanup");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");
    const auto convertedPath = sandbox.GetPath() / "work" / "converted.epub";
    WriteTextFile(convertedPath, "partial-output");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = LibriFlow::Domain::EConversionStatus::Cancelled,
        .OutputPath = convertedPath,
        .Warnings = {"Conversion cancelled."}
    };
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::Cancelled);
    REQUIRE_FALSE(std::filesystem::exists(convertedPath));
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import stops on cancellation before storage prepare", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-early-cancel");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;
    progressSink.CancellationRequested = true;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::Cancelled);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(managedStorage.LastPlan.has_value());
}

TEST_CASE("Single file import rolls back prepared storage when repository add fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-add-failure");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.ThrowOnAdd = true;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Repository add failed.");
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE_FALSE(std::filesystem::exists(managedStorage.LastPreparedStorage->StagedBookPath));
    REQUIRE(bookRepository.RemovedIds.empty());
}

TEST_CASE("Single file import removes persisted book when storage commit fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-importing-commit-failure");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    managedStorage.ThrowOnCommit = true;
    CTestProgressSink progressSink;

    const LibriFlow::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == LibriFlow::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Storage commit failed.");
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE_FALSE(std::filesystem::exists(managedStorage.LastPreparedStorage->StagedBookPath));
    REQUIRE(bookRepository.RemovedIds.size() == 1);
    REQUIRE(bookRepository.RemovedIds.front().Value == 1);
}
