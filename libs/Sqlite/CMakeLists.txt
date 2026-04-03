find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(LibrovaSqlite STATIC
    SqliteConnection.cpp
    SqliteStatement.cpp
)

target_include_directories(LibrovaSqlite PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaSqlite PUBLIC
    LibrovaDatabaseSchema
    LibrovaUnicode
    unofficial::sqlite3::sqlite3
)

target_compile_features(LibrovaSqlite PUBLIC cxx_std_20)
