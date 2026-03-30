find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(LibriFlowSqlite STATIC
    SqliteConnection.cpp
    SqliteStatement.cpp
)

target_include_directories(LibriFlowSqlite PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowSqlite PUBLIC
    LibriFlowDatabaseSchema
    unofficial::sqlite3::sqlite3
)

target_compile_features(LibriFlowSqlite PUBLIC cxx_std_20)
