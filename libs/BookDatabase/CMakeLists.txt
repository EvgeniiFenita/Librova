add_library(LibrovaBookDatabase STATIC
    SqliteBookQueryRepository.cpp
    SqliteBookRepository.cpp
)

target_include_directories(LibrovaBookDatabase PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaBookDatabase PUBLIC
    LibrovaDomain
    LibrovaSearchIndex
    LibrovaSqlite
)

target_compile_features(LibrovaBookDatabase PUBLIC cxx_std_20)
