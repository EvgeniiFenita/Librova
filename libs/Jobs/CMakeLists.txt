add_library(LibriFlowJobs STATIC
    ImportJobRunner.cpp
    ImportJobRunner.hpp
)

target_include_directories(LibriFlowJobs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowJobs
    PUBLIC
        LibriFlowApplication
        LibriFlowDomain
)

target_compile_features(LibriFlowJobs PUBLIC cxx_std_20)
