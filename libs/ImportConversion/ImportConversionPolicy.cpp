#include "ImportConversion/ImportConversionPolicy.hpp"

#include <utility>

namespace {

constexpr auto GConverterUnavailableWarning = "FB2 converter unavailable. Original FB2 will be stored.";
constexpr auto GConversionFailedWarning = "FB2 conversion failed. Original FB2 will be stored.";
constexpr auto GMissingConversionResultWarning = "FB2 conversion result is missing. Original FB2 will be stored.";

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

    if (converter == nullptr
        || !converter->CanConvert(
            Librova::Domain::EBookFormat::Fb2,
            Librova::Domain::EBookFormat::Epub))
    {
        plan.Warnings.emplace_back(GConverterUnavailableWarning);
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
        return outcome;
    }

    if (!conversionResult.has_value())
    {
        outcome.Warnings.emplace_back(GMissingConversionResultWarning);
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

    outcome.Warnings.emplace_back(GConversionFailedWarning);
    return outcome;
}

} // namespace Librova::ImportConversion
