add_library(LibrovaConverter STATIC
    ConverterCommandBuilder.cpp
    ConverterConfiguration.cpp
    ExternalBookConverter.cpp
)

target_include_directories(LibrovaConverter PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaConverter
    PUBLIC
        LibrovaDomain
    PRIVATE
        LibrovaFoundation
)

target_compile_features(LibrovaConverter PUBLIC cxx_std_20)
