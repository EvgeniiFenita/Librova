#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "ManagedFileEncoding/ManagedFileEncoding.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "ParserRegistry/BookParserRegistry.hpp"
#include "Unicode/UnicodeConversion.hpp"

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

[[nodiscard]] std::vector<std::byte> ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::vector<char> rawBytes{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };

    std::vector<std::byte> bytes(rawBytes.size());
    std::ranges::transform(rawBytes, bytes.begin(), [](char value) {
        return static_cast<std::byte>(static_cast<unsigned char>(value));
    });
    return bytes;
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

class CTestProgressSink final : public Librova::Domain::IProgressSink
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

class CStubBookRepository final : public Librova::Domain::IBookRepository
{
public:
    [[nodiscard]] Librova::Domain::SBookId ReserveId() override
    {
        ReservedId = Librova::Domain::SBookId{NextId++};
        return *ReservedId;
    }

    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        if (ThrowOnAdd)
        {
            throw std::runtime_error("Repository add failed.");
        }

        if (DuplicateHashConflictId.has_value())
        {
            throw Librova::Domain::CDuplicateHashException{*DuplicateHashConflictId};
        }

        AddedBook = book;
        return book.Id;
    }

    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        ForceAddCalled = true;
        if (ThrowOnForceAdd)
        {
            throw std::runtime_error("Repository force add failed.");
        }
        AddedBook = book;
        return book.Id;
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(Librova::Domain::SBookId) const override
    {
        return AddedBook;
    }

    void Remove(const Librova::Domain::SBookId id) override
    {
        if (ThrowOnRemove)
        {
            throw std::runtime_error("Repository remove failed.");
        }

        RemovedIds.push_back(id);
    }

    std::int64_t NextId = 1;
    std::optional<Librova::Domain::SBookId> ReservedId;
    std::optional<Librova::Domain::SBook> AddedBook;
    std::vector<Librova::Domain::SBookId> RemovedIds;
    bool ThrowOnAdd = false;
    bool ThrowOnForceAdd = false;
    bool ThrowOnRemove = false;
    bool ForceAddCalled = false;
    std::optional<Librova::Domain::SBookId> DuplicateHashConflictId;
};

class CStubQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery&) const override
    {
        return 0;
    }

    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<std::string> ListAvailableTags(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<std::string> ListAvailableGenres(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
    {
        return Duplicates;
    }

    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override
    {
        return {};
    }

    std::vector<Librova::Domain::SDuplicateMatch> Duplicates;
};

class CStubManagedStorage final : public Librova::Domain::IManagedStorage
{
public:
    explicit CStubManagedStorage(std::filesystem::path root)
        : Root(std::move(root))
    {
    }

    [[nodiscard]] Librova::Domain::SPreparedStorage PrepareImport(const Librova::Domain::SStoragePlan& plan) override
    {
        LastPlan = plan;
        const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(Root);
        std::filesystem::create_directories(layout.ObjectsDirectory);

        const std::filesystem::path stagingDirectory =
            Root / "RuntimeTestStaging" / Librova::StoragePlanning::CManagedLibraryLayout::GetBookFolderName(plan.BookId);
        std::filesystem::remove_all(stagingDirectory);
        std::filesystem::create_directories(stagingDirectory);

        const std::filesystem::path stagedBookPath = stagingDirectory / plan.SourcePath.filename();
        Librova::ManagedFileEncoding::EncodeFileToPath(plan.SourcePath, plan.StorageEncoding, stagedBookPath);

        const std::filesystem::path finalBookPath =
            Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(Root, plan.BookId, plan.Format, plan.StorageEncoding);

        std::optional<std::filesystem::path> stagedCoverPath;
        std::optional<std::filesystem::path> finalCoverPath;

        if (plan.CoverSourcePath.has_value())
        {
            stagedCoverPath = stagingDirectory / plan.CoverSourcePath->filename();
            std::filesystem::copy_file(
                *plan.CoverSourcePath,
                *stagedCoverPath,
                std::filesystem::copy_options::overwrite_existing);
            finalCoverPath = Librova::StoragePlanning::CManagedLibraryLayout::GetCoverPath(
                Root,
                plan.BookId,
                plan.CoverSourcePath->extension().string());
        }

        return {
            .StagedBookPath = stagedBookPath,
            .StagedCoverPath = stagedCoverPath,
            .FinalBookPath = finalBookPath,
            .FinalCoverPath = finalCoverPath,
            .RelativeBookPath = std::filesystem::relative(finalBookPath, Root),
            .RelativeCoverPath = finalCoverPath.has_value()
                ? std::make_optional(std::filesystem::relative(*finalCoverPath, Root))
                : std::nullopt
        };
    }

    void CommitImport(const Librova::Domain::SPreparedStorage& preparedStorage) override
    {
        if (ThrowOnCommit)
        {
            throw std::runtime_error(CommitFailureMessage);
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

    void RollbackImport(const Librova::Domain::SPreparedStorage& preparedStorage) noexcept override
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
    std::optional<Librova::Domain::SStoragePlan> LastPlan;
    std::optional<Librova::Domain::SPreparedStorage> LastPreparedStorage;
    bool CommitCalled = false;
    bool RollbackCalled = false;
    bool ThrowOnCommit = false;
    std::string CommitFailureMessage = "Storage commit failed.";
};

class CStubBookConverter final : public Librova::Domain::IBookConverter
{
public:
    [[nodiscard]] bool CanConvert(
        const Librova::Domain::EBookFormat sourceFormat,
        const Librova::Domain::EBookFormat destinationFormat) const override
    {
        return Enabled
            && sourceFormat == Librova::Domain::EBookFormat::Fb2
            && destinationFormat == Librova::Domain::EBookFormat::Epub;
    }

    [[nodiscard]] Librova::Domain::SConversionResult Convert(
        const Librova::Domain::SConversionRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        LastRequest = request;
        return Result;
    }

    bool Enabled = false;
    mutable std::optional<Librova::Domain::SConversionRequest> LastRequest;
    Librova::Domain::SConversionResult Result;
};

class CStubCoverImageProcessor final : public Librova::Domain::ICoverImageProcessor
{
public:
    [[nodiscard]] Librova::Domain::SCoverProcessingResult ProcessForManagedStorage(
        const Librova::Domain::SCoverProcessingRequest& request) const override
    {
        LastRequest = request;
        return Result;
    }

    mutable std::optional<Librova::Domain::SCoverProcessingRequest> LastRequest;
    Librova::Domain::SCoverProcessingResult Result;
};

} // namespace

TEST_CASE("Single file import imports FB2 without conversion when forced conversion is disabled", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-fallback");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
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
    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Imported);
    REQUIRE(result.StoredFormat == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(result.Warnings.empty());
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->Id.Value == 1);
    REQUIRE(bookRepository.AddedBook->File.Format == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(bookRepository.AddedBook->File.StorageEncoding == Librova::Domain::EStorageEncoding::Compressed);
    REQUIRE(bookRepository.AddedBook->File.Sha256Hex == "fb2-hash");
    REQUIRE_FALSE(bookRepository.AddedBook->File.ManagedPath.is_absolute());
    REQUIRE(bookRepository.AddedBook->CoverPath.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook->CoverPath->is_absolute());
    REQUIRE(managedStorage.LastPlan.has_value());
    REQUIRE(managedStorage.LastPlan->Format == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(managedStorage.LastPlan->StorageEncoding == Librova::Domain::EStorageEncoding::Compressed);
    REQUIRE(managedStorage.LastPlan->CoverSourcePath.has_value());
    REQUIRE(managedStorage.CommitCalled);
    REQUIRE_FALSE(managedStorage.RollbackCalled);
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("Single file import keeps plain storage when forced EPUB conversion is enabled", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-forced-epub-plain-storage");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = Librova::Domain::EConversionStatus::Succeeded,
        .OutputPath = sandbox.GetPath() / "work" / "converted.epub"
    };
    WriteTextFile(converter.Result.OutputPath, "converted-epub");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.StoredFormat == Librova::Domain::EBookFormat::Epub);
    REQUIRE(managedStorage.LastPlan.has_value());
    REQUIRE(managedStorage.LastPlan->Format == Librova::Domain::EBookFormat::Epub);
    REQUIRE(managedStorage.LastPlan->StorageEncoding == Librova::Domain::EStorageEncoding::Plain);
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->File.Format == Librova::Domain::EBookFormat::Epub);
    REQUIRE(bookRepository.AddedBook->File.StorageEncoding == Librova::Domain::EStorageEncoding::Plain);
}

TEST_CASE("Single file import uses repository override when provided", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-repository-override");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository defaultRepository;
    CStubBookRepository overrideRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        defaultRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = std::string{"override-hash"},
        .RepositoryOverride = std::ref(overrideRepository)
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE_FALSE(defaultRepository.AddedBook.has_value());
    REQUIRE(overrideRepository.AddedBook.has_value());
    REQUIRE(overrideRepository.AddedBook->File.Sha256Hex == "override-hash");
}

TEST_CASE("Single file import fails when forced EPUB conversion is enabled but converter is unavailable", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-forced-epub-unavailable");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Forced FB2 to EPUB conversion requires a configured converter.");
    REQUIRE(result.DiagnosticError == "Forced FB2 to EPUB conversion requires a configured converter.");
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.LastPlan.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import fails when forced EPUB conversion returns an error", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-forced-epub-failed");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = Librova::Domain::EConversionStatus::Failed,
        .Warnings = {"External converter returned exit code 1."}
    };
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Warnings == std::vector<std::string>({ "External converter returned exit code 1." }));
    REQUIRE(result.Error == "Forced FB2 to EPUB conversion failed.");
    REQUIRE(result.DiagnosticError == "Forced FB2 to EPUB conversion failed.");
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.LastPlan.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import stores compressed managed file size for fallback FB2", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-fallback-compressed-size");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = std::string{"fb2-size-hash"}
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE(bookRepository.AddedBook->File.SizeBytes == std::filesystem::file_size(managedStorage.LastPreparedStorage->FinalBookPath));
    REQUIRE(bookRepository.AddedBook->File.SizeBytes < std::filesystem::file_size(sourcePath));
    REQUIRE(bookRepository.AddedBook->File.Sha256Hex == "fb2-size-hash");
}

