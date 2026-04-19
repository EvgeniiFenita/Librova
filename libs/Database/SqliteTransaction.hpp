#pragma once

#include "Database/SqliteConnection.hpp"

namespace Librova::Sqlite {

class CSqliteTransaction final
{
public:
    explicit CSqliteTransaction(const CSqliteConnection& connection)
        : m_connection(connection)
    {
        m_connection.Execute("BEGIN IMMEDIATE;");
    }

    ~CSqliteTransaction()
    {
        if (!m_committed)
        {
            try
            {
                m_connection.Execute("ROLLBACK;");
            }
            catch (...)
            {
            }
        }
    }

    CSqliteTransaction(const CSqliteTransaction&) = delete;
    CSqliteTransaction& operator=(const CSqliteTransaction&) = delete;

    void Commit()
    {
        m_connection.Execute("COMMIT;");
        m_committed = true;
    }

private:
    const CSqliteConnection& m_connection;
    bool m_committed = false;
};

} // namespace Librova::Sqlite
