#include "Import/ImportConversionPolicy.hpp"

#include <utility>

namespace {

constexpr auto GRequiredConverterUnavailableError = "Forced FB2 to EPUB conversion requires a configured converter.";
constexpr auto GRequiredConversionFailedError = "Forced FB2 to EPUB conversion failed.";
constexpr auto GMissingConversionResultError = "Forced FB2 to EPUB conversion did not return a result.";

void AppendWarnings(
    std::vector<std::string>& target,
    const std::vector<std::string>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

} // namespace

namespace Librova::ImportConversion {

SImportConversionPlan PlanImportConversion(
    const std::filesystem::path& sourcePath,
    const Librova::Domain::EBookFormat sourceFormat,
    const std::filesystem::path& convertedDestinationPath,
    const Librova::Domain::IBookConverter* converter,
    const bool forceEpubConversion)
{
    SImportConversionPlan plan{
        .FallbackSourcePath = sourcePath,
        .FallbackFormat = sourceFormat
    };

    if (sourceFormat != Librova::Domain::EBookFormat::Fb2 || !forceEpubConversion)
    {
        return plan;
    }

    plan.RequiresSuccessfulConversion = true;

    if (converter == nullptr
        || !converter->CanConvert(
            Librova::Domain::EBookFormat::Fb2,
            Librova::Domain::EBookFormat::Epub))
    {
        return plan;
    }

    plan.Request = Librova::Domain::SConversionRequest{
        .SourcePath = sourcePath,
        .DestinationPath = convertedDestinationPath,
        .SourceFormat = Librova::Domain::EBookFormat::Fb2,
        .DestinationFormat = Librova::Domain::EBookFormat::Epub
    };

    return plan;
}

SImportConversionOutcome ResolveImportConversion(
    const SImportConversionPlan& plan,
    const std::optional<Librova::Domain::SConversionResult>& conversionResult)
{
    SImportConversionOutcome outcome{
        .Decision = EImportConversionDecision::StoreSource,
        .SourcePath = plan.FallbackSourcePath,
        .Format = plan.FallbackFormat,
        .Warnings = plan.Warnings
    };

    if (!plan.WillAttemptConversion())
    {
        if (plan.RequiresSuccessfulConversion)
        {
            outcome.Decision = EImportConversionDecision::FailImport;
            outcome.SourcePath.clear();
            outcome.Error = GRequiredConverterUnavailableError;
        }

        return outcome;
    }

    if (!conversionResult.has_value())
    {
        outcome.Decision = EImportConversionDecision::FailImport;
        outcome.SourcePath.clear();
        outcome.Error = GMissingConversionResultError;
        return outcome;
    }

    AppendWarnings(outcome.Warnings, conversionResult->Warnings);

    if (conversionResult->IsCancelled())
    {
        outcome.Decision = EImportConversionDecision::CancelImport;
        outcome.SourcePath.clear();
        return outcome;
    }

    if (conversionResult->IsSuccess() && conversionResult->HasOutput())
    {
        outcome.Decision = EImportConversionDecision::StoreConverted;
        outcome.SourcePath = conversionResult->OutputPath;
        outcome.Format = plan.Request->DestinationFormat;
        return outcome;
    }

    outcome.Decision = EImportConversionDecision::FailImport;
    outcome.SourcePath.clear();
    outcome.Error = GRequiredConversionFailedError;
    return outcome;
}

} // namespace Librova::ImportConversion
