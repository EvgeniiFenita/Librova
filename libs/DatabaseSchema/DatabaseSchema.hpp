#pragma once

#include <string_view>
#include <vector>

namespace LibriFlow::DatabaseSchema {

class CDatabaseSchema
{
public:
    [[nodiscard]] static int GetCurrentVersion() noexcept;
    [[nodiscard]] static const std::vector<std::string_view>& GetMigrationStatements();
    [[nodiscard]] static std::string_view GetCreateSchemaScript() noexcept;
};

} // namespace LibriFlow::DatabaseSchema