TEST_CASE("Single file import stores optimized cover bytes returned by the cover processor", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-cover-optimized");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubCoverImageProcessor coverImageProcessor;
    coverImageProcessor.Result = {
        .Status = Librova::Domain::ECoverProcessingStatus::Processed,
        .Cover = {
            .Extension = "jpg",
            .Bytes = {std::byte{'O'}, std::byte{'P'}, std::byte{'T'}}
        },
        .PixelWidth = 512,
        .PixelHeight = 768,
        .WasResized = true
    };
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr,
        &coverImageProcessor);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(coverImageProcessor.LastRequest.has_value());
    REQUIRE(coverImageProcessor.LastRequest->MaxWidth == 456);
    REQUIRE(coverImageProcessor.LastRequest->MaxHeight == 684);
    REQUIRE(coverImageProcessor.LastRequest->FallbackMaxWidth == 400);
    REQUIRE(coverImageProcessor.LastRequest->FallbackMaxHeight == 600);
    REQUIRE(coverImageProcessor.LastRequest->TargetMaxBytes == 120u * 1024u);
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE(managedStorage.LastPreparedStorage->FinalCoverPath.has_value());
    REQUIRE(ReadBinaryFile(*managedStorage.LastPreparedStorage->FinalCoverPath) == std::vector<std::byte>({
        std::byte{'O'},
        std::byte{'P'},
        std::byte{'T'}
    }));
}

TEST_CASE("Single file import falls back to original cover bytes when cover optimization fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-cover-fallback");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubCoverImageProcessor coverImageProcessor;
    coverImageProcessor.Result = {
        .Status = Librova::Domain::ECoverProcessingStatus::Failed,
        .DiagnosticMessage = "Decoder failure."
    };
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr,
        &coverImageProcessor);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(coverImageProcessor.LastRequest.has_value());
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE(managedStorage.LastPreparedStorage->FinalCoverPath.has_value());
    REQUIRE(ReadBinaryFile(*managedStorage.LastPreparedStorage->FinalCoverPath) == std::vector<std::byte>({
        std::byte{0x01},
        std::byte{0x23},
        std::byte{0x45}
    }));
}

TEST_CASE("Single file import rejects strict duplicates before reserving storage", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-strict-duplicate");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
        .Reason = Librova::Domain::EDuplicateReason::SameHash,
        .ExistingBookId = {42}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import rejects probable duplicates before reserving storage", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-probable-duplicate");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = Librova::Domain::EDuplicateSeverity::Probable,
        .Reason = Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {24}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import still rejects strict duplicates even when probable duplicate override is enabled", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-strict-force");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
        .Reason = Librova::Domain::EDuplicateReason::SameIsbn,
        .ExistingBookId = {77}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
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

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE(result.Warnings == std::vector<std::string>({
        "Import rejected because a strict duplicate already exists."
    }));
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import stores converted EPUB when conversion succeeds", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-conversion");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");
    const auto convertedPath = sandbox.GetPath() / "work" / "converted.epub";
    WriteTextFile(convertedPath, "converted-epub");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = Librova::Domain::EConversionStatus::Succeeded,
        .OutputPath = convertedPath
    };
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true,
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(result.StoredFormat == Librova::Domain::EBookFormat::Epub);
    REQUIRE(converter.LastRequest.has_value());
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->File.Format == Librova::Domain::EBookFormat::Epub);
    REQUIRE(managedStorage.LastPlan.has_value());
    REQUIRE(managedStorage.LastPlan->Format == Librova::Domain::EBookFormat::Epub);
    REQUIRE_FALSE(std::filesystem::exists(convertedPath));
}

