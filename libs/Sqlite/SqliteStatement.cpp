#include "Sqlite/SqliteStatement.hpp"

#include <stdexcept>
#include <string>

#include <sqlite3.h>

namespace LibriFlow::Sqlite {

CSqliteStatement::CSqliteStatement(sqlite3* connection, const std::string_view sql)
{
    sqlite3_stmt* rawStatement = nullptr;
    const int prepareResult = sqlite3_prepare_v2(
        connection,
        sql.data(),
        static_cast<int>(sql.size()),
        &rawStatement,
        nullptr);

    if (prepareResult != SQLITE_OK)
    {
        throw std::runtime_error(std::string{"Failed to prepare sqlite statement: "} + sqlite3_errmsg(connection));
    }

    m_statement.reset(rawStatement);
}

bool CSqliteStatement::Step() const
{
    const int stepResult = sqlite3_step(m_statement.get());

    if (stepResult == SQLITE_ROW)
    {
        return true;
    }

    if (stepResult == SQLITE_DONE)
    {
        return false;
    }

    throw std::runtime_error("Failed to step sqlite statement.");
}

int CSqliteStatement::GetColumnInt(const int columnIndex) const
{
    return sqlite3_column_int(m_statement.get(), columnIndex);
}

void CSqliteStatement::SStatementFinalizer::operator()(sqlite3_stmt* statement) const noexcept
{
    if (statement != nullptr)
    {
        sqlite3_finalize(statement);
    }
}

} // namespace LibriFlow::Sqlite
