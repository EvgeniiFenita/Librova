add_library(LibriFlowBookDatabase STATIC
    SqliteBookRepository.cpp
)

target_include_directories(LibriFlowBookDatabase PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowBookDatabase PUBLIC
    LibriFlowDomain
    LibriFlowSqlite
)

target_compile_features(LibriFlowBookDatabase PUBLIC cxx_std_20)
