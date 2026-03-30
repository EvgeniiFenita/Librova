add_library(LibrovaProtoServices STATIC
    LibraryJobServiceAdapter.cpp
    LibraryJobServiceAdapter.hpp
)

target_include_directories(LibrovaProtoServices
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaProtoServices
    PUBLIC
        LibrovaApplication
        LibrovaApplicationJobs
        LibrovaProtoContracts
        LibrovaProtoMapping
)

target_compile_features(LibrovaProtoServices PUBLIC cxx_std_20)
