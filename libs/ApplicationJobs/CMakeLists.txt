add_library(LibriFlowApplicationJobs STATIC
    ImportJobService.cpp
    ImportJobService.hpp
)

target_include_directories(LibriFlowApplicationJobs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowApplicationJobs
    PUBLIC
        LibriFlowApplication
        LibriFlowDomain
        LibriFlowJobs
)

target_compile_features(LibriFlowApplicationJobs PUBLIC cxx_std_20)
