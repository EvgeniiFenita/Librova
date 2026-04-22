#include "Database/SqliteTimePoint.hpp"

#include <format>
#include <sstream>
#include <stdexcept>

namespace Librova::Sqlite {

std::chrono::system_clock::time_point ParseTimePoint(const std::string_view value)
{
    std::istringstream input{std::string{value}};
    std::chrono::sys_seconds parsedValue{};
    input >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", parsedValue);

    if (input.fail())
    {
        throw std::runtime_error(
            std::string{"Failed to parse sqlite timestamp: "} + std::string{value});
    }

    return parsedValue;
}

std::string SerializeTimePoint(const std::chrono::system_clock::time_point value)
{
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(value));
}

} // namespace Librova::Sqlite
