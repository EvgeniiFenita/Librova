#include <catch2/catch_test_macros.hpp>

#include <format>

#include "Domain/ServiceContracts.hpp"

namespace {

std::uint32_t ComputeBookShardHash(const std::string& bookIdText)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char ch : bookIdText)
    {
        hash ^= ch;
        hash *= 16777619u;
    }

    return hash;
}

class CRecordingProgressSink final : public Librova::Domain::IProgressSink
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

class CStubBookParser final : public Librova::Domain::IBookParser
{
public:
    bool CanParse(const Librova::Domain::EBookFormat format) const override
    {
        return format == Librova::Domain::EBookFormat::Epub;
    }

    Librova::Domain::SParsedBook Parse(
        const std::filesystem::path& filePath,
        std::string_view) const override
    {
        return {
            .Metadata = {.TitleUtf8 = filePath.filename().string(), .AuthorsUtf8 = {"Parser Author"}},
            .SourceFormat = Librova::Domain::EBookFormat::Epub
        };
    }
};

class CStubBookConverter final : public Librova::Domain::IBookConverter
{
public:
    bool CanConvert(
        const Librova::Domain::EBookFormat sourceFormat,
        const Librova::Domain::EBookFormat destinationFormat) const override
    {
        return sourceFormat == Librova::Domain::EBookFormat::Fb2
            && destinationFormat == Librova::Domain::EBookFormat::Epub;
    }

    Librova::Domain::SConversionResult Convert(
        const Librova::Domain::SConversionRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(100, "Converted");
        return {
            .Status = Librova::Domain::EConversionStatus::Succeeded,
            .OutputPath = request.DestinationPath
        };
    }
};

class CStubManagedStorage final : public Librova::Domain::IManagedStorage
{
public:
    Librova::Domain::SPreparedStorage PrepareImport(const Librova::Domain::SStoragePlan& plan) override
    {
        const auto bookIdText = std::format("{:010}", plan.BookId.Value);
        const auto hash = ComputeBookShardHash(bookIdText);
        const auto relativeBookPath = std::filesystem::path{"Objects"}
            / std::format("{:02x}", hash & 0xffu)
            / std::format("{:02x}", (hash >> 8) & 0xffu)
            / (bookIdText + ".book.epub");
        const auto relativeCoverPath = plan.CoverSourcePath.has_value()
            ? std::make_optional(std::filesystem::path{"Objects"}
                / std::format("{:02x}", hash & 0xffu)
                / std::format("{:02x}", (hash >> 8) & 0xffu)
                / (bookIdText + ".cover.jpg"))
            : std::nullopt;
        return {
            .StagedBookPath = std::filesystem::path{"Temp"} / "book.epub",
            .StagedCoverPath = plan.CoverSourcePath.has_value() ? std::make_optional(std::filesystem::path{"Temp"} / "cover.jpg") : std::nullopt,
            .FinalBookPath = relativeBookPath,
            .RelativeBookPath = relativeBookPath,
            .RelativeCoverPath = relativeCoverPath
        };
    }

    void CommitImport(const Librova::Domain::SPreparedStorage& preparedStorage) override
    {
        LastCommittedPath = preparedStorage.FinalBookPath;
    }

    void RollbackImport(const Librova::Domain::SPreparedStorage& preparedStorage) noexcept override
    {
        LastRolledBackPath = preparedStorage.StagedBookPath;
    }

    std::filesystem::path LastCommittedPath;
    std::filesystem::path LastRolledBackPath;
};

class CStubTrashService final : public Librova::Domain::ITrashService
{
public:
    std::filesystem::path MoveToTrash(const std::filesystem::path& path) override
    {
        LastTrashedPath = path;
        LastTrashDestination = std::filesystem::path{"Trash"} / path;
        return LastTrashDestination;
    }

    void RestoreFromTrash(
        const std::filesystem::path& trashedPath,
        const std::filesystem::path& destinationPath) override
    {
        LastRestoredFromPath = trashedPath;
        LastRestoredToPath = destinationPath;
    }

    std::filesystem::path LastTrashedPath;
    std::filesystem::path LastTrashDestination;
    std::filesystem::path LastRestoredFromPath;
    std::filesystem::path LastRestoredToPath;
};

