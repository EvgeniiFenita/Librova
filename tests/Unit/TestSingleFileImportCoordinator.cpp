#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
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

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
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
        return false;
    }

    int LastPercent = 0;
    std::string LastMessage;
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
        const std::filesystem::path finalBookPath =
            Root / "Books" / std::to_string(plan.BookId.Value)
            / std::filesystem::path{"book"}.replace_extension(
                plan.Format == LibriFlow::Domain::EBookFormat::Epub ? ".epub" : ".fb2");

        return {
            .StagedBookPath = Root / "Temp" / "book.tmp",
            .StagedCoverPath = plan.CoverSourcePath,
            .FinalBookPath = finalBookPath,
            .FinalCoverPath = plan.CoverSourcePath.has_value() ? std::make_optional(Root / "Covers" / std::to_string(plan.BookId.Value) / "cover.jpg") : std::nullopt
        };
    }

    void CommitImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) override
    {
        LastPreparedStorage = preparedStorage;
        CommitCalled = true;
    }

    void RollbackImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) noexcept override
    {
        LastPreparedStorage = preparedStorage;
        RollbackCalled = true;
    }

    std::filesystem::path Root;
    std::optional<LibriFlow::Domain::SStoragePlan> LastPlan;
    std::optional<LibriFlow::Domain::SPreparedStorage> LastPreparedStorage;
    bool CommitCalled = false;
    bool RollbackCalled = false;
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
