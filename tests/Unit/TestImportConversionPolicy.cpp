#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "Domain/ServiceContracts.hpp"
#include "ImportConversion/ImportConversionPolicy.hpp"

namespace {

class CTestConverter final : public Librova::Domain::IBookConverter
{
public:
    explicit CTestConverter(const bool canConvert)
        : m_canConvert(canConvert)
    {
    }

    [[nodiscard]] bool CanConvert(
        const Librova::Domain::EBookFormat sourceFormat,
        const Librova::Domain::EBookFormat destinationFormat) const override
    {
        return m_canConvert
            && sourceFormat == Librova::Domain::EBookFormat::Fb2
            && destinationFormat == Librova::Domain::EBookFormat::Epub;
    }

    [[nodiscard]] Librova::Domain::SConversionResult Convert(
        const Librova::Domain::SConversionRequest&,
        Librova::Domain::IProgressSink&,
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
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.epub",
        Librova::Domain::EBookFormat::Epub,
        "C:/temp/converted.epub",
        nullptr,
        false);

    REQUIRE_FALSE(plan.WillAttemptConversion());
    REQUIRE(plan.FallbackSourcePath == std::filesystem::path("C:/books/source.epub"));
    REQUIRE(plan.FallbackFormat == Librova::Domain::EBookFormat::Epub);
    REQUIRE(plan.Warnings.empty());
}

TEST_CASE("Import conversion keeps original FB2 when forced conversion is disabled", "[import-conversion]")
{
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        nullptr,
        false);

    REQUIRE_FALSE(plan.WillAttemptConversion());
    REQUIRE(plan.FallbackFormat == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(plan.Warnings.empty());
}

TEST_CASE("Import conversion falls back to original FB2 when forced conversion is enabled but converter is unavailable", "[import-conversion]")
{
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        nullptr,
        true);

    REQUIRE_FALSE(plan.WillAttemptConversion());
    REQUIRE(plan.FallbackFormat == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(plan.Warnings == std::vector<std::string>({
        "FB2 converter unavailable. Original FB2 will be stored."
    }));
}

TEST_CASE("Import conversion creates FB2 to EPUB request when converter is available and forced conversion is enabled", "[import-conversion]")
{
    const CTestConverter converter(true);

    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter,
        true);

    REQUIRE(plan.WillAttemptConversion());
    REQUIRE(plan.Request.has_value());
    REQUIRE(plan.Request->SourcePath == std::filesystem::path("C:/books/source.fb2"));
    REQUIRE(plan.Request->DestinationPath == std::filesystem::path("C:/temp/converted.epub"));
    REQUIRE(plan.Request->DestinationFormat == Librova::Domain::EBookFormat::Epub);
    REQUIRE(plan.Warnings.empty());
}

TEST_CASE("Import conversion stores converted output on successful conversion", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter,
        true);

    const auto outcome = Librova::ImportConversion::ResolveImportConversion(
        plan,
        Librova::Domain::SConversionResult{
            .Status = Librova::Domain::EConversionStatus::Succeeded,
            .OutputPath = "C:/temp/converted.epub"
        });

    REQUIRE(outcome.Decision == Librova::ImportConversion::EImportConversionDecision::StoreConverted);
    REQUIRE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath == std::filesystem::path("C:/temp/converted.epub"));
    REQUIRE(outcome.Format == Librova::Domain::EBookFormat::Epub);
}

TEST_CASE("Import conversion falls back to original FB2 after failed conversion", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter,
        true);

    const auto outcome = Librova::ImportConversion::ResolveImportConversion(
        plan,
        Librova::Domain::SConversionResult{
            .Status = Librova::Domain::EConversionStatus::Failed,
            .Warnings = {"External converter returned exit code 1."}
        });

    REQUIRE(outcome.Decision == Librova::ImportConversion::EImportConversionDecision::StoreSource);
    REQUIRE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath == std::filesystem::path("C:/books/source.fb2"));
    REQUIRE(outcome.Format == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(outcome.Warnings == std::vector<std::string>({
        "External converter returned exit code 1.",
        "FB2 conversion failed. Original FB2 will be stored."
    }));
}

TEST_CASE("Import conversion preserves cancellation instead of silently falling back", "[import-conversion]")
{
    const CTestConverter converter(true);
    const auto plan = Librova::ImportConversion::PlanImportConversion(
        "C:/books/source.fb2",
        Librova::Domain::EBookFormat::Fb2,
        "C:/temp/converted.epub",
        &converter,
        true);

    const auto outcome = Librova::ImportConversion::ResolveImportConversion(
        plan,
        Librova::Domain::SConversionResult{
            .Status = Librova::Domain::EConversionStatus::Cancelled,
            .Warnings = {"Conversion cancelled."}
        });

    REQUIRE(outcome.Decision == Librova::ImportConversion::EImportConversionDecision::CancelImport);
    REQUIRE_FALSE(outcome.IsStorable());
    REQUIRE(outcome.SourcePath.empty());
    REQUIRE(outcome.Format == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(outcome.Warnings == std::vector<std::string>({
        "Conversion cancelled."
    }));
}
