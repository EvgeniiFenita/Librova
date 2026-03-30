add_library(LibriFlowApplicationClient STATIC
    ImportJobClient.cpp
    ImportJobClient.hpp
)

target_include_directories(LibriFlowApplicationClient
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowApplicationClient
    PUBLIC
        LibriFlowApplicationJobs
        LibriFlowPipeClient
        LibriFlowProtoMapping
)

target_compile_features(LibriFlowApplicationClient PUBLIC cxx_std_20)
