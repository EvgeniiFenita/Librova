find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(LibrovaEpubParsing STATIC
    EpubParser.cpp
)

target_include_directories(LibrovaEpubParsing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaEpubParsing PUBLIC
    LibrovaDomain
    LibrovaUnicode
    libzip::zip
    pugixml::pugixml
)

target_compile_features(LibrovaEpubParsing PUBLIC cxx_std_20)
