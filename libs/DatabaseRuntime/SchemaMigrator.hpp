#pragma once

#include <filesystem>

namespace Librova::DatabaseRuntime {

class CSchemaMigrator
{
public:
    static void Migrate(const std::filesystem::path& databasePath);
    [[nodiscard]] static int ReadUserVersion(const std::filesystem::path& databasePath);
};

} // namespace Librova::DatabaseRuntime
