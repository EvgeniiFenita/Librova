#include <catch2/catch_test_macros.hpp>

#include "Domain/ServiceContracts.hpp"

namespace {

class CRecordingProgressSink final : public LibriFlow::Domain::IProgressSink
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

class CStubBookParser final : public LibriFlow::Domain::IBookParser
{
public:
    bool CanParse(const LibriFlow::Domain::EBookFormat format) const override
    {
        return format == LibriFlow::Domain::EBookFormat::Epub;
    }

    LibriFlow::Domain::SParsedBook Parse(const std::filesystem::path& filePath) const override
    {
        return {
            .Metadata = {.TitleUtf8 = filePath.filename().string(), .AuthorsUtf8 = {"Parser Author"}},
            .SourceFormat = LibriFlow::Domain::EBookFormat::Epub
        };
    }
};

class CStubBookConverter final : public LibriFlow::Domain::IBookConverter
{
public:
    bool CanConvert(
        const LibriFlow::Domain::EBookFormat sourceFormat,
        const LibriFlow::Domain::EBookFormat destinationFormat) const override
    {
        return sourceFormat == LibriFlow::Domain::EBookFormat::Fb2
            && destinationFormat == LibriFlow::Domain::EBookFormat::Epub;
    }

    LibriFlow::Domain::SConversionResult Convert(
        const LibriFlow::Domain::SConversionRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(100, "Converted");
        return {
            .Succeeded = true,
            .OutputPath = request.DestinationPath
        };
    }
};

class CStubManagedStorage final : public LibriFlow::Domain::IManagedStorage
{
public:
    LibriFlow::Domain::SPreparedStorage PrepareImport(const LibriFlow::Domain::SStoragePlan& plan) override
    {
        return {
            .StagedBookPath = std::filesystem::path{"Temp"} / "book.epub",
            .StagedCoverPath = plan.CoverSourcePath.has_value() ? std::make_optional(std::filesystem::path{"Temp"} / "cover.jpg") : std::nullopt,
            .FinalBookPath = std::filesystem::path{"Books"} / std::to_string(plan.BookId.Value) / "book.epub"
        };
    }

    void CommitImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) override
    {
        LastCommittedPath = preparedStorage.FinalBookPath;
    }

    void RollbackImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) noexcept override
    {
        LastRolledBackPath = preparedStorage.StagedBookPath;
    }

    std::filesystem::path LastCommittedPath;
    std::filesystem::path LastRolledBackPath;
};

class CStubTrashService final : public LibriFlow::Domain::ITrashService
{
public:
    void MoveToTrash(const std::filesystem::path& path) override
    {
        LastTrashedPath = path;
    }

    std::filesystem::path LastTrashedPath;
};

class CStubCoverProvider final : public LibriFlow::Domain::ICoverProvider
{
public:
    std::optional<LibriFlow::Domain::SCoverData> TryResolve(const LibriFlow::Domain::SBookMetadata& metadata) const override
    {
        if (!metadata.HasTitle())
        {
            return std::nullopt;
        }

        return LibriFlow::Domain::SCoverData{
            .Extension = "jpg",
            .Bytes = {std::byte{0x01}, std::byte{0x02}}
        };
    }
};

} // namespace

TEST_CASE("Domain service contract value types expose minimal validity helpers", "[domain][services]")
{
    const LibriFlow::Domain::SConversionRequest request{
        .SourcePath = "source.fb2",
        .DestinationPath = "book.epub",
        .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
        .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
    };

    const LibriFlow::Domain::SStoragePlan storagePlan{
        .BookId = {5},
        .Format = LibriFlow::Domain::EBookFormat::Epub,
        .SourcePath = "source.epub",
        .CoverSourcePath = "source.jpg"
    };

    REQUIRE(request.IsValid());
    REQUIRE(storagePlan.IsValid());
}

TEST_CASE("Parser and converter ports are usable through fake implementations", "[domain][services]")
{
    const CStubBookParser parser;
    const CStubBookConverter converter;
    CRecordingProgressSink progressSink;

    const auto parsed = parser.Parse("book.epub");
    const auto converted = converter.Convert(
        {
            .SourcePath = "book.fb2",
            .DestinationPath = "book.epub",
            .SourceFormat = LibriFlow::Domain::EBookFormat::Fb2,
            .DestinationFormat = LibriFlow::Domain::EBookFormat::Epub
        },
        progressSink,
        {});

    REQUIRE(parser.CanParse(LibriFlow::Domain::EBookFormat::Epub));
    REQUIRE(parsed.Metadata.HasTitle());
    REQUIRE(converter.CanConvert(LibriFlow::Domain::EBookFormat::Fb2, LibriFlow::Domain::EBookFormat::Epub));
    REQUIRE(converted.Succeeded);
    REQUIRE(converted.HasOutput());
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("Storage, trash, and cover ports are usable through fake implementations", "[domain][services]")
{
    CStubManagedStorage storage;
    CStubTrashService trash;
    const CStubCoverProvider coverProvider;

    const LibriFlow::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {9},
        .Format = LibriFlow::Domain::EBookFormat::Epub,
        .SourcePath = "book.epub",
        .CoverSourcePath = "cover.jpg"
    });

    storage.CommitImport(prepared);
    storage.RollbackImport(prepared);
    trash.MoveToTrash("Books/9/book.epub");

    const auto cover = coverProvider.TryResolve({
        .TitleUtf8 = "Roadside Picnic",
        .AuthorsUtf8 = {"Arkady Strugatsky"}
    });

    REQUIRE(prepared.HasStagedBook());
    REQUIRE(prepared.StagedCoverPath.has_value());
    REQUIRE(storage.LastCommittedPath == std::filesystem::path{"Books/9/book.epub"});
    REQUIRE(storage.LastRolledBackPath == std::filesystem::path{"Temp/book.epub"});
    REQUIRE(trash.LastTrashedPath == std::filesystem::path{"Books/9/book.epub"});
    REQUIRE(cover.has_value());
    REQUIRE_FALSE(cover->IsEmpty());
}
