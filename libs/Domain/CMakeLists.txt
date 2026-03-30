add_library(LibriFlowDomain STATIC
    BookFormat.cpp
)

target_include_directories(LibriFlowDomain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibriFlowDomain PUBLIC cxx_std_20)
