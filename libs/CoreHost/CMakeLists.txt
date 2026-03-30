add_library(LibriFlowCoreHost STATIC
    HostOptions.cpp
    HostOptions.hpp
    LibraryBootstrap.cpp
    LibraryBootstrap.hpp
)

target_include_directories(LibriFlowCoreHost
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowCoreHost
    PUBLIC
        LibriFlowConverterConfiguration
        LibriFlowStoragePlanning
)

target_compile_features(LibriFlowCoreHost PUBLIC cxx_std_20)
