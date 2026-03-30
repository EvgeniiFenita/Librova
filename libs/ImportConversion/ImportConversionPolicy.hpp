#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"

namespace Librova::ImportConversion {

enum class EImportConversionDecision
{
    StoreSource,
    StoreConverted,
    CancelImport
};

struct SImportConversionPlan
{
    std::optional<Librova::Domain::SConversionRequest> Request;
    std::filesystem::path FallbackSourcePath;
    Librova::Domain::EBookFormat FallbackFormat = Librova::Domain::EBookFormat::Epub;
    std::vector<std::string> Warnings;

    [[nodiscard]] bool WillAttemptConversion() const noexcept
    {
        return Request.has_value();
    }
};

struct SImportConversionOutcome
{
    EImportConversionDecision Decision = EImportConversionDecision::StoreSource;
    std::filesystem::path SourcePath;
    Librova::Domain::EBookFormat Format = Librova::Domain::EBookFormat::Epub;
    std::vector<std::string> Warnings;

    [[nodiscard]] bool IsStorable() const noexcept
    {
        return Decision != EImportConversionDecision::CancelImport && !SourcePath.empty();
    }
};

[[nodiscard]] SImportConversionPlan PlanImportConversion(
    const std::filesystem::path& sourcePath,
    Librova::Domain::EBookFormat sourceFormat,
    const std::filesystem::path& convertedDestinationPath,
    const Librova::Domain::IBookConverter* converter);

[[nodiscard]] SImportConversionOutcome ResolveImportConversion(
    const SImportConversionPlan& plan,
    const std::optional<Librova::Domain::SConversionResult>& conversionResult);

} // namespace Librova::ImportConversion
