find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(LibriFlowEpubParsing STATIC
    EpubParser.cpp
)

target_include_directories(LibriFlowEpubParsing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowEpubParsing PUBLIC
    LibriFlowDomain
    libzip::zip
    pugixml::pugixml
)

target_compile_features(LibriFlowEpubParsing PUBLIC cxx_std_20)
