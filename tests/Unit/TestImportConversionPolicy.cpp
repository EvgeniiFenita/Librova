#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "Domain/ServiceContracts.hpp"
#include "ImportConversion/ImportConversionPolicy.hpp"

namespace {

class CTestConverter final : public LibriFlow::Domain::IBookConverter
{
public:
    explicit CTestConverter(const bool canConvert)
        : m_canConvert(canConvert)
    {
    }

    [[nodiscard]] bool CanConvert(
        const LibriFlow::Domain::EBookFormat sourceFormat,
        const LibriFlow::Domain::EBookFormat destinationFormat) const override
    {
        return m_canConvert
            && sourceFormat == LibriFlow::Domain::EBookFormat::Fb2
            && destinationFormat == LibriFlow::Domain::EBookFormat::Epub;
    }

    [[nodiscard]] LibriFlow::Domain::SConversionResult Convert(
        const LibriFlow::Domain::SConversionRequest&,
        LibriFlow::Domain::IProgressSink&,
        std::stop_token) const override
    {
        return {};
    }

private:
    bool m_canConvert = false;
};

} // namespace

TEST_CASE("Import conversion skips conversion for EPUB input", "[import-conversion]")
{
    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.epub",
        LibriFlow::Domain::EBookFormat::Epub,
        "C:/temp/converted.epub",
        nullptr);

    REQUIRE_FALSE(plan.WillAttemptConversion());
    REQUIRE(plan.FallbackSourcePath == std::filesystem::path("C:/books/source.epub"));
    REQUIRE(plan.FallbackFormat == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE(plan.Warnings.empty());
}

TEST_CASE("Import conversion falls back to original FB2 when converter is unavailable", "[import-conversion]")
{
    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        LibriFlow::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        nullptr);

    REQUIRE_FALSE(plan.WillAttemptConversion());
    REQUIRE(plan.FallbackFormat == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(plan.Warnings == std::vector<std::string>({
        "FB2 converter unavailable. Original FB2 will be stored."
    }));
}

TEST_CASE("Import conversion creates FB2 to EPUB request when converter is available", "[import-conversion]")
{
    const CTestConverter converter(true);

    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        LibriFlow::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter);

    REQUIRE(plan.WillAttemptConversion());
    REQUIRE(plan.Request.has_value());
    REQUIRE(plan.Request->SourcePath == std::filesystem::path("C:/books/source.fb2"));
    REQUIRE(plan.Request->DestinationPath == std::filesystem::path("C:/temp/converted.epub"));
    REQUIRE(plan.Request->DestinationFormat == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE(plan.Warnings.empty());
}

TEST_CASE("Import conversion stores converted output on successful conversion", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        LibriFlow::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter);

    const auto outcome = LibriFlow::ImportConversion::ResolveImportConversion(
        plan,
        LibriFlow::Domain::SConversionResult{
            .Status = LibriFlow::Domain::EConversionStatus::Succeeded,
            .OutputPath = "C:/temp/converted.epub"
        });

    REQUIRE(outcome.Decision == LibriFlow::ImportConversion::EImportConversionDecision::StoreConverted);
    REQUIRE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath == std::filesystem::path("C:/temp/converted.epub"));
    REQUIRE(outcome.Format == LibriFlow::Domain::EBookFormat::Epub);
}

TEST_CASE("Import conversion falls back to original FB2 after failed conversion", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        LibriFlow::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter);

    const auto outcome = LibriFlow::ImportConversion::ResolveImportConversion(
        plan,
        LibriFlow::Domain::SConversionResult{
            .Status = LibriFlow::Domain::EConversionStatus::Failed,
            .Warnings = {"External converter returned exit code 1."}
        });

    REQUIRE(outcome.Decision == LibriFlow::ImportConversion::EImportConversionDecision::StoreSource);
    REQUIRE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath == std::filesystem::path("C:/books/source.fb2"));
    REQUIRE(outcome.Format == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(outcome.Warnings == std::vector<std::string>({
        "External converter returned exit code 1.",
        "FB2 conversion failed. Original FB2 will be stored."
    }));
}

TEST_CASE("Import conversion preserves cancellation instead of silently falling back", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = LibriFlow::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        LibriFlow::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter);

    const auto outcome = LibriFlow::ImportConversion::ResolveImportConversion(
        plan,
        LibriFlow::Domain::SConversionResult{
            .Status = LibriFlow::Domain::EConversionStatus::Cancelled,
            .Warnings = {"Conversion cancelled."}
        });

    REQUIRE(outcome.Decision == LibriFlow::ImportConversion::EImportConversionDecision::CancelImport);
    REQUIRE_FALSE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath.empty());
    REQUIRE(outcome.Format == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(outcome.Warnings == std::vector<std::string>({
        "Conversion cancelled."
    }));
}
