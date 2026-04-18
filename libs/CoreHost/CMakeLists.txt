add_library(LibrovaCoreHost STATIC
    HostOptions.cpp
    HostOptions.hpp
    LibraryBootstrap.cpp
    LibraryBootstrap.hpp
)

target_include_directories(LibrovaCoreHost
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaCoreHost
    PUBLIC
        LibrovaConverter
        LibrovaFoundation
        LibrovaManagedPaths
        LibrovaStoragePlanning
)

target_compile_features(LibrovaCoreHost PUBLIC cxx_std_20)
