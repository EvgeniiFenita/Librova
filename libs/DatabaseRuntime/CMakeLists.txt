add_library(LibriFlowDatabaseRuntime STATIC
    SchemaMigrator.cpp
)

target_include_directories(LibriFlowDatabaseRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowDatabaseRuntime PUBLIC
    LibriFlowDatabaseSchema
    LibriFlowSqlite
)

target_compile_features(LibriFlowDatabaseRuntime PUBLIC cxx_std_20)