class CStubCoverProvider final : public Librova::Domain::ICoverProvider
{
public:
    std::optional<Librova::Domain::SCoverData> TryResolve(const Librova::Domain::SBookMetadata& metadata) const override
    {
        if (!metadata.HasTitle())
        {
            return std::nullopt;
        }

        return Librova::Domain::SCoverData{
            .Extension = "jpg",
            .Bytes = {std::byte{0x01}, std::byte{0x02}}
        };
    }
};

class CStubRecycleBinService final : public Librova::Domain::IRecycleBinService
{
public:
    void MoveToRecycleBin(const std::vector<std::filesystem::path>& paths) override
    {
        LastMovedPaths = paths;
    }

    std::vector<std::filesystem::path> LastMovedPaths;
};

} // namespace

TEST_CASE("Domain service contract value types expose minimal validity helpers", "[domain][services]")
{
    const Librova::Domain::SConversionRequest request{
        .SourcePath = "source.fb2",
        .DestinationPath = "book.epub",
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    };

    const Librova::Domain::SStoragePlan storagePlan{
        .BookId = {5},
        .Format = Librova::Domain::EBookFormat::Epub,
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

    const auto parsed = parser.Parse("book.epub", {});
    const auto converted = converter.Convert(
        {
            .SourcePath = "book.fb2",
            .DestinationPath = "book.epub",
            .SourceFormat = Librova::Domain::EBookFormat::Fb2,
            .DestinationFormat = Librova::Domain::EBookFormat::Epub
        },
        progressSink,
        {});

    REQUIRE(parser.CanParse(Librova::Domain::EBookFormat::Epub));
    REQUIRE(parsed.Metadata.HasTitle());
    REQUIRE(converter.CanConvert(Librova::Domain::EBookFormat::Fb2, Librova::Domain::EBookFormat::Epub));
    REQUIRE(converted.IsSuccess());
    REQUIRE(converted.Status == Librova::Domain::EConversionStatus::Succeeded);
    REQUIRE(converted.HasOutput());
    REQUIRE(progressSink.LastPercent == 100);
}

TEST_CASE("Storage, trash, and cover ports are usable through fake implementations", "[domain][services]")
{
    CStubManagedStorage storage;
    CStubTrashService trash;
    CStubRecycleBinService recycleBin;
    const CStubCoverProvider coverProvider;

    const Librova::Domain::SPreparedStorage prepared = storage.PrepareImport({
        .BookId = {9},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SourcePath = "book.epub",
        .CoverSourcePath = "cover.jpg"
    });

    storage.CommitImport(prepared);
    storage.RollbackImport(prepared);
    const auto trashPath = trash.MoveToTrash("Objects/c2/5b/0000000009.book.epub");
    trash.RestoreFromTrash(trashPath, "Objects/c2/5b/0000000009.book.epub");
    recycleBin.MoveToRecycleBin({trashPath});

    const auto cover = coverProvider.TryResolve({
        .TitleUtf8 = "Roadside Picnic",
        .AuthorsUtf8 = {"Arkady Strugatsky"}
    });

    REQUIRE(prepared.HasStagedBook());
    REQUIRE(prepared.StagedCoverPath.has_value());
    REQUIRE(storage.LastCommittedPath == std::filesystem::path{"Objects/c2/5b/0000000009.book.epub"});
    REQUIRE(storage.LastRolledBackPath == std::filesystem::path{"Temp/book.epub"});
    REQUIRE(trash.LastTrashedPath == std::filesystem::path{"Objects/c2/5b/0000000009.book.epub"});
    REQUIRE(trash.LastTrashDestination == std::filesystem::path{"Trash/Objects/c2/5b/0000000009.book.epub"});
    REQUIRE(trash.LastRestoredFromPath == std::filesystem::path{"Trash/Objects/c2/5b/0000000009.book.epub"});
    REQUIRE(trash.LastRestoredToPath == std::filesystem::path{"Objects/c2/5b/0000000009.book.epub"});
    REQUIRE(recycleBin.LastMovedPaths == std::vector<std::filesystem::path>{std::filesystem::path{"Trash/Objects/c2/5b/0000000009.book.epub"}});
    REQUIRE(cover.has_value());
    REQUIRE_FALSE(cover->IsEmpty());
}
