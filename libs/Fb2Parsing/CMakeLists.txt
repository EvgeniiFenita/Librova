find_package(pugixml CONFIG REQUIRED)

add_library(LibrovaFb2Parsing STATIC
    Fb2GenreMapper.cpp
    Fb2Parser.cpp
)

target_include_directories(LibrovaFb2Parsing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaFb2Parsing PUBLIC
    LibrovaDomain
    LibrovaUnicode
    LibrovaLogging
    pugixml::pugixml
)

target_compile_features(LibrovaFb2Parsing PUBLIC cxx_std_20)
