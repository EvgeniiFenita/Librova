add_library(LibrovaApplicationJobs STATIC
    ImportJobService.cpp
    ImportJobService.hpp
)

target_include_directories(LibrovaApplicationJobs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApplicationJobs
    PUBLIC
        LibrovaApplication
        LibrovaDomain
        LibrovaJobs
)

target_compile_features(LibrovaApplicationJobs PUBLIC cxx_std_20)
