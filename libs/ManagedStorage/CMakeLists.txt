add_library(LibrovaManagedStorage STATIC
    ManagedFileStorage.cpp
)

target_include_directories(LibrovaManagedStorage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaManagedStorage PUBLIC
    LibrovaDomain
    LibrovaManagedPaths
    LibrovaStoragePlanning
)

target_compile_features(LibrovaManagedStorage PUBLIC cxx_std_20)
