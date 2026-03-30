add_library(LibriFlowProtoServices STATIC
    LibraryJobServiceAdapter.cpp
    LibraryJobServiceAdapter.hpp
)

target_include_directories(LibriFlowProtoServices
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowProtoServices
    PUBLIC
        LibriFlowApplicationJobs
        LibriFlowProtoContracts
        LibriFlowProtoMapping
)

target_compile_features(LibriFlowProtoServices PUBLIC cxx_std_20)
