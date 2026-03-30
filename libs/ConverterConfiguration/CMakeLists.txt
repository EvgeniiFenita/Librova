add_library(LibrovaConverterConfiguration STATIC
    ConverterConfiguration.cpp
    ConverterConfiguration.hpp
)

target_include_directories(LibrovaConverterConfiguration
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaConverterConfiguration
    PUBLIC
        LibrovaConverterCommand
)

target_compile_features(LibrovaConverterConfiguration PUBLIC cxx_std_20)
