add_library(LibriFlowApplication STATIC
    LibraryImportFacade.cpp
    LibraryImportFacade.hpp
)

target_include_directories(LibriFlowApplication
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowApplication
    PUBLIC
        LibriFlowDomain
        LibriFlowImporting
        LibriFlowZipImporting
)

target_compile_features(LibriFlowApplication PUBLIC cxx_std_20)
