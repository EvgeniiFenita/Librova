#include "App/HostOptions.hpp"

#include <format>
#include <limits>
#include <stdexcept>

#include "Converter/ConverterCommandBuilder.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace Librova::CoreHost {
namespace {

[[nodiscard]] bool HasValue(const std::vector<std::string>& arguments, const std::size_t index)
{
    return index + 1 < arguments.size();
}

[[nodiscard]] std::size_t ParsePositiveSize(const std::string& value, const std::string_view optionName)
{
    try
    {
        const auto parsed = std::stoull(value);
        return static_cast<std::size_t>(parsed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument(std::format("Invalid numeric value for {}.", optionName));
    }
}

[[nodiscard]] std::uint32_t ParsePositiveProcessId(const std::string& value, const std::string_view optionName)
{
    try
    {
        const auto parsed = std::stoull(value);
        if (parsed == 0 || parsed > static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max()))
        {
            throw std::invalid_argument("out-of-range");
        }

        return static_cast<std::uint32_t>(parsed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument(std::format("Invalid numeric value for {}.", optionName));
    }
}

[[nodiscard]] std::uint64_t ParsePositiveUnixMilliseconds(const std::string& value, const std::string_view optionName)
{
    try
    {
        const auto parsed = std::stoull(value);
        constexpr auto MaxSupportedUnixMilliseconds =
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (parsed == 0 || parsed > MaxSupportedUnixMilliseconds)
        {
            throw std::invalid_argument("out-of-range");
        }

        return parsed;
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument(std::format("Invalid numeric value for {}.", optionName));
    }
}

} // namespace

SHostOptions CHostOptions::Parse(const std::vector<std::string>& arguments)
{
    SHostOptions options;

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const auto& argument = arguments[index];

        if (argument == "--pipe")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --pipe.");
            }

            options.PipePath = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--help" || argument == "-h")
        {
            options.ShowHelp = true;
            continue;
        }

        if (argument == "--version")
        {
            options.ShowVersion = true;
            continue;
        }

        if (argument == "--library-root")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --library-root.");
            }

            options.LibraryRoot = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--log-file")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --log-file.");
            }

            options.LogFilePath = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--converter-working-dir")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --converter-working-dir.");
            }

            options.ConverterWorkingDirectory = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--managed-storage-staging-root")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --managed-storage-staging-root.");
            }

            options.ManagedStorageStagingRoot = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--shutdown-event")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --shutdown-event.");
            }

            options.ShutdownEventName = arguments[++index];
            continue;
        }

        if (argument == "--library-mode")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --library-mode.");
            }

            const auto& mode = arguments[++index];
            if (mode == "open")
            {
                options.LibraryOpenMode = ELibraryOpenMode::OpenExisting;
            }
            else if (mode == "create")
            {
                options.LibraryOpenMode = ELibraryOpenMode::CreateNew;
            }
            else
            {
                throw std::invalid_argument("Unsupported value for --library-mode.");
            }

            continue;
        }

        if (argument == "--serve-one")
        {
            options.MaxSessions = 1;
            continue;
        }

        if (argument == "--parent-pid")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --parent-pid.");
            }

            options.ParentProcessId = ParsePositiveProcessId(arguments[++index], "--parent-pid");
            continue;
        }

        if (argument == "--parent-start-unix-ms")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --parent-start-unix-ms.");
            }

            options.ParentProcessCreatedAtUnixMs =
                ParsePositiveUnixMilliseconds(arguments[++index], "--parent-start-unix-ms");
            continue;
        }

        if (argument == "--max-sessions")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --max-sessions.");
            }

            options.MaxSessions = ParsePositiveSize(arguments[++index], "--max-sessions");
            continue;
        }

        if (argument == "--fb2cng-exe")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --fb2cng-exe.");
            }

            options.ConverterConfiguration.Mode =
                Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng;
            options.ConverterConfiguration.Fb2Cng.ExecutablePath = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        if (argument == "--fb2cng-config")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --fb2cng-config.");
            }

            options.ConverterConfiguration.Mode =
                Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng;
            options.ConverterConfiguration.Fb2Cng.ConfigPath = Librova::Unicode::PathFromUtf8(arguments[++index]);
            continue;
        }

        throw std::invalid_argument("Unknown host option: " + argument);
    }

    if (options.ShowHelp || options.ShowVersion)
    {
        return options;
    }

    if (options.PipePath.empty())
    {
        throw std::invalid_argument("Missing required option --pipe.");
    }

    if (options.LibraryRoot.empty())
    {
        throw std::invalid_argument("Missing required option --library-root.");
    }

    if (!options.ConverterConfiguration.IsValid())
    {
        throw std::invalid_argument("Invalid converter configuration.");
    }

    if (options.ParentProcessId.has_value() != options.ParentProcessCreatedAtUnixMs.has_value())
    {
        throw std::invalid_argument("--parent-pid and --parent-start-unix-ms must be provided together.");
    }

    return options;
}

} // namespace Librova::CoreHost
