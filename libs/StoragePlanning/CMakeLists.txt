add_library(LibrovaStoragePlanning STATIC
    ManagedLibraryLayout.cpp
)

target_include_directories(LibrovaStoragePlanning PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaStoragePlanning PUBLIC
    LibrovaDomain
)

target_compile_features(LibrovaStoragePlanning PUBLIC cxx_std_20)
