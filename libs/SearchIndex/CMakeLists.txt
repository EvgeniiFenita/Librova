add_library(LibrovaSearchIndex STATIC
    SearchIndexMaintenance.cpp
)

target_include_directories(LibrovaSearchIndex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaSearchIndex PUBLIC
    LibrovaDomain
    LibrovaSqlite
)

target_compile_features(LibrovaSearchIndex PUBLIC cxx_std_20)
