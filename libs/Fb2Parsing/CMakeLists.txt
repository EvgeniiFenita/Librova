find_package(pugixml CONFIG REQUIRED)

add_library(LibriFlowFb2Parsing STATIC
    Fb2Parser.cpp
)

target_include_directories(LibriFlowFb2Parsing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowFb2Parsing PUBLIC
    LibriFlowDomain
    pugixml::pugixml
)

target_compile_features(LibriFlowFb2Parsing PUBLIC cxx_std_20)
