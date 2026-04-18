find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(LibrovaDatabase STATIC
    SqliteConnection.cpp
    SqliteStatement.cpp
    DatabaseSchema.cpp
    SchemaMigrator.cpp
    SearchIndexMaintenance.cpp
    SqliteBookQueryRepository.cpp
    SqliteBookRepository.cpp
    SqliteGenreHelpers.cpp
)

target_include_directories(LibrovaDatabase PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaDatabase PUBLIC
    LibrovaDomain
    LibrovaFoundation
    unofficial::sqlite3::sqlite3
)

target_compile_features(LibrovaDatabase PUBLIC cxx_std_20)
