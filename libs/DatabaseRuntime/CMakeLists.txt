add_library(LibrovaDatabaseRuntime STATIC
    SchemaMigrator.cpp
)

target_include_directories(LibrovaDatabaseRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaDatabaseRuntime PUBLIC
    LibrovaDatabaseSchema
    LibrovaSqlite
)

target_compile_features(LibrovaDatabaseRuntime PUBLIC cxx_std_20)