TEST_CASE("Single file import can continue after probable duplicate when explicitly allowed", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-probable-force");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = Librova::Domain::EDuplicateSeverity::Probable,
        .Reason = Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {99}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
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
        "Import continued with explicit probable duplicate override."
    }));
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import removes temporary converter output on cancellation", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-cancel-cleanup");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");
    const auto convertedPath = sandbox.GetPath() / "work" / "converted.epub";
    WriteTextFile(convertedPath, "partial-output");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CStubBookConverter converter;
    converter.Enabled = true;
    converter.Result = {
        .Status = Librova::Domain::EConversionStatus::Cancelled,
        .OutputPath = convertedPath,
        .Warnings = {"Conversion cancelled."}
    };
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        &converter);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Cancelled);
    REQUIRE_FALSE(std::filesystem::exists(convertedPath));
    REQUIRE_FALSE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import stops on cancellation before storage prepare", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-early-cancel");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;
    progressSink.CancellationRequested = true;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Cancelled);
    REQUIRE_FALSE(bookRepository.ReservedId.has_value());
    REQUIRE_FALSE(managedStorage.LastPlan.has_value());
}

TEST_CASE("Single file import rolls back prepared storage when repository add fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-add-failure");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.ThrowOnAdd = true;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Repository add failed.");
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE_FALSE(std::filesystem::exists(managedStorage.LastPreparedStorage->StagedBookPath));
    REQUIRE(bookRepository.RemovedIds.empty());
}

TEST_CASE("Single file import removes persisted book when storage commit fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-commit-failure");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    managedStorage.ThrowOnCommit = true;
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Storage commit failed.");
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE(managedStorage.LastPreparedStorage.has_value());
    REQUIRE_FALSE(std::filesystem::exists(managedStorage.LastPreparedStorage->StagedBookPath));
    REQUIRE(bookRepository.RemovedIds.size() == 1);
    REQUIRE(bookRepository.RemovedIds.front().Value == 1);
}

TEST_CASE("Single file import reports cleanup failure when storage commit fails and catalog rollback also fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-commit-cleanup-double-failure");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.ThrowOnRemove = true;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    managedStorage.ThrowOnCommit = true;
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error.find("cleanup path could not remove partially persisted book 1") != std::string::npos);
    REQUIRE(result.Error.find("Managed storage was left unchanged") != std::string::npos);
    REQUIRE(result.DiagnosticError.find("Storage commit failed.") != std::string::npos);
    REQUIRE(result.DiagnosticError.find("Repository remove failed.") != std::string::npos);
    REQUIRE_FALSE(managedStorage.RollbackCalled);
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook->File.ManagedPath.empty());
    REQUIRE(bookRepository.RemovedIds.empty());
}

TEST_CASE("Single file import surfaces managed-storage rollback restore diagnostics when commit fails", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-commit-rollback-diagnostics");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    managedStorage.ThrowOnCommit = true;
    managedStorage.CommitFailureMessage =
        "Managed storage commit failed: Failed to move file from staged to final. "
        "Rollback could not restore the managed book from 'C:/Library/Objects/5a/68/0000000001.book.fb2' back to staging path "
        "'C:/Library/RuntimeTestStaging/0000000001/book.fb2'.";
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error.find("Managed storage commit failed") != std::string::npos);
    REQUIRE(result.Error.find("Rollback could not restore the managed book") != std::string::npos);
    REQUIRE(result.DiagnosticError == managedStorage.CommitFailureMessage);
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE(bookRepository.RemovedIds.size() == 1);
}

TEST_CASE("Single file import stores library-relative paths in book record", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-relative-paths");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook->File.ManagedPath.is_absolute());
    REQUIRE(bookRepository.AddedBook->File.ManagedPath == std::filesystem::path{"Objects/5a/68/0000000001.book.fb2.gz"});
    REQUIRE(bookRepository.AddedBook->CoverPath.has_value());
    REQUIRE_FALSE(bookRepository.AddedBook->CoverPath->is_absolute());
    REQUIRE(bookRepository.AddedBook->CoverPath == std::filesystem::path{"Objects/5a/68/0000000001.cover.jpg"});
}

