add_library(LibrovaManagedPaths STATIC
    ManagedPathSafety.cpp
    ManagedPathSafety.hpp
)

target_include_directories(LibrovaManagedPaths PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_compile_features(LibrovaManagedPaths PUBLIC cxx_std_20)
