add_library(LibrovaJobs STATIC
    ImportJobManager.cpp
    ImportJobManager.hpp
    ImportJobRunner.cpp
    ImportJobRunner.hpp
)

target_include_directories(LibrovaJobs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaJobs
    PUBLIC
        LibrovaApplication
        LibrovaDomain
)

target_compile_features(LibrovaJobs PUBLIC cxx_std_20)
