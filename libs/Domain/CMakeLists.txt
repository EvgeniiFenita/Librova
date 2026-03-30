add_library(LibrovaDomain STATIC
    BookFormat.cpp
    DomainError.cpp
    MetadataNormalization.cpp
)

target_include_directories(LibrovaDomain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(LibrovaDomain PUBLIC cxx_std_20)
