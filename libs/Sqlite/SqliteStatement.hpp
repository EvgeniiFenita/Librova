#pragma once

#include <memory>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace LibriFlow::Sqlite {

class CSqliteStatement
{
public:
    CSqliteStatement(sqlite3* connection, std::string_view sql);

    [[nodiscard]] bool Step() const;
    [[nodiscard]] int GetColumnInt(int columnIndex) const;

private:
    struct SStatementFinalizer
    {
        void operator()(sqlite3_stmt* statement) const noexcept;
    };

    std::unique_ptr<sqlite3_stmt, SStatementFinalizer> m_statement;
};

} // namespace LibriFlow::Sqlite
