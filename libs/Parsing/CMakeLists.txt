find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(LibrovaParsing STATIC
    EpubParser.cpp
    Fb2GenreMapper.cpp
    Fb2Parser.cpp
    BookParserRegistry.cpp
)

target_include_directories(LibrovaParsing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaParsing PUBLIC
    LibrovaDomain
    LibrovaFoundation
    libzip::zip
    pugixml::pugixml
)

target_compile_features(LibrovaParsing PUBLIC cxx_std_20)
