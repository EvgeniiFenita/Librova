#pragma once

#include <filesystem>

namespace LibriFlow::DatabaseRuntime {

class CSchemaMigrator
{
public:
    static void Migrate(const std::filesystem::path& databasePath);
    [[nodiscard]] static int ReadUserVersion(const std::filesystem::path& databasePath);
};

} // namespace LibriFlow::DatabaseRuntime
