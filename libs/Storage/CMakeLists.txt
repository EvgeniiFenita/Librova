find_package(ZLIB REQUIRED)
find_package(Stb REQUIRED)

add_library(LibrovaStorage STATIC
    ManagedPathSafety.cpp
    ManagedPathSafety.hpp
    ManagedLibraryLayout.cpp
    ManagedLibraryLayout.hpp
    ManagedFileEncoding.cpp
    ManagedFileEncoding.hpp
    ManagedFileStorage.cpp
    ManagedFileStorage.hpp
    ManagedTrashService.cpp
    ManagedTrashService.hpp
    WindowsRecycleBinService.cpp
    WindowsRecycleBinService.hpp
    StbCoverImageProcessor.cpp
    StbCoverImageProcessor.hpp
)

target_include_directories(LibrovaStorage
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
    PRIVATE
        ${Stb_INCLUDE_DIR}
)

target_link_libraries(LibrovaStorage
    PUBLIC
        LibrovaDomain
        LibrovaFoundation
    PRIVATE
        ZLIB::ZLIB
        shell32
)

target_compile_features(LibrovaStorage PUBLIC cxx_std_20)
