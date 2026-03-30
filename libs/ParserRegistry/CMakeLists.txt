add_library(LibrovaParserRegistry STATIC
    BookParserRegistry.cpp
)

target_include_directories(LibrovaParserRegistry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaParserRegistry PUBLIC
    LibrovaDomain
    LibrovaEpubParsing
    LibrovaFb2Parsing
)

target_compile_features(LibrovaParserRegistry PUBLIC cxx_std_20)
