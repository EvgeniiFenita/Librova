add_library(LibrovaManagedTrash STATIC
    ManagedTrashService.cpp
    ManagedTrashService.hpp
)

target_include_directories(LibrovaManagedTrash PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaManagedTrash PUBLIC
    LibrovaDomain
    LibrovaManagedPaths
    LibrovaStoragePlanning
)

target_compile_features(LibrovaManagedTrash PUBLIC cxx_std_20)
