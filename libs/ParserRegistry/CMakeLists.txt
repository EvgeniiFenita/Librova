add_library(LibriFlowParserRegistry STATIC
    BookParserRegistry.cpp
)

target_include_directories(LibriFlowParserRegistry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowParserRegistry PUBLIC
    LibriFlowDomain
    LibriFlowEpubParsing
    LibriFlowFb2Parsing
)

target_compile_features(LibriFlowParserRegistry PUBLIC cxx_std_20)
