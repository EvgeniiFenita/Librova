add_library(LibriFlowSearchIndex STATIC
    SearchIndexMaintenance.cpp
)

target_include_directories(LibriFlowSearchIndex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowSearchIndex PUBLIC
    LibriFlowDomain
    LibriFlowSqlite
)

target_compile_features(LibriFlowSearchIndex PUBLIC cxx_std_20)
