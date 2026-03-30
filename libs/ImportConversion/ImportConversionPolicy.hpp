#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::ImportConversion {

enum class EImportConversionDecision
{
    StoreSource,
    StoreConverted,
    CancelImport
};

struct SImportConversionPlan
{
    std::optional<LibriFlow::Domain::SConversionRequest> Request;
    std::filesystem::path FallbackSourcePath;
    LibriFlow::Domain::EBookFormat FallbackFormat = LibriFlow::Domain::EBookFormat::Epub;
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
    LibriFlow::Domain::EBookFormat Format = LibriFlow::Domain::EBookFormat::Epub;
    std::vector<std::string> Warnings;

    [[nodiscard]] bool IsStorable() const noexcept
    {
        return Decision != EImportConversionDecision::CancelImport && !SourcePath.empty();
    }
};

[[nodiscard]] SImportConversionPlan PlanImportConversion(
    const std::filesystem::path& sourcePath,
    LibriFlow::Domain::EBookFormat sourceFormat,
    const std::filesystem::path& convertedDestinationPath,
    const LibriFlow::Domain::IBookConverter* converter);

[[nodiscard]] SImportConversionOutcome ResolveImportConversion(
    const SImportConversionPlan& plan,
    const std::optional<LibriFlow::Domain::SConversionResult>& conversionResult);

} // namespace LibriFlow::ImportConversion
