add_library(LibriFlowProtoMapping STATIC
    ImportJobProtoMapper.cpp
    ImportJobProtoMapper.hpp
)

target_include_directories(LibriFlowProtoMapping
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowProtoMapping
    PUBLIC
        LibriFlowApplication
        LibriFlowApplicationJobs
        LibriFlowDomain
        LibriFlowProtoContracts
)

target_compile_features(LibriFlowProtoMapping PUBLIC cxx_std_20)
