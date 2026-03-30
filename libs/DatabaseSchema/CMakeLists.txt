add_library(LibriFlowDatabaseSchema STATIC
    DatabaseSchema.cpp
)

target_include_directories(LibriFlowDatabaseSchema PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibriFlowDatabaseSchema PUBLIC cxx_std_20)
