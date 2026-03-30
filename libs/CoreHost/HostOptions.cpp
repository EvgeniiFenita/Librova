#include "CoreHost/HostOptions.hpp"

#include <stdexcept>

#include "ConverterCommand/ConverterCommandBuilder.hpp"

namespace LibriFlow::CoreHost {
namespace {

[[nodiscard]] bool HasValue(const std::vector<std::string>& arguments, const std::size_t index)
{
    return index + 1 < arguments.size();
}

[[nodiscard]] std::size_t ParsePositiveSize(const std::string& value, const std::string& optionName)
{
    try
    {
        const auto parsed = std::stoull(value);
        return static_cast<std::size_t>(parsed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("Invalid numeric value for " + optionName + ".");
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

            options.PipePath = arguments[++index];
            continue;
        }

        if (argument == "--library-root")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --library-root.");
            }

            options.LibraryRoot = arguments[++index];
            continue;
        }

        if (argument == "--serve-one")
        {
            options.MaxSessions = 1;
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

            if (options.ConverterConfiguration.Mode
                == LibriFlow::ConverterConfiguration::EConverterConfigurationMode::CustomCommand)
            {
                throw std::invalid_argument("Built-in and custom converter options cannot be mixed.");
            }

            options.ConverterConfiguration.Mode =
                LibriFlow::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng;
            options.ConverterConfiguration.Fb2Cng.ExecutablePath = arguments[++index];
            continue;
        }

        if (argument == "--fb2cng-config")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --fb2cng-config.");
            }

            if (options.ConverterConfiguration.Mode
                == LibriFlow::ConverterConfiguration::EConverterConfigurationMode::CustomCommand)
            {
                throw std::invalid_argument("Built-in and custom converter options cannot be mixed.");
            }

            options.ConverterConfiguration.Mode =
                LibriFlow::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng;
            options.ConverterConfiguration.Fb2Cng.ConfigPath = std::filesystem::path{arguments[++index]};
            continue;
        }

        if (argument == "--converter-exe")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --converter-exe.");
            }

            if (options.ConverterConfiguration.Mode
                == LibriFlow::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng)
            {
                throw std::invalid_argument("Built-in and custom converter options cannot be mixed.");
            }

            options.ConverterConfiguration.Mode =
                LibriFlow::ConverterConfiguration::EConverterConfigurationMode::CustomCommand;
            options.ConverterConfiguration.Custom.ExecutablePath = arguments[++index];
            continue;
        }

        if (argument == "--converter-arg")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --converter-arg.");
            }

            if (options.ConverterConfiguration.Mode
                == LibriFlow::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng)
            {
                throw std::invalid_argument("Built-in and custom converter options cannot be mixed.");
            }

            options.ConverterConfiguration.Mode =
                LibriFlow::ConverterConfiguration::EConverterConfigurationMode::CustomCommand;
            options.ConverterConfiguration.Custom.ArgumentTemplate.push_back(arguments[++index]);
            continue;
        }

        if (argument == "--converter-output")
        {
            if (!HasValue(arguments, index))
            {
                throw std::invalid_argument("Missing value for --converter-output.");
            }

            if (options.ConverterConfiguration.Mode
                == LibriFlow::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng)
            {
                throw std::invalid_argument("Built-in and custom converter options cannot be mixed.");
            }

            options.ConverterConfiguration.Mode =
                LibriFlow::ConverterConfiguration::EConverterConfigurationMode::CustomCommand;

            const auto& mode = arguments[++index];
            if (mode == "exact")
            {
                options.ConverterConfiguration.Custom.OutputMode =
                    LibriFlow::ConverterCommand::EConverterOutputMode::ExactDestinationPath;
            }
            else if (mode == "directory")
            {
                options.ConverterConfiguration.Custom.OutputMode =
                    LibriFlow::ConverterCommand::EConverterOutputMode::SingleFileInDestinationDirectory;
            }
            else
            {
                throw std::invalid_argument("Unsupported value for --converter-output.");
            }

            continue;
        }

        throw std::invalid_argument("Unknown host option: " + argument);
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

    return options;
}

} // namespace LibriFlow::CoreHost