TEST_CASE("Single file import keeps detailed parser diagnostics out of transport-facing error text", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-parser-diagnostics");
    const auto sourcePath = sandbox.GetPath() / "invalid.fb2";
    WriteTextFile(sourcePath, "");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Failed);
    REQUIRE(result.Error == "Failed to parse FB2 XML from " + Librova::Unicode::PathToUtf8(sourcePath) + ": No document element found");
    REQUIRE(result.DiagnosticError.find("xml_preview=\"<empty>\"") != std::string::npos);
}

TEST_CASE("Single file import computes and stores SHA-256 when not provided by caller", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-auto-hash");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work"
        // Sha256Hex intentionally not set
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Imported);
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->File.Sha256Hex.size() == 64);
    for (const char ch : bookRepository.AddedBook->File.Sha256Hex)
    {
        REQUIRE(((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')));
    }
}

TEST_CASE("Single file import succeeds and stores empty hash when caller provides empty Sha256Hex", "[importing]")
{
    // Covers the graceful-degradation contract: when sha256 is empty (as it would be
    // if auto-computation failed at runtime), the import must not fail — it simply
    // skips write-side hash deduplication for that book.
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-empty-hash");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = std::string{}
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::Imported);
    REQUIRE(bookRepository.AddedBook.has_value());
    REQUIRE(bookRepository.AddedBook->File.Sha256Hex.empty());
}

TEST_CASE("Single file import returns RejectedDuplicate when hash conflict detected at write time", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-late-hash-reject");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.DuplicateHashConflictId = Librova::Domain::SBookId{42};
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = "deadbeef"
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.DuplicateMatches.size() == 1);
    REQUIRE(result.DuplicateMatches.front().Severity == Librova::Domain::EDuplicateSeverity::Strict);
    REQUIRE(result.DuplicateMatches.front().Reason == Librova::Domain::EDuplicateReason::SameHash);
    REQUIRE(result.DuplicateMatches.front().ExistingBookId.Value == 42);
    REQUIRE_FALSE(bookRepository.ForceAddCalled);
    REQUIRE(bookRepository.RemovedIds.empty());
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE_FALSE(managedStorage.CommitCalled);
}

TEST_CASE("Single file import rejects write-time hash conflicts even when probable duplicate override is enabled", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-late-hash-force");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.DuplicateHashConflictId = Librova::Domain::SBookId{42};
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = "deadbeef",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE_FALSE(bookRepository.ForceAddCalled);
    REQUIRE_FALSE(managedStorage.CommitCalled);
    REQUIRE(managedStorage.RollbackCalled);
    const bool hasStrictDuplicateWarning = std::any_of(
        result.Warnings.begin(), result.Warnings.end(),
        [](const std::string& w) { return w.find("strict duplicate") != std::string::npos; });
    REQUIRE(hasStrictDuplicateWarning);
}

TEST_CASE("Single file import emits exactly one strict duplicate warning when FindDuplicates and late hash check both fire", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-double-warning-guard");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.DuplicateHashConflictId = Librova::Domain::SBookId{99};
    CStubQueryRepository queryRepository;
    queryRepository.Duplicates = {{
        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
        .Reason = Librova::Domain::EDuplicateReason::SameHash,
        .ExistingBookId = Librova::Domain::SBookId{99}
    }};
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = "deadbeef",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE_FALSE(bookRepository.ForceAddCalled);
    const auto strictCount = std::count_if(
        result.Warnings.begin(), result.Warnings.end(),
        [](const std::string& w) { return w.find("strict duplicate") != std::string::npos; });
    REQUIRE(strictCount == 1);
}

TEST_CASE("Single file import rejects write-time hash conflicts without attempting ForceAdd", "[importing]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-importing-force-add-throws");
    const auto sourcePath = CreateFb2Fixture(sandbox.GetPath() / "source.fb2");

    const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
    CStubBookRepository bookRepository;
    bookRepository.DuplicateHashConflictId = Librova::Domain::SBookId{42};
    CStubQueryRepository queryRepository;
    CStubManagedStorage managedStorage(sandbox.GetPath() / "library");
    CTestProgressSink progressSink;

    const Librova::Importing::CSingleFileImportCoordinator coordinator(
        parserRegistry,
        bookRepository,
        queryRepository,
        managedStorage,
        nullptr);

    const auto result = coordinator.Run({
        .SourcePath = sourcePath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .Sha256Hex = "deadbeef",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(managedStorage.RollbackCalled);
    REQUIRE_FALSE(managedStorage.CommitCalled);
    REQUIRE(bookRepository.RemovedIds.empty());
    REQUIRE_FALSE(bookRepository.ForceAddCalled);
}
