add_library(LibrovaDatabaseSchema STATIC
    DatabaseSchema.cpp
)

target_include_directories(LibrovaDatabaseSchema PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibrovaDatabaseSchema PUBLIC cxx_std_20)
